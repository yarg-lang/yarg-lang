
#ifdef CYARG_PICO_TARGET
#include <pico/stdlib.h>
#include <pico/sync.h>
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
#endif
}

void platform_mutex_enter(platform_mutex* mutex) {
#ifdef CYARG_PICO_TARGET
    recursive_mutex_enter_blocking (mutex);
#endif
}

void platform_mutex_leave(platform_mutex* mutex) {
#ifdef CYARG_PICO_TARGET
    recursive_mutex_exit(mutex);
#endif
}
