package littlefs

/*
#include <stdlib.h>
#include "lfs.h"
*/
import "C"

func NewLittleFsConfig(blockCount, progSize, blockSize uint32) *C.struct_lfs_config {
	var ccfg C.struct_lfs_config

	// block device configuration
	ccfg.read_size = 1
	ccfg.prog_size = C.lfs_size_t(progSize)
	ccfg.block_size = C.lfs_size_t(blockSize)

	// the number of blocks we use for a flash fs.
	// Can be zero if we can read it from the fs.
	ccfg.block_count = C.lfs_size_t(blockCount)

	// cache needs to be a multiple of the programming page size.
	ccfg.cache_size = ccfg.prog_size * 1

	ccfg.lookahead_size = 16
	ccfg.block_cycles = 500

	return &ccfg
}
