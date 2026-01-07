#include <stdio.h>
#ifdef CYARG_PICO_TARGET
#include <pico/sync.h>
#else
#include <pthread.h>
#endif

#include "sync_group.h"

#include "common.h"
#include "value.h"
#include "object.h"
#include "channel.h"
#include "yargtype.h"
#include "routine.h"
#include "memory.h"

typedef struct ObjSyncGroup {
    Obj obj;
#ifdef CYARG_PICO_TARGET
    critical_section_t group_lock;
#else
    pthread_mutex_t group_lock;
#endif
    ObjPackedUniformArray* channel_array;
    ObjPackedUniformArray* result_array;
} ObjSyncGroup;

ObjSyncGroup* newSyncGroup(ObjRoutine* routine, ObjPackedUniformArray* items) {
    ObjSyncGroup* group = ALLOCATE_OBJ(ObjSyncGroup, OBJ_SYNCGROUP);
    push(routine, OBJ_VAL(group));
#ifdef CYARG_PICO_TARGET
    critical_section_init(&group->group_lock);
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&group->group_lock, &attr);
    pthread_mutexattr_destroy(&attr);
#endif
    group->channel_array = items;
    ObjConcreteYargTypeArray* t = (ObjConcreteYargTypeArray*)newYargArrayTypeFromType(NIL_VAL);
    push(routine, OBJ_VAL(t));
    t->cardinality = arrayCardinality(items->store);
    group->result_array = newPackedUniformArray(t);
    pop(routine);
    pop(routine);
    return group;
}

void freeSyncGroup(Obj* obj) {
    ObjSyncGroup* group = (ObjSyncGroup*)obj;

#ifdef CYARG_PICO_TARGET
    critical_section_deinit(&group->group_lock);
#else
    pthread_mutex_destroy(&group->group_lock);
#endif
    FREE(ObjSyncGroup, obj);
}

void markSyncGroup(ObjSyncGroup* group) {
    markObject((Obj*)group->channel_array);
    markObject((Obj*)group->result_array);
}

void printSyncGroup(FILE* op, ObjSyncGroup* group) {
    FPRINTMSG(op, "sync_group{");
    Value results = unpackValue(group->result_array->store);
    fprintValue(op, results);
    FPRINTMSG(op, "}");
}

Value receiveSyncGroup(ObjSyncGroup* group) {
    size_t num_channels = arrayCardinality(group->channel_array->store);

    bool wait_complete = false;
    while (!wait_complete && num_channels > 0) {
        wait_complete = false;
#ifdef CYARG_PICO_TARGET
        critical_section_enter_blocking(&group->group_lock);
#endif
        for (size_t i = 0; i < num_channels; i++) {
            PackedValue channel_cursor = arrayElement(group->channel_array->store, i);
            Value channelVal = unpackValue(channel_cursor);
            Value data = NIL_VAL;
            if (!IS_NIL(channelVal)) {
                data = peekChannel(AS_CHANNEL(channelVal));
                if (!IS_NIL(data)) {
                    data = collectFromChannel(AS_CHANNEL(channelVal));
                }
            }
            wait_complete |= !IS_NIL(data);
            PackedValue trg = arrayElement(group->result_array->store, i);
            assignToPackedValue(trg, data);
        }
#ifdef CYARG_PICO_TARGET
        critical_section_exit(&group->group_lock);
#endif
    }
    return OBJ_VAL(group->result_array);
}

#ifdef CYARG_PICO_TARGET
critical_section_t* getSyncGroupLock(ObjSyncGroup* group) {
    return &group->group_lock;
}
#else
pthread_mutex_t* getSyncGroupLock(ObjSyncGroup* group) {
    return &group->group_lock;
}
#endif
