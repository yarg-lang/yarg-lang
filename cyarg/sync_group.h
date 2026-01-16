#ifndef cyarg_sync_group_h
#define cyarg_sync_group_h

#include <stdio.h>
#include "value.h"
#include "object.h"
#include "platform_hal.h"

typedef struct ObjSyncGroup ObjSyncGroup;

ObjSyncGroup* newSyncGroup(ObjRoutine* routine, ObjPackedUniformArray* items);

void freeSyncGroup(Obj* group);
void markSyncGroup(ObjSyncGroup* group);

void printSyncGroup(FILE* op, ObjSyncGroup* group);

Value receiveSyncGroup(ObjSyncGroup* group);

platform_critical_section* getSyncGroupLock(ObjSyncGroup* group);

#endif
