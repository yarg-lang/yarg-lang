#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "routine.h"

#include "memory.h"
#include "vm.h"

void initRoutine(ObjRoutine* routine, RoutineKind type) {
    routine->type = type;
    routine->entryFunction = NULL;
    routine->entryArg = NIL_VAL;
    routine->state = EXEC_UNBOUND;

    resetRoutine(routine);
}

void resetRoutine(ObjRoutine* routine) {

    routine->stackTopIndex = 0;
    routine->frameCount = 0;

    routine->openUpvalues = NULL;
}

ObjRoutine* newRoutine(RoutineKind type) {
    ObjRoutine* routine = ALLOCATE_OBJ(ObjRoutine, OBJ_ROUTINE);
    initRoutine(routine, type);
    return routine;
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

void prepareRoutineStack(ObjRoutine* routine) {

    push(routine, OBJ_VAL(routine->entryFunction));

    if (routine->entryFunction->function->arity == 1) {
        push(routine, routine->entryArg);
    }
    
    callfn(routine, routine->entryFunction, routine->entryFunction->function->arity);
}

ValueCell* frameSlot(ObjRoutine* routine, CallFrame* frame, size_t index) {
    size_t stackElementIndex = frame->stackEntryIndex + index;

    return peekCell(routine, routine->stackTopIndex - (stackElementIndex + 1));
}

size_t stackOffsetOf(CallFrame* frame, size_t frameIndex) {
    return frame->stackEntryIndex + frameIndex;
}

void markRoutine(ObjRoutine* routine) {

    size_t stackSize = routine->stackTopIndex;
    for (int i = stackSize - 1; i >= 0; i--) {
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

    markObject((Obj*)routine->entryFunction);
    markValue(routine->entryArg);
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

    return &routine->stk.elements[index];
}

void push(ObjRoutine* routine, Value value) {
    ValueCell* nextSlot = slot(routine, routine->stackTopIndex);

    nextSlot->value = value;
    nextSlot->type = NIL_VAL;
    routine->stackTopIndex++;

    if (routine->stackTopIndex % SLICE_MAX == 0) {
        runtimeError(routine, "Value stack size exceeded.");
    }
}

void pushTyped(ObjRoutine* routine, Value value, Value type) {
    push(routine, value);
    ValueCell* top = peekCell(routine, 0);
    top->type = type;
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
