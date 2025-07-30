#ifndef cyarg_native_h
#define cyarg_native_h

#include "value.h"
#include "value_cell.h"

bool clockNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);
bool sleepNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);

bool alarmAddInMSNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);
bool alarmAddRepeatingMSNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);
bool alarmCancelRepeatingMSNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);

bool irq_add_shared_handlerNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);
bool irq_remove_handlerNative(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);


#endif