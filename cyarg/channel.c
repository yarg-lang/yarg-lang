#include <stdio.h>

#include "common.h"

#include "channel.h"
#include "memory.h"
#include "vm.h"
#include "debug.h"

ObjChannel* newChannel() {
    ObjChannel* channel = ALLOCATE_OBJ(ObjChannel, OBJ_CHANNEL);
    channel->data = NIL_VAL;
    channel->present = false;
    channel->overflow = false;
    return channel;
}

bool makeChannelBuiltin(ObjRoutine* routine, int argCount, ValueCell* args, Value* result) {
    if (argCount != 0) {
        runtimeError(routine, "Expected 0 arguments but got %d.", argCount);
        return false;
    }

    ObjChannel* channel = newChannel();

    *result = OBJ_VAL(channel);
    return true;
}

bool sendChannelBuiltin(ObjRoutine* routine, int argCount, ValueCell* args, Value* result) {
    if (argCount != 2) {
        runtimeError(routine, "Expected 2 arguments, got %d.", argCount);
        return false;
    }
    if (!IS_CHANNEL(args[0].value)) {
        runtimeError(routine, "Argument must be a channel.");
        return false;
    }

    ObjChannel* channel = AS_CHANNEL(args[0].value);
#ifdef CYARG_PICO_TARGET
    while (channel->present) {
        // stall/block until space
        tight_loop_contents();
    }
#endif

    channel->data = args[1].value;
    channel->present = true;
    channel->overflow = false;
    *result = BOOL_VAL(channel->overflow);

    return true;
}

bool receiveChannelBuiltin(ObjRoutine* routine, int argCount, ValueCell* args, Value* result) {
    if (argCount != 1) {
        runtimeError(routine, "Expected 1 arguments, got %d.", argCount);
        return false;
    }
    if (!IS_CHANNEL(args[0].value) && !IS_ROUTINE(args[0].value)) {
        runtimeError(routine, "Argument must be a channel or a routine.");
        return false;
    }

    if (IS_CHANNEL(args[0].value)) {
        ObjChannel* channel = AS_CHANNEL(args[0].value);
#ifdef CYARG_PICO_TARGET
        while (!channel->present) {
            // stall/block until space
            tight_loop_contents();
        }
#endif
        *result = channel->data;
        channel->present = false;
        channel->overflow = false;

    } 
    else if (IS_ROUTINE(args[0].value)) {
        ObjRoutine* routineParam = AS_ROUTINE(args[0].value);

#ifdef CYARG_PICO_TARGET
        while (routineParam->state == EXEC_RUNNING) {
            tight_loop_contents();
        }
#endif    
        
        if (routineParam->state == EXEC_CLOSED || routineParam->state == EXEC_SUSPENDED) {
            *result = peek(routineParam, -1);
        } 
        else {
            *result = NIL_VAL;
        }
    }
    return true;
}

bool shareChannelBuiltin(ObjRoutine* routine, int argCount, ValueCell* args, Value* result) {
    if (argCount != 2) {
        runtimeError(routine, "Expected 2 arguments, got %d.", argCount);
        return false;
    }
    if (!IS_CHANNEL(args[0].value)) {
        runtimeError(routine, "Argument must be a channel.");
        return false;
    }

    ObjChannel* channel = AS_CHANNEL(args[0].value);
    if (channel->present) {
        channel->overflow = true;
    }
    channel->data = args[1].value;
    channel->present = true;
    *result = BOOL_VAL(channel->overflow);

    return true;
}

bool peekChannelBuiltin(ObjRoutine* routine, int argCount, ValueCell* args, Value* result) {
    if (argCount != 1) {
        runtimeError(routine, "Expected 1 arguments, got %d.", argCount);
        return false;
    }
    if (!IS_CHANNEL(args[0].value)) {
        runtimeError(routine, "Argument must be a channel.");
        return false;
    }

    ObjChannel* channel = AS_CHANNEL(args[0].value);
    if (channel->present) {
        *result = channel->data;
    }
    else {
        *result = NIL_VAL;
    }

    return true;
}