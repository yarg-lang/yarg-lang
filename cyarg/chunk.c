#include "chunk.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "memory.h"
#include "vm.h"

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->numLines = 0;
    chunk->lineCapacity = 0;
    chunk->lines = 0;
    initDynamicValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk) {
    if (!chunk->xip) {
        FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    }
    FREE_ARRAY(ChunkSource, chunk->lines, chunk->lineCapacity);
    freeDynamicValueArray(&chunk->constants);
    initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;
}

int addConstant(Chunk* chunk, Value value) {

    for (int i = 0; i < chunk->constants.count; i++) {
        Value *is = &chunk->constants.values[i];

        if (valuesEqual(*is, value)) {
            return i;
        }
    }

    tempRootPush(value);
    appendToDynamicValueArray(&chunk->constants, value);
    tempRootPop();
    return chunk->constants.count - 1;
}
