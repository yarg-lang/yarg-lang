#ifndef cyarg_memory_h
#define cyarg_memory_h

/* see object.h for memory management details */

#include "common.h"
#include "object.h"
#include "value.h"

typedef struct O1HeapInstance O1HeapInstance;

// configuration
#define TEMP_ROOTS_MAX 8
#define FIRST_GC_AT 50 * 1024
#define ALWAYS_GC_ABOVE 100 * 1024

// general purpose memory allocation functions

#define ALLOCATE(type, count) \
    (type*)vm_allocate(sizeof(type) * (count))

void* vm_allocate(size_t newSize);
// vm_reallocate will call vm_free if newSize is 0
void* vm_reallocate(void* pointer, size_t newSize);
void vm_free(void* pointer);

// conveniences for the vm's internal dynamic arrays.
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)vm_reallocate(pointer, sizeof(type) * (newCount))

// temporary root management for the garbage collector
void tempRootPush(Value value);
void tempObjRootPush(Obj* object);
Value tempRootPop();

void init_heap_instance(O1HeapInstance** instance);


// functions that are 'internal to heap management'

typedef struct ObjFunction ObjFunction;
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
