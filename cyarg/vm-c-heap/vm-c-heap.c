#include <stddef.h>
#include <string.h>

// for the Pico SDK WRAPPER_FUNC macro
#include <pico/platform/compiler.h>

// the core of the vm's memory management
#include "memory.h"
#include "vm.h"

/**
 * Wrapped C heap functions for the VM.
 * These functions intercept standard C heap operations and redirect them
 * to the VM's memory management system.
 * 
 * This is for targets using the Pico SDK.
 */

void WRAPPER_FUNC(free)(void* ptr) {
    vm_free(ptr);
}

void *WRAPPER_FUNC(malloc)(size_t size) {
    return vm_allocate(size);
}

void *WRAPPER_FUNC(calloc)(size_t nmemb, size_t size) {
    void* cell = vm_allocate(nmemb * size);
    if (cell)
        memset(cell, 0, nmemb * size);
    return cell;
}

void *WRAPPER_FUNC(realloc)(void* ptr, size_t size) {
    return vm_reallocate(ptr, size);
}
