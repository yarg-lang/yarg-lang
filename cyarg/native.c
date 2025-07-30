#include <stdio.h>
#include <time.h>

#include "common.h"
#include "object.h"
#include "value.h"
#include "native.h"
#include "routine.h"
#include "vm.h"

#ifdef CYARG_PICO_TARGET
#include <hardware/gpio.h>
#include <hardware/irq.h>
#endif

bool irq_add_shared_handlerNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result) {
    *result = NIL_VAL;
    Value numVal = args[0].value;
    unsigned int num = as_positive_integer(numVal);

#ifdef CYARG_PICO_TARGET
    uintptr_t isrRoutine = AS_UINTEGER(args[1].value);
#else
    uintptr_t isrRoutine = (uintptr_t) vm.pinnedRoutineHandlers[AS_UINTEGER(args[1].value)];
#endif

    Value prioVal = args[2].value;
    unsigned int prio = as_positive_integer(prioVal);

    size_t handlerIndex = pinnedRoutineIndex(isrRoutine);

    prepareRoutineStack(vm.pinnedRoutines[handlerIndex]);
    vm.pinnedRoutines[handlerIndex]->state = EXEC_RUNNING;

#ifdef CYARG_PICO_TARGET
    irq_add_shared_handler(num, (irq_handler_t) isrRoutine, prio);
#endif

    return true;
}

bool irq_remove_handlerNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result) {
    *result = NIL_VAL;
    Value numVal = args[0].value;
    unsigned int num = as_positive_integer(numVal);

#ifdef CYARG_PICO_TARGET
    uintptr_t isrRoutine = AS_UINTEGER(args[1].value);
#else
    uintptr_t isrRoutine = (uintptr_t) vm.pinnedRoutineHandlers[AS_UINTEGER(args[1].value)];
#endif

#ifdef CYARG_PICO_TARGET
    irq_remove_handler(num, (irq_handler_t) isrRoutine);
#endif

    return true;
}


bool clockNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result) {
    if (argCount != 0) {
        runtimeError(routine, "Expected 0 arguments but got %d.", argCount);
        return false;
    }

    *result = UINTEGER_VAL(clock() / CLOCKS_PER_SEC);
    return true;
}

#ifdef CYARG_PICO_TARGET
bool sleepNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result) {
    if (argCount != 1) {
        runtimeError(routine, "Expected 1 arguments but got %d.", argCount);
        return false;
    }
    if (!IS_UINTEGER(args[0].value)) {
        runtimeError(routine, "Argument must be an unsigned integer");
        return false;
    }

    sleep_ms(AS_UINTEGER(args[0].value));

    *result = NIL_VAL;
    return true;
}
#endif

#ifdef CYARG_PICO_TARGET
static int64_t nativeOneShotCallback(alarm_id_t id, void* user_data) {

    ObjRoutine* routine = (ObjRoutine*)user_data;

    run(routine);

    if (routine->state != EXEC_ERROR) {
        routine->state = EXEC_CLOSED;
    }

    return 0;
}

static bool nativeRecurringCallback(struct repeating_timer* t) {

    ObjRoutine* routine = (ObjRoutine*)t->user_data;;

    run(routine);

    Value result = pop(routine); // unused.

    prepareRoutineStack(routine);

    return true;
}

bool alarmAddInMSNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result) {
    if (argCount < 2 || argCount > 3) {
        runtimeError(routine, "Unexpected argument count adding alarm");
        return false;
    }

    if (!IS_ROUTINE(args[1].value)) {
        runtimeError(routine, "Second argument must be a routine.");
        return false;
    }

    ObjRoutine* isrRoutine = AS_ROUTINE(args[1].value);

    int isrArity = 2 + isrRoutine->entryFunction->function->arity;
    if (argCount != isrArity) {
        runtimeError(routine, "Expected %d arguments but got %d.", isrArity, argCount);
        return false;
    }

    if (!IS_UINTEGER(args[0].value)) {
        runtimeError(routine, "First argument must be an unsigned integer.");
        return false;
    }

    if (isrRoutine->type != ROUTINE_ISR) {
        runtimeError(routine, "Argument must be an ISR routine.");
        return false;
    }

    if (argCount > 2) {
        bindEntryArgs(isrRoutine, args[2].value);
    }
    prepareRoutineStack(isrRoutine);

    isrRoutine->state = EXEC_RUNNING;
    add_alarm_in_ms(AS_UINTEGER(args[0].value), nativeOneShotCallback, isrRoutine, false);

    *result = NIL_VAL;
    return true;
}

bool alarmAddRepeatingMSNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result) {
    if (argCount < 2 || argCount > 3) {
        runtimeError(routine, "Unexpected argument count adding alarm");
        return false;
    }

    if (!IS_ROUTINE(args[1].value)) {
        runtimeError(routine, "Second argument must be a routine.");
        return false;
    }

    ObjRoutine* isrRoutine = AS_ROUTINE(args[1].value);

    int isrArity = 2 + isrRoutine->entryFunction->function->arity;
    if (argCount != isrArity) {
        runtimeError(routine, "Expected %d arguments but got %d.", isrArity, argCount);
        return false;
    }

    if (!IS_INTEGER(args[0].value)) {
        runtimeError(routine, "First argument must be an unsigned integer.");
        return false;
    }

    if (isrRoutine->type != ROUTINE_ISR) {
        runtimeError(routine, "Argument must be an ISR routine.");
        return false;
    }

    ObjBlob* handle = newBlob(sizeof(repeating_timer_t));

    if (argCount > 2) {
        bindEntryArgs(isrRoutine, args[2].value);
    }
    prepareRoutineStack(isrRoutine);

    isrRoutine->state = EXEC_RUNNING;
    add_repeating_timer_ms(AS_INTEGER(args[0].value), nativeRecurringCallback, isrRoutine, (repeating_timer_t*)handle->blob);

    *result = OBJ_VAL(handle);
    return true;
}

bool alarmCancelRepeatingMSNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result) {
    if (argCount != 1) {
        runtimeError(routine, "Expected 1 arguments but got %d.", argCount);
        return false;
    }
    if (!IS_BLOB(args[0].value)) {
        runtimeError(routine, "Argument must be a native blob.");
        return false;
    }

    ObjBlob* blob = AS_BLOB(args[0].value);

    bool cancelled = cancel_repeating_timer((repeating_timer_t*)blob->blob);

    // EXEC_CLOSED ?

    *result = BOOL_VAL(cancelled);
    return true;
}
#endif
