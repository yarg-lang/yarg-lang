#ifndef cyarg_vmobject_h
#define cyarg_vmobject_h

#include "object.h"
#include "value.h"
#include "value_cell.h"

#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_BUILTIN(value)      isObjType(value, OBJ_BUILTIN)

#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_BUILTIN(value) \
    (((ObjBuiltin*)AS_OBJ(value))->function)

typedef struct ObjFunction ObjFunction;

typedef struct ObjUpvalue {
    Obj obj;
    ValueCell* contents;
    size_t stackOffset;
    ValueCell closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct ObjClosure {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int cUpvalueCount;
} ObjClosure;

typedef struct {
    Obj obj;
    Value reciever;
    ObjClosure* method;
} ObjBoundMethod;

typedef bool (*NativeFn)(ObjRoutine* routine, int argCount, Value* result);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

ObjUpvalue* newUpvalue(ValueCell* slot, size_t stackOffset);
ObjClosure* newClosure(ObjFunction* function);
ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);
ObjNative* newNative(NativeFn function);


#endif