//
//  testSystem.cpp
//  yarg
//
//  Created by dlm on 31/10/2025.
//

#include "testSystem.h"

#include <cassert>
#include <thread>
#include <atomic>
#include <map>
#include <set>
#include <tuple>

using namespace std;

namespace {

std::mutex client;
class TestSystem *testSystem;

class TestSystem
{
public:
    enum class Expected
    {
        Read,
        ReadAny,
        Write,
        WriteAny
    };
    
public:
    static void simulateInterrupt(uint32_t id) // thread entry
    {
        unique_lock<mutex> lock(::testSystem->simulateCondition);
        ::testSystem->simulate.wait(lock);

        auto &ih{testSystem->interruptHandlers};
        auto entry{ih.find(id)};
        if (entry != ih.end())
        {
            (entry->second)();
        }
        else
        {
            printf("ts double remove handler\n");
            // todo log double remove handler
        }
    }
    
public:
    mutex simulateCondition;
    condition_variable simulate;
    multiset<tuple<Expected, uint32_t, uint32_t> > expected; // address, value - value == 0 if WriteAny or ReadAny
    map<uint32_t, uint32_t> memory; // address, value
    map<uint32_t, void (*)(void)> interruptHandlers; // num, address
    vector<thread> interrupts; // interrupt id
};

TestSystem *self()
{
    std::unique_lock<std::mutex> lock(client);
    if (::testSystem == 0)
    {
        ::testSystem = new TestSystem;
    }
    return ::testSystem;
}

} // anonymous

// system under test interface
uint32_t tsRead(uint32_t address)
{
    auto &mem{self()->memory};
    auto &exp{::testSystem->expected};

    auto i{mem.find(address)};
    if (i != mem.end())
    {
        auto er{exp.find(make_tuple(TestSystem::Expected::Read, address, i->second))};
        if (er != exp.end())
        {
            exp.erase(er);
        }
        else
        {
            auto era{exp.find(make_tuple(TestSystem::Expected::ReadAny, address, 0u))};
            if (era != exp.end())
            {
                exp.erase(era);
            }
            else
            {
                printf("ts unexpected read\n");
                // todo add unexpected read to log
            }
        }
        return i->second;
        
    }
    else
    {
        // todo add read without write to log
        printf("ts read without write\n");
        return 0xa5a5a5a5u;
    }
}
    
void tsWrite(uint32_t address, uint32_t value)
{
    auto &mem{self()->memory};
    mem[address] = value;

    auto &exp{::testSystem->expected};
    auto ew{exp.find(make_tuple(TestSystem::Expected::Write, address, value))};
    if (ew != exp.end())
    {
        exp.erase(ew);
    }
    else
    {
        auto ewa{exp.find(make_tuple(TestSystem::Expected::WriteAny, address, 0u))};
        if (ewa != exp.end())
        {
            exp.erase(ewa);
        }
        else
        {
            printf("ts Unexpected write\n");
            // todo add unexpected write to log
        }
    }
}

void tsAddInterruptHandler(uint32_t intId, void (*address)(void))
{
    auto &ih{self()->interruptHandlers};
    auto entry{ih.find(intId)};
    if (entry == ih.end())
    {
        ih.emplace(intId, address);
    }
    else
    {
        printf("ts double add handler\n");
        // todo log double add handler
    }
}

void tsRemoveInterruptHandler(uint32_t intId, void (*address)(void))
{
    auto &ih{self()->interruptHandlers};
    auto entry{ih.find(intId)};
    if (entry != ih.end())
    {
        ih.erase(entry);
    }
    else
    {
        printf("ts double remove handler\n");
        // todo log double remove handler
    }
}

// test code interface
void tsSync()
{
    std::unique_lock<std::mutex> lock(self()->simulateCondition);
    self()->simulate.notify_all();

    auto &ints{self()->interrupts};
    for (auto &th : ints)
    {
        th.join();
    }
    ints.clear();
    
    auto &exps{self()->expected};
//    multiset<tuple<Expected, uint32_t, uint32_t> > expected; // address, value - value == 0 if WriteAny or ReadAny
    for (auto &e : exps)
    {
        printf("ts unfulfilled expectatiions\n");
     // todo any unfulfilled expectatiions
    }
    exps.clear();
}

void tsExpectRead(uint32_t address, uint32_t value)
{
    auto &exp{self()->expected};
    exp.emplace(TestSystem::Expected::Read, address, value);
}

void tsExpectReadAnyValue(uint32_t address)
{
    auto &exp{self()->expected};
    exp.emplace(TestSystem::Expected::ReadAny, address, 0u);
    
}

void tsExpectWrite(uint32_t address, uint32_t value)
{
    auto &exp{self()->expected};
    exp.emplace(TestSystem::Expected::Write, address, value);
}

