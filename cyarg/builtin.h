#ifndef cyarg_builtin_h
#define cyarg_builtin_h

#include "value.h"
#include "vm-result.h"

Value getBuiltin(uint8_t builtin);

bool loadBuiltinDummy(ObjRoutine* routineContext, int argCount, Value* result);
InterpretResult loadBuiltin(ObjRoutine* routineContext, int argCount);

#endif
