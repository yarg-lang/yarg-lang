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
    routine->stackTop = routine->stack;
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

void markRoutine(ObjRoutine* routine) {
    for (ValueCell* slot = routine->stack; slot < routine->stackTop; slot++) {
        markValueCell(slot);
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
//        PRINTERR("<R%s 0x%8.x>[line %d] in ",
//                 routine->type == ROUTINE_ISR ? "i" : "n",
//                 routine,
//                 function->chunk.lines[instruction]);
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

void push(ObjRoutine* routine, Value value) {
    routine->stackTop->value = value;
    routine->stackTop->type = NIL_VAL;
    routine->stackTop++;

    if (routine->stackTop - &routine->stack[0] > STACK_MAX) {
        runtimeError(routine, "Value stack size exceeded.");
    }
}

Value pop(ObjRoutine* routine) {
    routine->stackTop--;
    return routine->stackTop->value;
}

Value peek(ObjRoutine* routine, int distance) {
    return routine->stackTop[-1 - distance].value;
}

ValueCell* peekCell(ObjRoutine* routine, int distance) {
    return &routine->stackTop[-1 - distance];
}
