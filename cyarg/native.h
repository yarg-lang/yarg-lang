#ifndef cyarg_native_h
#define cyarg_native_h

#include "value.h"
#include "value_cell.h"

bool clockNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);

bool irq_add_shared_handlerNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);
bool irq_remove_handlerNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);


#endif