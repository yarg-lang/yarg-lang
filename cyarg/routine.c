#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"

#include "routine.h"

#include "memory.h"
#include "vm.h"
#include "debug.h"

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

#ifdef CYARG_PICO_BUSY_SYNC
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
        int16_t line = 0;
        for (int s = 0; s < function->chunk.numLines; s++) {
            if (function->chunk.lines[s].address > instruction) break;
            line = function->chunk.lines[s].line;
        }
        PRINTERR("[line %d] in ", line);
        if (function->fName == NULL) {
            PRINTERR("script\n");
        } else {
            PRINTERR("%s()\n", function->fName->chars); // todo: if this is a synthetic fun e.g. boot or file.ya then don’t put parentheses
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
    ValueCell* cell = peekCell(routine, distance);
    return cell->value;
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

static void traceValueStack(ObjRoutine* routine) {
    const unsigned int max_stack_trace_lines = 3;
    const unsigned int max_stack_line_width = 5;
    assert(max_stack_trace_lines > 1);

    size_t stackSize = routine->stackTopIndex;
    size_t stack_lines = (stackSize / max_stack_line_width);
    if (stackSize % max_stack_line_width != 0) {
        stack_lines++;
    }

    for (int line = stack_lines, line_cursor = 0; line >= 0 && line_cursor < stack_lines; line--, line_cursor++) {
        char prefix[21] = "                    ";
        if (line_cursor == 0) { // first line identifes the routine and the total stack size
            ObjString* routineStr = valueToString(OBJ_VAL(routine));
            snprintf(prefix, sizeof(prefix), "%s[%3zu]:", routineStr->chars, stackSize);
        }
        // if this the last line to trace, and there are more stack elements skipped, then indicate that with an ellipsis
        if (line_cursor == max_stack_trace_lines - 1 && line_cursor > 0 && line_cursor != stack_lines - 1) {
            snprintf(&prefix[17], 4, "...");
        }
        printf("%s", prefix);
        for (size_t cursor = 0; cursor < max_stack_line_width; cursor++) {
            size_t slot = (line - 1) * max_stack_line_width + cursor;
            if (slot >= stackSize) break;
            ValueCell* cell = peekCell(routine, (int)(stackSize - 1 - slot));
            ObjString* valueStr = valueToString(cell->value);
            tempRootPush(OBJ_VAL(valueStr));
            char value_description[12];
            if (valueStr->length > 11) {
                snprintf(value_description, sizeof(value_description), "%8.8s...", valueStr->chars);
            } else {
                snprintf(value_description, sizeof(value_description), "%11.11s", valueStr->chars);
            }
            tempRootPop();
            ObjString* typeStr = valueToString(cell->cellType ? OBJ_VAL(cell->cellType) : NIL_VAL);
            tempRootPush(OBJ_VAL(typeStr));
            char type_description[11] = "       any";
            if (typeStr->length > 10 && cell->cellType) {
                snprintf(type_description, sizeof(type_description), "%7.7s...", typeStr->chars);
            } else if (cell->cellType) {
                snprintf(type_description, sizeof(type_description), "%7.7s", typeStr->chars);
            }
            tempRootPop();
            printf("[%s|%s]", value_description, type_description);
        }
        printf("\n");
        if (line_cursor == max_stack_trace_lines - 1 && line > 0) {
            break;
        }
    }
}

void traceExecution(ObjRoutine* routine) {
    CallFrame* frame = &routine->frames[routine->frameCount - 1];

    traceValueStack(routine);
    ObjString* routineStr = valueToString(OBJ_VAL(routine));
    printf("%s %s:", routineStr->chars, frame->closure->function->fName ? frame->closure->function->fName->chars : "script");
    disassembleInstruction(&frame->closure->function->chunk, 
        (int)(frame->ip - frame->closure->function->chunk.code));
}