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

//Int *iiii = 0;

int addConstant(Chunk* chunk, Value value) {
//    if (iiii != 0) {
//        printf("D %u %u\n", iiii->d_, iiii->m_);
//        if (iiii->d_ != 4 || iiii->m_ != 4) {
//            printf("D What!\n");
//        }
//    }

    for (int i = 0; i < chunk->constants.count; i++) {
        Value *is = &chunk->constants.values[i];
//        if (is->type == VAL_BIG_SMALL_INT && is->as.bigSmallInt.obj_) {
//            printf("D %d %u %u %p\n", i, is->as.bigSmallInt.objInt_->bigInt.d_, is->as.bigSmallInt.objInt_->bigInt.m_, &is->as.bigSmallInt.objInt_->bigInt);
//        }

        if (valuesEqual(*is, value)) {
            return i;
        }
    }

    tempRootPush(value);
    appendToDynamicValueArray(&chunk->constants, value);
    tempRootPop();
//    if (chunk->constants.count == 5 && value.type == VAL_BIG_SMALL_INT && value.as.bigSmallInt.obj_ && iiii == 0) {
//        iiii = &value.as.bigSmallInt.objInt_->bigInt;
//        printf("C %u %u %u %p\n", chunk->constants.count - 1, value.as.bigSmallInt.objInt_->bigInt.d_, value.as.bigSmallInt.objInt_->bigInt.m_, &value.as.bigSmallInt.objInt_->bigInt);
//    }
    return chunk->constants.count - 1;
}
