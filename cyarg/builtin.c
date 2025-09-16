#include <stdlib.h>
#include <stdio.h>

#ifdef CYARG_PICO_TARGET
#include <pico/multicore.h>
#endif

#include "common.h"
#include "object.h"
#include "value.h"
#include "builtin.h"
#include "native.h"
#include "routine.h"
#include "vm.h"
#include "debug.h"
#include "files.h"
#include "compiler.h"
#include "channel.h"
#include "yargtype.h"

InterpretResult interpretImport(const char* source) {

    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;
    tempRootPush(OBJ_VAL(function));

    ObjRoutine* routine = newRoutine(ROUTINE_THREAD);
    tempRootPush(OBJ_VAL(routine));

    ObjClosure* closure = newClosure(function);
    tempRootPop();
    tempRootPop();

    bindEntryFn(routine, closure);

    push(routine, OBJ_VAL(closure));
    callfn(routine, closure, 0);

    tempRootPush(OBJ_VAL(routine));
    InterpretResult result = run(routine);
    tempRootPop();
    
    pop(routine);

    return result;
}

static char* libraryNameFor(const char* importname) {
    size_t namelen = strlen(importname);
    char* filename = malloc(namelen + 3 + 1);
    if (filename) {
        strcpy(filename, importname);
        strcat(filename, ".ya");
    }
    return filename;
}

bool importBuiltin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
    if (argCount != 1) {
        runtimeError(routineContext, "Expected 1 arguments but got %d.", argCount);
        return false;
    }
    if (!IS_STRING(args[0].value)) {
        runtimeError(routineContext, "Argument to import must be string.");
        return false;
    }

    char* source = NULL;
    char* library = libraryNameFor(AS_CSTRING(args[0].value));
    if (library) {
        source = readFile(library);
        free(library);
    }

    if (source) {
        InterpretResult result = interpretImport(source);        
        free(source);

        if (result == INTERPRET_COMPILE_ERROR) {
            runtimeError(routineContext, "Interpret error");
            return false;
        }
        else if (result == INTERPRET_RUNTIME_ERROR) {
            runtimeError(routineContext, "Interpret error");
            return false;
        }
        return true;
    }
    else {
        runtimeError(routineContext, "source not found");
        return false;

    }

}

bool makeRoutineBuiltin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
    if (argCount != 2) {
        runtimeError(routineContext, "Expected 2 arguments but got %d.", argCount);
        return false;
    }
    if (!IS_CLOSURE(args[0].value) || !IS_BOOL(args[1].value)) {
        runtimeError(routineContext, "Argument to make_routine must be a function and a boolean.");
        return false;
    }

    ObjClosure* closure = AS_CLOSURE(args[0].value);
    bool isISR = AS_BOOL(args[1].value);

    ObjRoutine* routine = newRoutine(isISR ? ROUTINE_ISR : ROUTINE_THREAD);

    if (bindEntryFn(routine, closure)) {
        *result = OBJ_VAL(routine);
        return true;
    }
    else {
        runtimeError(routineContext, "Function must take 0 or 1 arguments.");
        return false;
    }
}

bool resumeBuiltin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
    if (argCount < 1 || argCount > 2) {
        runtimeError(routineContext, "Expected one or two arguments to resume.");
        return false;
    }

    if (!IS_ROUTINE(args[0].value)) {
        runtimeError(routineContext, "Argument to resume must be a routine.");
        return false;
    }

    ObjRoutine* target = AS_ROUTINE(args[0].value);

    if (target->state != EXEC_SUSPENDED && target->state != EXEC_UNBOUND) {
        runtimeError(routineContext, "routine must be suspended or unbound to resume.");
        return false;
    }

    if (argCount > 1) {
        bindEntryArgs(target, args[1].value);
    }

    if (target->state == EXEC_UNBOUND) {
        prepareRoutineStack(target);
        target->state = EXEC_SUSPENDED;
    }
    else if (target->state == EXEC_SUSPENDED) {
        push(target, target->entryArg);
    }

    InterpretResult execResult = run(target);
    if (execResult != INTERPRET_OK) {
        return false;
    }

    Value coroResult = pop(target);

    *result = coroResult;
    return true;
}

// cyarg: use ascii 'y' 'a' 'r' 'g'
#define FLAG_VALUE 0x79617267

