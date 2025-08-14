#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"
#include "native.h"
#include "builtin.h"
#include "routine.h"
#include "channel.h"
#include "ws2812_native.h"
#include "yargtype.h"

VM vm;

void vmPinnedRoutineHandler(size_t handler) {

    ObjRoutine* routine = vm.pinnedRoutines[handler];

    run(routine);

    Value result = pop(routine); // unused.

    prepareRoutineStack(routine);

    return;
}

void pinnedRoutine0(void) { vmPinnedRoutineHandler(0); }
void pinnedRoutine1(void) { vmPinnedRoutineHandler(1); }
void pinnedRoutine2(void) { vmPinnedRoutineHandler(2); }
void pinnedRoutine3(void) { vmPinnedRoutineHandler(3); }
void pinnedRoutine4(void) { vmPinnedRoutineHandler(4); }
void pinnedRoutine5(void) { vmPinnedRoutineHandler(5); }
void pinnedRoutine6(void) { vmPinnedRoutineHandler(6); }
void pinnedRoutine7(void) { vmPinnedRoutineHandler(7); }
void pinnedRoutine8(void) { vmPinnedRoutineHandler(8); }
void pinnedRoutine9(void) { vmPinnedRoutineHandler(9); }

size_t pinnedRoutineIndex(uintptr_t handler) {
    for (size_t i = 0; i < MAX_PINNED_ROUTINES; i++) {
        if (vm.pinnedRoutineHandlers[i] == (PinnedRoutineHandler)handler) {
            return i;
        }
    }
    return MAX_PINNED_ROUTINES;
}

void fatalVMError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "fatal VM error\n");
    exit(5);
}

static void defineNative(const char* name, NativeFn function) {
    tempRootPush(OBJ_VAL(copyString(name, (int)strlen(name))));
    tempRootPush(OBJ_VAL(newNative(function)));
    ValueCell cell;
    cell.value = vm.tempRoots[1];
    cell.type = NIL_VAL;
    tableCellSet(&vm.globals, AS_STRING(vm.tempRoots[0]), cell);
    tempRootPop();
    tempRootPop();
}

void initVM() {
    
    // We have an Obj here not on the heap. hack up its init.
    vm.core0.obj.type = OBJ_ROUTINE;
    vm.core0.obj.isMarked = false;
    vm.core0.obj.next = NULL;
    initRoutine(&vm.core0, ROUTINE_THREAD);

    vm.core1 = NULL;
    for (int i = 0; i < MAX_PINNED_ROUTINES; i++) {
        vm.pinnedRoutines[i] = NULL;
    }
    vm.pinnedRoutineHandlers[0] = pinnedRoutine0;
    vm.pinnedRoutineHandlers[1] = pinnedRoutine1;
    vm.pinnedRoutineHandlers[2] = pinnedRoutine2;
    vm.pinnedRoutineHandlers[3] = pinnedRoutine3;
    vm.pinnedRoutineHandlers[4] = pinnedRoutine4;
    vm.pinnedRoutineHandlers[5] = pinnedRoutine5;
    vm.pinnedRoutineHandlers[6] = pinnedRoutine6;
    vm.pinnedRoutineHandlers[7] = pinnedRoutine7;
    vm.pinnedRoutineHandlers[8] = pinnedRoutine8;
    vm.pinnedRoutineHandlers[9] = pinnedRoutine9;

    platform_mutex_init(&vm.heap);

    vm.tempRootsTop = vm.tempRoots;

    vm.objects = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC = FIRST_GC_AT;

    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;

    initCellTable(&vm.globals);
    initTable(&vm.strings);

    vm.initString = NULL;
    vm.initString = copyString("init", 4);

    defineNative("clock", clockNative);
#ifdef CYARG_PICO_TARGET
    defineNative("sleep_ms", sleepNative);

    defineNative("alarm_add_in_ms", alarmAddInMSNative);
    defineNative("alarm_add_repeating_ms", alarmAddRepeatingMSNative);
    defineNative("alarm_cancel_repeating", alarmCancelRepeatingMSNative);
#endif
    defineNative("ws2812_init", ws2812initNative);
    defineNative("ws2812_write_pixel", ws2812writepixelNative);

    defineNative("irq_remove_handler", irq_remove_handlerNative);
    defineNative("irq_add_shared_handler", irq_add_shared_handlerNative);

}

