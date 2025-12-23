//
//  testIntrinsics.hpp
//  yarg
//
//  Created by dlm on 25/11/2025.
//

#include <cstdint>
#include <string>
#include <vector>

class TestIntrinsics
{
public:
    static std::vector<std::string> &sync();
    static void expectRead(uint32_t address, uint32_t value);
    static void expectReadAnyValue(uint32_t address);
    static void expectWrite(uint32_t address, uint32_t value);
    static void expectWriteAnyValue(uint32_t address);
    static void setMemory(uint32_t address, uint32_t value);
    static void triggerInterrupt(uint32_t intId);
    static void triggerInterrupt(std::string const&);
};
