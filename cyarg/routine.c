#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"

#include "routine.h"

#include "memory.h"
#include "vm.h"

bool addSlice(ObjRoutine* routine);

void initRoutine(ObjRoutine* routine) {
    routine->entryFunction = NULL;
    routine->entryArg = NIL_VAL;
    routine->state = EXEC_UNBOUND;
    routine->sliceCount = 0;
    initDynamicObjArray(&routine->additionalSlicesArray);

    routine->stackSliceCapacity = GROW_CAPACITY(0);
    routine->stackSlices = GROW_ARRAY(StackSlice*, routine->stackSlices, 0, routine->stackSliceCapacity);

    routine->stackSlices[0] = &routine->stk;
    routine->sliceCount = 1;
    routine->addSlice = addSlice;

#ifdef DEBUG_TRACE_EXECUTION
    routine->traceExecution = true;
#else
    routine->traceExecution = false;
#endif

    resetRoutine(routine);
}

void resetRoutine(ObjRoutine* routine) {

    routine->result = NIL_VAL;

    routine->stackTopIndex = 0;
    routine->frameCount = 0;
    routine->openUpvalues = NULL;
}

bool addSlice(ObjRoutine* routine) {

    ObjStackSlice* sliceObj = ALLOCATE_OBJ(ObjStackSlice, OBJ_STACKSLICE);
    if (sliceObj == NULL) return false;

    tempRootPush(OBJ_VAL(sliceObj));

    if (routine->stackSliceCapacity < routine->sliceCount + 1) {
        size_t oldCapacity = routine->stackSliceCapacity;
        routine->stackSliceCapacity = GROW_CAPACITY(routine->stackSliceCapacity);
        routine->stackSlices = GROW_ARRAY(StackSlice*, routine->stackSlices, oldCapacity, routine->stackSliceCapacity);
    }

    if (routine->stackSlices) {
        routine->stackSlices[routine->sliceCount] = &sliceObj->slice;
        routine->sliceCount++;
        appendToDynamicObjArray(&routine->additionalSlicesArray, (Obj*)sliceObj);
    }

    tempRootPop();

    return (routine->stackSlices != NULL);
}

ObjRoutine* newRoutine() {
    ObjRoutine* routine = ALLOCATE_OBJ(ObjRoutine, OBJ_ROUTINE);
    tempRootPush(OBJ_VAL(routine));
    initRoutine(routine);
    tempRootPop();
    return routine;
}

void runAndRenter(ObjRoutine* routine) {
    run(routine);
    pop(routine);
    pushEntryElements(routine);
    enterEntryFunction(routine);
}

bool pinRoutine(ObjRoutine* routine, uintptr_t* address) {

    assert(routine->entryFunction);
    assert(IS_NIL(routine->entryArg));
    assert(routine->entryFunction->function->arity == 0);

    if (installPinnedRoutine(routine, address)) {
        pushEntryElements(routine);
        enterEntryFunction(routine);
        routine->addSlice = NULL;
        return true;
    }
    return false;
}

bool bindEntryFn(ObjRoutine* routine, ObjClosure* closure) {

    if (closure->function->arity == 0 || closure->function->arity == 1) {
        routine->entryFunction = closure;
        return true;
    }
    else {
        return false;
    }

}

void bindEntryArgs(ObjRoutine* routine, Value entryArg) {

    routine->entryArg = entryArg;
}

void pushEntryElements(ObjRoutine* routine) {
    push(routine, OBJ_VAL(routine->entryFunction));

    if (routine->entryFunction->function->arity == 1) {
        push(routine, routine->entryArg);
    }
}

void enterEntryFunction(ObjRoutine* routine) {
    callfn(routine, routine->entryFunction, routine->entryFunction->function->arity);
}

bool resumeRoutine(ObjRoutine* context, ObjRoutine* target, size_t argCount, Value argument, Value* result) {
    if (target->state == EXEC_UNBOUND) {
        push(target, OBJ_VAL(target->entryFunction));
    }

    if (argCount == 1) {
        bindEntryArgs(target, argument);
        push(target, argument);
    } else {
        assert(argCount == 0);
    }

    callfn(target, target->entryFunction, target->entryFunction->function->arity);

    InterpretResult execResult = run(target);
    if (execResult == INTERPRET_OK) {
        target->result = pop(target);
        *result = target->result;
        return true;
    } else {
        return false;
    }
}

void yieldFromRoutine(ObjRoutine* context) {
    context->result = peek(context, 0);
    context->state = EXEC_SUSPENDED;
}

