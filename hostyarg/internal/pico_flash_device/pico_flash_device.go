package pico_flash_device

const (
	BaseAddr      uint32 = 0x10000000
	Size                 = 2 * 1024 * 1024
	ErasePageSize        = 4096
	ProgPageSize         = 256
)
