//
//  testSystem.h
//  yarg
//
//  Created by dlm on 25/11/2025.
//

#include <stdint.h>
#include <stddef.h>

// system under test interface
uint32_t tsRead(size_t address);
void tsWrite(size_t address, uint32_t value);
void tsAddInterruptHandler(uint32_t intId, void (*address)(void));
void tsRemoveInterruptHandler(uint32_t intId, void (*address)(void));
