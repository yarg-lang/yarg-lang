package io

type BlockIOReaderInfo interface {
	ProgPageSize() int
	PagePerBlock() int

	BlockCount() int

	PagePresent(block, page int) bool
}

type BlockIOReader interface {
	BlockIOReaderInfo

	ReadPage(p []byte, block, page, offset int) (n int, err error)
}

type BlockIOWriterInfo interface {
	EraseBlockSize() int
}

type BlockIOWriter interface {
	BlockIOWriterInfo

	EraseBlock(block int) error
	WritePage(p []byte, block, page int) (n int, err error)
}

type BlockIOReaderWriter interface {
	BlockIOReader
	BlockIOWriter
}

type DeviceAddress uint32

type BlockDeviceReaderInfo interface {
	BlockIOReaderInfo
	BlocksInUse() int
	TargetAddress(block, page int) DeviceAddress
}

type BlockDeviceReader interface {
	BlockDeviceReaderInfo
	ReadPage(p []byte, address DeviceAddress) (n int, err error)
}

type BlockDeviceWriterInfo interface {
	BlockIOWriterInfo
	IsBlockStart(targetAddr DeviceAddress) bool
}

type BlockDeviceWriter interface {
	BlockDeviceWriterInfo
	EraseBlock(address DeviceAddress)
	WritePage(p []byte, address DeviceAddress) (n int, err error)
}

type BlockDeviceReaderWriter interface {
	BlockDeviceReader
	BlockDeviceWriter
}
