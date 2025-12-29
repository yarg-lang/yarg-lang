//
//  testSystem.cpp
//  yarg
//
//  Created by dlm on 31/10/2025.
//

#include "testSystem.h"
#include "testIntrinsics.hpp"

#include <cassert>
#include <thread>
#include <map>
#include <vector>
#include <set>
#include <tuple>
#include <print>
#include <mutex>
#include <format>
#include <condition_variable>

using namespace std;

namespace {

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
    static void simulateInterrupt(uint32_t id); // thread entry
    static TestSystem &self()
    {
        volatile static TestSystem ts;
        return const_cast<TestSystem &>(ts);
    }

public:
    uint32_t read(uint32_t address);
    void write(uint32_t address, uint32_t value);
    void addInterruptHandler(uint32_t intId, void (*address)(void));
    void removeInterruptHandler(uint32_t intId, void (*address)(void));

public:
    void log(string const &);

public:
    mutex simulateInterruptsMutex_;
    condition_variable simulateInterrupts_;
    mutex expectedMutex_;
    multiset<tuple<Expected, uint32_t, uint32_t> > expected_; // address, value - value == 0 if WriteAny or ReadAny
    mutex memoryMutex_;
    map<uint32_t, uint32_t> memory_; // address, value
    mutex interruptHandlersMutex_;
    map<uint32_t, void (*)(void)> interruptHandlers_; // num, address
    mutex interruptsMutex_;
    vector<thread> interrupts_; // interrupt id
    mutex logMutex_;
    vector<string> log_;
};

const map<string const, uint32_t> lut
{
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

} // anonymous

uint32_t tsRead(uint32_t address)
{
    return TestSystem::self().read(address);
}

void tsWrite(uint32_t address, uint32_t value)
{
    TestSystem::self().write(address, value);
}

void tsAddInterruptHandler(uint32_t intId, void (*address)(void))
{
    TestSystem::self().addInterruptHandler(intId, address);
}

void tsRemoveInterruptHandler(uint32_t intId, void (*address)(void))
{
    TestSystem::self().removeInterruptHandler(intId, address);
}

void TestSystem::simulateInterrupt(uint32_t id) // thread entry
{
    TestSystem &self{TestSystem::self()};
    {
        unique_lock<mutex> lock(self.simulateInterruptsMutex_);
        self.simulateInterrupts_.wait(lock);
    } // release lock here allows all interrupts to run simultaneously
    
    bool ihFound;
    void (*foundIsr)(){0};
    {
        unique_lock<mutex> lock(self.interruptHandlersMutex_);
        auto entry{self.interruptHandlers_.find(id)};
        ihFound = entry != self.interruptHandlers_.end();
        if (ihFound)
        {
            foundIsr = entry->second;
        }
    }
    if (ihFound)
    {
        (*foundIsr)();
    }
    else
    {
        self.log(format("missing irq_add_shared_handler({}, , );", id));
    }
}

// system under test interface
uint32_t TestSystem::read(uint32_t address)
{
    bool writtenOrSet;
    uint32_t valueWrittenOrSet;
    {
        unique_lock<mutex> lock(memoryMutex_);
        auto i{memory_.find(address)};
        writtenOrSet = i != memory_.end();
        valueWrittenOrSet = i->second;
    }
    if (writtenOrSet)
    {
        bool read;
        auto explicitExpectedRead{make_tuple(TestSystem::Expected::Read, address, valueWrittenOrSet)};
        {
            unique_lock<mutex> lock(expectedMutex_);
            auto er{expected_.find(explicitExpectedRead)};
            read = er != expected_.end();
            if (read)
            {
                expected_.erase(er);
            }
        }
        if (!read)
        {
            auto anyExpectedRead{make_tuple(TestSystem::Expected::ReadAny, address, 0u)};
            {
                unique_lock<mutex> lock(expectedMutex_);
                auto ar{expected_.find(anyExpectedRead)};
                read = ar != expected_.end();
                if (read)
                {
                    expected_.erase(ar);
                }
            }
        }
        if (!read)
        {
            log(format("missing test_read(@x{:06x});", address));
        }
    }
    else
    {
        log(format("missing poke/test_set @x{:06x}", address));
        valueWrittenOrSet = 0xa5a5a5u;
    }
    return valueWrittenOrSet;
}
    
void TestSystem::write(uint32_t address, uint32_t value)
{
    {
        unique_lock<mutex> lock(memoryMutex_);
        memory_[address] = value;
    }

    bool written;
    auto explicitExpectedWrite{make_tuple(TestSystem::Expected::Write, address, value)};
    {
        unique_lock<mutex> lock(expectedMutex_);
        auto ew{expected_.find(explicitExpectedWrite)};
        written = ew != expected_.end();
        if (written)
        {
            expected_.erase(ew);
        }
    }
    if (!written)
    {
        auto anyExpectedWrite{make_tuple(TestSystem::Expected::WriteAny, address, 0u)};
        {
            unique_lock<mutex> lock(expectedMutex_);
            auto aw{expected_.find(anyExpectedWrite)};
            written = aw != expected_.end();
            if (written)
            {
                expected_.erase(aw);
            }
        };
        if (!written)
        {
            log(format("missing test_write(@x{:06x}, {});", address, value));
        }
    }
}

