#ifndef cyarg_platform_hal_h
#define cyarg_platform_hal_h

void plaform_hal_init();

#if defined(CYARG_PICO_SDK_SYNC)
#include <pico/sync.h>
typedef recursive_mutex_t platform_mutex;
typedef critical_section_t platform_critical_section;
#elif defined(CYARG_PTHREADS_SYNC)
#include <pthread.h>
typedef pthread_mutex_t platform_mutex;
typedef pthread_mutex_t platform_critical_section;
#endif

void platform_mutex_init(platform_mutex* mutex);
void platform_mutex_enter(platform_mutex* mutex);
void platform_mutex_leave(platform_mutex* mutex);

void platform_critical_section_init(platform_critical_section* cs);
void platform_critical_section_deinit(platform_critical_section* cs);
void platform_critical_section_enter_blocking(platform_critical_section* cs);
void platform_critical_section_exit(platform_critical_section* cs);
#endif
