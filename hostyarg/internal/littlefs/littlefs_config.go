package littlefs

/*
#include <stdlib.h>
#include "lfs.h"
*/
import "C"
import (
	"runtime"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_flash_device"
)

type LittleFsConfig struct {
	C_handle *C.struct_lfs_config
}

func NewLittleFsConfig(blockCount uint32) *LittleFsConfig {
	var ccfg C.struct_lfs_config

	// block device configuration
	ccfg.read_size = 1
	ccfg.prog_size = pico_flash_device.PICO_PROG_PAGE_SIZE
	ccfg.block_size = pico_flash_device.PICO_ERASE_PAGE_SIZE

	// the number of blocks we use for a flash fs.
	// Can be zero if we can read it from the fs.
	ccfg.block_count = C.lfs_size_t(blockCount)

	// cache needs to be a multiple of the programming page size.
	ccfg.cache_size = ccfg.prog_size * 1

	ccfg.lookahead_size = 16
	ccfg.block_cycles = 500

	cfg := LittleFsConfig{C_handle: &ccfg}
	return &cfg
}

func (cfg *LittleFsConfig) PinStorage(pins *runtime.Pinner) {
	pins.Pin(cfg.C_handle)
	pins.Pin(cfg.C_handle.context)
}
