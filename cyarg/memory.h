#ifndef cyarg_memory_h
#define cyarg_memory_h

/* see object.h for memory management details */

#include "common.h"
#include "object.h"
#include "value.h"

typedef struct ObjFunction ObjFunction;
typedef struct O1HeapInstance O1HeapInstance;
typedef struct ValueCell ValueCell;

void init_heap_instance(O1HeapInstance** instance);

#define TEMP_ROOTS_MAX 8
#define FIRST_GC_AT 50 * 1024
#define ALWAYS_GC_ABOVE 100 * 1024

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) gc_free(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    gc_free(pointer, sizeof(type) * oldCount, 0)

void* gc_free(void* pointer, size_t oldSize, size_t newSize);
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

void tempRootPush(Value value);
void tempObjRootPush(Obj* object);
Value tempRootPop();

void markObject(Obj* object);
void markDynamicObjArray(DynamicObjArray* array);
void markValue(Value value);
void markValueCell(ValueCell* value);
void markFunction(ObjFunction* function);
void collectGarbage();
void freeObjects();
void printObjects();
void pinObj(Obj* object);

#endif
