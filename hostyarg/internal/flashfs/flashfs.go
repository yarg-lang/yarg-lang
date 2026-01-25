package flashfs

import (
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/block_device"
)

type FlashFS struct {
	Device       block_device.BlockFSReadWriter
	Base_address uint32
}

func (fs FlashFS) AddressForBlock(block, offset uint32) uint32 {

	byte_offset := block*fs.Device.EraseBlockSize() + offset

	return fs.Base_address + byte_offset
}
