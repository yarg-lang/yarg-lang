#include <stdio.h>

#if defined(CYARG_PICO_SDK_SYNC)
#include <pico/sync.h>
#elif defined(CYARG_PTHREADS_SYNC)
#include <pthread.h>
#else
#error "No platform mutex implementation defined."
#endif

#include "platform_hal.h"

void vm_mutex_init(vm_mutex* cs) {
#if defined(CYARG_PICO_SDK_SYNC)
    critical_section_init(cs);
#elif defined(CYARG_PTHREADS_SYNC)
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(cs, &attr);
    pthread_mutexattr_destroy(&attr);
#else
    #error "No platform critical section implementation defined."
#endif
}

void vm_mutex_deinit(vm_mutex* cs) {
#if defined(CYARG_PICO_SDK_SYNC)
    critical_section_deinit(cs);
#elif defined(CYARG_PTHREADS_SYNC)
    pthread_mutex_destroy(cs);
#else
    #error "No platform critical section implementation defined."
#endif
}

void vm_mutex_enter_blocking(vm_mutex* cs) {
#if defined(CYARG_PICO_SDK_SYNC)
    critical_section_enter_blocking(cs);
#elif defined(CYARG_PTHREADS_SYNC)
    pthread_mutex_lock(cs);
#else
    #error "No platform critical section implementation defined."
#endif

}

void vm_mutex_exit(vm_mutex* cs) {
#if defined(CYARG_PICO_SDK_SYNC)
    critical_section_exit(cs);
#elif defined(CYARG_PTHREADS_SYNC)
    pthread_mutex_unlock(cs);
#else
    #error "No platform critical section implementation defined."
#endif
}