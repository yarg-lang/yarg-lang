#ifndef cyarg_memory_h
#define cyarg_memory_h

/**
 * A heap that provides a locking guarantee for memory operations.
 * No assumptions are made about the underlying heap's thread-safety.
 * 
 * In self-hosted builds, this heap can be exposed to other c libraries
 * with a stdlib compatible interface. See vm-c-heap/ .
 */

#include <stddef.h>
#include "vm_mutex.h"

void vm_heap_init(vm_mutex* mutex);

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

#endif
