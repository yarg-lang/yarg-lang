//
//  testIntrinsics.h
//  yarg
//
//  Created by dlm on 25/11/2025.
//

#include <stdbool.h>

typedef struct TsLog {
    int n_;
    int e_;
    char **i_;
} TsLog;

TsLog *testIntrinsicsSync(void);
void testIntrinsicsExpectRead(size_t address, uint32_t value);
void testIntrinsicsExpectReadAnyValue(size_t address);
void testIntrinsicsExpectWrite(size_t address, uint32_t value);
void testIntrinsicsExpectWriteAnyValue(size_t address);
void testIntrinsicsSetMemory(size_t address, uint32_t value);
bool testIntrinsicsTriggerInterrupt(uint32_t intId);
bool testIntrinsicsTriggerInterruptNamed(char const *);