#ifdef CYARG_PICO_TARGET
void nativeCore1Entry() {
    multicore_fifo_push_blocking(FLAG_VALUE);
    uint32_t g = multicore_fifo_pop_blocking();

    if (g != FLAG_VALUE) {
        fatalVMError("Core1 entry and sync failed.");
    }

    ObjRoutine* core = vm.core1;

    InterpretResult execResult = run(core);

    if (core->state != EXEC_ERROR) {
        core->state = EXEC_CLOSED;
    }

    vm.core1 = NULL;
}
#endif

bool startBuiltin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
#ifdef CYARG_PICO_TARGET
    if (argCount < 1 || argCount > 2) {
        runtimeError(routineContext, "Expected one or two arguments to start.");
        return false;
    }

    if (!IS_ROUTINE(args[0].value)) {
        runtimeError(routineContext, "Argument to start must be a routine.");
        return false;
    }

    ObjRoutine* target = AS_ROUTINE(args[0].value);

    if (target->state != EXEC_UNBOUND) {
        runtimeError(routineContext, "routine must be unbound to start.");
        return false;
    }

    if (vm.core1 != NULL) {
        runtimeError(routineContext, "Core1 already occupied.");
        return false;
    }

    if (argCount > 1) {
        bindEntryArgs(target, args[1].value);
    }

    prepareRoutineStack(target);

    vm.core1 = target;
    vm.core1->state = EXEC_RUNNING;

    multicore_reset_core1();
    multicore_launch_core1(nativeCore1Entry);

    // Wait for it to start up
    uint32_t g = multicore_fifo_pop_blocking();
    if (g != FLAG_VALUE) {
        fatalVMError("Core1 startup failure.");
        return false;
    }
    multicore_fifo_push_blocking(FLAG_VALUE);

#endif
    *result = NIL_VAL;
    return true;
}

bool peekBuiltin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {

    if (!isAddressValue(args[0].value)) {
        runtimeError(routineContext, "Expected an address or pointer.");
        return false;
    }

    uintptr_t nominal_address = 0;
    if (IS_POINTER(args[0].value)) {
        nominal_address = (uintptr_t) AS_POINTER(args[0].value)->destination;
    } else {
        nominal_address = AS_ADDRESS(args[0].value);
    }
    volatile uint32_t* reg = (volatile uint32_t*) nominal_address;

#ifdef CYARG_PICO_TARGET
    uint32_t res = *reg;
    *result = UI32_VAL(res);
#else
    printf("peek(%p)\n", (void*)nominal_address);
    *result = UI32_VAL(0);
#endif
    return true;
}

bool lenBuiltin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
    if (argCount != 1) {
        runtimeError(routineContext, "Expected 1 argument, but got %d.", argCount);
        return false;
    }

    if (IS_STRING(args[0].value)) {
        ObjString* string = AS_STRING(args[0].value);
        *result = UI32_VAL(string->length);
        return true;
    } else if (IS_UNIFORMARRAY(args[0].value)) {
        ObjPackedUniformArray* array = AS_UNIFORMARRAY(args[0].value);
        *result = UI32_VAL(array->type->cardinality);
        return true;
    } else {
        runtimeError(routineContext, "Expected a string or array.");
        return false;
    }
}

bool pinBuiltin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
    if (argCount != 1) {
        runtimeError(routineContext, "Expected 1 argument, but got %d.", argCount);
        return false;
    }

    if (!IS_ROUTINE(args[0].value)) {
        runtimeError(routineContext, "Argument to pin must be a routine.");
        return false;
    }

    for (size_t i = 0; i < MAX_PINNED_ROUTINES; i++) {
        if (vm.pinnedRoutines[i] == NULL) {
            vm.pinnedRoutines[i] = AS_ROUTINE(args[0].value);
            *result = ADDRESS_VAL((uintptr_t)vm.pinnedRoutineHandlers[i]);
            return true;
        }
    }

    runtimeError(routineContext, "No more pinned routines available.");
    *result = NIL_VAL;
    return false;
}

