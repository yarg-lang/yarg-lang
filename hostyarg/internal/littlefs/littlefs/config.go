package littlefs

/*
#include "../../../littlefs/lfs.h"
*/
import "C"
import (
	"runtime"
)

type Config struct {
	lfs_config *C.struct_lfs_config
	store      *storeOps
	pins       runtime.Pinner
}

func NewConfig(device Store, blockCount uint32) *Config {

	var lfsCfg C.struct_lfs_config

	// block device sizes.
	lfsCfg.read_size = C.lfs_size_t(device.ReadSize())
	lfsCfg.prog_size = C.lfs_size_t(device.ProgPageSize())
	lfsCfg.block_size = C.lfs_size_t(device.EraseBlockSize())

	// wear leveling limit.
	lfsCfg.block_cycles = C.int32_t(device.BlockCycles())

	// the number of blocks we use for a flash fs.
	// Can be zero if we can read it from the fs.
	lfsCfg.block_count = C.lfs_size_t(blockCount)

	// cache needs to be a multiple of the programming page size.
	lfsCfg.cache_size = lfsCfg.prog_size * 1

	lfsCfg.lookahead_size = 16

	cfg := Config{lfs_config: &lfsCfg, store: &storeOps{StoreOperations: device}}

	cfg.pins.Pin(cfg.lfs_config)
	cfg.pins.Pin(cfg.store)

	cfg.installStoreOperations()

	return &cfg
}

func (fs *Config) Close() error {
	fs.pins.Unpin()

	return nil
}
