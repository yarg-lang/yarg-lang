#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "object.h"
#include "value.h"
#include "builtin.h"
#include "native.h"
#include "routine.h"
#include "vm.h"
#include "debug.h"
#include "files.h"
#include "compiler.h"
#include "channel.h"
#include "yargtype.h"
#include "sync_group.h"

#ifndef CYARG_PICO_TARGET
#include "testSystem.h"
#include "testBuiltin.h"
#endif

bool importBuiltinDummy(ObjRoutine* routineContext, int argCount, Value* result) {
    *result = NIL_VAL;
    return true;
}

bool execBuiltinDummy(ObjRoutine* routineContext, int argCount, Value* result) {
    *result = NIL_VAL;
    return true;
}


static char* libraryNameFor(const char* importname) {
    size_t namelen = strlen(importname);
    char* filename = malloc(namelen + 3 + 1);
    if (filename) {
        strcpy(filename, importname);
        strcat(filename, ".ya");
    }
    return filename;
}

bool importBuiltin(ObjRoutine* routineContext, int argCount) {
    if (argCount != 1) {
        runtimeError(routineContext, "Expected 1 arguments but got %d.", argCount);
        return false;
    }
    if (!IS_STRING(peek(routineContext, 0))) {
        runtimeError(routineContext, "Argument to import must be string.");
        return false;
    }

    Value val;
    if (tableGet(&vm.imports, AS_STRING(peek(routineContext, 0)), &val)) {
        pop(routineContext);
        pop(routineContext);
        push(routineContext, NIL_VAL);
        return true;
    }

    char* source = NULL;
    char* library = libraryNameFor(AS_CSTRING(peek(routineContext, 0)));
    if (library) {
        source = readFile(library);
        free(library);
    }

    if (source) {
        ObjFunction* function = compile(source);
        free(source);
        if (function == NULL) {
            runtimeError(routineContext, "Interpret error; compiling source failed.");
            return false;
        }

        tempRootPush(OBJ_VAL(function));

        Value libstring = pop(routineContext);
        tempRootPush(libstring);
        pop(routineContext);

        ObjClosure* closure = newClosure(function);
        push(routineContext, OBJ_VAL(closure));

        tableSet(&vm.imports, AS_STRING(libstring), BOOL_VAL(true));

        tempRootPop();
        tempRootPop();

        callfn(routineContext, closure, 0);
        return true;
    }
    else {
        runtimeError(routineContext, "source not found");
        return false;
    }

}

bool readSourceBuiltin(ObjRoutine* routineContext, int argCount, Value* result) {
    if (argCount != 1) {
        runtimeError(routineContext, "Expected 1 argument but got %d.", argCount);
        return false;
    }
    if (!IS_STRING(nativeArgument(routineContext, argCount, 0))) {
        runtimeError(routineContext, "Argument to read_source must be string.");
        return false;
    }

    const char* filename = AS_CSTRING(nativeArgument(routineContext, argCount, 0));
    char* source = readFile(filename);
    if (source == NULL) {
        runtimeError(routineContext, "Could not read source file '%s'.", filename);
        return false;
    }

    ObjString* sourceString = copyString(source, (int)strlen(source));
    free(source);

    *result = OBJ_VAL(sourceString);
    return true;
}

