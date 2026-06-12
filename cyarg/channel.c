#include <stdio.h>
#ifdef CYARG_PTHREADS_SYNC
#include <semaphore.h>
#include <fcntl.h>   // for O_CREAT
#include <sys/stat.h> // for S_IRUSR and S_IWUSR
#endif

#include "common.h"
#include "vm_mutex.h"

#include "channel.h"
#include "memory.h"
#include "vm.h"
#include "debug.h"
#include "yargtype.h"
#include "sync_group.h"

typedef struct ObjChannelContainer {
    Obj obj;
    bool overflow;
    size_t writeCursor;
    vm_mutex lock;
    vm_mutex* lock_access;
#ifdef CYARG_PTHREADS_SYNC
    sem_t* access;
#endif

    Value* buffer;
    size_t bufferSize;
    volatile size_t occupied;
} ObjChannelContainer;

ObjChannelContainer* newChannel(ObjRoutine* routine, size_t capacity) {
    ObjChannelContainer* channel = ALLOCATE_OBJ(ObjChannelContainer, OBJ_CHANNELCONTAINER);
    tempRootPush(OBJ_VAL(channel));
    channel->overflow = false;

    channel->buffer = ALLOCATE(Value, capacity);
    channel->bufferSize = capacity;

    for (int i = 0; i < capacity; i++) {
        channel->buffer[i] = NIL_VAL;
    }
    vm_mutex_init(&channel->lock);
    channel->lock_access = &channel->lock;
#ifdef CYARG_PTHREADS_SYNC
    channel->access = sem_open("/semaphore", O_CREAT, S_IRUSR | S_IWUSR, 0);
    if (channel->access == SEM_FAILED) {
        tempRootPop();
        runtimeError(routine, "Semaphore open failed.");
    }
#endif
    tempRootPop();
    return channel;
}

void freeChannelObject(Obj* object) {
    ObjChannelContainer* channel = (ObjChannelContainer*)object;
    vm_mutex_deinit(&channel->lock);
#ifdef CYARG_PTHREADS_SYNC    
    sem_close(channel->access);
#endif

    FREE_ARRAY(Value, channel->buffer, channel->bufferSize);
    FREE(ObjChannelContainer, object); 
}

size_t readCursor(ObjChannelContainer* channel) {
    size_t count = channel->occupied;
    size_t size = channel->bufferSize;
    size_t cursor = (channel->writeCursor + size - count) % size;
    return cursor;
}

static void channelMutexEnter(ObjChannelContainer* channel) {
    vm_mutex_enter_blocking(channel->lock_access);
}

static void channelMutexLeave(ObjChannelContainer* channel) {
    vm_mutex_exit(channel->lock_access);
}
 
void markChannel(ObjChannelContainer* channel) {
    if (channel->buffer != 0) {
        size_t cursor = readCursor(channel);
        for (int i = 0; i < channel->occupied; i++) {
            markValue(channel->buffer[cursor]);
            cursor = (cursor + 1) % channel->bufferSize;
        }
    }
}

ObjString* channelToString(ObjChannelContainer* channel) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "channel{");
    size_t string_cursor = strlen(buffer);
    size_t cursor = readCursor(channel);
    for (int i = 0; i < channel->occupied; i++) {
        ObjString* valueStr = valueToString(channel->buffer[cursor]);
        snprintf(buffer + string_cursor, sizeof(buffer) - string_cursor, "%s", valueStr->chars);
        string_cursor = strlen(buffer);
        if (i < channel->occupied - 1) {
            snprintf(buffer + string_cursor, sizeof(buffer) - string_cursor, ", ");
            string_cursor = strlen(buffer);
        }
        cursor = (cursor + 1) % channel->bufferSize;
    }
    snprintf(buffer + string_cursor, sizeof(buffer) - string_cursor, "}");
    return copyString(buffer, (int)strlen(buffer));
}

void sendChannel(ObjChannelContainer* channel, Value data) {
#ifdef CYARG_PICO_BUSY_SYNC
    while (channel->occupied == channel->bufferSize) {
        // stall/block until space
        tight_loop_contents();
    }
#endif

    channelMutexEnter(channel);
    channel->buffer[channel->writeCursor] = data;
    channel->occupied++;
    channel->writeCursor = (channel->writeCursor + 1) % channel->bufferSize;
    channel->overflow = false;
    channelMutexLeave(channel);

#ifdef CYARG_PTHREADS_SYNC
    sem_post(channel->access);
#endif
}

Value collectFromChannel(ObjChannelContainer* channel) {
    Value result = NIL_VAL;

    channelMutexEnter(channel);
    size_t cursor = readCursor(channel);
    result = channel->buffer[cursor];
    channel->occupied--;
    channel->overflow = false;
    channelMutexLeave(channel);

    return result;
}

Value receiveChannel(ObjChannelContainer* channel) {

#if defined (CYARG_PICO_BUSY_SYNC)
    while (channel->occupied == 0) {
        // stall/block until data
        tight_loop_contents();
    }
#elif defined(CYARG_PTHREADS_SYNC)
    sem_wait(channel->access);
#else
#error "No channel synchronization implementation for this build."
#endif
    return collectFromChannel(channel);
}

bool shareChannel(ObjChannelContainer* channel, Value data) {
    bool result = false;

    channelMutexEnter(channel);
    channel->buffer[channel->writeCursor] = data;
    channel->occupied++;
    channel->writeCursor = (channel->writeCursor + 1) % channel->bufferSize;
    if (channel->occupied > channel->bufferSize) {
        channel->overflow = true;
        channel->occupied = channel->bufferSize;
    }
    result = channel->overflow;
    channelMutexLeave(channel);

#ifdef CYARG_PTHREADS_SYNC
    if (!result) {
        sem_post(channel->access);
    }
#endif

    return result;
}

Value peekChannel(ObjChannelContainer* channel) {
    Value result = NIL_VAL;

    if (channel->occupied > 0) {
        result = channel->buffer[readCursor(channel)];
    }

    return result;
}

void joinSyncGroup(ObjChannelContainer* channel, ObjSyncGroup* group) {
    vm_mutex_deinit(&channel->lock);
    channel->lock_access = getSyncGroupLock(group);
}

void leaveSyncGroup(ObjChannelContainer* channel, ObjSyncGroup* group) {
    vm_mutex_init(&channel->lock);
    channel->lock_access = &channel->lock;
}
