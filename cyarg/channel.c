#include <stdio.h>
#ifdef CYARG_PICO_TARGET
#include <pico/sync.h>
#else
#include <semaphore.h>
#include <pthread.h>
#endif

#include "common.h"

#include "channel.h"
#include "memory.h"
#include "vm.h"
#include "debug.h"

typedef struct ObjChannelContainer {
    Obj obj;
    bool overflow;
    size_t writeCursor;
#ifdef CYARG_PICO_TARGET
    critical_section_t lock;
#else
    pthread_mutex_t lock;
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
#ifdef CYARG_PICO_TARGET
    critical_section_init(&channel->lock);
#else
    channel->access = sem_open("/semaphore", O_CREAT, S_IRUSR | S_IWUSR, 0);
    if (channel->access == SEM_FAILED) {
        tempRootPop();
        runtimeError(routine, "Semaphore open failed.");
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&channel->lock, &attr);
    pthread_mutexattr_destroy(&attr);
#endif
    tempRootPop();
    return channel;
}

void freeChannelObject(Obj* object) {
    ObjChannelContainer* channel = (ObjChannelContainer*)object;
#ifdef CYARG_PICO_TARGET
    critical_section_deinit(&channel->lock);
#else
    pthread_mutex_destroy(&channel->lock);
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
#ifdef CYARG_PICO_TARGET
    critical_section_enter_blocking(&channel->lock);
#else
    pthread_mutex_lock(&channel->lock);
#endif
}

static void channelMutexLeave(ObjChannelContainer* channel) {
#ifdef CYARG_PICO_TARGET
    critical_section_exit(&channel->lock);
#else
    pthread_mutex_unlock(&channel->lock);
#endif
}
 
void markChannel(ObjChannelContainer* channel) {
    size_t cursor = readCursor(channel);
    for (int i = 0; i < channel->occupied; i++) {
        markValue(channel->buffer[cursor]);
        cursor = (cursor + 1) % channel->bufferSize;
    }
}

void printChannel(FILE* op, ObjChannelContainer* channel) {
    FPRINTMSG(op, "channel{");
    size_t cursor = readCursor(channel);
    for (int i = 0; i < channel->occupied; i++) {
        fprintValue(op, channel->buffer[cursor]);
        if (i < channel->occupied - 1) {
            FPRINTMSG(op, ", ");
        }
        cursor = (cursor + 1) % channel->bufferSize;
    }
    FPRINTMSG(op, "}");
}

void sendChannel(ObjChannelContainer* channel, Value data) {
#ifdef CYARG_PICO_TARGET
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

#ifndef CYARG_PICO_TARGET
    sem_post(channel->access);
#endif
}

Value receiveChannel(ObjChannelContainer* channel) {
    Value result = NIL_VAL;

#ifdef CYARG_PICO_TARGET
    while (channel->occupied == 0) {
        // stall/block until data
        tight_loop_contents();
    }
#else
    sem_wait(channel->access);
#endif

    channelMutexEnter(channel);
    size_t cursor = readCursor(channel);
    result = channel->buffer[cursor];
    channel->occupied--;
    channel->overflow = false;
    channelMutexLeave(channel);

    return result;
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

#ifdef CYARG_PICO_TARGET
#else    
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
