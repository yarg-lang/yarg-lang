#ifndef cyarg_platform_hal_h
#define cyarg_platform_hal_h

void plaform_hal_init();

#if defined(CYARG_PICO_SDK_SYNC)
#include <pico/sync.h>
typedef recursive_mutex_t platform_mutex;
#elif defined(CYARG_PTHREADS_SYNC)
#include <pthread.h>
typedef pthread_mutex_t platform_mutex;
#endif

void platform_mutex_init(platform_mutex* mutex);
void platform_mutex_enter(platform_mutex* mutex);
void platform_mutex_leave(platform_mutex* mutex);

#endif
