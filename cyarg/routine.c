#include "routine.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"

#include "memory.h"
#include "vm.h"
#include "debug.h"
#include "yargobject.h"
#include "vmobject.h"
#include "value_cell.h"

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

    tempObjRootPush((Obj*)sliceObj);

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
    tempObjRootPush((Obj*)routine);
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
    push(routine, FUNCTION_VAL(routine->entryFunction));

    if (routine->entryFunction->function->arity == 1) {
        push(routine, routine->entryArg);
    }
}

void enterEntryFunction(ObjRoutine* routine) {
    callfn(routine, routine->entryFunction, routine->entryFunction->function->arity);
}

bool resumeRoutine(ObjRoutine* context, ObjRoutine* target, size_t argCount, Value argument, Value* result) {
    if (target->state == EXEC_UNBOUND) {
        push(target, FUNCTION_VAL(target->entryFunction));
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
    size_t buffer_required = vsnprintf(NULL, 0, format, args);
    char* message_buffer = ALLOCATE(char, buffer_required + 1);
    vsnprintf(message_buffer, buffer_required + 1, format, args);
    va_end(args);

    fputs(message_buffer, stderr);
    fputs("\n", stderr);
    vm_free(message_buffer);

    for (int i = routine->frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &routine->frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        int16_t line = 0;
        for (int s = 0; s < function->chunk.numLines; s++) {
            if (function->chunk.lines[s].address > instruction) break;
            line = function->chunk.lines[s].line;
        }
        // todo: add parenthesis after function name if appropriate
        const char* scriptName = function->fName ? function->fName->chars : "<script>";
        size_t buffer_required = snprintf(NULL, 0, "[line %d] in %s", line, scriptName);
        char* buffer = ALLOCATE(char, buffer_required + 1);
        snprintf(buffer, buffer_required + 1, "[line %d] in %s", line, scriptName);
        fputs(buffer, stderr);
        fputs("\n", stderr);
        vm_free(buffer);
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

static void bufferEscape(const char* src, char* dest, size_t destSize) {
    size_t src_cursor = 0;
    size_t dest_cursor = 0;
    
    for (; src[src_cursor] != '\0' && dest_cursor < destSize - 1; src_cursor++) {
        if (src[src_cursor] == '\n') {
            dest[dest_cursor] = '\\';
            if (dest_cursor + 1 < destSize - 1) {
                dest[dest_cursor + 1] = 'n';
                dest_cursor++;
            }
        } else if (src[src_cursor] == '\t') {
            dest[dest_cursor] = '\\';
            if (dest_cursor + 1 < destSize - 1) {
                dest[dest_cursor + 1] = 't';
                dest_cursor++;
            }
        } else if (src[src_cursor] == '\\') {
            dest[dest_cursor] = '\\';
            if (dest_cursor + 1 < destSize - 1) {
                dest[dest_cursor + 1] = '\\';
                dest_cursor++;
            }
        } else {
            dest[dest_cursor] = src[src_cursor];
        }
        dest_cursor++;
    }
    dest[dest_cursor] = '\0';
}


// configure the grid used for stack trace display

// prefix + 5 cells
#define LOG_WIDTH 141

#define TYPEFIELD_WIDTH 7
#define VALUEFIELD_WIDTH 14

// maximum number of stack trace lines to display; 0 means no limit
#define MAX_STACK_TRACE_LINES 2

static void snprintValueDescription(char* buffer, size_t bufferSize, Value value) {
    if (IS_STRING(value)) {
        ObjString* str = AS_STRING(value);
        char escaped[VALUEFIELD_WIDTH + 1];
        bufferEscape(str->chars, escaped, sizeof(escaped));
        char display[VALUEFIELD_WIDTH + 2 + 1];
        snprintf(display, sizeof(display), "\"%s\"", escaped);
        size_t displayLength = strlen(display);

        if (displayLength > bufferSize - 1) {
            int width = bufferSize - 1 - 3; // account for null terminator, and "..." 
            snprintf(buffer, bufferSize, "%*.*s...", width, width, display);
        } else {
            int width = bufferSize - 1; // account for quotes and null terminator
            snprintf(buffer, bufferSize, "%*.*s", width, width, display);
        }
    } else {
        ObjString* valueStr = valueToString(value);
    
        assert(bufferSize > 4);
        int max_width = (int)(bufferSize - 1);

        if (valueStr->length > max_width) {
            int width = max_width - 3; // length of "..."
            snprintf(buffer, bufferSize, "%*.*s...", width, width, valueStr->chars);
        } else {
            snprintf(buffer, bufferSize, "%*.*s", max_width, max_width, valueStr->chars);
        }
    }
}

static void snprintTypeDescription(char* buffer, size_t bufferSize, const ObjConcreteYargType* type) {
    ObjString* typeStr = typeToString(type);

    assert(bufferSize > 4);
    int max_width = (int)(bufferSize - 1);
    if (typeStr->length > max_width && type) {
        int width = max_width - 3;
        snprintf(buffer, bufferSize, "%*.*s...", width, width, typeStr->chars);
    } else if (type) {
        snprintf(buffer, bufferSize, "%*.*s", max_width, max_width, typeStr->chars);
    } else {
        snprintf(buffer, bufferSize, "%*.*s", max_width, max_width, "any");
    }
}

#define PREFIX_WIDTH 21
#define VALUECELL_WIDTH (TYPEFIELD_WIDTH + VALUEFIELD_WIDTH + 3)
#define MAX_STACK_LINE_VALUECELLS ((LOG_WIDTH - PREFIX_WIDTH) / VALUECELL_WIDTH)
static void traceValueStack(ObjRoutine* routine) {

    size_t stackSize = routine->stackTopIndex;
    size_t stack_lines = (stackSize + MAX_STACK_LINE_VALUECELLS - 1) / MAX_STACK_LINE_VALUECELLS;
    size_t max_lines = MAX_STACK_TRACE_LINES == 0 ? stack_lines : MAX_STACK_TRACE_LINES;

    for (size_t line = stack_lines - 1, line_cursor = 0; line_cursor < stack_lines && line_cursor < max_lines; line--, line_cursor++) {
        char prefix[PREFIX_WIDTH] = "                    ";
        if (line_cursor == 0) { // first line identifes the routine and the total stack size
            ObjString* routineStr = valueToString(ROUTINE_VAL(routine));
            snprintf(prefix, sizeof(prefix), "%s[%3zu]:", routineStr->chars, stackSize);
        }
        // if this the last line to trace, and there are more stack elements skipped, then indicate that with an ellipsis
        if (line_cursor == max_lines - 1 && line_cursor != stack_lines - 1) {
            snprintf(&prefix[PREFIX_WIDTH - 4], 4, "...");
        }
        printf("%s", prefix);

        for (size_t cursor = 0; cursor < MAX_STACK_LINE_VALUECELLS; cursor++) {
            size_t slot =  line * MAX_STACK_LINE_VALUECELLS + cursor;
            if (slot >= stackSize) break;
            ValueCell* cell = peekCell(routine, (int)(stackSize - 1 - slot));

            char value_description[VALUEFIELD_WIDTH];
            snprintValueDescription(value_description, VALUEFIELD_WIDTH, cell->value);

            char type_description[TYPEFIELD_WIDTH];
            snprintTypeDescription(type_description, TYPEFIELD_WIDTH, cell->cellType);

            printf("[%s|%s]", value_description, type_description);
        }
        printf("\n");
    }
}

void traceExecution(ObjRoutine* routine) {
    CallFrame* frame = &routine->frames[routine->frameCount - 1];

    traceValueStack(routine);
    char functionName[17];
    snprintf(functionName, sizeof(functionName), "%s:", frame->closure->function->fName ? frame->closure->function->fName->chars : "script");
    printf("    %-16s", functionName);
    disassembleInstruction(&frame->closure->function->chunk, 
        (int)(frame->ip - frame->closure->function->chunk.code));
}

ObjString* routineToString(const ObjRoutine* routine) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "<R%p>", routine);
    return copyString(buffer, (int)strlen(buffer));
}
