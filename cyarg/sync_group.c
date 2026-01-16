#include <stdio.h>

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
    platform_critical_section group_lock;
    ObjPackedUniformArray* channel_array;
    ObjPackedUniformArray* result_array;
} ObjSyncGroup;

ObjSyncGroup* newSyncGroup(ObjRoutine* routine, ObjPackedUniformArray* items) {
    ObjSyncGroup* group = ALLOCATE_OBJ(ObjSyncGroup, OBJ_SYNCGROUP);
    push(routine, OBJ_VAL(group));
    platform_critical_section_init(&group->group_lock);
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
    platform_critical_section_deinit(&group->group_lock);
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
        platform_critical_section_enter_blocking(&group->group_lock);
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
        platform_critical_section_exit(&group->group_lock);
    }
    return OBJ_VAL(group->result_array);
}

platform_critical_section* getSyncGroupLock(ObjSyncGroup* group) {
    return &group->group_lock;
}
