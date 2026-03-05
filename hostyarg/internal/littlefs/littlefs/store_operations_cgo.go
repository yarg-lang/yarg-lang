package littlefs

// provides a C hook to install both the callbacks from go, and the fs_store object passed to them.

/*
#include <stdint.h>
#include "store_operations.h"

int store_read_page(lfs_const_config_ptr c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int store_prog_page(lfs_const_config_ptr c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
int store_erase_block(lfs_const_config_ptr c, lfs_block_t block);

// our memory based store immediately commits reads and writes, so no sync operation is required.
static int sync_nop(lfs_const_config_ptr c) {

    return LFS_ERR_OK;
}

void install_store_operations(struct lfs_config* cfg, uintptr_t storage_ops_handle) {
    cfg->read  = store_read_page;
    cfg->prog  = store_prog_page;
    cfg->erase = store_erase_block;
    cfg->sync  = sync_nop;

    cfg->context = (void*)storage_ops_handle;
}
*/
import "C"
