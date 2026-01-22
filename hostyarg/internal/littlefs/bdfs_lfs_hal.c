#include "lfs.h"
#include "bdfs_lfs_hal.h"

int bdfs_read_cgo(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int bdfs_prog_page_cgo(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
int bdfs_erase_block_cgo(const struct lfs_config* c, lfs_block_t block);

static int sync_nop(const struct lfs_config *c) {

    return LFS_ERR_OK;
}

void install_bdfs_cb(struct lfs_config* cfg, uintptr_t flash_fs) {
    cfg->read = bdfs_read_cgo;
    cfg->prog  = bdfs_prog_page_cgo;
    cfg->erase = bdfs_erase_block_cgo;
    cfg->sync  = sync_nop;

    cfg->context = (void*)flash_fs;
}
