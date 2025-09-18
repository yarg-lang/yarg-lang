#ifndef cyarg_ws2812_builtin_h
#define cyarg_ws2812_builtin_h

#include "value.h"
#include "value_cell.h"

bool ws2812initNative(ObjRoutine* routineContext, int argCount, Value* result);
bool ws2812writepixelNative(ObjRoutine* routineContext, int argCount, Value* result);


#endif