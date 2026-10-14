#include "sync_group.h"

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "value.h"
#include "object.h"
#include "channel.h"
#include "yargtype.h"
#include "yargobject.h"
#include "routine.h"
#include "memory.h"
#include "packed_containers.h"

typedef struct ObjSyncGroup {
    Obj obj;
    vm_mutex group_lock;
    Value channel_array_val;
    Value result_array_val;
    size_t num_channels;
} ObjSyncGroup;

ObjSyncGroup* newSyncGroup(ObjRoutine* routine, Value itemArray) {
    ObjSyncGroup* group = ALLOCATE_OBJ(ObjSyncGroup, OBJ_SYNCGROUP);
    push(routine, SYNCGROUP_VAL(group));
    vm_mutex_init(&group->group_lock);

    ObjConcreteYargTypeArray* channelArrayType = (ObjConcreteYargTypeArray*)itemArray.type;
    group->num_channels = channelArrayType->cardinality;

    group->channel_array_val = itemArray;
    group->result_array_val = allocValueArray(group->num_channels);;
    pop(routine);
    return group;
}

void freeSyncGroup(Obj* obj) {
    ObjSyncGroup* group = (ObjSyncGroup*)obj;
    vm_mutex_deinit(&group->group_lock);
    FREE(ObjSyncGroup, obj);
}

void markSyncGroup(ObjSyncGroup* group) {
    markValue(group->channel_array_val);
    markValue(group->result_array_val);
}

ObjString* syncGroupToString(const ObjSyncGroup* group) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "sync_group{");
    size_t cursor = strlen(buffer);
    ObjString* resultsStr = valueToString(group->result_array_val);
    snprintf(buffer + cursor, sizeof(buffer) - cursor, "%s", resultsStr->chars);
    cursor = strlen(buffer);
    snprintf(buffer + cursor, sizeof(buffer) - cursor, "}");
    return copyString(buffer, (int)strlen(buffer));
}

Value receiveSyncGroup(ObjSyncGroup* group) {
    size_t num_channels = group->num_channels;

    bool wait_complete = false;
    while (!wait_complete && num_channels > 0) {
        wait_complete = false;
        vm_mutex_enter_blocking(&group->group_lock);
        for (size_t i = 0; i < num_channels; i++) {
            Value channelVal = arrayElement(group->channel_array_val, i);

            Value data = NIL_VAL;
            if (!IS_NIL(channelVal)) {
                data = peekChannel(AS_CHANNEL(channelVal));
                if (!IS_NIL(data)) {
                    data = collectFromChannel(AS_CHANNEL(channelVal));
                }
            }
            wait_complete |= !IS_NIL(data);
            setArrayElement(group->result_array_val, i, data);
        }
        vm_mutex_exit(&group->group_lock);
    }
    return group->result_array_val;
}

vm_mutex* getSyncGroupLock(ObjSyncGroup* group) {
    return &group->group_lock;
}