bool newBuiltin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {

    Value typeToCreate = NIL_VAL;
    if (   argCount == 1
        && IS_YARGTYPE(args[0].value)) {
        typeToCreate = args[0].value;
    }

    ConcreteYargType typeRequested = IS_NIL(typeToCreate) ? TypeAny : AS_YARGTYPE(typeToCreate)->yt;
    switch (typeRequested) {
        case TypeBool:
        case TypeDouble:
        case TypeStruct:
        case TypeAny: {
            StoredValue* heap_cell = createHeapCell(typeToCreate);
            initialisePackedStorage(typeToCreate, heap_cell);
            tempRootPush(heap_cell->asValue);
            *result = OBJ_VAL(newPointerForHeapCell(typeToCreate, heap_cell));
            tempRootPop();
            return true;
        }
        case TypeInt8:  // fall through
        case TypeUint8:
        case TypeInt32:
        case TypeUint32:
        case TypeInt64:
        case TypeUint64: {
            StoredValue* heap_cell = createHeapCell(typeToCreate);
            initialisePackedStorage(typeToCreate, heap_cell);
            *result = OBJ_VAL(newPointerForHeapCell(typeToCreate, heap_cell));
            return true;
        }
        case TypeArray: {
            *result = defaultValue(typeToCreate);
            return true;
        }
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeNativeBlob:
        case TypeRoutine:
        case TypeChannel:
        case TypePointer: 
        case TypeYargType:
            return false; // unsupported for now.
    }

    return false;
}

bool uint64Builtin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
    if (IS_I8(args[0].value)) {
        *result = UI64_VAL(AS_I8(args[0].value));
        return true;
    } else if (IS_UI8(args[0].value)) {
        *result = UI64_VAL(AS_UI8(args[0].value));
        return true;
    } else if (IS_I32(args[0].value)) {
        *result = UI64_VAL(AS_I32(args[0].value));
        return true;
    } else if (IS_UI32(args[0].value)) {
        *result = UI64_VAL(AS_UI32(args[0].value));
        return true;
    } else if (IS_UI64(args[0].value)) {
        *result = args[0].value;
        return true;
    } else if (IS_I64(args[0].value) && AS_I64(args[0].value) >= 0) {
        *result = UI64_VAL(AS_I64(args[0].value));
        return true;
    }
    return false;
}

bool int64Builtin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
    if (IS_I8(args[0].value)) {
        *result = I64_VAL(AS_I8(args[0].value));
        return true;
    } else if (IS_I32(args[0].value)) {
        *result = I64_VAL(AS_I32(args[0].value));
        return true;
    } else if (IS_I64(args[0].value)) {
        *result = args[0].value;
        return true;
    } else if (IS_UI8(args[0].value)) {
        *result = I64_VAL(AS_UI8(args[0].value));
        return true;
    } else if (IS_UI32(args[0].value)) {
        *result = I64_VAL(AS_UI32(args[0].value));
        return true;
    } else if (IS_UI64(args[0].value) && AS_UI64(args[0].value) <= INT64_MAX) {
        *result = I64_VAL(AS_UI64(args[0].value));
        return true;
    } else {
        return false;
    }
}

bool uint32Builtin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
    if (IS_I8(args[0].value) && AS_I8(args[0].value) >= 0) {
        *result = UI32_VAL(AS_I8(args[0].value));
        return true;
    } else if (IS_I32(args[0].value) && AS_I32(args[0].value) >= 0) {
        *result = UI32_VAL(AS_I32(args[0].value));
        return true;
    } else if (IS_I64(args[0].value) && AS_I64(args[0].value) >= 0 && AS_I64(args[0].value) < UINT32_MAX) {
        *result = UI32_VAL(AS_I64(args[0].value));
        return true;
    } else if (IS_UI8(args[0].value)) {
        *result = UI32_VAL(AS_UI8(args[0].value));
        return true;
    } else if (IS_UI32(args[0].value)) {
        *result = args[0].value;
        return true;
    } else if (IS_UI64(args[0].value) && AS_UI64(args[0].value) <= UINT32_MAX) {
        *result = UI32_VAL(AS_UI64(args[0].value));
        return true;
    }
    return false;
}

bool int32Builtin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
    if (IS_I8(args[0].value)) {
        *result = I32_VAL(AS_I8(args[0].value));
        return true;
    } else if (IS_I32(args[0].value)) {
        *result = args[0].value;
        return true;
    } else if (IS_I64(args[0].value) && AS_I64(args[0].value) >= INT32_MIN && AS_I64(args[0].value) <= INT32_MAX) {
        *result = I32_VAL(AS_I64(args[0].value));
        return true;
    } else if (IS_UI8(args[0].value)) {
        *result = I32_VAL(AS_UI8(args[0].value));
        return true;
    } else if (IS_UI32(args[0].value) && AS_UI32(args[0].value) <= INT32_MAX) {
        *result = I32_VAL(AS_UI32(args[0].value));
        return true;
    } else if (IS_UI64(args[0].value) && AS_UI64(args[0].value) <= INT32_MAX) {
        *result = I32_VAL(AS_UI64(args[0].value));
        return true;
    } else {
        return false;
    }
}

