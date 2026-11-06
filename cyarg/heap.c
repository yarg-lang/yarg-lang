#include "heap.h"

#include <stdalign.h>
#include <stdio.h>

#if defined CYARG_CSTDLIB_HEAP
#include <stdlib.h>
#else
#include "../external/o1heap/o1heap/o1heap.h"
#endif

#ifdef CYARG_CSTDLIB_HEAP
typedef struct heap_item {
    size_t size;
    uint8_t data[];
} heap_item;
#endif

typedef struct heap {
#ifdef CYARG_CSTDLIB_HEAP
    size_t bytes_allocated;
#else
    O1HeapInstance* instance;
#if defined(CYARG_SELF_HOSTED)
    // TODO: configure heap size for all self-hosted boards.
    alignas(O1HEAP_ALIGNMENT)uint8_t heapArena[190 * 1024];
#else
    alignas(O1HEAP_ALIGNMENT)uint8_t heapArena[10000 * 1024];
#endif
#endif
} heap;

heap cyarg_heap;

void heap_init() {
#if defined CYARG_CSTDLIB_HEAP
    cyarg_heap.bytes_allocated = 0;
    return;
#else
    cyarg_heap.instance = o1heapInit(&cyarg_heap.heapArena[0], sizeof(cyarg_heap.heapArena));
    return;
#endif
}

void* heap_allocate(size_t size) {
#if defined CYARG_CSTDLIB_HEAP
    cyarg_heap.bytes_allocated += size;
    heap_item* item = malloc(size + sizeof(heap_item));
    if (item == NULL) {
        return NULL;
    }
    item->size = size;
    return &item->data[0];
#else
    return o1heapAllocate(cyarg_heap.instance, size);
#endif
}

void* heap_reallocate(void* old_pointer, size_t new_size) {
#if defined CYARG_CSTDLIB_HEAP
    heap_item* old_item = (heap_item*)((uint8_t*)old_pointer - offsetof(heap_item, data));
    if (old_pointer != NULL) {
        cyarg_heap.bytes_allocated -= old_item->size;
    }

    if (new_size == 0) {
        if (old_pointer != NULL) {
            free(old_item);
        }
        return NULL;
    }

    heap_item* item = realloc(old_pointer == NULL ? NULL : old_item, new_size + sizeof(heap_item));
    if (item != NULL) {
        item->size = new_size;
        cyarg_heap.bytes_allocated += new_size;
        return &item->data[0];
    }
    return NULL;
#else
    if (new_size == 0 && old_pointer != NULL) {
        o1heapFree(cyarg_heap.instance, old_pointer);
        return NULL;
    }
    return o1heapReallocate(cyarg_heap.instance, old_pointer, new_size);
#endif
}

void heap_free(void* pointer) {
#if defined CYARG_CSTDLIB_HEAP
    if (pointer == NULL) {
        return;
    }
    heap_item* item = (heap_item*)((uint8_t*)pointer - offsetof(heap_item, data));
    cyarg_heap.bytes_allocated -= item->size;
    free(item);
#else
    o1heapFree(cyarg_heap.instance, pointer);
#endif
}

size_t heap_bytes_allocated() {
#if defined CYARG_CSTDLIB_HEAP
    return cyarg_heap.bytes_allocated;
#else
    O1HeapDiagnostics diagnostics = o1heapGetDiagnostics(cyarg_heap.instance);
    return diagnostics.allocated;
#endif
}

void heap_log_diagnostics() {
#if defined CYARG_CSTDLIB_HEAP
    fprintf(stderr, "Heap bytes allocated: %zu\n", cyarg_heap.bytes_allocated);
#else
    O1HeapDiagnostics diagnostics = o1heapGetDiagnostics(cyarg_heap.instance);
    fprintf(stderr, "Heap bytes allocated: %zu\n", diagnostics.allocated);
#endif
}

bool heap_invariant() {
#if defined CYARG_CSTDLIB_HEAP
    return true;
#else
    return o1heapDoInvariantsHold(cyarg_heap.instance);
#endif
}