bool compileBuiltin(ObjRoutine* routineContext, int argCount, Value* result) {
    if (argCount != 1) {
        runtimeError(routineContext, "Expected 1 argument but got %d.", argCount);
        return false;
    }
    if (!IS_STRING(nativeArgument(routineContext, argCount, 0))) {
        runtimeError(routineContext, "Argument to compile must be string.");
        return false;
    }

    const char* source = AS_CSTRING(nativeArgument(routineContext, argCount, 0));
    ObjFunction* function = compile(source);
    if (function == NULL) {
        runtimeError(routineContext, "Compile error; compiling source failed.");
        return false;
    }

    tempRootPush(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    tempRootPop();

    *result = OBJ_VAL(closure);
    return true;
}

bool execBuiltin(ObjRoutine* routineContext, int argCount) {
    if (argCount != 1) {
        runtimeError(routineContext, "Expected 1 arguments but got %d.", argCount);
        return false;
    }
    if (!IS_STRING(peek(routineContext, 0))) {
        runtimeError(routineContext, "Argument to exec must be string.");
        return false;
    }

    char* source = AS_CSTRING(peek(routineContext, 0));
    ObjFunction* function = compile(source);
    if (function == NULL) {
        runtimeError(routineContext, "Interpret error; compiling source failed.");
        return false;
    }

    tempRootPush(OBJ_VAL(function));

    Value sourceVal = pop(routineContext);
    tempRootPush(sourceVal);
    pop(routineContext);

    ObjClosure* closure = newClosure(function);
    push(routineContext, OBJ_VAL(closure));

    tempRootPop();
    tempRootPop();

    callfn(routineContext, closure, 0);
    return true;
}

bool makeChannelBuiltin(ObjRoutine* routine, int argCount, Value* result) {
    if (argCount >= 2) {
        runtimeError(routine, "Expected 0 or 1 arguments but got %d.", argCount);
        return false;
    }
    Value valCapacity;
    if (argCount == 0) {
        valCapacity = UI32_VAL(1);
    } else {
        Value arg1 = nativeArgument(routine, argCount, 0);
        if (!is_positive_integer(arg1)) {
            runtimeError(routine, "Expected a positive integer");
        }
        valCapacity = arg1;
    }

    size_t capacity = as_positive_integer(valCapacity);

    ObjChannelContainer* channel = newChannel(routine, capacity);

    *result = OBJ_VAL((Obj*)channel);
    return true;
}

bool sendChannelBuiltin(ObjRoutine* routine, int argCount, Value* result) {
    if (argCount != 2) {
        runtimeError(routine, "Expected 2 arguments, got %d.", argCount);
        return false;
    }

    Value channelVal = nativeArgument(routine, argCount, 0);

    if (!IS_CHANNEL(channelVal)) {
        runtimeError(routine, "Argument must be a channel.");
        return false;
    }

    ObjChannelContainer* channel = AS_CHANNEL(channelVal);
    Value data = nativeArgument(routine, argCount, 1);
    sendChannel(channel, data);

    return true;
}

bool shareChannelBuiltin(ObjRoutine* routine, int argCount, Value* result) {
    if (argCount != 2) {
        runtimeError(routine, "Expected 2 arguments, got %d.", argCount);
        return false;
    }

    Value channelVal = nativeArgument(routine, argCount, 0);
    
    if (!IS_CHANNEL(channelVal)) {
        runtimeError(routine, "Argument must be a channel.");
        return false;
    }

    ObjChannelContainer* channel = AS_CHANNEL(channelVal);
    Value data = nativeArgument(routine, argCount, 1);
    bool overflow = shareChannel(channel, data);
    *result = BOOL_VAL(overflow);

    return true;
}

bool cpeekBuiltin(ObjRoutine* routine, int argCount, Value* result) {
    if (argCount != 1) {
        runtimeError(routine, "Expected 1 arguments, got %d.", argCount);
        return false;
    }

    Value channelVal = nativeArgument(routine, argCount, 0);

    if (!IS_CHANNEL(channelVal)) {
        runtimeError(routine, "Argument must be a channel.");
        return false;
    }

    ObjChannelContainer* channel = AS_CHANNEL(channelVal);
    *result = peekChannel(channel);

    return true;
}

bool makeSyncGroupBuiltin(ObjRoutine* routineContext, int argCount, Value* result) {
    if (argCount != 1) {
        runtimeError(routineContext, "Expected 1 argument but got %d.", argCount);
        return false;
    }
    Value items = nativeArgument(routineContext, argCount, 0);
    if (!IS_UNIFORMARRAY(items)) {
        runtimeError(routineContext, "Expected an array.");
        return false;
    }
    ObjPackedUniformArray* array = AS_UNIFORMARRAY(items);
    ObjConcreteYargTypeArray* t = (ObjConcreteYargTypeArray*)array->store.storedType;
    if (t->element_type == NULL) {
        for (size_t i = 0; i < arrayCardinality(array->store); i++) {
            Value element = unpackValue(arrayElement(array->store, i));
            if (!IS_CHANNEL(element)) {
                runtimeError(routineContext, "Array must contain only channel items.");
                return false;
            }
        }
    } else if (t->element_type->yt != TypeArray) {
        runtimeError(routineContext, "Array must contain only channel items.");
        return false;
    }

    ObjSyncGroup* group = newSyncGroup(routineContext, AS_UNIFORMARRAY(items));

    *result = OBJ_VAL((Obj*)group);
    return true;
}

bool makeRoutineBuiltin(ObjRoutine* routineContext, int argCount, Value* result) {
    if (argCount != 2) {
        runtimeError(routineContext, "Expected 2 arguments but got %d.", argCount);
        return false;
    }
    if (!IS_CLOSURE(nativeArgument(routineContext, argCount, 0)) || !IS_BOOL(nativeArgument(routineContext, argCount, 1))) {
        runtimeError(routineContext, "Argument to make_routine must be a function and a boolean.");
        return false;
    }

    ObjClosure* closure = AS_CLOSURE(nativeArgument(routineContext, argCount, 0));
    bool isISR = AS_BOOL(nativeArgument(routineContext, argCount, 1));

    ObjRoutine* routine = newRoutine();

    if (bindEntryFn(routine, closure)) {
        *result = OBJ_VAL(routine);
        return true;
    }
    else {
        runtimeError(routineContext, "Function must take 0 or 1 arguments.");
        return false;
    }
}

bool resumeBuiltin(ObjRoutine* routineContext, int argCount, Value* result) {
    if (argCount < 1 || argCount > 2) {
        runtimeError(routineContext, "Expected one or two arguments to resume.");
        return false;
    }

    if (!IS_ROUTINE(nativeArgument(routineContext, argCount, 0))) {
        runtimeError(routineContext, "Argument to resume must be a routine.");
        return false;
    }

    ObjRoutine* target = AS_ROUTINE(nativeArgument(routineContext, argCount, 0));

    if (target->state != EXEC_SUSPENDED && target->state != EXEC_UNBOUND) {
        runtimeError(routineContext, "routine must be suspended or unbound to resume.");
        return false;
    }

    Value arg = NIL_VAL;
    if (argCount == 2) {
        arg = nativeArgument(routineContext, argCount, 1);
    }

    return resumeRoutine(routineContext, target, argCount == 2 ? 1 : 0, arg, result);
}

bool startBuiltin(ObjRoutine* routineContext, int argCount, Value* result) {
    if (argCount < 1 || argCount > 2) {
        runtimeError(routineContext, "Expected one or two arguments to start.");
        return false;
    }

    Value targetRoutineVal = nativeArgument(routineContext, argCount, 0);

    if (!IS_ROUTINE(targetRoutineVal)) {
        runtimeError(routineContext, "Argument to start must be a routine.");
        return false;
    }

    ObjRoutine* target = AS_ROUTINE(targetRoutineVal);

    Value arg = NIL_VAL;
    if (argCount == 2) {
        arg = nativeArgument(routineContext, argCount, 1);
    }

    return startRoutine(routineContext, target, argCount == 2 ? 1 : 0, arg);
}

bool receiveBuiltin(ObjRoutine* routine, int argCount, Value* result) {
    if (argCount != 1) {
        runtimeError(routine, "Expected 1 arguments, got %d.", argCount);
        return false;
    }

    Value targetVal = nativeArgument(routine, argCount, 0);

    if (!IS_CHANNEL(targetVal) && !IS_ROUTINE(targetVal) && !IS_SYNCGROUP(targetVal)) {
        runtimeError(routine, "Argument must be a channel, a routine or a sync group.");
        return false;
    }

    if (IS_CHANNEL(targetVal)) {
        *result = receiveChannel(AS_CHANNEL(targetVal));
    } 
    else if (IS_ROUTINE(targetVal)) {
        return receiveFromRoutine(AS_ROUTINE(targetVal), result);
    } else if (IS_SYNCGROUP(targetVal)) {
        *result = receiveSyncGroup(AS_SYNCGROUP(targetVal));
        return true;
    }
    return true;
}

bool peekBuiltin(ObjRoutine* routineContext, int argCount, Value* result) {

    Value address = nativeArgument(routineContext, argCount, 0);

    if (!isAddressValue(address)) {
        runtimeError(routineContext, "Expected an address or pointer.");
        return false;
    }

    uintptr_t nominal_address = 0;
    if (IS_POINTER(address)) {
        nominal_address = (uintptr_t) AS_POINTER(address)->destination;
    } else {
        nominal_address = AS_ADDRESS(address);
    }
    volatile uint32_t* reg = (volatile uint32_t*) nominal_address;

#ifdef CYARG_PICO_TARGET
    uint32_t res = *reg;
    *result = UI32_VAL(res);
#else
    *result = UI32_VAL(tsRead((uint32_t)nominal_address));
    printf("peek(%p) -> %x\n", (void*)nominal_address, AS_UI32(*result));
#endif
    return true;
}

bool lenBuiltin(ObjRoutine* routineContext, int argCount, Value* result) {
    if (argCount != 1) {
        runtimeError(routineContext, "Expected 1 argument, but got %d.", argCount);
        return false;
    }

    Value arg = nativeArgument(routineContext, argCount, 0);

    if (IS_STRING(arg)) {
        ObjString* string = AS_STRING(arg);
        *result = UI32_VAL(string->length);
        return true;
    } else if (IS_UNIFORMARRAY(arg)) {
        ObjPackedUniformArray* array = AS_UNIFORMARRAY(arg);
        *result = SIZE_T_UI_VAL(arrayCardinality(array->store));
        return true;
    } else {
        runtimeError(routineContext, "Expected a string or array.");
        return false;
    }
}

bool pinBuiltin(ObjRoutine* routineContext, int argCount, Value* result) {
    if (argCount != 1) {
        runtimeError(routineContext, "Expected 1 argument, but got %d.", argCount);
        return false;
    }

    Value arg = nativeArgument(routineContext, argCount, 0);

    if (!IS_ROUTINE(arg)) {
        runtimeError(routineContext, "Argument to pin must be a routine.");
        return false;
    }
    ObjRoutine* isrRoutine = AS_ROUTINE(arg);
    if (isrRoutine->entryFunction->function->arity != 0) {
        runtimeError(routineContext, "Can only pin routines with 0 argument entry functions.");
        return false;        
    }

    uintptr_t addr;
    if (pinRoutine(isrRoutine, &addr)) {
        *result = ADDRESS_VAL(addr);
        return true;
    } else {
        runtimeError(routineContext, "No more pinned routines available.");
        return false;
    }
}

bool newBuiltin(ObjRoutine* routineContext, int argCount, Value* result) {

    Value typeToCreate = NIL_VAL;
    if (   argCount == 1
        && IS_YARGTYPE(nativeArgument(routineContext, argCount, 0))) {
        typeToCreate = nativeArgument(routineContext, argCount, 0);
    }

    ConcreteYargType typeRequested = IS_NIL(typeToCreate) ? TypeAny : AS_YARGTYPE(typeToCreate)->yt;
    switch (typeRequested) {
        case TypeAny:    // fall through
        case TypeBool:
        case TypeDouble:
        case TypeInt8:
        case TypeUint8:
        case TypeInt16:
        case TypeUint16:
        case TypeInt32:
        case TypeUint32:
        case TypeInt64:
        case TypeUint64: {
            PackedValue heapValue = allocPackedValue(typeToCreate);
            initialisePackedValue(heapValue);
            *result = OBJ_VAL(newPointerForHeapCell(heapValue));
            return true;
        }
        case TypeStruct: {
            PackedValue heapValue = allocPackedValue(typeToCreate);
            initialisePackedValue(heapValue);
            *result = OBJ_VAL(newPointerForHeapCell(heapValue));
            return true;
        }
        case TypeArray: {
            *result = defaultValue(typeToCreate);
            return true;
        }
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeNativeBlob:
        case TypeRoutine:
        case TypeChannel:
        case TypePointer: 
        case TypeYargType:
            return false; // unsupported for now.
    }

    return false;
}

bool uint64Builtin(ObjRoutine* routineContext, int argCount, Value* result) {
    Value arg = nativeArgument(routineContext, argCount, 0);
    if (IS_I8(arg)) {
        *result = UI64_VAL(AS_I8(arg));
        return true;
    } else if (IS_UI8(arg)) {
        *result = UI64_VAL(AS_UI8(arg));
        return true;
    } else if (IS_I16(arg)) {
        *result = UI64_VAL(AS_I16(arg));
        return true;
    } else if (IS_UI16(arg)) {
        *result = UI64_VAL(AS_UI16(arg));
        return true;
    } else if (IS_I32(arg)) {
        *result = UI64_VAL(AS_I32(arg));
        return true;
    } else if (IS_UI32(arg)) {
        *result = UI64_VAL(AS_UI32(arg));
        return true;
    } else if (IS_UI64(arg)) {
        *result = arg;
        return true;
    } else if (IS_I64(arg) && AS_I64(arg) >= 0) {
        *result = UI64_VAL(AS_I64(arg));
        return true;
    }
    return false;
}

bool int64Builtin(ObjRoutine* routineContext, int argCount, Value* result) {
    Value arg = nativeArgument(routineContext, argCount, 0);
    if (IS_I8(arg)) {
        *result = I64_VAL(AS_I8(arg));
        return true;
    } else if (IS_I16(arg)) {
        *result = I64_VAL(AS_I16(arg));
        return true;
    } else if (IS_I32(arg)) {
        *result = I64_VAL(AS_I32(arg));
        return true;
    } else if (IS_I64(arg)) {
        *result = arg;
        return true;
    } else if (IS_UI8(arg)) {
        *result = I64_VAL(AS_UI8(arg));
        return true;
    } else if (IS_UI16(arg)) {
        *result = I64_VAL(AS_UI16(arg));
        return true;
    } else if (IS_UI32(arg)) {
        *result = I64_VAL(AS_UI32(arg));
        return true;
    } else if (IS_UI64(arg) && AS_UI64(arg) <= INT64_MAX) {
        *result = I64_VAL(AS_UI64(arg));
        return true;
    } else {
        return false;
    }
}

bool uint32Builtin(ObjRoutine* routineContext, int argCount, Value* result) {
    Value arg = nativeArgument(routineContext, argCount, 0);
    if (IS_I8(arg) && AS_I8(arg) >= 0) {
        *result = UI32_VAL(AS_I8(arg));
        return true;
    } else if (IS_I16(arg) && AS_I16(arg) >= 0) {
        *result = UI32_VAL(AS_I16(arg));
        return true;
    } else if (IS_I32(arg) && AS_I32(arg) >= 0) {
        *result = UI32_VAL(AS_I32(arg));
        return true;
    } else if (IS_I64(arg) && AS_I64(arg) >= 0 && AS_I64(arg) < UINT32_MAX) {
        *result = UI32_VAL((uint32_t)AS_I64(arg));
        return true;
    } else if (IS_UI8(arg)) {
        *result = UI32_VAL(AS_UI8(arg));
        return true;
    } else if (IS_UI16(arg)) {
        *result = UI32_VAL(AS_UI16(arg));
        return true;
    } else if (IS_UI32(arg)) {
        *result = arg;
        return true;
    } else if (IS_UI64(arg) && AS_UI64(arg) <= UINT32_MAX) {
        *result = UI32_VAL((uint32_t)AS_UI64(arg));
        return true;
    }
    return false;
}

bool int32Builtin(ObjRoutine* routineContext, int argCount, Value* result) {
    Value arg = nativeArgument(routineContext, argCount, 0);
    if (IS_I8(arg)) {
        *result = I32_VAL(AS_I8(arg));
        return true;
    } else if (IS_I16(arg)) {
        *result = I32_VAL(AS_I16(arg));
        return true;
    } else if (IS_I32(arg)) {
        *result = arg;
        return true;
    } else if (IS_I64(arg) && AS_I64(arg) >= INT32_MIN && AS_I64(arg) <= INT32_MAX) {
        *result = I32_VAL((int32_t)AS_I64(arg));
        return true;
    } else if (IS_UI8(arg)) {
        *result = I32_VAL(AS_UI8(arg));
        return true;
    } else if (IS_UI16(arg)) {
        *result = I32_VAL(AS_UI16(arg));
        return true;
    } else if (IS_UI32(arg) && AS_UI32(arg) <= INT32_MAX) {
        *result = I32_VAL(AS_UI32(arg));
        return true;
    } else if (IS_UI64(arg) && AS_UI64(arg) <= INT32_MAX) {
        *result = I32_VAL((int32_t)AS_UI64(arg));
        return true;
    } else {
        return false;
    }
}

bool uint16Builtin(ObjRoutine* routineContext, int argCount, Value* result) {
    Value arg = nativeArgument(routineContext, argCount, 0);
    if (IS_I8(arg) && AS_I8(arg) >= 0) {
        *result = UI16_VAL(AS_I8(arg));
        return true;
    } else if (IS_I16(arg) && AS_I16(arg) >= 0) {
        *result = UI16_VAL(AS_I16(arg));
        return true;
    } else if (IS_I32(arg) && AS_I32(arg) >= 0 && AS_I32(arg) <= UINT16_MAX) {
        *result = UI16_VAL(AS_I32(arg));
        return true;
    } else if (IS_I64(arg) && AS_I64(arg) >= 0 && AS_I64(arg) <= UINT16_MAX) {
        *result = UI16_VAL(AS_I64(arg));
        return true;
    } else if (IS_UI8(arg)) {
        *result = UI16_VAL(AS_UI8(arg));
        return true;
    } else if (IS_UI16(arg)) {
        *result = arg;
        return true;
    } else if (IS_UI32(arg) && AS_UI32(arg) <= UINT16_MAX) {
        *result = UI16_VAL(AS_UI32(arg));
        return true;
    } else if (IS_UI64(arg) && AS_UI64(arg) <= UINT16_MAX) {
        *result = UI16_VAL(AS_UI64(arg));
        return true;
    } else {
        return false;
    }
}

bool int16Builtin(ObjRoutine* routineContext, int argCount, Value* result) {
    Value arg = nativeArgument(routineContext, argCount, 0);
    if (IS_I8(arg)) {
        *result = I16_VAL(AS_I8(arg));
        return true;
    } else if (IS_I16(arg)) {
        *result = arg;
        return true;
    } else if (IS_I32(arg) && AS_I32(arg) >= INT16_MIN && AS_I32(arg) <= INT16_MAX) {
        *result = I16_VAL(AS_I32(arg));
        return true;
    } else if (IS_I64(arg) && AS_I64(arg) >= INT16_MIN && AS_I64(arg) <= INT16_MAX) {
        *result = I16_VAL(AS_I64(arg));
        return true;
    } else if (IS_UI8(arg)) {
        *result = I16_VAL(AS_UI8(arg));
        return true;
    } else if (IS_UI16(arg) && AS_UI16(arg) <= INT16_MAX) {
        *result = I16_VAL(AS_UI16(arg));
        return true;
    } else if (IS_UI32(arg) && AS_UI32(arg) <= INT16_MAX) {
        *result = I16_VAL(AS_UI32(arg));
        return true;
    } else if (IS_UI64(arg) && AS_UI64(arg) <= INT16_MAX) {
        *result = I16_VAL(AS_UI64(arg));
        return true;
    } else {
        return false;
    }
}

bool uint8Builtin(ObjRoutine* routineContext, int argCount, Value* result) {
    Value arg = nativeArgument(routineContext, argCount, 0);
    if (IS_I8(arg) && AS_I8(arg) >= 0) {
        *result = UI8_VAL(AS_I8(arg));
        return true;
    } else if (IS_I16(arg) && AS_I16(arg) >= 0 && AS_I16(arg) <= UINT8_MAX) {
        *result = UI8_VAL(AS_I16(arg));
        return true;
    } else if (IS_I32(arg) && AS_I32(arg) >= 0 && AS_I32(arg) <= UINT8_MAX) {
        *result = UI8_VAL(AS_I32(arg));
        return true;
    } else if (IS_I64(arg) && AS_I64(arg) >= 0 && AS_I64(arg) <= UINT8_MAX) {
        *result = UI8_VAL(AS_I64(arg));
        return true;
    } else if (IS_UI8(arg)) {
        *result = arg;
        return true;
    } else if (IS_UI32(arg) && AS_UI32(arg) <= UINT8_MAX) {
        *result = UI8_VAL(AS_UI32(arg));
        return true;
    } else if (IS_UI16(arg) && AS_UI16(arg) <= UINT8_MAX) {
        *result = UI8_VAL(AS_UI16(arg));
        return true;
    } else if (IS_UI64(arg) && AS_UI64(arg) <= UINT8_MAX) {
        *result = UI8_VAL(AS_UI64(arg));
        return true;
    } else {
        return false;
    }
}

bool int8Builtin(ObjRoutine* routineContext, int argCount, Value* result) {
    Value arg = nativeArgument(routineContext, argCount, 0);
    if (IS_I8(arg)) {
        *result = arg;
        return true;
    } else if (IS_I16(arg) && AS_I16(arg) >= INT8_MIN && AS_I16(arg) <= INT8_MAX) {
        *result = I8_VAL(AS_I16(arg));
        return true;
    } else if (IS_I32(arg) && AS_I32(arg) >= INT8_MIN && AS_I32(arg) <= INT8_MAX) {
        *result = I8_VAL(AS_I32(arg));
        return true;
    } else if (IS_I64(arg) && AS_I64(arg) >= INT8_MIN && AS_I64(arg) <= INT8_MAX) {
        *result = I8_VAL(AS_I64(arg));
        return true;
    } else if (IS_UI8(arg) && AS_UI8(arg) <= INT8_MAX) {
        *result = I8_VAL(AS_UI8(arg));
        return true;
    } else if (IS_UI16(arg) && AS_UI16(arg) <= INT8_MAX) {
        *result = I8_VAL(AS_UI16(arg));
        return true;
    } else if (IS_UI32(arg) && AS_UI32(arg) <= INT8_MAX) {
        *result = I8_VAL(AS_UI32(arg));
        return true;
    } else if (IS_UI64(arg) && AS_UI64(arg) <= INT8_MAX) {
        *result = I8_VAL(AS_UI64(arg));
        return true;
    } else {
        return false;
    }
}

Value getBuiltin(uint8_t builtin) {
    switch (builtin) {
        case BUILTIN_PEEK: return OBJ_VAL(newNative(peekBuiltin));
        case BUILTIN_IMPORT: return OBJ_VAL(newNative(importBuiltinDummy));
        case BUILTIN_READ_SOURCE: return OBJ_VAL(newNative(readSourceBuiltin));
        case BUILTIN_EXEC: return OBJ_VAL(newNative(execBuiltinDummy));
        case BUILTIN_COMPILE: return OBJ_VAL(newNative(compileBuiltin));
        case BUILTIN_MAKE_ROUTINE: return OBJ_VAL(newNative(makeRoutineBuiltin));
        case BUILTIN_RESUME: return OBJ_VAL(newNative(resumeBuiltin));
        case BUILTIN_START: return OBJ_VAL(newNative(startBuiltin));
        case BUILTIN_MAKE_CHANNEL: return OBJ_VAL(newNative(makeChannelBuiltin));
        case BUILTIN_SEND: return OBJ_VAL(newNative(sendChannelBuiltin));
        case BUILTIN_RECEIVE: return OBJ_VAL(newNative(receiveBuiltin));
        case BUILTIN_SHARE: return OBJ_VAL(newNative(shareChannelBuiltin));
        case BUILTIN_CPEEK: return OBJ_VAL(newNative(cpeekBuiltin));
        case BUILTIN_MAKE_SYNCGROUP: return OBJ_VAL(newNative(makeSyncGroupBuiltin));
        case BUILTIN_LEN: return OBJ_VAL(newNative(lenBuiltin));
        case BUILTIN_PIN: return OBJ_VAL(newNative(pinBuiltin));
        case BUILTIN_NEW: return OBJ_VAL(newNative(newBuiltin));
        case BUILTIN_INT8: return OBJ_VAL(newNative(int8Builtin));
        case BUILTIN_INT16: return OBJ_VAL(newNative(int16Builtin));
        case BUILTIN_UINT16: return OBJ_VAL(newNative(uint16Builtin));
        case BUILTIN_UINT8: return OBJ_VAL(newNative(uint8Builtin));
        case BUILTIN_INT32: return OBJ_VAL(newNative(int32Builtin));
        case BUILTIN_UINT32: return OBJ_VAL(newNative(uint32Builtin));
        case BUILTIN_INT64: return OBJ_VAL(newNative(int64Builtin));
        case BUILTIN_UINT64: return OBJ_VAL(newNative(uint64Builtin));
#ifdef CYARG_PICO_TARGET
        default: return NIL_VAL;
#else
        default: return getTestSystemBuiltin(builtin);
#endif
    }
}
