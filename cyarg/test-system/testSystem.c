//
//  testSystem.c
//  yarg
//
//  Created by dlm on 31/10/2025.
//

#include "testSystem.h"

#include "testIntrinsics.h"
#include "../object.h"
#include "../memory.h"

#include <pthread.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

enum TsExpectedType
{
    TsExpectedRead,
    TsExpectedReadAny,
    TsExpectedWrite,
    TsExpectedWriteAny
};

typedef struct TsExpected {
    int n_;
    int e_;
    struct TsExpectedItem {
        enum TsExpectedType operation_;
        uint32_t address_, value_;
    } *i_;
    pthread_mutex_t mutex_;
} TsExpected;

typedef struct TsMemory {
    int n_;
    int e_;
    struct TsMemoryItem {
        uint32_t address_, value_;
    } *i_;
    pthread_mutex_t mutex_;
} TsMemory;

typedef struct TsScheduledInterrupts {
    int n_;
    int e_;
    uint32_t *i_;
    pthread_mutex_t mutex_;
} TsScheduledInterrupts;

typedef struct TsInterruptHandlers {
    int n_;
    int e_;
    struct TsInterruptHandler {
        pthread_t t_;
        uint32_t id_;
        void (*isr_)(void);
        enum State {
            Installing,
            Waiting,
            Go,
            Running,
            Die,
            Dead
        } state_;
    } *i_;
    pthread_mutex_t itemsMutex_;

    pthread_mutex_t *triggerOrDoneMutex_;
    pthread_cond_t triggerInterrupts_; // test-system sets handlers running
    pthread_cond_t doneInterrupts_; // last running handler completes, handler is created or handler is killed
    int numWaiting_; // protected by triggerOrDoneMutex_
} TsInterruptHandlers;

typedef struct TestSystem
{
    TsExpected expected_;
    TsMemory memory_;
    TsInterruptHandlers handlers_;
    TsScheduledInterrupts scheduled_;

    pthread_mutex_t logMutex_;
    TsLog log_;
} TestSystem;

static void *simulateInterrupt(void *); // thread entry

static TestSystem *self(void);
static void extend(void *, size_t); // ensure appropriate mutex is held

static uint32_t read(uint32_t address);
static void write(uint32_t address, uint32_t value);
static void addInterruptHandler(uint32_t intId, void (*address)(void));
static void removeInterruptHandler(uint32_t intId, void (*address)(void));
static void log(char const *, ...);

struct { char *name; int number_; } lut[] =
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

TestSystem *self(void)
{
    static TestSystem ts;
    static bool firstTime = true; // the first time self() is called there are no threads so this is safe
    if (firstTime)
    {
        firstTime = false;
        pthread_cond_init(&ts.handlers_.triggerInterrupts_, 0);
        pthread_cond_init(&ts.handlers_.doneInterrupts_, 0);
        pthread_mutex_init(&ts.expected_.mutex_, 0);
        pthread_mutex_init(&ts.memory_.mutex_, 0);
        pthread_mutex_init(&ts.scheduled_.mutex_, 0);
        pthread_mutex_init(&ts.handlers_.itemsMutex_, 0);
//        pthread_mutex_init(&ts.handlers_.triggerOrDoneMutex_, 0);

        ts.handlers_.triggerOrDoneMutex_ = &ts.handlers_.itemsMutex_;
        pthread_mutex_init(&ts.logMutex_, 0);
    }
    return (TestSystem *)&ts;
}

uint32_t tsRead(uint32_t address)
{
    return read(address);
}

void tsWrite(uint32_t address, uint32_t value)
{
    write(address, value);
}

void tsAddInterruptHandler(uint32_t intId, void (*address)(void))
{
    addInterruptHandler(intId, address);
}

void tsRemoveInterruptHandler(uint32_t intId, void (*address)(void))
{
    removeInterruptHandler(intId, address);
}

