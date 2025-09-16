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

    if (!IS_ADDRESS(args[1].value)) {
        runtimeError(routine, "Expected an address.");
        return false;
    }

    *result = NIL_VAL;
    Value numVal = args[0].value;
    unsigned int num = as_positive_integer(numVal);

    uintptr_t isrRoutine = AS_ADDRESS(args[1].value);

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

    if (!IS_ADDRESS(args[1].value)) {
        runtimeError(routine, "Expected an address.");
        return false;
    }

    *result = NIL_VAL;
    Value numVal = args[0].value;
    unsigned int num = as_positive_integer(numVal);

    uintptr_t isrRoutine = AS_ADDRESS(args[1].value);

#ifdef CYARG_PICO_TARGET
    irq_remove_handler(num, (irq_handler_t) isrRoutine);
#endif

    size_t handlerIndex = pinnedRoutineIndex(isrRoutine);
    vm.pinnedRoutines[handlerIndex] = NULL;

    return true;
}


bool clockNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result) {
    if (argCount != 0) {
        runtimeError(routine, "Expected 0 arguments but got %d.", argCount);
        return false;
    }

    *result = UI32_VAL(clock() / CLOCKS_PER_SEC);
    return true;
}
