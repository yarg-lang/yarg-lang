package littlefs_store

import (
	block_device "github.com/yarg-lang/yarg-lang/hostyarg/internal/block/device"
	block_io "github.com/yarg-lang/yarg-lang/hostyarg/internal/block/io"
)

type Device struct {
	dev         block_io.BlockDeviceReaderWriter
	baseAddress block_io.DeviceAddress
}

func NewDevice(device block_io.BlockDeviceReaderWriter, baseAddr block_io.DeviceAddress) Device {
	return Device{dev: device, baseAddress: baseAddr}
}

func (fs *Device) addressForBlock(block, offset int) block_io.DeviceAddress {

	byte_offset := block*fs.dev.EraseBlockSize() + offset

	return fs.baseAddress + block_io.DeviceAddress(byte_offset)
}

func (fs *Device) ProgPageSize() uint32 {
	return uint32(fs.dev.ProgPageSize())
}

func (fs *Device) EraseBlockSize() uint32 {
	return uint32(fs.dev.EraseBlockSize())
}

func (fs *Device) ReadSize() uint32 {
	return 1
}

// our memory based device does not need wear leveling, so we return -1 to indicate that.
func (fs *Device) BlockCycles() int32 {
	return -1
}

func (fs *Device) ReadPage(p []byte, block, off uint32) (n int, err error) {
	device_address := fs.addressForBlock(int(block), int(off))

	n, err = fs.dev.ReadPage(p, device_address)
	return n, err
}

func (fs *Device) ProgPage(p []byte, block, off uint32) (n int, err error) {
	device_address := fs.addressForBlock(int(block), int(off))

	n, err = fs.dev.WritePage(p, device_address)

	block_device.DebugPrintDevice(fs.dev)

	return n, err
}

func (fs *Device) EraseBlock(block uint32) {
	device_address := fs.addressForBlock(int(block), 0)

	fs.dev.EraseBlock(device_address)
}