bool uint8Builtin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
    if (IS_I8(args[0].value) && AS_I8(args[0].value) >= 0) {
        *result = UI8_VAL(AS_I8(args[0].value));
        return true;
    } else if (IS_I32(args[0].value) && AS_I32(args[0].value) >= 0 && AS_I32(args[0].value) < UINT8_MAX) {
        *result = UI8_VAL(AS_I32(args[0].value));
        return true;
    } else if (IS_I64(args[0].value) && AS_I64(args[0].value) >= 0 && AS_I64(args[0].value) < UINT8_MAX) {
        *result = UI8_VAL(AS_I64(args[0].value));
        return true;
    } else if (IS_UI8(args[0].value)) {
        *result = args[0].value;
        return true;
    } else if (IS_UI32(args[0].value) && AS_UI32(args[0].value) <= UINT8_MAX) {
        *result = UI8_VAL(AS_UI32(args[0].value));
        return true;
    } else if (IS_UI64(args[0].value) && AS_UI64(args[0].value) <= UINT8_MAX) {
        *result = UI8_VAL(AS_UI64(args[0].value));
        return true;
    } else {
        return false;
    }
}

bool int8Builtin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
    if (IS_I8(args[0].value)) {
        *result = args[0].value;
        return true;
    } else if (IS_I32(args[0].value) && AS_I32(args[0].value) >= INT8_MIN && AS_I32(args[0].value) < INT8_MAX) {
        *result = I8_VAL(AS_I32(args[0].value));
        return true;
    } else if (IS_I64(args[0].value) && AS_I64(args[0].value) >= INT8_MIN && AS_I64(args[0].value) < INT8_MAX) {
        *result = I8_VAL(AS_I64(args[0].value));
        return true;
    } else if (IS_UI8(args[0].value) && AS_UI8(args[0].value) <= INT8_MAX) {
        *result = I8_VAL(AS_UI8(args[0].value));
        return true;
    } else if (IS_UI32(args[0].value) && AS_UI32(args[0].value) <= INT8_MAX) {
        *result = I8_VAL(AS_UI32(args[0].value));
        return true;
    } else if (IS_UI64(args[0].value) && AS_UI64(args[0].value) <= INT8_MAX) {
        *result = I8_VAL(AS_UI64(args[0].value));
        return true;
    } else {
        return false;
    }
}

Value getBuiltin(uint8_t builtin) {
    switch (builtin) {
        case BUILTIN_PEEK: return OBJ_VAL(newNative(peekBuiltin));
        case BUILTIN_IMPORT: return OBJ_VAL(newNative(importBuiltin));
        case BUILTIN_MAKE_ROUTINE: return OBJ_VAL(newNative(makeRoutineBuiltin));
        case BUILTIN_RESUME: return OBJ_VAL(newNative(resumeBuiltin));
        case BUILTIN_START: return OBJ_VAL(newNative(startBuiltin));
        case BUILTIN_MAKE_CHANNEL: return OBJ_VAL(newNative(makeChannelBuiltin));
        case BUILTIN_SEND: return OBJ_VAL(newNative(sendChannelBuiltin));
        case BUILTIN_RECEIVE: return OBJ_VAL(newNative(receiveChannelBuiltin));
        case BUILTIN_SHARE: return OBJ_VAL(newNative(shareChannelBuiltin));
        case BUILTIN_CPEEK: return OBJ_VAL(newNative(peekChannelBuiltin));
        case BUILTIN_LEN: return OBJ_VAL(newNative(lenBuiltin));
        case BUILTIN_PIN: return OBJ_VAL(newNative(pinBuiltin));
        case BUILTIN_NEW: return OBJ_VAL(newNative(newBuiltin));
        case BUILTIN_INT8: return OBJ_VAL(newNative(int8Builtin));
        case BUILTIN_UINT8: return OBJ_VAL(newNative(uint8Builtin));
        case BUILTIN_INT32: return OBJ_VAL(newNative(int32Builtin));
        case BUILTIN_UINT32: return OBJ_VAL(newNative(uint32Builtin));
        case BUILTIN_INT64: return OBJ_VAL(newNative(int64Builtin));
        case BUILTIN_UINT64: return OBJ_VAL(newNative(uint64Builtin));
        default: return NIL_VAL;
    }
}
