#ifndef pico_lfs_hal_h
#define pico_lfs_hal_h

#include "../../hostyarg/littlefs/lfs.h"

// block device functions required for littlefs
int pico_read_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int pico_prog_flash_block(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
int pico_erase_flash_block(const struct lfs_config *c, lfs_block_t block);
int pico_sync_flash_block(const struct lfs_config *c);

// Pico's flash can be programed in 256byte pages, and must be erased in 4K pages.
#define PICO_ERASE_PAGE_SIZE 4096
#define PICO_PROG_PAGE_SIZE 256

// Pico's flash device appears at XIP_MAIN_BASE, and reads are identical to reading from memory.
// The flash device has a cache, and the device is mapped to 4 different locations in the 
// address map. Which address range you use, determines the cache behaviour.
// I assume it makes most sense to use the non-cached access, as littlefs has it's own ram
// cache. I assume this leaves the flash device cache for access for other purposes such as 
// code. 
//#define PICO_FLASH_BASE_ADDR XIP_MAIN_BASE
//#define PICO_FLASH_BASE_ADDR XIP_NOALLOC_BASE
//#define PICO_FLASH_BASE_ADDR XIP_NOCACHE_BASE
#define PICO_FLASH_BASE_ADDR XIP_NOCACHE_NOALLOC_BASE

#endif
