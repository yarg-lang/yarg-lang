
#ifdef CYARG_PICO_TARGET
#include <pico/stdlib.h>
#include <pico/sync.h>
#else
#include <pthread.h>
#endif

#include "platform_hal.h"

void plaform_hal_init() {
#ifdef CYARG_PICO_TARGET
    stdio_init_all();
#endif
}

void platform_mutex_init(platform_mutex* mutex) {
#ifdef CYARG_PICO_TARGET
    recursive_mutex_init(mutex);
#else
    static pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    int i = pthread_mutex_init(mutex, &attr); // not recursive
    while (i);
#endif
}

void platform_mutex_enter(platform_mutex* mutex) {
#ifdef CYARG_PICO_TARGET
    recursive_mutex_enter_blocking (mutex);
#else
    pthread_mutex_lock(mutex);
#endif
}

void platform_mutex_leave(platform_mutex* mutex) {
#ifdef CYARG_PICO_TARGET
    recursive_mutex_exit(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif
}
