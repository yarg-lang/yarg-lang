#ifndef cyarg_builtin_h
#define cyarg_builtin_h

#include "value.h"

Value getBuiltin(uint8_t builtin);

bool importBuiltinDummy(ObjRoutine* routineContext, int argCount, Value* result);
bool importBuiltin(ObjRoutine* routineContext, int argCount);

#endif