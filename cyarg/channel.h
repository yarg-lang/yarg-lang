#ifndef cyarg_channel_h
#define cyarg_channel_h

#include "value.h"
#include "object.h"

ObjChannel* newChannel();

bool makeChannelBuiltin(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);
bool sendChannelBuiltin(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);
bool receiveChannelBuiltin(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);
bool shareChannelBuiltin(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);
bool peekChannelBuiltin(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);

#endif