void *simulateInterrupt(void *indexAsP) // thread entry
{
    uint32_t index = (uint32_t)(uintptr_t)indexAsP;
    TestSystem *ts = self();

    uint32_t id = UINT32_MAX;
    {
        assert(pthread_mutex_lock(&ts->handlers_.itemsMutex_) == 0);
        id = ts->handlers_.i_[index].id_;

        assert(ts->handlers_.i_[index].state_ == Installing);

        ts->handlers_.i_[index].state_ = Waiting;
        pthread_mutex_unlock(&ts->handlers_.itemsMutex_);
    }

    while (1)
    {
        enum State s;
        {
            assert(pthread_mutex_lock(&ts->handlers_.itemsMutex_) == 0);
            s = ts->handlers_.i_[index].state_;
            pthread_mutex_unlock(&ts->handlers_.itemsMutex_);
        }
        {
            assert(pthread_mutex_lock(ts->handlers_.triggerOrDoneMutex_) == 0);
            ts->handlers_.numWaiting_++;
            int e = pthread_cond_signal(&ts->handlers_.doneInterrupts_);
            assert(e == 0);
            pthread_mutex_unlock(ts->handlers_.triggerOrDoneMutex_);
        }
        if (s == Dead)
        {
            return 0;
        }

        {
            assert(pthread_mutex_lock(ts->handlers_.triggerOrDoneMutex_) == 0);
            int e;
            while (ts->handlers_.i_[index].state_ == Waiting)
            {
                e = pthread_cond_wait(&ts->handlers_.triggerInterrupts_, ts->handlers_.triggerOrDoneMutex_); // unlocks then locks triggerOrDoneMutex_
            }
            pthread_mutex_unlock(ts->handlers_.triggerOrDoneMutex_);
        }
        void (*theIsr)() = 0;
        {
            assert(pthread_mutex_lock(&ts->handlers_.itemsMutex_) == 0);
            s = ts->handlers_.i_[index].state_;
            if (s == Die)
            {
                s = ts->handlers_.i_[index].state_ = Dead;
            }
            else if (s == Go)
            {
                s = ts->handlers_.i_[index].state_ = Running;
                theIsr = ts->handlers_.i_[index].isr_;
            }
            else
            {
                assert(s == Waiting);
            }
            pthread_mutex_unlock(&ts->handlers_.itemsMutex_);
        }
        if (s != Running) continue;

        if (theIsr != 0)
        {
            (*theIsr)();
        }
        else
        {
            log("missing irq_add_shared_handler(%u, , );", id);
        }
        {
            assert(pthread_mutex_lock(&ts->handlers_.itemsMutex_) == 0);
            ts->handlers_.i_[index].state_ = Waiting;
            pthread_mutex_unlock(&ts->handlers_.itemsMutex_);
        }
    }
}

// system under test interface
uint32_t read(uint32_t address)
{
    TestSystem *ts = self();

    bool writtenOrSet = false;
    uint32_t valueWrittenOrSet;
    {
        assert(pthread_mutex_lock(&ts->memory_.mutex_) == 0);
        for (int i = 0; i < ts->memory_.n_; i++)
        {
            if (ts->memory_.i_[i].address_ == address)
            {
                writtenOrSet = true;
                valueWrittenOrSet = ts->memory_.i_[i].value_;
                break;
            }
        }
        pthread_mutex_unlock(&ts->memory_.mutex_);
    }
    if (writtenOrSet)
    {
        bool read = false;
        int i = 0;
        {
            assert(pthread_mutex_lock(&ts->expected_.mutex_) == 0);
            for (; i < ts->expected_.n_; i++)
            {
                if (ts->expected_.i_[i].operation_ == TsExpectedRead &&
                    ts->expected_.i_[i].address_ == address &&
                    ts->expected_.i_[i].value_ == valueWrittenOrSet)
                {
                    read = true;
                    break;
                }
            }
            if (read)
            {
                for (i++; i < ts->expected_.n_; i++)
                {
                    ts->expected_.i_[i - 1] = ts->expected_.i_[i];
                }
                ts->expected_.n_--;
            }
            pthread_mutex_unlock(&ts->expected_.mutex_);
        }
        if (!read)
        {
            i = 0;
            {
                assert(pthread_mutex_lock(&ts->expected_.mutex_) == 0);
                for (; i < ts->expected_.n_; i++)
                {
                    if (ts->expected_.i_[i].operation_ == TsExpectedReadAny && ts->expected_.i_[i].address_ == address)
                    {
                        read = true;
                        break;
                    }
                }
                if (read)
                {
                    for (i++; i < ts->expected_.n_; i++)
                    {
                        ts->expected_.i_[i - 1] = ts->expected_.i_[i];
                    }
                    ts->expected_.n_--;
                }
                pthread_mutex_unlock(&ts->expected_.mutex_);
            }
        }
        if (!read)
        {
            log("missing test_read(0x%06x);", address);
        }
    }
    else
    {
        log("missing poke/test_set 0x%06x", address);
        valueWrittenOrSet = 0xa5a5a5u;
    }
    return valueWrittenOrSet;
}
    
