#ifndef store_callbacks_h
#define store_callbacks_h

#include <stdint.h>
#include "../../../littlefs/lfs.h"

// typedef to include const, as cgo doesn't support it as declaration.
typedef const struct lfs_config* lfs_const_config_ptr;

void install_store_operations(struct lfs_config* cfg, uintptr_t storage_ops_handle);

#endif