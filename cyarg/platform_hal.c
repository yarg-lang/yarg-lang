#include <stdio.h>
#ifdef CYARG_PICO_STDLIB
#include <pico/stdlib.h>
#endif

#if defined(CYARG_PICO_SDK_SYNC)
#include <pico/sync.h>
#elif defined(CYARG_PTHREADS_SYNC)
#include <pthread.h>
#else
#error "No platform mutex implementation defined."
#endif

#include "platform_hal.h"

void plaform_hal_init() {
#ifdef CYARG_PICO_STDLIB
    stdio_init_all();
#endif
}

void platform_mutex_init(platform_mutex* mutex) {
#if defined(CYARG_PICO_SDK_SYNC)
    recursive_mutex_init(mutex);
#elif defined(CYARG_PTHREADS_SYNC)
    static pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    int i = pthread_mutex_init(mutex, &attr); // not recursive
    if (i != 0)
	printf("pthread_mutex_init %d\n", i);
#else
    #error "No platform mutex implementation defined."
#endif
}

void platform_mutex_enter(platform_mutex* mutex) {
#if defined(CYARG_PICO_SDK_SYNC)
    recursive_mutex_enter_blocking (mutex);
#elif defined(CYARG_PTHREADS_SYNC)
    pthread_mutex_lock(mutex);
#else
    #error "No platform mutex implementation defined."
#endif
}

void platform_mutex_leave(platform_mutex* mutex) {
#if defined(CYARG_PICO_SDK_SYNC)
    recursive_mutex_exit(mutex);
#elif defined(CYARG_PTHREADS_SYNC)
    pthread_mutex_unlock(mutex);
#else
    #error "No platform mutex implementation defined."
#endif
}

void platform_critical_section_init(platform_critical_section* cs) {
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

void platform_critical_section_deinit(platform_critical_section* cs) {
#if defined(CYARG_PICO_SDK_SYNC)
    critical_section_deinit(cs);
#elif defined(CYARG_PTHREADS_SYNC)
    pthread_mutex_destroy(cs);
#else
    #error "No platform critical section implementation defined."
#endif
}

void platform_critical_section_enter_blocking(platform_critical_section* cs) {
#if defined(CYARG_PICO_SDK_SYNC)
    critical_section_enter_blocking(cs);
#elif defined(CYARG_PTHREADS_SYNC)
    // currently using nop pthread mutex for critical section
#else
    #error "No platform critical section implementation defined."
#endif

}

void platform_critical_section_exit(platform_critical_section* cs) {
#if defined(CYARG_PICO_SDK_SYNC)
    critical_section_exit(cs);
#elif defined(CYARG_PTHREADS_SYNC)
    // currently using nop pthread mutex for critical section
#else
    #error "No platform critical section implementation defined."
#endif
}