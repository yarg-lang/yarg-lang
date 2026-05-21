//
//  testBuiltin.c
//  xcode-yarg
//
//  Created by dlm on 12/12/2025.
//

#include "testBuiltin.h"
#include "testIntrinsics.h"

#include "../chunk.h"
#include "../object.h"
#include "../routine.h"
#include "../yargtype.h"
#include "../object.h"
#include "../memory.h"

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
    if (argCount >= 1)
    {
        Value arg0 = nativeArgument(routineContext, argCount, 0);
        uint32_t address = AS_UI32(arg0);
        if (argCount == 1 && !IS_OBJ(arg0))
        {
            testIntrinsicsExpectReadAnyValue(address);
            ok = true;
        }
        else if (argCount == 2)
        {
            Value arg1 = nativeArgument(routineContext, argCount, 1);
            if (!IS_OBJ(arg0) && ! IS_OBJ(arg1))
            {
                uint32_t value = AS_UI32(arg1);
                testIntrinsicsExpectRead(address, value);
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
            testIntrinsicsExpectWriteAnyValue(address);
            ok = true;
        }
        else if (argCount == 2)
        {
            Value arg1 = nativeArgument(routineContext, argCount, 1);
            if (!IS_OBJ(arg0) && ! IS_OBJ(arg1))
            {
                uint32_t value = AS_UI32(arg1);
                testIntrinsicsExpectWrite(address, value);
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
        if (is_positive_integer32(arg0))
        {
            uint32_t interruptNumber = as_positive_integer32(arg0);
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

    Obj emptyString;
    ObjConcreteYargType *array = newYargArrayTypeFromType(OBJ_VAL(&emptyString));
    tempRootPush(OBJ_VAL(array));

    ObjConcreteYargTypeArray *arrayAsArray = (ObjConcreteYargTypeArray *)array;
    arrayAsArray->cardinality = log->n_;
    ObjPackedUniformArray* result_array = newPackedUniformArray(arrayAsArray);
    tempRootPop(); // array
    tempRootPush(OBJ_VAL(result_array));

    for (size_t i = 0; i < log->n_; i++)
    {
//        printf("%s\n", log->i_[i]); // until log gets coppied to *result
        ObjString *s = copyString(log->i_[i], (int)strlen(log->i_[i]));
        tempRootPush(OBJ_VAL(s));
        reallocate(log->i_[i], (int)strlen(log->i_[i]) + 1, 0);
        PackedValue p = arrayElement(result_array->store, i);
        assignToPackedValue(p, OBJ_VAL(s));
        tempRootPop(); // s
    }

    tempRootPop(); // array
    *result = OBJ_VAL(result_array);
    log->n_ = 0;

    return true;
}
