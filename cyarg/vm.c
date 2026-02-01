#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef CYARG_PICO_SDK_TARGET
#include <pico/multicore.h>
#endif

#ifdef CYARG_FEATURE_TEST_SYSTEM
#include "test-system/testSystem.h"
#endif

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
#include "yargtype.h"

VM vm;

static void binaryIntOp(ObjRoutine* routine, char const *c);
static void binaryIntBoolOp(ObjRoutine* routine, char const *c);

void vmPinnedRoutineHandler(size_t handler) {
    ObjRoutine* routine = vm.pinnedRoutines[handler];
    runAndRenter(routine);
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

bool installPinnedRoutine(ObjRoutine* pinnedRoutine, uintptr_t* address) {
    for (size_t i = 0; i < MAX_PINNED_ROUTINES; i++) {
        if (vm.pinnedRoutines[i] == NULL) {
            vm.pinnedRoutines[i] = pinnedRoutine;
            *address = (uintptr_t)vm.pinnedRoutineHandlers[i];
            return true;
        }
    }
    return false;
}

bool removePinnedRoutine(uintptr_t address) {
    for (size_t i = 0; i < MAX_PINNED_ROUTINES; i++) {
        if (vm.pinnedRoutineHandlers[i] == (PinnedRoutineHandler)address) {
            vm.pinnedRoutineHandlers[i] = NULL;
            return true;
        }
    }
    return false;
}

// cyarg: use ascii 'y' 'a' 'r' 'g'
#define FLAG_VALUE 0x79617267

void vmCore1Entry() {
#ifdef CYARG_PICO_SDK_TARGET
    multicore_fifo_push_blocking(FLAG_VALUE);
    uint32_t g = multicore_fifo_pop_blocking();

    if (g != FLAG_VALUE) {
        fatalVMError("Core1 entry and sync failed.");
    }
#endif

    ObjRoutine* core = vm.core1;
    runAndRenter(core);
    vm.core1 = NULL;
}

void runOnCore1(ObjRoutine* routine) {
#ifdef CYARG_PICO_SDK_TARGET
    vm.core1 = routine;

    vm.core1->state = EXEC_RUNNING;
    multicore_reset_core1();
    multicore_launch_core1(vmCore1Entry);

    // Wait for it to start up
    uint32_t g = multicore_fifo_pop_blocking();
    if (g != FLAG_VALUE) {
        fatalVMError("Core1 startup failure.");
        return;
    }
    multicore_fifo_push_blocking(FLAG_VALUE);
#else
#endif
}

void fatalVMError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    FPRINTMSG(stderr, "fatal VM error\n");
    exit(5);
}

static void defineNative(const char* name, NativeFn function) {
    tempRootPush(OBJ_VAL(copyString(name, (int)strlen(name))));
    tempRootPush(OBJ_VAL(newNative(function)));
    ValueCell cell;
    cell.value = vm.tempRoots[1];
    cell.cellType = NULL;
    tableCellSet(&vm.globals, AS_STRING(vm.tempRoots[0]), cell);
    tempRootPop();
    tempRootPop();
}

void initVM() {
    
    // must be done before initRoutine as initRoutine does a realloc
    platform_mutex_init(&vm.heap);
    platform_mutex_init(&vm.env);

    // We have an Obj here not on the heap. hack up its init.
    vm.core0.obj.type = OBJ_ROUTINE;
    vm.core0.obj.isMarked = false;
    vm.core0.obj.next = NULL;
    initRoutine(&vm.core0);

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

    vm.tempRootsTop = vm.tempRoots;

    vm.objects = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC = FIRST_GC_AT;

    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;

    initCellTable(&vm.globals);
    initTable(&vm.strings);
    initTable(&vm.imports);

    vm.initString = NULL;
    vm.initString = copyString("init", 4);

    defineNative("clock", clockNative);
    defineNative("c_clock_get_hz", clock_get_hzNative);

    defineNative("irq_remove_handler", irq_remove_handlerNative);
    defineNative("irq_add_shared_handler", irq_add_shared_handlerNative);

}

void freeVM() {
    freeCellTable(&vm.globals);
    freeTable(&vm.strings);
    freeTable(&vm.imports);
    vm.initString = NULL;
    freeObjects();
}

void markVMRoots() {
    
    // Don't use markObject, as this is not on the heap.
    markRoutine(&vm.core0);
    
    markObject((Obj*)vm.core1);

    for (int i = 0; i < MAX_PINNED_ROUTINES; i++) {
        markObject((Obj*)vm.pinnedRoutines[i]);
    }

    for (Value* slot = vm.tempRoots; slot < vm.tempRootsTop; slot++) {
        markValue(*slot);
    }

    markTable(&vm.imports);
    markCellTable(&vm.globals);
    markObject((Obj*)vm.initString);
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
    frame->stackEntryIndex = routine->stackTopIndex - (argCount + 1);
    return true;
}

