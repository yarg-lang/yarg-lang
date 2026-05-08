#include <stdlib.h>

#include "chunk.h"
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

        if (is->type != value.type) continue;
        switch (value.type) {
        case VAL_BOOL: if (is->as.boolean != value.as.boolean) continue; break;
        case VAL_NIL: break;
        case VAL_DOUBLE: if (is->as.dbl != value.as.dbl) continue; break;
        case VAL_I8:
        case VAL_UI8: if (is->as.ui8 != value.as.ui8) continue; break;
        case VAL_I16:
        case VAL_UI16: if (is->as.ui16 != value.as.ui16) continue; break;
        case VAL_I32:
        case VAL_UI32: if (is->as.ui32 != value.as.ui32) continue; break;
        case VAL_UI64:
        case VAL_I64: if (is->as.i64 != value.as.i64) continue; break;
        case VAL_ADDRESS: if (is->as.address != value.as.address) continue; break;
        case VAL_OBJ:
            if (is->as.obj == value.as.obj) break;
            if (is->as.obj->type != value.as.obj->type) continue;
            switch (value.as.obj->type) {
            case OBJ_INT:
                if (AS_INT(*is) == AS_INT(value)) break;
                if (int_is(AS_INT(*is), AS_INT(value)) != INT_EQ) continue;
                break;
            case OBJ_STRING:
                if (AS_STRING(*is)->length != AS_STRING(value)->length) continue;
                if (AS_STRING(*is)->chars == AS_STRING(value)->chars) break;
                if (memcmp(AS_STRING(*is)->chars, AS_STRING(value)->chars, AS_STRING(value)->length) != 0) continue;
                printf("str had to compare\n");
                break;
            case OBJ_FUNCTION: continue;
            default:
                printf("Unexpected const %d\n", value.as.obj->type);
                break;
            }
        }
        return i;
    }


    tempRootPush(value);
    appendToDynamicValueArray(&chunk->constants, value);
    tempRootPop();
    return chunk->constants.count - 1;
}