void TestSystem::addInterruptHandler(uint32_t intId, void (*address)(void))
{
    bool removed;
    {
        unique_lock<mutex> lock(interruptHandlersMutex_);
        auto ih{interruptHandlers_.find(intId)};
        removed = ih == interruptHandlers_.end();
        if (removed)
        {
            interruptHandlers_.emplace(intId, address);
        }
    }
    if (!removed)
    {
        log(format("missing irq_remove_handler({}, );", intId));
    }
}

void TestSystem::removeInterruptHandler(uint32_t intId, void (*address)(void))
{
    bool added;
    {
        unique_lock<mutex> lock(interruptHandlersMutex_);
        auto ih{interruptHandlers_.find(intId)};
        added = ih != interruptHandlers_.end();
        if (added)
        {
            interruptHandlers_.erase(ih);
        }
    }
    if (!added)
    {
        log(format("missing irq_add_shared_handler({}, , );", intId));
    }
}

void TestSystem::log(string const &s)
{
    unique_lock<mutex> lock(logMutex_);
    log_.push_back(s);
}

// test code interface
vector<string> &TestIntrinsics::sync()
{
    print("Waiting for interrupts to be simulated - ");
    TestSystem &ts{TestSystem::self()};
    {
        unique_lock<mutex> lock(ts.simulateInterruptsMutex_);
        ts.simulateInterrupts_.notify_all();
    } // release lock here allows all simulateInterrupt to continue

    for (auto &th : ts.interrupts_)
    {
        th.join();
    }
    println("done");

    ts.interrupts_.clear();

    // todo: mutex
    size_t numUnfulfilledExpectations{ts.expected_.size()};
    if (numUnfulfilledExpectations != 0)
    {
        ts.log(format("{} unfulfilled expectation{}:", numUnfulfilledExpectations, numUnfulfilledExpectations > 1 ? "s" : ""));
        //    multiset<tuple<Expected, uint32_t, uint32_t> > expected; // address, value - value == 0 if WriteAny or ReadAny
        for (auto &e : ts.expected_)
        {
            switch (get<0>(e))
            {
            case TestSystem::Expected::Read:
                ts.log(format("test_read(@x{:06x}, {});", get<1>(e), get<2>(e)));
                break;
            case TestSystem::Expected::ReadAny:
                ts.log(format("test_read(@x{:06x});", get<1>(e)));
                break;
            case TestSystem::Expected::Write:
                ts.log(format("test_write(@x{:06x}, {});", get<1>(e), get<2>(e)));
                break;
            case TestSystem::Expected::WriteAny:
                ts.log(format("test_write(@x{:06x});", get<1>(e)));
                break;
            default:
                assert(!"expectations corrupt");
                break;
            }
        }
    }
    ts.expected_.clear();

    return ts.log_; // log cleared by caller
}

void TestIntrinsics::expectRead(uint32_t address, uint32_t value)
{
    TestSystem &ts{TestSystem::self()};
    {
        unique_lock<mutex> lock(ts.expectedMutex_);
        ts.expected_.emplace(TestSystem::Expected::Read, address, value);
    }
}

void TestIntrinsics::expectReadAnyValue(uint32_t address)
{
    TestSystem &ts{TestSystem::self()};
    {
        unique_lock<mutex> lock(ts.expectedMutex_);
        ts.expected_.emplace(TestSystem::Expected::ReadAny, address, 0u);
    }
}

void TestIntrinsics::expectWrite(uint32_t address, uint32_t value)
{
    TestSystem &ts{TestSystem::self()};
    {
        unique_lock<mutex> lock(ts.expectedMutex_);
        ts.expected_.emplace(TestSystem::Expected::Write, address, value);
    }
}

void TestIntrinsics::expectWriteAnyValue(uint32_t address)
{
    TestSystem &ts{TestSystem::self()};
    {
        unique_lock<mutex> lock(ts.expectedMutex_);
        ts.expected_.emplace(TestSystem::Expected::WriteAny, address, 0u);
    }
}

void TestIntrinsics::setMemory(uint32_t address, uint32_t value)
{
    TestSystem &ts{TestSystem::self()};
    {
        unique_lock<mutex> lock(ts.memoryMutex_);
        ts.memory_[address] = value;
    }
}

void TestIntrinsics::triggerInterrupt(uint32_t intId)
{
    TestSystem &ts{TestSystem::self()};
    {
        unique_lock<mutex> lock(ts.interruptsMutex_);
        ts.interrupts_.emplace_back(thread{TestSystem::simulateInterrupt, intId});
    }
}

void TestIntrinsics::triggerInterrupt(string const &s)
{
    TestSystem &ts{TestSystem::self()};
    auto ini{lut.find(s)};
    if (ini != lut.end())
    {
        {
            unique_lock<mutex> lock(ts.interruptsMutex_);
            ts.interrupts_.emplace_back(thread{TestSystem::simulateInterrupt, ini->second});
        }
    }
    else
    {
        ts.log(format("Unable to simulate an interrupt named {}", s));
    }
}
