//
//  testBuiltin.c
//  xcode-yarg
//
//  Created by dlm on 12/12/2025.
//

#include "testBuiltin.h"
#include "testIntrinsics.h"

#include <string.h>

#include "../chunk.h"
#include "../object.h"
#include "../routine.h"
#include "../yargtype.h"
#include "../object.h"
#include "../memory.h"
#include "../builtin.h"
#include "../yargobject.h"
#include "../packed_containers.h"

static bool setBuiltin(ObjRoutine *, int, Value *);
static bool readBuiltin(ObjRoutine *, int, Value *);
static bool writeBuiltin(ObjRoutine *, int, Value *);
static bool interruptBuiltin(ObjRoutine *, int, Value *);
static bool syncBuiltin(ObjRoutine *, int, Value *);

Value getTestSystemBuiltin(uint8_t builtin)
{
    switch (builtin) {
        case BUILTIN_TS_SET: return BUILTIN_VAL(newBuiltin(setBuiltin));
        case BUILTIN_TS_READ: return BUILTIN_VAL(newBuiltin(readBuiltin));
        case BUILTIN_TS_WRITE: return BUILTIN_VAL(newBuiltin(writeBuiltin));
        case BUILTIN_TS_INTERRUPT: return BUILTIN_VAL(newBuiltin(interruptBuiltin));
        case BUILTIN_TS_SYNC: return BUILTIN_VAL(newBuiltin(syncBuiltin));
        default: return NIL_VAL;
    }
}

bool setBuiltin(ObjRoutine *routineContext, int argCount, Value* result)
{
    bool ok = false;
    if (argCount == 2)
    {
        Value arg0 = peek(routineContext, 1);
        Value arg1 = peek(routineContext, 0);
        if (IS_ADDRESS(arg0) && IS_UI32(arg1))
        {
            size_t address = AS_ADDRESS(arg0);
            uint32_t value = AS_UI32(arg1);
            
            testIntrinsicsSetMemory(address, value);
            ok = true;
        }
    }
    if (!ok)
    {
        runtimeError(routineContext, "Expected an address and a value.");
    }
    return ok;
}

bool readBuiltin(ObjRoutine *routineContext, int argCount, Value *result) {
    bool ok = false;
    if (argCount == 1 && IS_ADDRESS(peek(routineContext, 0)))
    {
        Value arg0 = peek(routineContext, 0);
        size_t address = AS_ADDRESS(arg0);
        testIntrinsicsExpectReadAnyValue(address);
        ok = true;
    }
    else if (argCount == 2 && IS_UI32(peek(routineContext, 0)) && IS_ADDRESS(peek(routineContext, 1)))
    {
        Value arg0 = peek(routineContext, 1);
        Value arg1 = peek(routineContext, 0);
        size_t address = AS_ADDRESS(arg0);
        uint32_t value = AS_UI32(arg1);
        testIntrinsicsExpectRead(address, value);
        ok = true;
    }

    if (!ok)
    {
        runtimeError(routineContext, "Expected an address to peek or and address and a value.");
    }
    return ok;
}

bool writeBuiltin(ObjRoutine *routineContext, int argCount, Value *result) {
    bool ok = false;
    if (argCount == 1 && IS_ADDRESS(peek(routineContext, 0)))
    {
        Value arg0 = peek(routineContext, 0);
        size_t address = AS_ADDRESS(arg0);
        testIntrinsicsExpectWriteAnyValue(address);
        ok = true;
    }
    else if (argCount == 2 && IS_ADDRESS(peek(routineContext, 1)) && IS_UI32(peek(routineContext, 0)))
    {
        Value arg0 = peek(routineContext, 1);
        Value arg1 = peek(routineContext, 0);
        size_t address = AS_ADDRESS(arg0);
        uint32_t value = AS_UI32(arg1);
        testIntrinsicsExpectWrite(address, value);
        ok = true;
    }

    if (!ok)
    {
        runtimeError(routineContext, "Expected an address to poke or and address and a value.");
    }
    return ok;
}

bool interruptBuiltin(ObjRoutine *routineContext, int argCount, Value *result) {
    bool ok = false;
    if (argCount == 1)
    {
        Value arg0 = peek(routineContext, 0);
        if (is_positive_integer(arg0) && as_positive_integer(arg0) <= UINT32_MAX)
        {
            uint32_t interruptNumber = (uint32_t) as_positive_integer(arg0);
            ok = testIntrinsicsTriggerInterrupt(interruptNumber);
        }
        else if (IS_STRING(arg0))
        {
            char *name = AS_CSTRING(arg0);
            ok = testIntrinsicsTriggerInterruptNamed(name);
        }
    }
    if (!ok)
    {
        runtimeError(routineContext, "Expected an interrupt name or number.");
    }
    return ok;
}

bool syncBuiltin(ObjRoutine *routineContext, int argCount, Value *result)
{
    TsLog *log = testIntrinsicsSync();

    Value resultArrayVal = allocValueArray(log->n_);
    tempRootPush(resultArrayVal);

    for (size_t i = 0; i < log->n_; i++)
    {
//        printf("%s\n", log->i_[i]); // until log gets coppied to *result
        ObjString *s = copyString(log->i_[i], (int)strlen(log->i_[i]));
        tempRootPush(STRING_VAL(s));
        vm_free(log->i_[i]);
        setArrayElement(resultArrayVal, i, STRING_VAL(s));
        tempRootPop(); // s
    }

    tempRootPop(); // array
    *result = resultArrayVal;
    log->n_ = 0;

    return true;
}
