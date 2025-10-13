#ifndef cyarg_channel_h
#define cyarg_channel_h

#include <stdio.h>
#include "value.h"
#include "object.h"

typedef struct ObjChannelContainer ObjChannelContainer;
typedef struct ObjSyncGroup ObjSyncGroup;

ObjChannelContainer* newChannel(ObjRoutine* routine, size_t capacity);

void freeChannelObject(Obj* channel);
void markChannel(ObjChannelContainer* channel);

void printChannel(FILE* op, ObjChannelContainer* channel);

void sendChannel(ObjChannelContainer* channel, Value data);
Value receiveChannel(ObjChannelContainer* channel);
Value peekChannel(ObjChannelContainer* channel);
bool shareChannel(ObjChannelContainer* channel, Value data);

Value collectFromChannel(ObjChannelContainer* channel);
void joinSyncGroup(ObjChannelContainer* channel, ObjSyncGroup* group);
void leaveSyncGroup(ObjChannelContainer* channel, ObjSyncGroup* group);

#endif