void write(uint32_t address, uint32_t value)
{
    TestSystem *ts = self();
    {
        assert(pthread_mutex_lock(&ts->memory_.mutex_) == 0);
        int i = 0;
        for (; i < ts->memory_.n_; i++)
        {
            if (ts->memory_.i_[i].address_ == address)
            {
                ts->memory_.i_[i].value_ = value;
                break;
            }
        }
        extend(&ts->memory_, sizeof ts->memory_.i_[0]);
        ts->memory_.i_[ts->memory_.n_++] = (struct TsMemoryItem){address, value};
        pthread_mutex_unlock(&ts->memory_.mutex_);
    }

    bool written = false;
    int i = 0;
    {
        assert(pthread_mutex_lock(&ts->expected_.mutex_) == 0);
        for (; i < ts->expected_.n_; i++)
        {
            if (ts->expected_.i_[i].operation_ == TsExpectedWrite &&
                ts->expected_.i_[i].address_ == address &&
                ts->expected_.i_[i].value_ == value)
            {
                written = true;
                break;
            }
        }
        if (written)
        {
            for (i++; i < ts->expected_.n_; i++)
            {
                ts->expected_.i_[i - 1] = ts->expected_.i_[i];
            }
            ts->expected_.n_--;
        }
        pthread_mutex_unlock(&ts->expected_.mutex_);
    }
    if (!written)
    {
        i = 0;
        {
            assert(pthread_mutex_lock(&ts->expected_.mutex_) == 0);
            for (; i < ts->expected_.n_; i++)
            {
                if (ts->expected_.i_[i].operation_ == TsExpectedWriteAny && ts->expected_.i_[i].address_ == address)
                {
                    written = true;
                    break;
                }
            }
            if (written)
            {
                for (i++; i < ts->expected_.n_; i++)
                {
                    ts->expected_.i_[i - 1] = ts->expected_.i_[i];
                }
                ts->expected_.n_--;
            }
            pthread_mutex_unlock(&ts->expected_.mutex_);
        }
    }
    if (!written)
    {
        log("missing test_write(0x%06x, %u);", address, value);
    }
}

void addInterruptHandler(uint32_t intId, void (*address)(void))
{
    TestSystem *ts = self();

    struct TsInterruptHandler *newHandler = 0;
    {
        pthread_mutex_lock(&ts->handlers_.itemsMutex_);
        for (int i = 0; i < ts->handlers_.n_; i++)
        {
            if (ts->handlers_.i_[i].id_ == intId && ts->handlers_.i_[i].isr_ == address)
            {
                newHandler = &ts->handlers_.i_[i];
                break;
            }
        }
        if (newHandler == 0)
        {
            extend(&ts->handlers_, sizeof ts->handlers_.i_[0]);
            newHandler = &ts->handlers_.i_[ts->handlers_.n_++];
            newHandler->id_ = intId;
            newHandler->isr_ = address;
            newHandler->state_ = Installing;
            pthread_create(&newHandler->t_, 0, simulateInterrupt, (void *)(uintptr_t)(ts->handlers_.n_ - 1));
        }
        pthread_mutex_unlock(&ts->handlers_.itemsMutex_);
    }
    if (newHandler != 0)
    {
        assert(pthread_mutex_lock(ts->handlers_.triggerOrDoneMutex_) == 0);
        int e;
        while (newHandler->state_ == Installing)
        {
            e = pthread_cond_wait(&ts->handlers_.doneInterrupts_, ts->handlers_.triggerOrDoneMutex_); // unlocks (allows all triggered handlers to run) then locks triggerOrDoneMutex_
        }
        pthread_mutex_unlock(ts->handlers_.triggerOrDoneMutex_);
    }
    else
    {
        log("missing irq_remove_handler(%u, );", intId);
    }
}

void removeInterruptHandler(uint32_t intId, void (*address)(void))
{
    TestSystem *ts = self();

    bool added = false;
    {
        pthread_mutex_unlock(&ts->handlers_.itemsMutex_);
        int i = 0;
        for (; i < ts->handlers_.n_; i++)
        {
            if (ts->handlers_.i_[i].id_ == intId && ts->handlers_.i_[i].isr_ == address)
            {
                added = true;
                break;
            }
        }
        if (added)
        {
            for (i++; i < ts->expected_.n_; i++)
            {
                ts->handlers_.i_[i - 1] = ts->handlers_.i_[i];
            }
            ts->handlers_.n_--;
        }
        pthread_mutex_unlock(&ts->handlers_.itemsMutex_);
    }
    if (!added)
    {
        log("missing irq_add_shared_handler(%u, , );", intId);
    }
}

