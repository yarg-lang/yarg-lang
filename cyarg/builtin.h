#ifndef cyarg_builtin_h
#define cyarg_builtin_h

#include "value.h"
#include "object.h"

typedef bool (*BuiltinFun)(ObjRoutine* routine, int argCount, Value* result);

typedef struct {
    Obj obj;
    BuiltinFun function;
} ObjBuiltin;

BuiltinFun getBuiltinFunction(uint8_t builtin);

ObjBuiltin* newBuiltin(BuiltinFun function);
ObjString* builtinToString(const ObjBuiltin* builtin);

#define BUILTIN_VAL(object)     ((Value){.type = &yargTypes.function, .as.obj = (Obj*) object})

bool lenBuiltin(ObjRoutine* routineContext, int argCount, Value* result);

#endif
