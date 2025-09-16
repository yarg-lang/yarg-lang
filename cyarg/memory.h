#ifndef cyarg_memory_h
#define cyarg_memory_h

#include "common.h"
#include "object.h"

#define TEMP_ROOTS_MAX 8
#define FIRST_GC_AT 50 * 1024
#ifdef CYARG_PICO_TARGET
#define ALWAYS_GC_ABOVE 100 * 1024
#else
#define ALWAYS_GC_ABOVE 1024 * 1024
#endif

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * oldCount, 0)

void* reallocate(void* pointer, size_t oldSize, size_t newSize);

void tempRootPush(Value value);
Value tempRootPop();

void markObject(Obj* object);
void markDynamicObjArray(DynamicObjArray* array);
void markValue(Value value);
void markValueCell(ValueCell* value);
void collectGarbage();
void freeObjects();
void printObjects();

#endif