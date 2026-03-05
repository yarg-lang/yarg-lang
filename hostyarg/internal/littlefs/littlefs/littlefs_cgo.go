package littlefs

// pull in the littlefs implementation so that cgo can include the symbols.

/*
#include "../../../littlefs/lfs.c"
#include "../../../littlefs/lfs_util.c"
#include "littlefs_utilities.h"

static int traverse_block(void* data, lfs_block_t block) {

    *((uint32_t*)data) += 1;

    return LFS_ERR_OK;
}

int lfs_fs_usage(lfs_t *lfs) {

    uint32_t count = 0;

    int result = lfs_fs_traverse(lfs, &traverse_block, &count);
    if (result < 0) {
        return result;
    } else {
        return count;
    }
}
*/
import "C"
