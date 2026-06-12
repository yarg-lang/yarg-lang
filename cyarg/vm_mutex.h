#ifndef cyarg_vm_mutex_h
#define cyarg_vm_mutex_h

#if defined(CYARG_PICO_SDK_SYNC)
#include <pico/sync.h>
typedef critical_section_t vm_mutex;
#elif defined(CYARG_PTHREADS_SYNC)
#include <pthread.h>
typedef pthread_mutex_t vm_mutex;
#endif

void vm_mutex_init(vm_mutex* cs);
void vm_mutex_deinit(vm_mutex* cs);
void vm_mutex_enter_blocking(vm_mutex* cs);
void vm_mutex_exit(vm_mutex* cs);
#endif