void log(char const *s, ...)
{
    va_list args;
    va_start(args, s);
    int len = vsnprintf((char *)0, 0, s, args);
    va_end(args);

    len++;

    char *newString = reallocate(0, 0, len);
    va_start(args, s);
    (void)vsnprintf(newString, len, s, args);
    va_end(args);

    TestSystem *ts = self();
    {
        assert(pthread_mutex_lock(&ts->logMutex_) == 0);
        extend(&ts->log_, sizeof ts->log_.i_[0]);
        ts->log_.i_[ts->log_.n_++] = newString;
        pthread_mutex_unlock(&ts->logMutex_);
    }
}

// test code interface
TsLog *testIntrinsicsSync(void)
{
    printf("Waiting for interrupts to be simulated - ");
    TestSystem *ts = self();

    // trigger interrupts
    int numberInTranche = 0;
    while (1)
    {
        int numberToSchedule;
        {
            assert(pthread_mutex_lock(&ts->scheduled_.mutex_) == 0);
            numberToSchedule = ts->scheduled_.n_;
            pthread_mutex_unlock(&ts->scheduled_.mutex_);
        }
        if (numberToSchedule == 0) break;

        uint32_t id;
        {
            assert(pthread_mutex_lock(&ts->scheduled_.mutex_) == 0);
            id = ts->scheduled_.i_[0];
            pthread_mutex_unlock(&ts->scheduled_.mutex_);
        }

        bool triggerThisTranche = false;
        bool found = false;
        {
            assert(pthread_mutex_lock(&ts->handlers_.itemsMutex_) == 0);
            for (int i = 0; i < ts->handlers_.n_ && !triggerThisTranche; i++)
            {
                if (ts->handlers_.i_[i].id_ == id)
                {
                    if (ts->handlers_.i_[i].state_ == Waiting)
                    {
                        ts->handlers_.i_[i].state_ = Go;
                        found = true;
                        if (numberToSchedule == 1)
                        {
                            triggerThisTranche = true;
                        }
                        break;
                    }
                    else
                    {
                        triggerThisTranche = true; // if it is already triggered the
                    }
                    break;
                }
            }
            pthread_mutex_unlock(&ts->handlers_.itemsMutex_);
        }

        if (found) // remove from scheduled
        {
            numberInTranche++;
            assert(pthread_mutex_lock(&ts->scheduled_.mutex_) == 0);
            for (int i = 1; i < ts->scheduled_.n_; i++)
            {
                ts->scheduled_.i_[i - 1] = ts->scheduled_.i_[i];
            }
            ts->scheduled_.n_--;
            pthread_mutex_unlock(&ts->scheduled_.mutex_);
        }
        if (triggerThisTranche)
        {
            {
                assert(pthread_mutex_lock(ts->handlers_.triggerOrDoneMutex_) == 0);
                assert(ts->handlers_.numWaiting_ == ts->handlers_.n_);
                ts->handlers_.numWaiting_ -= numberInTranche;
                numberInTranche = 0;
                pthread_cond_broadcast(&ts->handlers_.triggerInterrupts_);

                int e;
                assert(ts->handlers_.numWaiting_ <= ts->handlers_.n_);
                while (ts->handlers_.numWaiting_ != ts->handlers_.n_)
                {
                    e = pthread_cond_wait(&ts->handlers_.doneInterrupts_, ts->handlers_.triggerOrDoneMutex_); // unlocks (allows all triggered handlers to run) then locks triggerOrDoneMutex_
                }
                pthread_mutex_unlock(ts->handlers_.triggerOrDoneMutex_);
            }
        }
    }

    printf("done\n");

    {
        assert(pthread_mutex_lock(&ts->expected_.mutex_) == 0);
        int numUnfulfilledExpectations = ts->expected_.n_;
        if (numUnfulfilledExpectations != 0)
        {
            log("%d unfulfilled expectation%s:", numUnfulfilledExpectations, numUnfulfilledExpectations > 1 ? "s" : "");
            //    multiset<tuple<Expected, uint32_t, uint32_t> > expected; // address, value - value == 0 if WriteAny or ReadAny
            for (int i = 0; i < ts->expected_.n_; i++)
            {
                switch (ts->expected_.i_[i].operation_)
                {
                case TsExpectedRead:
                    log("test_read(0x%06x, %u);", ts->expected_.i_[i].address_, ts->expected_.i_[i].value_);
                    break;
                case TsExpectedReadAny:
                    log("test_read(0x%06x);", ts->expected_.i_[i].address_);
                    break;
                case TsExpectedWrite:
                    log("test_write(0x%06x, %u);", ts->expected_.i_[i].address_, ts->expected_.i_[i].value_);
                    break;
                case TsExpectedWriteAny:
                    log("test_write(0x%06x);", ts->expected_.i_[i].address_);
                    break;
                default:
                    assert(!"expectations corrupt");
                    break;
                }
            }
        }
        ts->expected_.n_ = 0;
        pthread_mutex_unlock(&ts->expected_.mutex_);
    }
    return &ts->log_; // log cleared by caller
}

