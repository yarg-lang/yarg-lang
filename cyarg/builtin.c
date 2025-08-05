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
    char* filename = malloc(namelen + 5);
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

typedef volatile uint32_t Register;

bool rpeekBuiltin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
    
    uint32_t nominal_address = AS_UINTEGER(args[0].value);
    Register* reg = (Register*) (uintptr_t)nominal_address;

#ifdef CYARG_PICO_TARGET
    uint32_t res = *reg;
    *result = UINTEGER_VAL(res);
#else
    printf("rpeek(%08x)\n", nominal_address);
    *result = UINTEGER_VAL(0);
#endif
    return true;
}

bool rpokeBuiltin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {

    uint32_t nominal_address = AS_UINTEGER(args[0].value);
    Register* reg = (Register*) (uintptr_t)nominal_address;

    uint32_t val = AS_UINTEGER(args[1].value);
#ifdef CYARG_PICO_TARGET
    *reg = val;
#else
    printf("rpoke(%08x, %08x)\n", nominal_address, val);
#endif
    return true;
}

bool makeArrayBuiltin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {

    if (argCount < 1 || argCount > 2) {
        runtimeError(routineContext, "Expected 1 or two arguments, but got %d.", argCount);
        return false;
    }

    int index_arg = 0;
    if ((IS_YARGTYPE(args[0].value) || IS_NIL(args[0].value)) && argCount == 2) {
        index_arg = 1;
    }

    if (!IS_UINTEGER(args[index_arg].value) && !IS_INTEGER(args[index_arg].value)) {
        runtimeError(routineContext, "Size argument must be integer or unsigned integer.");
        return false;
    }
    uint32_t capacity = 0;
    if (IS_UINTEGER(args[index_arg].value)) {
        capacity = AS_UINTEGER(args[index_arg].value);
    } else if (IS_INTEGER(args[index_arg].value) && AS_INTEGER(args[index_arg].value) >= 0) {
        capacity = AS_INTEGER(args[index_arg].value);
    } else {
        runtimeError(routineContext, "Argument must be positive.");
        return false;
    }

    if (capacity == 0) {
        runtimeError(routineContext, "Argument must be non-zero.");
        return false;
    }

   
    if (IS_YARGTYPE(args[0].value)) {
        ObjConcreteYargType* type = AS_YARGTYPE(args[0].value);
        ObjUniformArray* array = newUniformArray(type, capacity);
        *result = OBJ_VAL(array);
    } else {
        ObjValArray* array = newValArray(capacity);
        *result = OBJ_VAL(array);
    }

    return true;
}

bool lenBuiltin(ObjRoutine* routineContext, int argCount, ValueCell* args, Value* result) {
    if (argCount != 1) {
        runtimeError(routineContext, "Expected 1 argument, but got %d.", argCount);
        return false;
    }

    if (IS_STRING(args[0].value)) {
        ObjString* string = AS_STRING(args[0].value);
        *result = UINTEGER_VAL(string->length);
        return true;
    } else if (IS_VALARRAY(args[0].value)) {
        ObjValArray* array = AS_VALARRAY(args[0].value);
        *result = UINTEGER_VAL(array->array.count);
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
#ifdef CYARG_PICO_TARGET
            *result = UINTEGER_VAL((uintptr_t)vm.pinnedRoutineHandlers[i]);
#else
            *result = UINTEGER_VAL(i);
#endif
            return true;
        }
    }

    runtimeError(routineContext, "No more pinned routines available.");
    *result = NIL_VAL;
    return false;
}

Value getBuiltin(uint8_t builtin) {
    switch (builtin) {
        case BUILTIN_RPEEK: return OBJ_VAL(newNative(rpeekBuiltin));
        case BUILTIN_RPOKE: return OBJ_VAL(newNative(rpokeBuiltin));
        case BUILTIN_IMPORT: return OBJ_VAL(newNative(importBuiltin));
        case BUILTIN_MAKE_ARRAY: return OBJ_VAL(newNative(makeArrayBuiltin));
        case BUILTIN_MAKE_ROUTINE: return OBJ_VAL(newNative(makeRoutineBuiltin));
        case BUILTIN_RESUME: return OBJ_VAL(newNative(resumeBuiltin));
        case BUILTIN_START: return OBJ_VAL(newNative(startBuiltin));
        case BUILTIN_MAKE_CHANNEL: return OBJ_VAL(newNative(makeChannelBuiltin));
        case BUILTIN_SEND: return OBJ_VAL(newNative(sendChannelBuiltin));
        case BUILTIN_RECEIVE: return OBJ_VAL(newNative(receiveChannelBuiltin));
        case BUILTIN_SHARE: return OBJ_VAL(newNative(shareChannelBuiltin));
        case BUILTIN_PEEK: return OBJ_VAL(newNative(peekChannelBuiltin));
        case BUILTIN_LEN: return OBJ_VAL(newNative(lenBuiltin));
        case BUILTIN_PIN: return OBJ_VAL(newNative(pinBuiltin));
        default: return NIL_VAL;
    }
}
