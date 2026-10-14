#ifndef cyarg_builtin_h
#define cyarg_builtin_h

#include "value.h"
#include "object.h"

Value getBuiltin(uint8_t builtin);

typedef bool (*BuiltinFun)(ObjRoutine* routine, int argCount, Value* result);

typedef struct {
    Obj obj;
    BuiltinFun function;
} ObjBuiltin;

ObjBuiltin* newBuiltin(BuiltinFun function);
ObjString* builtinToString(const ObjBuiltin* builtin);

#define BUILTIN_VAL(object)     ((Value){.type = &yargTypes.function, .as.obj = (Obj*) object})

#endif
