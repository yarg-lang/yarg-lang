#ifndef cyarg_memory_h
#define cyarg_memory_h

/* see object.h for memory management details */

#include "common.h"
#include "object.h"
#include "value.h"

#define TEMP_ROOTS_MAX 8
#define FIRST_GC_AT 50 * 1024
#define ALWAYS_GC_ABOVE 100 * 1024

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

// general purpose memory management functions
void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void vm_free(void* pointer);

void tempRootPush(Value value);
void tempObjRootPush(Obj* object);
Value tempRootPop();

void init_heap_instance(O1HeapInstance** instance);


// functions that are 'internal to heap management'

typedef struct ObjFunction ObjFunction;
typedef struct O1HeapInstance O1HeapInstance;
typedef struct ValueCell ValueCell;

// for use only when the garbage collector is freeing an object.
void gc_free(void* pointer);

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
