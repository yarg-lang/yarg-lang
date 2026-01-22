package littlefs

/*
#include <stdint.h>
#include "lfs.h"

int go_bdfs_read(uintptr_t fs, lfs_block_t block, lfs_off_t off, uint8_t *buffer, lfs_size_t size);
int go_bdfs_prog_page(uintptr_t fs, lfs_block_t block, lfs_off_t off, const uint8_t *buffer, lfs_size_t size);
int go_bdfs_erase_block(uintptr_t fs, lfs_block_t block);

int bdfs_read_cgo(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {

    return go_bdfs_read((uintptr_t)c->context, block, off, buffer, size);
}

int bdfs_prog_page_cgo(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {

    return go_bdfs_prog_page((uintptr_t)c->context, block, off, buffer, size);
}

int bdfs_erase_block_cgo(const struct lfs_config* c, lfs_block_t block) {

    return go_bdfs_erase_block((uintptr_t)c->context, block);
}
*/
import "C"
