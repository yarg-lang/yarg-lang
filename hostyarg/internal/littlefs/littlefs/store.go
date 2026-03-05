package littlefs

// clients of littlefs provide this block based interface for writing blocks to underlying storage.
// the access pattern matches that of flash devices littlefs is designed for.

type StoreConfig interface {
	ProgPageSize() uint32
	EraseBlockSize() uint32
	ReadSize() uint32
	BlockCycles() int32
}

type StoreOperations interface {
	ReadPage(p []byte, block, off uint32) (n int, err error)
	ProgPage(p []byte, block, off uint32) (n int, err error)
	EraseBlock(block uint32)
}

type Store interface {
	StoreConfig
	StoreOperations
}
