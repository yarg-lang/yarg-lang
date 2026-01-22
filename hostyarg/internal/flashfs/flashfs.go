package flashfs

import (
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/block_device"
)

type FlashFS struct {
	Device       block_device.BlockDevice
	Base_address uint32
}

func (fs FlashFS) AddressForBlock(block uint32, off uint32) uint32 {

	byte_offset := block*fs.Device.EraseBlockSize() + off

	return fs.Base_address + byte_offset
}
