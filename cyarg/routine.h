#ifndef cyarg_routine_h
#define cyarg_routine_h

#include "value.h"
#include "value_cell.h"
#include "object.h"

#define FRAMES_MAX 20
#define SLICE_MAX 64

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    size_t stackEntryIndex;
} CallFrame;

typedef enum {
    ROUTINE_THREAD,
    ROUTINE_ISR
} RoutineKind;

typedef enum {
    EXEC_UNBOUND,
    EXEC_RUNNING,
    EXEC_SUSPENDED,
    EXEC_CLOSED,
    EXEC_ERROR
} ExecState;

typedef struct StackSlice StackSlice;
typedef struct ObjStackSlice ObjStackSlice;

typedef struct StackSlice {
    ValueCell elements[SLICE_MAX];
} StackSlice;

typedef struct ObjStackSlice{
    Obj obj;
    StackSlice slice;
} ObjStackSlice;

typedef struct ObjRoutine {
    Obj obj;

    CallFrame frames[FRAMES_MAX];
    int frameCount;

    StackSlice** stackSlices;
    size_t stackSliceCapacity;
    size_t sliceCount;
    
    size_t stackTopIndex;

    StackSlice stk;
    DynamicObjArray additionalSlicesArray;

    ObjClosure* entryFunction;
    Value entryArg;

    ObjUpvalue* openUpvalues;

    RoutineKind type;
    volatile ExecState state;
} ObjRoutine;

void initRoutine(ObjRoutine* routine, RoutineKind type);
ObjRoutine* newRoutine(RoutineKind type);
void resetRoutine(ObjRoutine* routine);
bool bindEntryFn(ObjRoutine* routine, ObjClosure* closure);
void bindEntryArgs(ObjRoutine* routine, Value entryArg);
void prepareRoutineStack(ObjRoutine* routine);
ValueCell* frameSlot(ObjRoutine* routine, CallFrame* frame, size_t index);
Value nativeArgument(ObjRoutine* routine, size_t argCount, size_t argument);
size_t stackOffsetOf(CallFrame* frame, size_t frameIndex);


void markRoutine(ObjRoutine* routine);

void push(ObjRoutine* routine, Value value);
void pushTyped(ObjRoutine* routine, Value value, Value type);
Value pop(ObjRoutine* routine);
void popN(ObjRoutine* routine, size_t count);
void popFrame(ObjRoutine* routine, CallFrame* frame);
Value peek(ObjRoutine* routine, int distance);
ValueCell* peekCell(ObjRoutine* routine, int distance);

void runtimeError(ObjRoutine* routine, const char* format, ...);

#endif