//
//  missing.c
//  yarg
//
//  Created by dlm on 31/10/2025.
//

#include "lfs.h"

void flash_range_erase(){}
void flash_range_program(){}
void flash_safe_execute(){}
int bdfs_read_cgo(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size){return 0;}
int bdfs_prog_page_cgo(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size){return 0;}
int bdfs_erase_block_cgo(const struct lfs_config* c, lfs_block_t block){return 0;}