void tsExpectWriteAnyValue(uint32_t address)
{
    auto &exp{self()->expected};
    exp.emplace(TestSystem::Expected::WriteAny, address, 0u);
}

void tsSetMemory(uint32_t address, uint32_t value)
{
    auto &mem{self()->memory};
    mem[address] = value;
}

void tsTriggerInterrupt(uint32_t intId)
{
    auto &ints{self()->interrupts};
    ints.emplace_back(thread{TestSystem::simulateInterrupt, intId});
}

void tsTriggerInterruptByName(std::string interruptName)
{
    static const map<string const, uint32_t> lut{
        {"TIMER_IRQ_0", 0},
        {"TIMER_IRQ_1", 1},
        {"TIMER_IRQ_2", 2},
        {"TIMER_IRQ_3", 3},
        {"PWM_IRQ_WRAP", 4},
        {"USBCTRL_IRQ", 5},
        {"XIP_IRQ", 6},
        {"PIO0_IRQ_0", 7},
        {"PIO0_IRQ_1", 8},
        {"PIO1_IRQ_0", 9},
        {"PIO1_IRQ_1", 10},
        {"DMA_IRQ_0", 11},
        {"DMA_IRQ_1", 12},
        {"IO_IRQ_BANK0", 13},
        {"IO_IRQ_QSPI", 14},
        {"SIO_IRQ_PROC0", 15},
        {"SIO_IRQ_PROC1", 16},
        {"CLOCKS_IRQ", 17},
        {"SPI0_IRQ", 18},
        {"SPI1_IRQ", 19},
        {"UART0_IRQ", 20},
        {"UART1_IRQ", 21},
        {"ADC0_IRQ_FIFO", 22},
        {"I2C0_IRQ", 23},
        {"I2C1_IRQ", 24},
        {"RTC_IRQ", 24},
        {"TIMER0_IRQ_0", 0},
        {"TIMER0_IRQ_1", 1},
        {"TIMER0_IRQ_2", 2},
        {"TIMER0_IRQ_3", 3},
        {"TIMER1_IRQ_0", 4},
        {"TIMER1_IRQ_1", 5},
        {"TIMER1_IRQ_2", 6},
        {"TIMER1_IRQ_3", 7},
        {"PWM_IRQ_WRAP_0", 8},
        {"PWM_IRQ_WRAP_1", 9},
        {"2350_DMA_IRQ_0", 10},
        {"2350_DMA_IRQ_1", 11},
        {"DMA_IRQ_2", 12},
        {"DMA_IRQ_3", 13},
        {"2350_USBCTRL_IRQ", 14},
        {"2350_PIO0_IRQ_0", 15},
        {"2350_PIO0_IRQ_1", 16},
        {"2350_PIO1_IRQ_0", 17},
        {"2350_PIO1_IRQ_1", 18},
        {"PIO2_IRQ_0", 19},
        {"PIO2_IRQ_1", 20},
        {"2350_IO_IRQ_BANK0", 21},
        {"IO_IRQ_BANK0_NS", 22},
        {"2350_IO_IRQ_QSPI", 23},
        {"IO_IRQ_QSPI_NS", 24},
        {"SIO_IRQ_FIFO", 25},
        {"SIO_IRQ_BELL", 26},
        {"SIO_IRQ_FIFO_NS", 27},
        {"SIO_IRQ_BELL_NS", 28},
        {"SIO_IRQ_MTIMECMP", 29},
        {"2350_CLOCKS_IRQ", 30},
        {"2350_SPI0_IRQ", 31},
        {"2350_SPI1_IRQ", 32},
        {"2350_UART0_IRQ", 33},
        {"2350_UART1_IRQ", 34},
        {"ADC_IRQ_FIFO", 35},
        {"2350_I2C0_IRQ", 36},
        {"2350_I2C1_IRQ", 37},
        {"OTP_IRQ", 38},
        {"TRNG_IRQ", 39},
        {"PROC0_IRQ_CTI", 40},
        {"PROC1_IRQ_CTI", 41},
        {"PLL_SYS_IRQ", 42},
        {"PLL_USB_IRQ", 43},
        {"POWMAN_IRQ_POW", 44},
        {"POWMAN_IRQ_TIMER", 45},
        {"SPAREIRQ_IRQ_0", 46},
        {"SPAREIRQ_IRQ_1", 47},
        {"SPAREIRQ_IRQ_2", 48},
        {"SPAREIRQ_IRQ_3", 49},
        {"SPAREIRQ_IRQ_4", 50},
        {"SPAREIRQ_IRQ_5", 51}
    };
    
    auto interrupt{lut.find(interruptName)};
    if (interrupt != lut.end())
    {
        tsTriggerInterrupt(interrupt->second);
    }
    else
    {
        printf("ts failed to find interrupt by name\n");
        // todo log failed to find interrupt by name
    }
}
