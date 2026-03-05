package yarg_littlefs

import "github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_flash_device"

// Defines one region of flash to use for a filesystem. The size is a multiple of
// the 4096 byte erase size. We calculate it's location working back from the end of the
// flash device, so that code flashed at the start of the device will not collide.
// Pico's have a 2Mb flash device, so we're looking to be less than 2Mb.

const (
	// 128 blocks will reserve a 512K filsystem - 1/4 of the 2Mb device on a Pico
	BlockCount = 128

	sizeBytes = BlockCount * pico_flash_device.ErasePageSize
	// A start location counted back from the end of the device.
	BaseAddr uint32 = pico_flash_device.BaseAddr + pico_flash_device.SizeBytes - sizeBytes
)
