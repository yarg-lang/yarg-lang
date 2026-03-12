#ifndef cyarg_native_h
#define cyarg_native_h

#include "value.h"

bool clockNative(ObjRoutine* routine, int argCount, Value* result);
bool clock_get_hzNative(ObjRoutine* routine, int argCount, Value* result);

bool irq_add_shared_handlerNative(ObjRoutine* routine, int argCount, Value* result);
bool irq_remove_handlerNative(ObjRoutine* routine, int argCount, Value* result);

bool stdin_getsNative(ObjRoutine* routine, int argCount, Value* result);
bool stdin_eofNative(ObjRoutine* routine, int argCount, Value* result);
bool stdout_putsNative(ObjRoutine* routine, int argCount, Value* result);

#endif
