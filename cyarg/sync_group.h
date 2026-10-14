#ifndef cyarg_sync_group_h
#define cyarg_sync_group_h

#include <stdio.h>
#include "value.h"
#include "object.h"
#include "vm_mutex.h"

typedef struct ObjSyncGroup ObjSyncGroup;

ObjSyncGroup* newSyncGroup(ObjRoutine* routine, Value itemArray);

void freeSyncGroup(Obj* group);
void markSyncGroup(ObjSyncGroup* group);

ObjString* syncGroupToString(const ObjSyncGroup* group);

Value receiveSyncGroup(ObjSyncGroup* group);

vm_mutex* getSyncGroupLock(ObjSyncGroup* group);

#endif
