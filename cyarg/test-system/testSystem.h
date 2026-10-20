//
//  testSystem.h
//  yarg
//
//  Created by dlm on 25/11/2025.
//

#include <stdint.h>
#include <stddef.h>

// system under test interface
size_t tsRead(size_t address);
void tsWrite(size_t address, size_t value);
void tsAddInterruptHandler(uint32_t intId, void (*address)(void));
void tsRemoveInterruptHandler(uint32_t intId, void (*address)(void));