void freeVM() {
    freeCellTable(&vm.globals);
    freeTable(&vm.strings);
    vm.initString = NULL;
    freeObjects();
}

bool callfn(ObjRoutine* routine, ObjClosure* closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError(routine, "Expected %d arguments but got %d.",
                     closure->function->arity, argCount);
        return false;
    }

    if (routine->frameCount == FRAMES_MAX) {
        runtimeError(routine, "Stack overflow.");
        return false;
    }

    CallFrame* frame = &routine->frames[routine->frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = routine->stackTop - argCount - 1;
    return true;
}

static bool callValue(ObjRoutine* routine, Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                routine->stackTop[-argCount - 1].value = bound->reciever;
                routine->stackTop[-argCount - 1].type = NIL_VAL;
                return callfn(routine, bound->method, argCount);
            }
            case OBJ_CLASS: {
                ObjClass* klass = AS_CLASS(callee);
                routine->stackTop[-argCount  - 1].value = OBJ_VAL(newInstance(klass));
                routine->stackTop[-argCount  - 1].type = NIL_VAL;
                Value initializer;
                if (tableGet(&klass->methods, vm.initString, &initializer)) {
                    return callfn(routine, AS_CLOSURE(initializer), argCount);
                } else if (argCount != 0) {
                    runtimeError(routine, "Expected 0 arguments but got %d.", argCount);
                    return false;
                }
                return true;
            }
            case OBJ_CLOSURE:
                return callfn(routine, AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = NIL_VAL; 
                if (native(routine, argCount, (routine->stackTop - argCount), &result)) {
                    routine->stackTop -= argCount + 1;
                    push(routine, result);
                    return true;
                } else {
                    return false;
                }
            }
            default:
                break; // Non-callable object type.
        }
    }
    runtimeError(routine, "Can only call functions and classes.");
    return false;
}

static bool invokeFromClass(ObjRoutine* routine, ObjClass* klass, ObjString* name,
                            int argCount) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError(routine, "Undefined property '%s'.", name->chars);
        return false;
    }
    return callfn(routine, AS_CLOSURE(method), argCount);
}

static bool invoke(ObjRoutine* routine, ObjString* name, int argCount) {
    Value receiver = peek(routine, argCount);

    if (!IS_INSTANCE(receiver)) {
        runtimeError(routine, "Only instances have methods.");
        return false;
    }

    ObjInstance* instance = AS_INSTANCE(receiver);

    Value value;
    if (tableGet(&instance->fields, name, &value)) {
        routine->stackTop[-argCount - 1].value = value;
        routine->stackTop[-argCount - 1].type = NIL_VAL;
        return callValue(routine, value, argCount);
    }

    return invokeFromClass(routine, instance->klass, name, argCount);
}

static bool bindMethod(ObjRoutine* routine, ObjClass* klass, ObjString* name) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError(routine, "Undefined property '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod* bound = newBoundMethod(peek(routine, 0), AS_CLOSURE(method));
    pop(routine);
    push(routine, OBJ_VAL(bound));
    return true;
}

