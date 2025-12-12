//
//  flash.h
//  yarg
//
//  Created by dlm on 31/10/2025.
//

#define PICO_OK 1

typedef void flash_fn(void *);

void flash_range_program(uint32_t, const uint8_t *, size_t);
void flash_range_erase(uint32_t, uint32_t);

int flash_safe_execute(flash_fn, void *, uint32_t);
