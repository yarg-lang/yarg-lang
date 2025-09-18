#ifndef cyarg_channel_h
#define cyarg_channel_h

#include "value.h"
#include "object.h"

ObjChannel* newChannel();

bool makeChannelBuiltin(ObjRoutine* routine, int argCount, Value* result);
bool sendChannelBuiltin(ObjRoutine* routine, int argCount, Value* result);
bool receiveChannelBuiltin(ObjRoutine* routine, int argCount, Value* result);
bool shareChannelBuiltin(ObjRoutine* routine, int argCount, Value* result);
bool peekChannelBuiltin(ObjRoutine* routine, int argCount, Value* result);

#endif