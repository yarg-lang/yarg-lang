#ifndef bdfs_lfs_hal_h
#define bdfs_lfs_hal_h

#include <stdint.h>

struct lfs_config;

void install_bdfs_cb(struct lfs_config* cfg, uintptr_t flash_fs);

#endif