static ObjUpvalue* captureUpvalue(ObjRoutine* routine, ValueCell* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = routine->openUpvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) {
        routine->openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalues(ObjRoutine* routine, ValueCell* last) {
    while (routine->openUpvalues != NULL && routine->openUpvalues->location >= last) {
        ObjUpvalue* upvalue = routine->openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        routine->openUpvalues = upvalue->next;
    }
}

static void defineMethod(ObjRoutine* routine, ObjString* name) {
    Value method = peek(routine, 0);
    ObjClass* klass = AS_CLASS(peek(routine, 1));
    tableSet(&klass->methods, name, method);
    pop(routine);
}

static bool derefElement(ObjRoutine* routine) {
    if (!isArray(peek(routine, 1)) || !is_positive_integer(peek(routine, 0))) {
        runtimeError(routine, "Expected an array and a positive or unsigned integer.");
        return false;
    }
    uint32_t index = as_positive_integer(peek(routine, 0));
    Value result = NIL_VAL;

    if (IS_VALARRAY(peek(routine, 1))) {
        ObjValArray* array = AS_VALARRAY(peek(routine, 1));
        if (index >= array->array.count) {
            runtimeError(routine, "Array index %d out of bounds (0:%d)", index, array->array.count - 1);
            return false;
        }

        result = array->array.values[index];
    } else if (IS_UNIFORMARRAY(peek(routine, 1))) {
        ObjUniformArray* array = AS_UNIFORMARRAY(peek(routine, 1));
        if (index >= array->count) {
            runtimeError(routine, "Array index %d out of bounds (0:%d)", index, array->count - 1);
            return false;
        }
        uint32_t* entries = (uint32_t*) array->array;

        result = UINTEGER_VAL(entries[index]);
    }
    pop(routine);
    pop(routine);
    push(routine, result);
    return true;
}

static bool setArrayElement(ObjRoutine* routine) {
    if (!isArray(peek(routine, 2)) || !is_positive_integer(peek(routine, 1))) {
        runtimeError(routine, "Expected an array and a positive or unsigned integer.");
        return false;
    }

    Value new_value = pop(routine);
    uint32_t index = as_positive_integer(pop(routine));
    Value boxed_array = pop(routine);

    if (IS_VALARRAY(boxed_array)) {
        ObjValArray* array = AS_VALARRAY(boxed_array);

        if (index >= array->array.count || index < 0) {
            runtimeError(routine, "Array index %d out of bounds (0:%d)", index, array->array.count - 1);
            return false;
        }

        array->array.values[index] = new_value;
    } else if (IS_UNIFORMARRAY(boxed_array)) {
        ObjUniformArray* array = AS_UNIFORMARRAY(boxed_array);
        if (index >= array->count || index < 0) {
            runtimeError(routine, "Array index %d out of bounds (0:%d)", index, array->count - 1);
            return false;
        }
        if (!IS_UINTEGER(new_value)) {
            runtimeError(routine, "Expected a MachineUint32.");
            return false;
        }
        uint32_t* entries = (uint32_t*) array->array;
        
        entries[index] = AS_UINTEGER(new_value);
    }

    push(routine, boxed_array);
    return true;
}

static bool derefPtr(ObjRoutine* routine) {
    Value ptr = peek(routine, 0);
    ObjPointer* pointer = AS_POINTER(ptr);
    ObjConcreteYargType* target = AS_YARGTYPE(pointer->type);

    Value result = NIL_VAL;

    if (IS_NIL(pointer->type)
        || target->yt == TypeAny
        || target->yt == TypeBool
        || target->yt == TypeInteger
        || target->yt == TypeDouble) {
        Value* remote = (Value*)pointer->destination;
        result = *remote;
    } else if (target->yt == TypeMachineUint32) {
        uint32_t* remote = (uint32_t*)pointer->destination;
        result = UINTEGER_VAL(*remote);
    } else {
        Obj** remote = (Obj**)pointer->destination;
        if (*remote == NULL) {
            result = NIL_VAL;
        } else {
            result = OBJ_VAL(*remote);
        }
    }
    pop(routine);
    push(routine, result);
    return true;
}

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(ObjRoutine* routine) {
    ObjString* b = AS_STRING(peek(routine, 0));
    ObjString* a = AS_STRING(peek(routine, 1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    pop(routine);
    pop(routine);
    push(routine, OBJ_VAL(result));
}

static bool assignTo(ValueCellTarget lhs, Value rhsValue) {
    if (IS_NIL(*lhs.type)) {
        *lhs.value = rhsValue;
        return true;
    } else {
        ObjConcreteYargType* lhsType = (ObjConcreteYargType*) AS_OBJ(*lhs.type);
        if (isCompatibleType(lhsType, rhsValue)) {
            *(lhs.value) = rhsValue;
            return true;
        } else {
            return false;
        }
    }
}

static bool initialiseTo(ValueCellTarget lhs, Value rhsValue) {
    if (IS_NIL(*lhs.type)) {
        *lhs.value = rhsValue;
        return true;
    } else {
        ObjConcreteYargType* lhsType = (ObjConcreteYargType*) AS_OBJ(*lhs.type);
        if (isInitialisableType(lhsType, rhsValue)) {
            *(lhs.value) = rhsValue;
            return true;
        } else {
            return false;
        }
    }
}

static void createConcreteType(ObjRoutine* routine, ConcreteYargType type) {
    ObjConcreteYargType* typeObj = newYargTypeFromType(type);
    push(routine, OBJ_VAL(typeObj));
}

static void makeConcreteTypeConst(ObjRoutine* routine) {
    if (IS_NIL(peek(routine, 0))) {
        pop(routine);
        ObjConcreteYargType* typeObject = newYargTypeFromType(TypeAny);
        typeObject->isConst = true;
        push(routine, OBJ_VAL(typeObject));
        return;
    } else {
        ObjConcreteYargType* typeObj = (ObjConcreteYargType*) AS_OBJ(peek(routine, 0));
        typeObj->isConst = true;
        return;
    }
}

static void makeConcreteTypeArray(ObjRoutine* routine) {
    ObjConcreteYargType* typeObject = newYargArrayTypeFromType(peek(routine, 0));
    pop(routine);
    push(routine, OBJ_VAL(typeObject));
}

InterpretResult run(ObjRoutine* routine) {
    CallFrame* frame = &routine->frames[routine->frameCount - 1];
    routine->state = EXEC_RUNNING;

#define READ_BYTE() (*frame->ip++)

#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])

#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(routine, valueType, op) \
    do { \
        if (IS_UINTEGER(peek(routine, 0)) && IS_UINTEGER(peek(routine, 1))) { \
            uint32_t b = AS_UINTEGER(pop(routine)); \
            uint32_t a = AS_UINTEGER(pop(routine)); \
            push(routine, valueType(a op b)); \
        } else if (IS_INTEGER(peek(routine, 0)) && IS_INTEGER(peek(routine, 1))) { \
            int32_t b = AS_INTEGER(pop(routine)); \
            int32_t a = AS_INTEGER(pop(routine)); \
            push(routine, valueType(a op b)); \
        } else if (IS_DOUBLE(peek(routine, 0)) && IS_DOUBLE(peek(routine, 1))) { \
            double b = AS_DOUBLE(pop(routine)); \
            double a = AS_DOUBLE(pop(routine)); \
            push(routine, valueType(a op b)); \
        } else { \
            runtimeError(routine, "Operands must both be numbers, integers or unsigned integers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
    } while (false)
#define BINARY_INTGRAL_OP(routine, valueType, op) \
    do { \
        if (IS_UINTEGER(peek(routine, 0)) && IS_UINTEGER(peek(routine, 1))) { \
            uint32_t b = AS_UINTEGER(pop(routine)); \
            uint32_t a = AS_UINTEGER(pop(routine)); \
            push(routine, valueType(a op b)); \
        } else if (IS_INTEGER(peek(routine, 0)) && IS_INTEGER(peek(routine, 1))) { \
            int32_t b = AS_INTEGER(pop(routine)); \
            int32_t a = AS_INTEGER(pop(routine)); \
            push(routine, valueType(a op b)); \
        } else { \
            runtimeError(routine, "Operands must both be integers or unsigned integers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
    } while (false)
#define BINARY_UINT_OP(routine, valueType, op) \
    do { \
        if (!IS_UINTEGER(peek(routine, 0)) || !IS_UINTEGER(peek(routine, 1))) { \
            runtimeError(routine, "Operands must be unsigned integers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        uint32_t b = AS_UINTEGER(pop(routine)); \
        uint32_t a = AS_UINTEGER(pop(routine)); \
        uint32_t c = a op b; \
        push(routine, valueType(c)); \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printValueStack(routine, "          ");
        disassembleInstruction(&frame->closure->function->chunk, 
                               (int)(frame->ip - frame->closure->function->chunk.code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(routine, constant);
                break;
            }
            case OP_IMMEDIATE: {
                uint8_t byte = READ_BYTE();
                Value constant = INTEGER_VAL((int8_t)byte);
                push(routine, constant);
                break;
            }
            case OP_NIL: push(routine, NIL_VAL); break;
            case OP_TRUE: push(routine, BOOL_VAL(true)); break;
            case OP_FALSE: push(routine, BOOL_VAL(false)); break;
            case OP_POP: pop(routine); break;
            case OP_GET_BUILTIN: {
                uint8_t builtin = READ_BYTE();
                Value bFn = getBuiltin(builtin);
                push(routine, bFn);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                ValueCell* rhs = peekCell(routine, 0);
                ValueCell* lhs = &frame->slots[slot];
                ValueCellTarget trg = { .type = &lhs->type, .value = &lhs->value };

                if (!assignTo(trg, rhs->value)) {
                    runtimeError(routine, "Cannot set local variable to incompatible type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(routine, frame->slots[slot].value);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                ValueCell cell;
                if (!tableCellGet(&vm.globals, name, &cell)) {
                    runtimeError(routine, "Undefined variable (OP_GET_GLOBAL) '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(routine, cell.value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableCellSet(&vm.globals, name, *peekCell(routine, 0));
                pop(routine);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                ValueCell* lhs = NULL;
                if (tableCellGetPlace(&vm.globals, name, &lhs)) {
                    ValueCell* rhs = peekCell(routine, 0);
                    ValueCellTarget trg = { .type = &lhs->type, .value = &lhs->value };

                    if (!assignTo(trg, rhs->value)) {
                        runtimeError(routine, "Cannot set global variable to incompatible type.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    runtimeError(routine, "Undefined variable (OP_SET_GLOBAL) '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_INITIALISE: {
                ValueCell* lhs = peekCell(routine, 1);
                ValueCell* rhs = peekCell(routine, 0);
                ValueCellTarget trg = { .type = &lhs->type, .value = &lhs->value };
                if (!initialiseTo(trg, rhs->value)) {
                    runtimeError(routine, "Cannot initialise variable with this value.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                pop(routine);
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(routine, frame->closure->upvalues[slot]->location->value);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                ValueCell* rhs = peekCell(routine, 0);
                ValueCellTarget trg = { 
                    .type = &frame->closure->upvalues[slot]->location->type, 
                    .value = &frame->closure->upvalues[slot]->location->value 
                };

                if (!assignTo(trg, rhs->value)) {
                    runtimeError(routine, "Cannot set local variable to incompatible type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(peek(routine, 0)) && !IS_STRUCT(peek(routine, 0))) {
                    runtimeError(routine, "Only instances and structs have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (IS_INSTANCE(peek(routine, 0))) {
                    ObjInstance* instance = AS_INSTANCE(peek(routine, 0));
                    ObjString* name = READ_STRING();

                    Value value;
                    if (tableGet(&instance->fields, name, &value)) {
                        pop(routine); // Instance
                        push(routine, value);
                        break;
                    }

                    if (!bindMethod(routine, instance->klass, name)) {
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else if (IS_STRUCT(peek(routine, 0))) {
                    ObjStruct* object = AS_STRUCT(peek(routine, 0));
                    ObjConcreteYargTypeStruct* type = (ObjConcreteYargTypeStruct*)object->type;
                    ObjString* name = READ_STRING();

                    Value indexVal;
                    Value result = NIL_VAL;
                    if (tableGet(&type->field_names, name, &indexVal)) {
                        uint32_t index = AS_UINTEGER(indexVal);
                        result = object->fields[index];
                    } else {
                        runtimeError(routine, "field not present in struct.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    pop(routine);
                    push(routine, result);
                }
                break;
            }
            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(routine, 1)) && !IS_STRUCT(peek(routine, 1))) {
                    runtimeError(routine, "Only instances and structs have fields.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (IS_INSTANCE(peek(routine, 1))) {
                    ObjInstance* instance = AS_INSTANCE(peek(routine, 1));
                    tableSet(&instance->fields, READ_STRING(), peek(routine, 0));
                    Value value = pop(routine);
                    pop(routine);
                    push(routine, value);
                } else if (IS_STRUCT(peek(routine, 1))) {
                    ObjStruct* object = AS_STRUCT(peek(routine, 1));
                    ObjConcreteYargTypeStruct* type = (ObjConcreteYargTypeStruct*)object->type;
                    ObjString* name = READ_STRING();
                    
                    Value indexVal;
                    Value result = peek(routine, 0);
                    if (tableGet(&type->field_names, name, &indexVal)) {
                        uint32_t index = AS_UINTEGER(indexVal);
                        ValueCellTarget trg = { .type = &type->field_types[index], .value = &object->fields[index] };
                        if (!assignTo(trg, result)) {
                            runtimeError(routine, "cannot assign to field type.");
                        }
                    } else {
                        runtimeError(routine, "field not present in struct.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    pop(routine);                    
                    pop(routine);
                    push(routine, result);
                }
                break;
            }
            case OP_GET_SUPER: {
                ObjString* name = READ_STRING();
                ObjClass* superclass = AS_CLASS(pop(routine));

                if (!bindMethod(routine, superclass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_EQUAL: {
                Value b = pop(routine);
                Value a = pop(routine);
                push(routine, BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER:  BINARY_OP(routine, BOOL_VAL, >); break;
            case OP_LESS:     BINARY_OP(routine, BOOL_VAL, <); break;
            case OP_LEFT_SHIFT:  BINARY_UINT_OP(routine, UINTEGER_VAL, <<); break;
            case OP_RIGHT_SHIFT: BINARY_UINT_OP(routine, UINTEGER_VAL, >>); break;
            case OP_BITOR:       BINARY_UINT_OP(routine, UINTEGER_VAL, |); break;
            case OP_BITAND:      BINARY_UINT_OP(routine, UINTEGER_VAL, &); break;
            case OP_BITXOR:      BINARY_UINT_OP(routine, UINTEGER_VAL, ^); break;
            case OP_ADD: {
                if (IS_STRING(peek(routine, 0)) && IS_STRING(peek(routine, 1))) {
                    concatenate(routine);
                } else if (IS_INTEGER(peek(routine, 0)) && IS_INTEGER(peek(routine, 1))) {
                    int32_t b = AS_INTEGER(pop(routine));
                    int32_t a = AS_INTEGER(pop(routine));
                    push(routine, INTEGER_VAL(a + b));
                } else if (IS_UINTEGER(peek(routine, 0)) && IS_UINTEGER(peek(routine, 1))) {
                    uint32_t b = AS_UINTEGER(pop(routine));
                    uint32_t a = AS_UINTEGER(pop(routine));
                    push(routine, UINTEGER_VAL(a + b));
                } else if (IS_DOUBLE(peek(routine, 0)) && IS_DOUBLE(peek(routine, 1))) {
                    double b = AS_DOUBLE(pop(routine));
                    double a = AS_DOUBLE(pop(routine));
                    push(routine, DOUBLE_VAL(a + b));
                } else {
                    runtimeError(routine, "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_MODULO: {
                if (IS_INTEGER(peek(routine, 0)) && IS_INTEGER(peek(routine, 1))) {
                    int32_t b = AS_INTEGER(pop(routine));
                    int32_t a = AS_INTEGER(pop(routine));
                    int32_t r = a % b;
                    if (r < 0) {
                        r += b;
                    }
                    push(routine, INTEGER_VAL(r));
                } else if (IS_UINTEGER(peek(routine, 0)) && IS_UINTEGER(peek(routine, 1))) {
                    BINARY_INTGRAL_OP(routine, UINTEGER_VAL, %);
                } else {
                    runtimeError(routine, "Operands must integers or unsigned integers of same type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: {
                if (IS_INTEGER(peek(routine, 0)) && IS_INTEGER(peek(routine, 1))) {
                    BINARY_OP(routine, INTEGER_VAL, -);
                } else if (IS_UINTEGER(peek(routine, 0)) && IS_UINTEGER(peek(routine, 1))) {
                    BINARY_OP(routine, UINTEGER_VAL, -);
                } else if (IS_DOUBLE(peek(routine, 0)) && IS_DOUBLE(peek(routine, 1))) {
                    BINARY_OP(routine, DOUBLE_VAL, -);
                } else {
                    runtimeError(routine, "Operands must be of same type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_MULTIPLY: {
                if (IS_INTEGER(peek(routine, 0)) && IS_INTEGER(peek(routine, 1))) {
                    BINARY_OP(routine, INTEGER_VAL, *);
                } else if (IS_UINTEGER(peek(routine, 0)) && IS_UINTEGER(peek(routine, 1))) {
                    BINARY_OP(routine, UINTEGER_VAL, *);
                } else if (IS_DOUBLE(peek(routine, 0)) && IS_DOUBLE(peek(routine, 1))) {
                    BINARY_OP(routine, DOUBLE_VAL, *);
                } else {
                    runtimeError(routine, "Operands must be of same type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_DIVIDE: {
                if (IS_INTEGER(peek(routine, 0)) && IS_INTEGER(peek(routine, 1))) {
                    BINARY_OP(routine, INTEGER_VAL, /);
                } else if (IS_UINTEGER(peek(routine, 0)) && IS_UINTEGER(peek(routine, 1))) {
                    BINARY_OP(routine, UINTEGER_VAL, /);
                } else if (IS_DOUBLE(peek(routine, 0)) && IS_DOUBLE(peek(routine, 1))) {
                    BINARY_OP(routine, DOUBLE_VAL, /);
                } else {
                    runtimeError(routine, "Operands must be of same type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_NOT:
                push(routine, BOOL_VAL(isFalsey(pop(routine))));
                break;
            case OP_NEGATE: {
                if (IS_DOUBLE(peek(routine, 0))) {
                    push(routine, DOUBLE_VAL(-AS_DOUBLE(pop(routine))));
                } else if (IS_INTEGER(peek(routine, 0))) {
                    push(routine, INTEGER_VAL(-AS_INTEGER(pop(routine))));
                } else {
                    runtimeError(routine, "Operand must be a number or integer.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_PRINT: {
                printValue(pop(routine));
                printf("\n");
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(routine, 0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(routine, peek(routine, argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &routine->frames[routine->frameCount - 1];
                break;
            }
            case OP_INVOKE: {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                if (!invoke(routine, method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &routine->frames[routine->frameCount - 1];
                break;
            }
            case OP_SUPER_INVOKE: {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                ObjClass* superclass = AS_CLASS(pop(routine));
                if (!invokeFromClass(routine, superclass, method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &routine->frames[routine->frameCount - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(function);
                push(routine, OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(routine, frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpvalues(routine, routine->stackTop - 1);
                pop(routine);
                break;
            case OP_YIELD: {
                if (routine == &vm.core0) {
                    runtimeError(routine, "Cannot yield from initial routine.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                routine->state = EXEC_SUSPENDED;
                return INTERPRET_OK;
            }
            case OP_RETURN: {
                Value result = pop(routine);
                closeUpvalues(routine, frame->slots);
                routine->frameCount--;
                if (routine->frameCount == 0) {
                    pop(routine);
                    push(routine, result);
                    routine->state = EXEC_CLOSED;
                    return INTERPRET_OK;
                }

                routine->stackTop = frame->slots;
                push(routine, result);
                frame = &routine->frames[routine->frameCount - 1];
                break;
            }
            case OP_CLASS:
                push(routine, OBJ_VAL(newClass(READ_STRING())));
                break;
            case OP_INHERIT: {
                Value superclass = peek(routine, 1);
                if (!IS_CLASS(superclass)) {
                    runtimeError(routine, "Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjClass* subclass = AS_CLASS(peek(routine, 0));
                tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
                pop(routine); // Subclass.
                break;
            }
            case OP_METHOD:
                defineMethod(routine, READ_STRING());
                break;
            case OP_ELEMENT: {
                if (!derefElement(routine)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_ELEMENT: {
                if (!setArrayElement(routine)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_TYPE_LITERAL: {
                uint8_t typeCode = READ_BYTE();
                switch (typeCode) {
                    case TYPE_LITERAL_BOOL: createConcreteType(routine, TypeBool); break;
                    case TYPE_LITERAL_INTEGER: createConcreteType(routine, TypeInteger); break;
                    case TYPE_LITERAL_MACHINE_UINT32: createConcreteType(routine, TypeMachineUint32); break;
                    case TYPE_LITERAL_MACHINE_FLOAT64: createConcreteType(routine, TypeDouble); break;
                    case TYPE_LITERAL_STRING: createConcreteType(routine, TypeString); break;
                }
                break;
            }
            case OP_TYPE_MODIFIER: {
                uint8_t typeCode = READ_BYTE();
                switch (typeCode) {
                    case TYPE_MODIFIER_CONST: makeConcreteTypeConst(routine); break;
                }
                break;
            }
            case OP_TYPE_STRUCT: {
                uint8_t fieldCount = READ_BYTE();
                ObjConcreteYargTypeStruct* st = (ObjConcreteYargTypeStruct*) newYargStructType(fieldCount);
                tempRootPush(OBJ_VAL(st));
                for (uint8_t i = 0; i < fieldCount; i++) {
                    addFieldType(st, i, peek(routine, 1), peek(routine, 0));
                    pop(routine);
                    pop(routine);
                }
                push(routine, OBJ_VAL(st));
                tempRootPop();
                break;
            }
            case OP_SET_CELL_TYPE: {
                ValueCell* last = peekCell(routine, 0);
                last->type = peek(routine, 0);
                last->value = defaultValue(last->type);
                break;
            }
            case OP_DEREF_PTR: {
                derefPtr(routine);
                break;
            }
            case OP_SET_PTR_TARGET: {
                Value rhs = peek(routine, 0);
                ValueCell* lhs = peekCell(routine, 1);
                ObjPointer* pLhs = AS_POINTER(lhs->value);
                Value* remote = (Value*)pLhs->destination;
                ValueCellTarget trg = { .type = &pLhs->type, .value = remote };
                if (assignTo(trg, rhs)) {
                    pop(routine);
                    pop(routine);
                    push(routine, rhs);
                } else {
                    runtimeError(routine, "Cannot set pointer target to incompatible type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_PLACE: {
                Value location = peek(routine, 0);
                Value type = peek(routine, 1);
                ObjPointer* pointer = newPointerAt(type, location);
                pop(routine);
                pop(routine);
                push(routine, OBJ_VAL(pointer));
                break;
            }
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

#ifdef DEBUG_TRACE_EXECUTION
    disassembleChunk(&function->chunk, "<script>");
    for (int i = 0; i < function->chunk.constants.count; i++) {
        if (IS_FUNCTION(function->chunk.constants.values[i])) {
            ObjFunction* fun = AS_FUNCTION(function->chunk.constants.values[i]);
            disassembleChunk(&fun->chunk, fun->name->chars);
        }
    }
#endif

    tempRootPush(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    tempRootPop();

    bindEntryFn(&vm.core0, closure);

    push(&vm.core0, OBJ_VAL(closure));
    callfn(&vm.core0, closure, 0);

    InterpretResult result = run(&vm.core0);

    pop(&vm.core0);

    return result;
}