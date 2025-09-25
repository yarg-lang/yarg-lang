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
#include <hardware/clocks.h>
#endif

bool irq_add_shared_handlerNative(ObjRoutine* routine, int argCount, Value* result) {

    Value numVal = nativeArgument(routine, argCount, 0);
    Value address = nativeArgument(routine, argCount, 1);
    Value prioVal = nativeArgument(routine, argCount, 2);

    if (!IS_ADDRESS(address)) {
        runtimeError(routine, "Expected an address.");
        return false;
    }

    *result = NIL_VAL;
    unsigned int num = as_positive_integer(numVal);

    uintptr_t isrRoutine = AS_ADDRESS(address);

    unsigned int prio = as_positive_integer(prioVal);

    size_t handlerIndex = pinnedRoutineIndex(isrRoutine);

    prepareRoutineStack(vm.pinnedRoutines[handlerIndex]);
    vm.pinnedRoutines[handlerIndex]->state = EXEC_RUNNING;

#ifdef CYARG_PICO_TARGET
    irq_add_shared_handler(num, (irq_handler_t) isrRoutine, prio);
#endif

    return true;
}

bool irq_remove_handlerNative(ObjRoutine* routine, int argCount, Value* result) {

    Value numVal = nativeArgument(routine, argCount, 0);
    Value address = nativeArgument(routine, argCount, 1);

    if (!IS_ADDRESS(address)) {
        runtimeError(routine, "Expected an address.");
        return false;
    }

    *result = NIL_VAL;
    unsigned int num = as_positive_integer(numVal);

    uintptr_t isrRoutine = AS_ADDRESS(address);

#ifdef CYARG_PICO_TARGET
    irq_remove_handler(num, (irq_handler_t) isrRoutine);
#endif

    size_t handlerIndex = pinnedRoutineIndex(isrRoutine);
    vm.pinnedRoutines[handlerIndex] = NULL;

    return true;
}


bool clockNative(ObjRoutine* routine, int argCount, Value* result) {
    if (argCount != 0) {
        runtimeError(routine, "Expected 0 arguments but got %d.", argCount);
        return false;
    }

    *result = UI32_VAL(clock() / CLOCKS_PER_SEC);
    return true;
}

bool sleepNative(ObjRoutine* routine, int argCount, Value* result) {
    if (argCount != 1) {
        runtimeError(routine, "Expected 1 arguments but got %d.", argCount);
        return false;
    }
    Value numVal = nativeArgument(routine, argCount, 0);

    if (!is_positive_integer(numVal)) {
        runtimeError(routine, "Argument must be a positive integer");
        return false;
    }

#ifdef CYARG_PICO_TARGET
    sleep_ms(as_positive_integer(numVal));
#endif

    *result = NIL_VAL;
    return true;
}

bool clock_get_hzNative(ObjRoutine* routine, int argCount, Value* result) {
    if (argCount != 1) {
        runtimeError(routine, "Expected 1 arguments but got %d.", argCount);
        return false;
    }
    Value numVal = nativeArgument(routine, argCount, 0);

    if (!is_positive_integer(numVal)) {
        runtimeError(routine, "Argument must be a positive integer");
        return false;
    }

#ifdef CYARG_PICO_TARGET
    int res = clock_get_hz(as_positive_integer(numVal));
#else
    int res = 0;
#endif

    *result = I32_VAL(res);
    return true;
}