//
//  testBuiltin.cpp
//  xcode-yarg
//
//  Created by dlm on 12/12/2025.
//

#include "testBuiltin.h"

extern "C" {
#include "chunk.h"
#include "object.h"
#include "routine.h"
}

#include "testSystem.hpp"
#include <map>
#include <vector>
#include <print>

using namespace std;

static bool setBuiltin(ObjRoutine *, int, Value *);
static bool readBuiltin(ObjRoutine *, int, Value *);
static bool writeBuiltin(ObjRoutine *, int, Value *);
static bool interruptBuiltin(ObjRoutine *, int, Value *);
static bool syncBuiltin(ObjRoutine *, int, Value *);

Value getTestSystemBuiltin(uint8_t builtin)
{
    switch (builtin) {
        case BUILTIN_TS_SET: return OBJ_VAL(newNative(setBuiltin));
        case BUILTIN_TS_READ: return OBJ_VAL(newNative(readBuiltin));
        case BUILTIN_TS_WRITE: return OBJ_VAL(newNative(writeBuiltin));
        case BUILTIN_TS_INTERRUPT: return OBJ_VAL(newNative(interruptBuiltin));
        case BUILTIN_TS_SYNC: return OBJ_VAL(newNative(syncBuiltin));
        default: return NIL_VAL;
    }
}

bool setBuiltin(ObjRoutine *routineContext, int argCount, Value* result)
{
    bool ok = false;
    if (argCount == 2)
    {
        Value arg0 = nativeArgument(routineContext, argCount, 0);
        Value arg1 = nativeArgument(routineContext, argCount, 1);
        if (!IS_OBJ(arg0) && !IS_OBJ(arg1))
        {
            uint32_t address = AS_UI32(arg0);
            uint32_t value = AS_UI32(arg1);
            
            TestIntrinsics::setMemory(address, value);
            ok = true;
        }
    }
    if (!ok)
    {
        runtimeError(routineContext, "Expected an address and a value.");
    }
    return false;
}

bool readBuiltin(ObjRoutine *routineContext, int argCount, Value *result) {
    bool ok = false;
    if (argCount >= 1)
    {
        Value arg0 = nativeArgument(routineContext, argCount, 0);
        uint32_t address = AS_UI32(arg0);
        if (argCount == 1 && !IS_OBJ(arg0))
        {
            TestIntrinsics::expectReadAnyValue(address);
            ok = true;
        }
        else if (argCount == 2)
        {
            Value arg1 = nativeArgument(routineContext, argCount, 1);
            if (!IS_OBJ(arg0) && ! IS_OBJ(arg1))
            {
                uint32_t value = AS_UI32(arg1);
                TestIntrinsics::expectRead(address, value);
                ok = true;
            }
        }
    }
    if (!ok)
    {
        runtimeError(routineContext, "Expected an address to peek or and address and a value.");
    }
    return false;
}

bool writeBuiltin(ObjRoutine *routineContext, int argCount, Value *result) {
    bool ok = false;
    if (argCount >= 1)
    {
        Value arg0 = nativeArgument(routineContext, argCount, 0);
        uint32_t address = AS_UI32(arg0);
        if (argCount == 1 && !IS_OBJ(arg0))
        {
            TestIntrinsics::expectWriteAnyValue(address);
            ok = true;
        }
        else if (argCount == 2)
        {
            Value arg1 = nativeArgument(routineContext, argCount, 1);
            if (!IS_OBJ(arg0) && ! IS_OBJ(arg1))
            {
                uint32_t value = AS_UI32(arg1);
                TestIntrinsics::expectWrite(address, value);
                ok = true;
            }
        }
    }
    if (!ok)
    {
        runtimeError(routineContext, "Expected an address to poke or and address and a value.");
    }
    return false;
}

bool interruptBuiltin(ObjRoutine *routineContext, int argCount, Value *result) {
    bool ok = false;
    if (argCount == 1)
    {
        Value arg0 = nativeArgument(routineContext, argCount, 0);
        if (!IS_OBJ(arg0))
        {
            uint32_t interruptNumber = AS_UI32(arg0);
            TestIntrinsics::triggerInterrupt(interruptNumber);
            ok = true;
        }
        else if (IS_STRING(arg0))
        {
            ObjString *name = (ObjString *)AS_OBJ(arg0);
            TestIntrinsics::triggerInterrupt(string(name->chars, name->length));
            ok = true;
        }
    }
    if (!ok)
    {
        runtimeError(routineContext, "Expected an interrupt name or number.");
    }
    return false;
}

bool syncBuiltin(ObjRoutine *routineContext, int argCount, Value *result)
{
    vector<string> &log = TestIntrinsics::sync();
    
    for (auto const &i : log)
    {
        println("{}", i); // until log gets coppied to *result
    }
    *result = NIL_VAL; // until log gets coppied to *result
    
    log.clear();
    
    return true; // todo convert log to array of strings (result)
}