static bool callValue(ObjRoutine* routine, Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                ValueCell* target = peekCell(routine, argCount);
                target->value = bound->reciever;
                target->cellType = NULL;
                return callfn(routine, bound->method, argCount);
            }
            case OBJ_CLASS: {
                ObjClass* klass = AS_CLASS(callee);
                ValueCell* target = peekCell(routine, argCount);
                target->value = OBJ_VAL(newInstance(klass));
                target->cellType = NULL;
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
                if (native == importBuiltinDummy) {
                    return importBuiltin(routine, argCount);
                } else if (native == execBuiltinDummy) {
                    return execBuiltin(routine, argCount);
                } else {
                    Value result = NIL_VAL; 
                    if (native(routine, argCount, &result)) {
                        popN(routine, argCount + 1);
                        push(routine, result);
                        return true;
                    } else {
                        return false;
                    }
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
        ValueCell* target = peekCell(routine, argCount);

        target->value = value;
        target->cellType = NULL;
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

static ObjUpvalue* captureUpvalue(ObjRoutine* routine, ValueCell* local, size_t stackOffset) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = routine->openUpvalues;
    while (upvalue != NULL && upvalue->stackOffset > stackOffset) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->stackOffset == stackOffset) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = newUpvalue(local, stackOffset);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) {
        routine->openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalues(ObjRoutine* routine, size_t last) {
    while (routine->openUpvalues != NULL && routine->openUpvalues->stackOffset >= last) {
        ObjUpvalue* upvalue = routine->openUpvalues;
        upvalue->closed = *upvalue->contents;
        upvalue->contents = &upvalue->closed;
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
    if (!(IS_UNIFORMARRAY(peek(routine, 1)) || isArrayPointer(peek(routine, 1))) || !is_positive_integer(peek(routine, 0))) {
        runtimeError(routine, "Expected an array and a positive or unsigned integer.");
        return false;
    }
    size_t index = as_positive_integer(peek(routine, 0));
    Value result = NIL_VAL;

    if (IS_UNIFORMARRAY(peek(routine, 1))) {
        ObjPackedUniformArray* array = AS_UNIFORMARRAY(peek(routine, 1));
        if (index >= arrayCardinality(array->store)) {
            runtimeError(routine, "Array index %d out of bounds.", index);
            return false;
        }
        PackedValue element = arrayElement(array->store, index);
        result = unpackValue(element);

    } else {
        ObjPackedUniformArray* arrayObj = (ObjPackedUniformArray*)destinationObject(peek(routine, 1));
        if (index >= arrayCardinality(arrayObj->store)) {
            runtimeError(routine, "Array index %d out of bounds (0:%zu)", index, arrayCardinality(arrayObj->store) - 1);
            return false;
        }
        tempRootPush(OBJ_VAL(arrayObj));

        PackedValue element = arrayElement(arrayObj->store, index);
        result = OBJ_VAL(newPointerAtHeapCell(element));
        tempRootPop();
    }

    pop(routine);
    pop(routine);
    push(routine, result);
    return true;
}

static bool setArrayElement(ObjRoutine* routine) {
    if (!IS_UNIFORMARRAY(peek(routine, 2)) || !is_positive_integer(peek(routine, 1))) {
        runtimeError(routine, "Expected an array and a positive or unsigned integer.");
        return false;
    }

    Value new_value = peek(routine, 0);
    uint32_t index = as_positive_integer(peek(routine, 1));
    Value boxed_array = peek(routine, 2);

    if (IS_UNIFORMARRAY(boxed_array)) {
        ObjPackedUniformArray* array = AS_UNIFORMARRAY(boxed_array);
        if (index >= arrayCardinality(array->store)) {
            runtimeError(routine, "Array index %d out of bounds (0:%d)", index, arrayCardinality(array->store) - 1);
            return false;
        }

        PackedValue trg = arrayElement(array->store, index);
        if (!assignToPackedValue(trg, new_value)) {
            runtimeError(routine, "Cannot set array element to incompatible type.");
            return false;
        }
    }

    pop(routine);
    pop(routine);
    return true;
}

static void derefPtr(ObjRoutine* routine) {
    Value pointerVal = peek(routine, 0);

    ObjPackedPointer* pointer = AS_POINTER(pointerVal);
    PackedValue dest;
    dest.storedType = pointer->type->target_type;
    dest.storedValue = pointer->destination;
    Value result = unpackValue(dest);

    pop(routine);
    push(routine, result);
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

//static void makeConcreteTypeArray(ObjRoutine* routine) {
//    ObjConcreteYargType* typeObject = newYargArrayTypeFromType(peek(routine, 0));
//    pop(routine);
//    push(routine, OBJ_VAL(typeObject));
//}

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
#define BINARY_OP(routine, op) \
    do { \
        if (IS_I32(peek(routine, 0)) && IS_I32(peek(routine, 1))) { \
            int32_t b = AS_I32(pop(routine)); \
            int32_t a = AS_I32(pop(routine)); \
            push(routine, I32_VAL(a op b)); \
        } else if (IS_UI32(peek(routine, 0)) && IS_UI32(peek(routine, 1))) { \
            uint32_t b = AS_UI32(pop(routine)); \
            uint32_t a = AS_UI32(pop(routine)); \
            push(routine, UI32_VAL(a op b)); \
        } else if (IS_I8(peek(routine, 0)) && IS_I8(peek(routine, 1))) { \
            int8_t b = AS_I8(pop(routine)); \
            int8_t a = AS_I8(pop(routine)); \
            push(routine, I8_VAL(a op b)); \
        } else if (IS_UI8(peek(routine, 0)) && IS_UI8(peek(routine, 1))) { \
            uint8_t b = AS_UI8(pop(routine)); \
            uint8_t a = AS_UI8(pop(routine)); \
            push(routine, UI8_VAL(a op b)); \
        } else if (IS_I16(peek(routine, 0)) && IS_I16(peek(routine, 1))) { \
            int16_t b = AS_I16(pop(routine)); \
            int16_t a = AS_I16(pop(routine)); \
            push(routine, I16_VAL(a op b)); \
        } else if (IS_UI16(peek(routine, 0)) && IS_UI16(peek(routine, 1))) { \
            uint16_t b = AS_UI16(pop(routine)); \
            uint16_t a = AS_UI16(pop(routine)); \
            push(routine, UI16_VAL(a op b)); \
        } else if (IS_UI64(peek(routine, 0)) && IS_UI64(peek(routine, 1))) { \
            int64_t b = AS_UI64(pop(routine)); \
            int64_t a = AS_UI64(pop(routine)); \
            push(routine, UI64_VAL(a op b)); \
        } else if (IS_I64(peek(routine, 0)) && IS_I64(peek(routine, 1))) { \
            int64_t b = AS_I64(pop(routine)); \
            int64_t a = AS_I64(pop(routine)); \
            push(routine, I64_VAL(a op b)); \
        } else if (IS_DOUBLE(peek(routine, 0)) && IS_DOUBLE(peek(routine, 1))) { \
            double b = AS_DOUBLE(pop(routine)); \
            double a = AS_DOUBLE(pop(routine)); \
            push(routine, DOUBLE_VAL(a op b)); \
        } else if (IS_INT(peek(routine, 0)) && IS_INT(peek(routine, 1))) { \
            binaryIntOp(routine, #op); \
        } else { \
            runtimeError(routine, "Operands must both be numbers, integers or unsigned integers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
    } while (false)
#define BINARY_BOOLEAN_OP(routine, op) \
    do { \
        if (IS_I32(peek(routine, 0)) && IS_I32(peek(routine, 1))) { \
            int32_t b = AS_I32(pop(routine)); \
            int32_t a = AS_I32(pop(routine)); \
            push(routine, BOOL_VAL(a op b)); \
        } else if (IS_UI32(peek(routine, 0)) && IS_UI32(peek(routine, 1))) { \
            uint32_t b = AS_UI32(pop(routine)); \
            uint32_t a = AS_UI32(pop(routine)); \
            push(routine, BOOL_VAL(a op b)); \
        } else if (IS_I8(peek(routine, 0)) && IS_I8(peek(routine, 1))) { \
            int8_t b = AS_I8(pop(routine)); \
            int8_t a = AS_I8(pop(routine)); \
            push(routine, BOOL_VAL(a op b)); \
        } else if (IS_UI8(peek(routine, 0)) && IS_UI8(peek(routine, 1))) { \
            uint8_t b = AS_UI8(pop(routine)); \
            uint8_t a = AS_UI8(pop(routine)); \
            push(routine, BOOL_VAL(a op b)); \
        } else if (IS_I16(peek(routine, 0)) && IS_I16(peek(routine, 1))) { \
            int16_t b = AS_I16(pop(routine)); \
            int16_t a = AS_I16(pop(routine)); \
            push(routine, BOOL_VAL(a op b)); \
        } else if (IS_UI16(peek(routine, 0)) && IS_UI16(peek(routine, 1))) { \
            uint16_t b = AS_UI16(pop(routine)); \
            uint16_t a = AS_UI16(pop(routine)); \
            push(routine, BOOL_VAL(a op b)); \
        } else if (IS_UI64(peek(routine, 0)) && IS_UI64(peek(routine, 1))) { \
            uint64_t b = AS_UI64(pop(routine)); \
            uint64_t a = AS_UI64(pop(routine)); \
            push(routine, BOOL_VAL(a op b)); \
        } else if (IS_I64(peek(routine, 0)) && IS_I64(peek(routine, 1))) { \
            int64_t b = AS_I64(pop(routine)); \
            int64_t a = AS_I64(pop(routine)); \
            push(routine, BOOL_VAL(a op b)); \
        } else if (IS_DOUBLE(peek(routine, 0)) && IS_DOUBLE(peek(routine, 1))) { \
            double b = AS_DOUBLE(pop(routine)); \
            double a = AS_DOUBLE(pop(routine)); \
            push(routine, BOOL_VAL(a op b)); \
        } else if (IS_INT(peek(routine, 0)) && IS_INT(peek(routine, 1))) { \
            binaryIntBoolOp(routine, #op); \
        } else { \
            runtimeError(routine, "Operands must both be numbers, integers or unsigned integers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
    } while (false)
#define BINARY_UINT_OP(routine, op) \
    do { \
        if (IS_UI32(peek(routine, 0)) && IS_UI32(peek(routine, 1))) { \
            uint32_t b = AS_UI32(pop(routine)); \
            uint32_t a = AS_UI32(pop(routine)); \
            uint32_t c = a op b; \
            push(routine, UI32_VAL(c)); \
        } else if (IS_UI8(peek(routine, 0)) && IS_UI8(peek(routine, 1))) { \
            uint8_t b = AS_UI8(pop(routine)); \
            uint8_t a = AS_UI8(pop(routine)); \
            uint8_t c = a op b; \
            push(routine, UI8_VAL(c)); \
        } else if (IS_UI16(peek(routine, 0)) && IS_UI16(peek(routine, 1))) { \
            uint16_t b = AS_UI16(pop(routine)); \
            uint16_t a = AS_UI16(pop(routine)); \
            uint16_t c = a op b; \
            push(routine, UI16_VAL(c)); \
        } else if (IS_UI64(peek(routine, 0)) && IS_UI64(peek(routine, 1))) { \
            uint64_t b = AS_UI64(pop(routine)); \
            uint64_t a = AS_UI64(pop(routine)); \
            uint64_t c = a op b; \
            push(routine, UI64_VAL(c)); \
        } else { \
            runtimeError(routine, "Operands must be unsigned integers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
    } while (false)

    for (;;) {
        if (routine->state == EXEC_ERROR) return INTERPRET_RUNTIME_ERROR;

        if (routine->traceExecution) {
            PRINTERR("[%p]", routine);
            printValueStack(routine, "          ");
            PRINTERR("[%p]", routine);
            disassembleInstruction(&frame->closure->function->chunk, 
                                (int)(frame->ip - frame->closure->function->chunk.code));
        }

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(routine, constant);
                break;
            }
            case OP_IMMEDIATEi8: {
                uint8_t byte = READ_BYTE();
                Value constant = I8_VAL((int8_t)byte);
                push(routine, constant);
                break;
            }
            case OP_IMMEDIATEui8: {
                uint8_t byte = READ_BYTE();
                Value constant = UI8_VAL(byte);
                push(routine, constant);
                break;
            }
            case OP_IMMEDIATEi16: {
                uint8_t byte = READ_BYTE();
                Value constant = I16_VAL((int8_t)byte);
                push(routine, constant);
                break;
            }
            case OP_IMMEDIATEui16: {
                uint8_t byte = READ_BYTE();
                Value constant = UI16_VAL(byte);
                push(routine, constant);
                break;
            }
            case OP_IMMEDIATEi32: {
                uint8_t byte = READ_BYTE();
                Value constant = I32_VAL((int8_t)byte);
                push(routine, constant);
                break;
            }
            case OP_IMMEDIATEui32: {
                uint8_t byte = READ_BYTE();
                Value constant = UI32_VAL(byte);
                push(routine, constant);
                break;
            }
            case OP_IMMEDIATEi64: {
                uint8_t byte = READ_BYTE();
                Value constant = I64_VAL((int8_t)byte);
                push(routine, constant);
                break;
            }
            case OP_IMMEDIATEui64: {
                uint8_t byte = READ_BYTE();
                Value constant = UI64_VAL(byte);
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
                ValueCell* lhs = frameSlot(routine, frame, slot);
                ValueCellTarget lhsTrg = { .cellType = lhs->cellType, .value = &lhs->value };

                if (!assignToValueCellTarget(lhsTrg, rhs->value)) {
                    runtimeError(routine, "Cannot set local variable to incompatible type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(routine, frameSlot(routine, frame, slot)->value);
                break;
            }
            case OP_GET_GLOBAL: {
                platform_mutex_enter(&vm.env);
                ObjString* name = READ_STRING();
                ValueCell cell;
                if (!tableCellGet(&vm.globals, name, &cell)) {
                    runtimeError(routine, "Undefined variable (OP_GET_GLOBAL) '%s'.", name->chars);
                    platform_mutex_leave(&vm.env);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(routine, cell.value);
                platform_mutex_leave(&vm.env);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                platform_mutex_enter(&vm.env);
                ObjString* name = READ_STRING();
                tableCellSet(&vm.globals, name, *peekCell(routine, 0));
                pop(routine);
                platform_mutex_leave(&vm.env);
                break;
            }
            case OP_SET_GLOBAL: {
                platform_mutex_enter(&vm.env);
                ObjString* name = READ_STRING();
                ValueCell* lhs = NULL;
                if (tableCellGetPlace(&vm.globals, name, &lhs)) {
                    ValueCell* rhs = peekCell(routine, 0);
                    ValueCellTarget lhsTrg = { .cellType = lhs->cellType, .value = &lhs->value };

                    if (!assignToValueCellTarget(lhsTrg, rhs->value)) {
                        runtimeError(routine, "Cannot set global variable to incompatible type.");
                        platform_mutex_leave(&vm.env);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    runtimeError(routine, "Undefined variable (OP_SET_GLOBAL) '%s'.", name->chars);
                    platform_mutex_leave(&vm.env);
                    return INTERPRET_RUNTIME_ERROR;
                }
                platform_mutex_leave(&vm.env);
                break;
            }
            case OP_INITIALISE: {
                ValueCellTarget lhsTrg = peekCellTarget(routine, 1);
                ValueCell* rhs = peekCell(routine, 0);
                if (!initialiseValueCellTarget(lhsTrg, rhs->value)) {
                    runtimeError(routine, "Cannot initialise variable with this value.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                pop(routine);
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(routine, frame->closure->upvalues[slot]->contents->value);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                ValueCell* rhs = peekCell(routine, 0);
                ValueCellTarget lhsTrg = { 
                    .cellType = frame->closure->upvalues[slot]->contents->cellType, 
                    .value = &frame->closure->upvalues[slot]->contents->value 
                };

                if (!assignToValueCellTarget(lhsTrg, rhs->value)) {
                    runtimeError(routine, "Cannot set local variable to incompatible type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(peek(routine, 0)) && !IS_STRUCT(peek(routine, 0)) && !isStructPointer(peek(routine, 0))) {
                    runtimeError(routine, "Only instances, structs and pointers to structs have properties.");
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
                    ObjPackedStruct* object = AS_STRUCT(peek(routine, 0));
                    ObjString* name = READ_STRING();
                    size_t index;
                    if (!structFieldIndex(object->store.storedType, name, &index)) {
                        runtimeError(routine, "field not present in struct.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    PackedValue f = structField(object->store, index);
                    Value result = unpackValue(f);

                    pop(routine);
                    push(routine, result);
                } else if (isStructPointer(peek(routine, 0))) {
                    ObjPackedStruct* object = (ObjPackedStruct*) destinationObject(peek(routine, 0));
                    tempRootPush(OBJ_VAL(object));
                    ObjString* name = READ_STRING();
                    size_t index;
                    if (!structFieldIndex(object->store.storedType, name, &index)) {
                        runtimeError(routine, "field not present in struct.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    PackedValue f = structField(object->store, index);
                    Value result = OBJ_VAL(newPointerAtHeapCell(f));
                    tempRootPop();

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
                    ObjPackedStruct* object = AS_STRUCT(peek(routine, 1));
                    ObjString* name = READ_STRING();
                    size_t index;
                    if (!structFieldIndex(object->store.storedType, name, &index)) {
                        runtimeError(routine, "field not present in struct.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    PackedValue trg = structField(object->store, index);
                    if (!assignToPackedValue(trg, peek(routine, 0))) {
                        runtimeError(routine, "cannot assign to field type.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    Value result = pop(routine);                 
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
                if (IS_INT(peek(routine, 0)) && IS_INT(peek(routine, 1))) {
                    binaryIntBoolOp(routine, "==");
                } else {
                    Value b = pop(routine);
                    Value a = pop(routine);
                    push(routine, BOOL_VAL(valuesEqual(a, b)));
                }
                break;
            }
            case OP_GREATER:  BINARY_BOOLEAN_OP(routine, >); break;
            case OP_LESS:     BINARY_BOOLEAN_OP(routine, <); break;
            case OP_LEFT_SHIFT:  BINARY_UINT_OP(routine, <<); break;
            case OP_RIGHT_SHIFT: BINARY_UINT_OP(routine, >>); break;
            case OP_BITOR:       BINARY_UINT_OP(routine, |); break;
            case OP_BITAND:      BINARY_UINT_OP(routine, &); break;
            case OP_BITXOR:      BINARY_UINT_OP(routine, ^); break;
            case OP_ADD: {
                if (IS_I32(peek(routine, 0)) && IS_I32(peek(routine, 1))) {
                    int32_t b = AS_I32(pop(routine));
                    int32_t a = AS_I32(pop(routine));
                    push(routine, I32_VAL(a + b));
                } else if (IS_UI32(peek(routine, 0)) && IS_UI32(peek(routine, 1))) {
                    uint32_t b = AS_UI32(pop(routine));
                    uint32_t a = AS_UI32(pop(routine));
                    push(routine, UI32_VAL(a + b));
                } else if (IS_I8(peek(routine, 0)) && IS_I8(peek(routine, 1))) {
                    int8_t b = AS_I8(pop(routine));
                    int8_t a = AS_I8(pop(routine));
                    push(routine, I8_VAL(a + b));
                } else if (IS_UI8(peek(routine, 0)) && IS_UI8(peek(routine, 1))) {
                    uint8_t b = AS_UI8(pop(routine));
                    uint8_t a = AS_UI8(pop(routine));
                    push(routine, UI8_VAL(a + b));
                } else if (IS_I16(peek(routine, 0)) && IS_I16(peek(routine, 1))) {
                    int16_t b = AS_I16(pop(routine));
                    int16_t a = AS_I16(pop(routine));
                    push(routine, I16_VAL(a + b));
                } else if (IS_UI16(peek(routine, 0)) && IS_UI16(peek(routine, 1))) {
                    uint16_t b = AS_UI16(pop(routine));
                    uint16_t a = AS_UI16(pop(routine));
                    push(routine, UI16_VAL(a + b));
                } else if (IS_I64(peek(routine, 0)) && IS_I64(peek(routine, 1))) {
                    int64_t b = AS_I64(pop(routine));
                    int64_t a = AS_I64(pop(routine));
                    push(routine, I64_VAL(a + b));
                } else if (IS_UI64(peek(routine, 0)) && IS_UI64(peek(routine, 1))) {
                    uint64_t b = AS_UI64(pop(routine));
                    uint64_t a = AS_UI64(pop(routine));
                    push(routine, UI64_VAL(a + b));
                } else if (IS_DOUBLE(peek(routine, 0)) && IS_DOUBLE(peek(routine, 1))) {
                    double b = AS_DOUBLE(pop(routine));
                    double a = AS_DOUBLE(pop(routine));
                    push(routine, DOUBLE_VAL(a + b));
                } else if (IS_ADDRESS(peek(routine, 1)) && IS_I32(peek(routine, 0))) {
                    int32_t b = AS_I32(pop(routine));
                    uintptr_t a = AS_ADDRESS(pop(routine));
                    push(routine, ADDRESS_VAL(a + b));
                } else if (IS_ADDRESS(peek(routine, 1)) && IS_UI32(peek(routine, 0))) {
                    uint32_t b = AS_UI32(pop(routine));
                    uintptr_t a = AS_ADDRESS(pop(routine));
                    push(routine, ADDRESS_VAL(a + b));
                } else if (IS_POINTER(peek(routine, 1)) && IS_UI32(peek(routine, 0))) {
                    uint32_t b = AS_UI32(pop(routine));
                    ObjPackedPointer* pointer = AS_POINTER(pop(routine));
                    offsetPointerDestination(pointer, b);
                    push(routine, OBJ_VAL(pointer));
                } else if (IS_STRING(peek(routine, 0)) && IS_STRING(peek(routine, 1))) {
                    concatenate(routine);
                } else if (IS_INT(peek(routine, 0)) && IS_INT(peek(routine, 1))) {
                    binaryIntOp(routine, "+");
                } else {
                    runtimeError(routine, "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_MODULO: {
                if (IS_I32(peek(routine, 0)) && IS_I32(peek(routine, 1))) {
                    int32_t b = AS_I32(pop(routine));
                    int32_t a = AS_I32(pop(routine));
                    int32_t r = a % b;
                    if (r < 0) {
                        r += b;
                    }
                    push(routine, I32_VAL(r));
                } else if (IS_I8(peek(routine, 0)) && IS_I8(peek(routine, 1))) {
                    int8_t b = AS_I8(pop(routine));
                    int8_t a = AS_I8(pop(routine));
                    int8_t r = a % b;
                    if (r < 0) {
                        r += b;
                    }
                    push(routine, I8_VAL(r));
                } else if (IS_I16(peek(routine, 0)) && IS_I16(peek(routine, 1))) {
                    int16_t b = AS_I16(pop(routine));
                    int16_t a = AS_I16(pop(routine));
                    int16_t r = a % b;
                    if (r < 0) {
                        r += b;
                    }
                    push(routine, I16_VAL(r));
                } else if (IS_I64(peek(routine, 0)) && IS_I64(peek(routine, 1))) {
                    int64_t b = AS_I64(pop(routine));
                    int64_t a = AS_I64(pop(routine));
                    int64_t r = a % b;
                    if (r < 0) {
                        r += b;
                    }
                    push(routine, I64_VAL(r));
                } else if (IS_UI32(peek(routine, 0)) && IS_UI32(peek(routine, 1))) {
                    uint32_t b = AS_UI32(pop(routine));
                    uint32_t a = AS_UI32(pop(routine));
                    push(routine, UI32_VAL(a % b));
                } else if (IS_UI8(peek(routine, 0)) && IS_UI8(peek(routine, 1))) {
                    uint8_t b = AS_UI8(pop(routine));
                    uint8_t a = AS_UI8(pop(routine));
                    push(routine, UI8_VAL(a % b));
                } else if (IS_UI16(peek(routine, 0)) && IS_UI16(peek(routine, 1))) {
                    uint16_t b = AS_UI16(pop(routine));
                    uint16_t a = AS_UI16(pop(routine));
                    push(routine, UI16_VAL(a % b));
                } else if (IS_UI64(peek(routine, 0)) && IS_UI64(peek(routine, 1))) {
                    uint64_t b = AS_UI64(pop(routine));
                    uint64_t a = AS_UI64(pop(routine));
                    push(routine, UI64_VAL(a % b));
                } else if (IS_INT(peek(routine, 0)) && IS_INT(peek(routine, 1))) {
                    binaryIntOp(routine, "%");
                } else {
                    runtimeError(routine, "Operands must integers or unsigned integers of same type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(routine, -); break;
            case OP_MULTIPLY: BINARY_OP(routine, *); break;
            case OP_DIVIDE: BINARY_OP(routine, /); break;
            case OP_NOT:
                push(routine, BOOL_VAL(isFalsey(pop(routine))));
                break;
            case OP_NEGATE: {
                if (IS_DOUBLE(peek(routine, 0))) {
                    push(routine, DOUBLE_VAL(-AS_DOUBLE(pop(routine))));
                } else if (IS_I32(peek(routine, 0))) {
                    push(routine, I32_VAL(-AS_I32(pop(routine))));
                } else if (IS_I8(peek(routine, 0))) {
                    push(routine, I8_VAL(-AS_I8(pop(routine))));
                } else if (IS_I16(peek(routine, 0))) {
                    push(routine, I16_VAL(-AS_I16(pop(routine))));
                } else if (IS_I64(peek(routine, 0))) {
                    push(routine, I64_VAL(-AS_I64(pop(routine))));
                } else if (IS_INT(peek(routine, 0))) {
                    Int *b = AS_INT(peek(routine, 0));
                    int_neg(b);
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
            case OP_POKE: {
                Value location = peek(routine, 0);
                Value assignment = peek(routine, 1);
                Value assignment_type = concrete_typeof(assignment);
                tempRootPush(assignment_type);
                if (!isAddressValue(location) && !isUint32Pointer(location)) {
                    tempRootPop();
                    runtimeError(routine, "Location must be a pointer to an uint32 or address.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (!is_positive_integer(assignment) && !is_stored_type(assignment_type)) {
                    tempRootPop();
                    runtimeError(routine, "Value must be a positive integer or a placeable type.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                uintptr_t nominal_address = 0;
                if (isUint32Pointer(location)) {
                    nominal_address = (uintptr_t) AS_POINTER(location)->destination;
                } else {
                    nominal_address = AS_ADDRESS(location);
                }
                volatile uint32_t* reg = (volatile uint32_t*) nominal_address;

                uint32_t val = 0;

                if (is_positive_integer(assignment)) {
                    val = as_positive_integer(assignment);
                } else if (is_stored_type(assignment_type)) {
#if IS_32BIT
                    val = (uintptr_t)storedAddressof(assignment);
#elif IS_64BIT
                    // this is a bug on a 64bit addressed image.
                    val = (uint32_t)(uintptr_t)storedAddressof(assignment);
#endif
                }

#if defined (CYARG_SELF_HOSTED)
                *reg = val;
#elif defined(CYARG_OS_HOSTED)
#if defined(CYARG_FEATURE_TEST_SYSTEM)
                tsWrite((uint32_t)nominal_address, val);
#endif
                printf("poke 0x%08lx, 0x%08x\n", nominal_address, val);
#endif
                tempRootPop();
                pop(routine);
                pop(routine);

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
                        closure->upvalues[i] = captureUpvalue(routine, frameSlot(routine, frame, index), stackOffsetOf(frame, index));
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpvalues(routine, routine->stackTopIndex - 1);
                pop(routine);
                break;
            case OP_YIELD: {
                if (routine == &vm.core0) {
                    runtimeError(routine, "Cannot yield from initial routine.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                yieldFromRoutine(routine);
                return INTERPRET_OK;
            }
            case OP_RETURN: {
                Value result = pop(routine);
                tempRootPush(result);
                closeUpvalues(routine, frame->stackEntryIndex);
                routine->frameCount--;
                popFrame(routine, frame);
                push(routine, result);
                tempRootPop();
                if (routine->frameCount == 0) {
                    returnFromRoutine(routine, result);
                    return INTERPRET_OK;
                }
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
                ObjConcreteYargType* typeObj = NULL;
                switch (typeCode) {
                    case TYPE_LITERAL_BOOL: typeObj = newYargTypeFromType(TypeBool); break;
                    case TYPE_LITERAL_INT8: typeObj = newYargTypeFromType(TypeInt8); break;
                    case TYPE_LITERAL_UINT8: typeObj = newYargTypeFromType(TypeUint8); break;
                    case TYPE_LITERAL_INT16: typeObj = newYargTypeFromType(TypeInt16); break;
                    case TYPE_LITERAL_UINT16: typeObj = newYargTypeFromType(TypeUint16); break;
                    case TYPE_LITERAL_INT32: typeObj = newYargTypeFromType(TypeInt32); break;
                    case TYPE_LITERAL_UINT32: typeObj = newYargTypeFromType(TypeUint32); break;
                    case TYPE_LITERAL_INT64: typeObj = newYargTypeFromType(TypeInt64); break;
                    case TYPE_LITERAL_UINT64: typeObj = newYargTypeFromType(TypeUint64); break;
                    case TYPE_LITERAL_MACHINE_FLOAT64: typeObj = newYargTypeFromType(TypeDouble); break;
                    case TYPE_LITERAL_STRING: typeObj = newYargTypeFromType(TypeString); break;
                    case TYPE_LITERAL_INT: typeObj = newYargTypeFromType(TypeInt); break;
                }
                if (typeObj == NULL) {
                    runtimeError(routine, "Unknown type literal.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(routine, OBJ_VAL(typeObj));
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
                size_t fieldOffset = 0;
                for (uint8_t i = 0; i < fieldCount; i++) {
                    fieldOffset = addFieldType(st, i, fieldOffset, peek(routine, 2), peek(routine, 1), peek(routine, 0));
                    pop(routine);
                    pop(routine);
                    pop(routine);
                }
                push(routine, OBJ_VAL(st));
                tempRootPop();
                break;
            }
            case OP_TYPE_ARRAY: {
                if (!IS_NIL(peek(routine, 0)) && !is_positive_integer(peek(routine, 0))) {
                    runtimeError(routine, "Array cardinality must be positive integer or nil");
                }
                uint32_t cardinality = 0;
                if (is_positive_integer(peek(routine, 0))) {
                    cardinality = as_positive_integer(peek(routine, 0));
                }
                ObjConcreteYargTypeArray* typeObject = (ObjConcreteYargTypeArray*) newYargArrayTypeFromType(peek(routine, 1));
                typeObject->cardinality = cardinality;

                pop(routine);
                pop(routine);
                push(routine, OBJ_VAL(typeObject));
                break;
            }
            case OP_SET_CELL_TYPE: {
                Value type = peek(routine, 0);
                Value def = defaultValue(type);
                pop(routine);
                pushTyped(routine, def, type);
                break;
            }
            case OP_DEREF_PTR: {
                derefPtr(routine);
                break;
            }
            case OP_SET_PTR_TARGET: {
                Value rhs = peek(routine, 0);
                Value lhs = peek(routine, 1);
                ObjPackedPointer* pLhs = AS_POINTER(lhs);
                PackedValue trgLhs = { 
                    .storedType = pLhs->type->target_type, 
                    .storedValue = pLhs->destination 
                };
                if (assignToPackedValue(trgLhs, rhs)) {
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
                if (!is_placeable_type(type)) {
                    runtimeError(routine, "Cannot place this type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (!IS_ADDRESS(location)) {
                    runtimeError(routine, "Value must be an address");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value result = placeObjectAt(type, location);

                pop(routine);
                pop(routine);
                push(routine, result);
                break;
            }
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_BOOLEAN_OP
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
    pushEntryElements(&vm.core0);

    enterEntryFunction(&vm.core0);
    InterpretResult result = run(&vm.core0);
    if (result == INTERPRET_OK) {
        pop(&vm.core0);
    }

    return result;
}

void binaryIntOp(ObjRoutine* routine, char const *c)
{
    Int *b = AS_INT(pop(routine));
    Int *a = AS_INT(pop(routine));
    ObjInt *r = ALLOCATE_OBJ(ObjInt, OBJ_INT);
    int_init(&r->bigInt);
    switch (*c)
    {
    case '+': int_add(a, b, &r->bigInt); break;
    case '-': int_sub(a, b, &r->bigInt); break;
    case '*': int_mul(a, b, &r->bigInt); break;
    case '/': int_div(a, b, &r->bigInt, 0); break; // todo - compiler should optimise for /%
    case '%': {
        Int q;
        int_init(&q);
        int_div(a, b, &q, &r->bigInt);
        break;
    }
    }
    push(routine, OBJ_VAL(r));
}

void binaryIntBoolOp(ObjRoutine* routine, char const *op)
{
    Int *b = AS_INT(pop(routine));
    Int *a = AS_INT(pop(routine));
    IntComp ic = int_is(a, b);
    bool r;
    switch (ic)
    {
    case INT_LT:
        r = strcmp(op, "<") == 0 || strcmp(op, "<=") == 0;
        break;
    case INT_GT:
        r = strcmp(op, ">") == 0 || strcmp(op, ">=") == 0;
        break;
    case INT_EQ:
        r = strcmp(op, "==") == 0;
        break;
    }
    push(routine, BOOL_VAL(r));
}
