//
//  testSystem.hpp
//  yarg
//
//  Created by dlm on 25/11/2025.
//

#include <cstdint>
#include <string>

// test code interface
void tsSync(void);
void tsExpectRead(uint32_t address, uint32_t value);
void tsExpectReadAnyValue(uint32_t address);
void tsExpectWrite(uint32_t address, uint32_t value);
void tsExpectWriteAnyValue(uint32_t address);
void tsSetMemory(uint32_t address, uint32_t value);
void tsTriggerInterrupt(uint32_t intId);
void tsTriggerInterruptByName(std::string interruptName);
