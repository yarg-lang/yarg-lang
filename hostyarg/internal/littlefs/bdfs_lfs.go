package littlefs

/*
#include "bdfs_lfs_hal.h"
*/
import "C"
import (
	"runtime"
	"unsafe"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/block_device"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/flashfs"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_flash_device"
)

// Defines one region of flash to use for a filesystem. The size is a multiple of
// the 4096 byte erase size. We calculate it's location working back from the end of the
// flash device, so that code flashed at the start of the device will not collide.
// Pico's have a 2Mb flash device, so we're looking to be less than 2Mb.

const (
	// 128 blocks will reserve a 512K filsystem - 1/4 of the 2Mb device on a Pico
	FLASHFS_BLOCK_COUNT = 128
	FLASHFS_SIZE_BYTES  = pico_flash_device.PICO_ERASE_PAGE_SIZE * FLASHFS_BLOCK_COUNT

	// A start location counted back from the end of the device.
	FLASHFS_BASE_ADDR uint32 = pico_flash_device.PICO_FLASH_BASE_ADDR + pico_flash_device.PICO_FLASH_SIZE_BYTES - FLASHFS_SIZE_BYTES
)

type BdFS struct {
	Cfg      *C.struct_lfs_config
	Storage  block_device.BlockDevice
	Flash_fs flashfs.FlashFS
	gohandle *flashfs.FlashFS
	pins     *runtime.Pinner
}

func NewBdFS(device block_device.BlockDevice, baseAddr uint32, blockCount uint32) *BdFS {

	cfg := BdFS{Cfg: NewLittleFsConfig(blockCount,
		pico_flash_device.PICO_PROG_PAGE_SIZE,
		pico_flash_device.PICO_ERASE_PAGE_SIZE),
		Storage: device,
		Flash_fs: flashfs.FlashFS{Device: device,
			Base_address: baseAddr},
		pins: &runtime.Pinner{}}
	cfg.gohandle = &cfg.Flash_fs

	cfg.pins.Pin(cfg.gohandle)
	cfg.pins.Pin(cfg.Cfg)
	cfg.Storage.PinStorage(cfg.pins)

	C.install_bdfs_cb(cfg.Cfg, C.uintptr_t(uintptr(unsafe.Pointer(cfg.gohandle))))
	cfg.pins.Pin(cfg.Cfg.context)

	return &cfg
}

func (fs BdFS) Close() error {
	fs.pins.Unpin()

	return nil
}
