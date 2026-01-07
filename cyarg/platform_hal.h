#ifndef cyarg_platform_hal_h
#define cyarg_platform_hal_h

void plaform_hal_init();

#ifdef CYARG_PICO_TARGET
#include <pico/sync.h>

typedef recursive_mutex_t platform_mutex;
#else
// currently the host platform is single-threaded.
typedef int platform_mutex;
#endif

void platform_mutex_init(platform_mutex* mutex);
void platform_mutex_enter(platform_mutex* mutex);
void platform_mutex_leave(platform_mutex* mutex);

#endif
