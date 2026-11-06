#ifndef cyarg_heap_h
#define cyarg_heap_h

#include <stddef.h>
#include <stdbool.h>

/**
 * A heap that provides accounting of bytes allocated.
 * 
 * No locking promises are made; this heap is not thread-safe.
 */

void heap_init();

void* heap_allocate(size_t size);
// heap_reallocate will call heap_free if new_size is 0
void* heap_reallocate(void* pointer, size_t new_size);
void heap_free(void* pointer);

size_t heap_bytes_allocated();

// debug support
bool heap_invariant();
void heap_log_diagnostics();
#endif