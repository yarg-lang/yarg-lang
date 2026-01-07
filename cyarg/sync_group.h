#ifndef cyarg_sync_group_h
#define cyarg_sync_group_h

#include <stdio.h>
#include "value.h"
#include "object.h"

#ifdef CYARG_PICO_TARGET
#include <pico/sync.h>
#else
#include <semaphore.h>
#endif

typedef struct ObjSyncGroup ObjSyncGroup;

ObjSyncGroup* newSyncGroup(ObjRoutine* routine, ObjPackedUniformArray* items);

void freeSyncGroup(Obj* group);
void markSyncGroup(ObjSyncGroup* group);

void printSyncGroup(FILE* op, ObjSyncGroup* group);

Value receiveSyncGroup(ObjSyncGroup* group);

#ifdef CYARG_PICO_TARGET
critical_section_t* getSyncGroupLock(ObjSyncGroup* group);
#else
pthread_mutex_t* getSyncGroupLock(ObjSyncGroup* group);
#endif

#endif
