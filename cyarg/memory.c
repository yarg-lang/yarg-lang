#include "memory.h"

#include "common.h"
#include "heap.h"
#include "vm_mutex.h"

void gc_objects(size_t size_requested);

typedef struct vm_heap {
    vm_mutex* mutex;
} vm_heap;

static vm_heap heap_instance;

void vm_heap_init(vm_mutex* mutex) {
    heap_instance.mutex = mutex;
    heap_init();
}

size_t vm_bytes_allocated() {
    vm_mutex_enter_blocking(heap_instance.mutex);
    size_t bytes = heap_bytes_allocated();
    vm_mutex_exit(heap_instance.mutex);
    return bytes;
}

void* vm_allocate(size_t newSize) {
    if (newSize == 0) return NULL;

    vm_mutex_enter_blocking(heap_instance.mutex);

    gc_objects(newSize);

    void* result = heap_allocate(newSize);
    if (result == NULL) {
        fatalVMError("Available memory exhausted.");
    }
    vm_mutex_exit(heap_instance.mutex);
    return result;
}

void* vm_reallocate(void* pointer, size_t newSize) {
    vm_mutex_enter_blocking(heap_instance.mutex);

    gc_objects(newSize);

    if (newSize == 0) {
        heap_free(pointer);
        vm_mutex_exit(heap_instance.mutex);
        return NULL;
    }

    void* result = heap_reallocate(pointer, newSize);
    if (result == NULL) {
        fatalVMError("Available memory exhausted.");
    }
    vm_mutex_exit(heap_instance.mutex);
    return result;
}

void vm_free(void* pointer) {
    vm_mutex_enter_blocking(heap_instance.mutex);
    heap_free(pointer);
    vm_mutex_exit(heap_instance.mutex);
    return;
}
