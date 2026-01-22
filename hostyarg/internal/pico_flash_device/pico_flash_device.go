package pico_flash_device

const (
	PICO_FLASH_BASE_ADDR  uint32 = 0x10000000
	PICO_FLASH_SIZE_BYTES        = 2 * 1024 * 1024
	PICO_ERASE_PAGE_SIZE         = 4096
	PICO_PROG_PAGE_SIZE          = 256
)