void testIntrinsicsExpectRead(uint32_t address, uint32_t value)
{
    TestSystem *ts = self();
    {
        assert(pthread_mutex_lock(&ts->expected_.mutex_) == 0);
        extend(&ts->expected_, sizeof ts->expected_.i_[0]);
        ts->expected_.i_[ts->expected_.n_++] = (struct TsExpectedItem){TsExpectedRead, address, value};
        pthread_mutex_unlock(&ts->expected_.mutex_);
    }
}

void testIntrinsicsExpectReadAnyValue(uint32_t address)
{
    TestSystem *ts = self();
    {
        assert(pthread_mutex_lock(&ts->expected_.mutex_) == 0);
        extend(&ts->expected_, sizeof ts->expected_.i_[0]);
        ts->expected_.i_[ts->expected_.n_++] = (struct TsExpectedItem){TsExpectedReadAny, address, 0u};
        pthread_mutex_unlock(&ts->expected_.mutex_);
    }
}

void testIntrinsicsExpectWrite(uint32_t address, uint32_t value)
{
    TestSystem *ts = self();
    {
        assert(pthread_mutex_lock(&ts->expected_.mutex_) == 0);
        extend(&ts->expected_, sizeof ts->expected_.i_[0]);
        ts->expected_.i_[ts->expected_.n_++] = (struct TsExpectedItem){TsExpectedWrite, address, value};
        pthread_mutex_unlock(&ts->expected_.mutex_);
    }
}

void testIntrinsicsExpectWriteAnyValue(uint32_t address)
{
    TestSystem *ts = self();
    {
        assert(pthread_mutex_lock(&ts->expected_.mutex_) == 0);
        extend(&ts->expected_, sizeof ts->expected_.i_[0]);
        ts->expected_.i_[ts->expected_.n_++] = (struct TsExpectedItem){TsExpectedWriteAny, address, 0u};
        pthread_mutex_unlock(&ts->expected_.mutex_);
    }
}

void testIntrinsicsSetMemory(uint32_t address, uint32_t value)
{
    TestSystem *ts = self();
    {
        assert(pthread_mutex_lock(&ts->memory_.mutex_) == 0);
        int i = 0;
        for (; i < ts->memory_.n_; i++)
        {
            if (ts->memory_.i_[i].address_ == address)
            {
                ts->memory_.i_[i].value_ = value;
                break;
            }
        }
        if (i == ts->memory_.n_)
        {
            extend(&ts->memory_, sizeof ts->memory_.i_[0]);
            ts->memory_.i_[ts->memory_.n_++] = (struct TsMemoryItem){address, value};
        }
        pthread_mutex_unlock(&ts->memory_.mutex_);
    }
}

bool testIntrinsicsTriggerInterrupt(uint32_t intId)
{
    TestSystem *ts = self();
    {
        assert(pthread_mutex_lock(&ts->scheduled_.mutex_) == 0);
        extend(&ts->scheduled_, sizeof ts->scheduled_.i_[0]);
        ts->scheduled_.i_[ts->scheduled_.n_++] = intId;
        pthread_mutex_unlock(&ts->scheduled_.mutex_);
    }
    return true;
}

bool testIntrinsicsTriggerInterruptNamed(char const *s)
{
    int i = 0;
    for (; i < sizeof lut / sizeof lut[0]; i++)
    {
        if (strcmp(lut[i].name, s) == 0)
        {
            testIntrinsicsTriggerInterrupt(lut[i].number_);
            break;
        }
    }
    if (i == sizeof lut / sizeof lut[0])
    {
        log("Unable to simulate an interrupt named %s", s);
        return false;
    }
    return true;
}

void extend(void *collection, size_t itemSize)
{
    TsScheduledInterrupts *c = (TsScheduledInterrupts *)collection;

    if (c->n_ == c->e_)
    {
        int newSize = c->e_ + 4;
        c->i_ = reallocate(c->i_, c->e_ * itemSize, newSize * itemSize);
        c->e_ = newSize;
    }
}

static void destroy(void) // never gets called - call from GC?
{
    TsInterruptHandlers *tsi = &self()->handlers_;

    // kill
    // wait for done
    for (int i = 0; i < tsi->n_; i++)
    {
        pthread_t *th = &tsi->i_[i].t_;
        (void) pthread_join(*th, 0); // no results
    }
    // todo: free all the collections
}
