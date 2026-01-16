#include <string.h>

#include "pico_lfs_hal.h"

#include <pico/flash.h>
#include <hardware/flash.h>

#include "pico_flash_fs.h"

// We need an offset in bytes for erase and program operations.
#define FLASHFS_FLASH_OFFSET ((const size_t)(FLASHFS_BASE_ADDR - PICO_FLASH_BASE_ADDR))

const uint8_t* fsAddressForBlock(uint32_t block, uint32_t off) {

    uint32_t byte_offset = block * PICO_ERASE_PAGE_SIZE + off;

    return (const uint8_t*) FLASHFS_BASE_ADDR + byte_offset;
}

/*
 * Read from the flash device. Pico's flash is memory mapped, so memcpy will work well
 */
int pico_read_flash_block(const struct lfs_config *c, 
                          lfs_block_t block,
                          lfs_off_t off, 
                          void *buffer, 
                          lfs_size_t size) {

    memcpy(buffer, fsAddressForBlock(block, off), size);
    return LFS_ERR_OK;
}

/*
 * The Pico SDK provides flash_safe_execute which can be used to wrap write (erase/program)
 * operations to the flash device. The implementation varies, depending on your target 
 * configuration. It will disable interupts, and pause code execution on core 1 if needed.
 * 
 * Using it needs the underlying functions to be passed parameters through a single data
 * pointer, hence the need for a couple of structures to pass them.
 */

struct prog_param {
    uint32_t flash_offs;
    const uint8_t *data; 
    size_t count;
};

static void call_flash_range_program(void* param) {
    struct prog_param* p = (struct prog_param*)param;
    flash_range_program(p->flash_offs, p->data, p->count);
}

/*
 * Write (program) a block of data to the flash device. 
 */
int pico_prog_flash_block(const struct lfs_config *c, 
                          lfs_block_t block,
                          lfs_off_t off, 
                          const void *buffer, 
                          lfs_size_t size) {

    struct prog_param p = {
        .flash_offs = FLASHFS_FLASH_OFFSET + block * PICO_ERASE_PAGE_SIZE + off,
        .data = buffer,
        .count = size
    };

    int rc = flash_safe_execute(call_flash_range_program, &p, UINT32_MAX);
    if (rc == PICO_OK) {
        return LFS_ERR_OK;
    }
    else {
        return LFS_ERR_IO;
    }
}

// we only need to pass one parameter, so cast it into the pointer.
static void call_flash_range_erase(void* param) {
    uint32_t offset = (uint32_t)param;
    flash_range_erase(offset, PICO_ERASE_PAGE_SIZE);
}

/*
 * Erase a block of flash, in preparation for future writes
 */
int pico_erase_flash_block(const struct lfs_config *c, lfs_block_t block) {
    
    uint32_t offset = FLASHFS_FLASH_OFFSET + block * PICO_ERASE_PAGE_SIZE;
    
    int rc = flash_safe_execute(call_flash_range_erase, (void*)offset, UINT32_MAX);
    if (rc == PICO_OK) {
        return LFS_ERR_OK;
    }
    else {
        return LFS_ERR_IO;
    }
}

/*
 * Our writes appear to be atomic without the need for a second 'flush' to complete
 * any pending writes.
 */
int pico_sync_flash_block(const struct lfs_config *c) {
    
    return LFS_ERR_OK;
}
