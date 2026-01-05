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
#include "yargtype.h"
#include "object.h"
#include "memory.h"
}

#include "testIntrinsics.hpp"
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
    return ok;
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
    return ok;
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
    return ok;
}

bool interruptBuiltin(ObjRoutine *routineContext, int argCount, Value *result) {
    bool ok = false;
    if (argCount == 1)
    {
        Value arg0 = nativeArgument(routineContext, argCount, 0);
        if (!IS_OBJ(arg0))
        {
            uint32_t interruptNumber = AS_UI32(arg0);
            ok = TestIntrinsics::triggerInterrupt(interruptNumber);
        }
        else if (IS_STRING(arg0))
        {
            ObjString *name = (ObjString *)AS_OBJ(arg0);
            ok = TestIntrinsics::triggerInterrupt(string(name->chars, name->length));
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
    vector<string> &log = TestIntrinsics::sync();
        
    Obj emptyString;
    ObjConcreteYargType *array{newYargArrayTypeFromType(OBJ_VAL(&emptyString))};
    ObjConcreteYargTypeArray *arrayAsArray{reinterpret_cast<ObjConcreteYargTypeArray *>(array)};
    arrayAsArray->cardinality = log.size();
    ObjPackedUniformArray* result_array{newPackedUniformArray(arrayAsArray)};

    size_t index{0};
    for (auto const &i : log)
    {
//        println("{}", i); // until log gets coppied to *result
        ObjString *s{copyString(&i[0], static_cast<int>(i.size()))};
        PackedValue p{arrayElement(result_array->store, index)};
        assignToPackedValue(p, OBJ_VAL(s));
        index++;
    }

    *result = OBJ_VAL(result_array);
    log.clear();
    
    return true;
}
