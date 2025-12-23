//
//  testSystem.h
//  yarg
//
//  Created by dlm on 25/11/2025.
//

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// system under test interface
uint32_t tsRead(uint32_t address);
void tsWrite(uint32_t address, uint32_t value);
void tsAddInterruptHandler(uint32_t intId, void (*address)(void));
void tsRemoveInterruptHandler(uint32_t intId, void (*address)(void));

#ifdef __cplusplus
}
#endif