void returnFromRoutine(ObjRoutine* context, Value result) {
    assert(context->frameCount == 0);
    context->result = result;
    context->state = EXEC_CLOSED;
}

bool startRoutine(ObjRoutine* context, ObjRoutine* target, size_t argCount, Value argument) {

    if (target->state != EXEC_UNBOUND) {
        return false;
    }

    if (vm.core1 != NULL) {
        return false;
    }

    if (argCount == 1) {
        bindEntryArgs(target, argument);
    }

    pushEntryElements(target);

    enterEntryFunction(target);
    runOnCore1(target);
    
    return true;
}

bool receiveFromRoutine(ObjRoutine* routine, Value* result) {

#ifdef CYARG_PICO_TARGET
    while (routine->state == EXEC_RUNNING) {
        tight_loop_contents();
    }
#endif    
        
    if (routine->state == EXEC_CLOSED || routine->state == EXEC_SUSPENDED) {
        *result = routine->result;
        return true;
    } 
    else {
        return false;
    }
}

ValueCell* frameSlot(ObjRoutine* routine, CallFrame* frame, size_t index) {
    size_t stackElementIndex = frame->stackEntryIndex + index;

    return peekCell(routine, (int) (routine->stackTopIndex - (stackElementIndex + 1)));
}

Value nativeArgument(ObjRoutine* routine, size_t argCount, size_t argument) {
    return peek(routine, (int)argCount - 1 - (int)argument);
}

size_t stackOffsetOf(CallFrame* frame, size_t frameIndex) {
    return frame->stackEntryIndex + frameIndex;
}

void markRoutine(ObjRoutine* routine) {

    size_t stackSize = routine->stackTopIndex;
    for (int i = (int)(stackSize - 1); i >= 0; i--) {
        markValueCell(peekCell(routine, i));
    }

    for (int i = 0; i < routine->frameCount; i++) {
        markObject((Obj*)routine->frames[i].closure);
    }

    for (ObjUpvalue* upvalue = routine->openUpvalues;
        upvalue != NULL;
        upvalue = upvalue->next) {
        markObject((Obj*)upvalue);
    }

    markDynamicObjArray(&routine->additionalSlicesArray);

    markObject((Obj*)routine->entryFunction);
    markValue(routine->entryArg);
    markValue(routine->result);
}

void runtimeError(ObjRoutine* routine, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = routine->frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &routine->frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        PRINTERR("[line %d] in ",
                 function->chunk.lines[instruction]);
        if (function->name == NULL) {
            PRINTERR("script\n");
        } else {
            PRINTERR("%s()\n", function->name->chars);
        }
    }

    routine->state = EXEC_ERROR;
    resetRoutine(routine);
}

static ValueCell* slot(ObjRoutine* routine, size_t index) {
    size_t sliceIndex = index / SLICE_MAX;

    return &routine->stackSlices[sliceIndex]->elements[index % SLICE_MAX];
}

void push(ObjRoutine* routine, Value value) {
    ValueCell* nextSlot = slot(routine, routine->stackTopIndex);

    nextSlot->value = value;
    nextSlot->cellType = NULL;
    routine->stackTopIndex++;

    if (((routine->stackTopIndex / SLICE_MAX) + 1) > routine->sliceCount) {
        if (routine->addSlice) {
            if (!routine->addSlice(routine)) {
                runtimeError(routine, "Value stack size exceeded.");
            }
        } else {
            runtimeError(routine, "Fixed Value stack size exceeded.");
        }
    }
}

void pushTyped(ObjRoutine* routine, Value value, Value type) {
    push(routine, value);
    ValueCell* top = peekCell(routine, 0);
    top->cellType = IS_NIL(type) ? NULL : AS_YARGTYPE(type);
}

Value pop(ObjRoutine* routine) {

    Value ret = peek(routine, 0);
    routine->stackTopIndex--;

    return ret;
}

void popN(ObjRoutine* routine, size_t count) {
    for (size_t i = 0; i < count; i++) {
        pop(routine);
    }
}

void popFrame(ObjRoutine* routine, CallFrame* frame) {
    routine->stackTopIndex = frame->stackEntryIndex;
}

Value peek(ObjRoutine* routine, int distance) {
    return peekCell(routine, distance)->value;
}

ValueCell* peekCell(ObjRoutine* routine, int distance) {

    ValueCell* targetCell = slot(routine, routine->stackTopIndex - 1 - distance);

    return targetCell;
}

ValueCellTarget peekCellTarget(ObjRoutine* routine, int distance) {
    ValueCell* target = peekCell(routine, distance);
    ValueCellTarget result = { .value = &target->value, .cellType = target->cellType};
    return result;
}