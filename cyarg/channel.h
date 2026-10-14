#ifndef cyarg_channel_h
#define cyarg_channel_h

#include "value.h"

typedef struct ObjChannelContainer ObjChannelContainer;
typedef struct ObjSyncGroup ObjSyncGroup;
typedef struct ObjString ObjString;
typedef struct ObjRoutine ObjRoutine;

ObjChannelContainer* newChannel(ObjRoutine* routine, size_t capacity);

void freeChannelObject(ObjChannelContainer* channel);
void markChannel(ObjChannelContainer* channel);

ObjString* channelToString(const ObjChannelContainer* channel);

void sendChannel(ObjChannelContainer* channel, Value data);
Value receiveChannel(ObjChannelContainer* channel);
Value peekChannel(ObjChannelContainer* channel);
bool shareChannel(ObjChannelContainer* channel, Value data);

Value collectFromChannel(ObjChannelContainer* channel);
void joinSyncGroup(ObjChannelContainer* channel, ObjSyncGroup* group);
void leaveSyncGroup(ObjChannelContainer* channel, ObjSyncGroup* group);

#endif
