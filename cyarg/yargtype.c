#include "common.h"

#include "yargtype.h"

#include "memory.h"
#include "vm.h"

ObjYargType* newYargTypeFromType(YargType yt) {
    ObjYargType* t = ALLOCATE_OBJ(ObjYargType, OBJ_YARGTYPE);
    t->yt = yt;
    return t;
}

ObjYargType* newYargArrayTypeFromType(Value elementType) {
    ObjYargType* t = ALLOCATE_OBJ(ObjYargType, OBJ_YARGTYPE);
    if (IS_YARGTYPE(elementType)) {
        t->element_type = AS_YARGTYPE(elementType);
    }
    t->yt = TypeArray;
    return t;
}

YargType yt_typeof(Value a) {
    if (IS_BOOL(a)) {
        return TypeBool;
    } else if (IS_DOUBLE(a)) {
        return TypeDouble;
    } else if (IS_UINTEGER(a)) {
        return TypeMachineUint32;
    } else if (IS_INTEGER(a)) {
        return TypeInteger;
    } else if (IS_FUNCTION(a)) {
        return TypeFunction;
    } else if (IS_CLOSURE(a)) {
        return TypeFunction;
    } else if (IS_NATIVE(a)) {
        return TypeFunction;
    } else if (IS_BOUND_METHOD(a)) {
        return TypeFunction;
    } else if (IS_CLASS(a)) {
        return TypeClass;
    } else if (IS_INSTANCE(a)) {
        return TypeInstance;
    } else if (IS_BLOB(a)) {
        return TypeNativeBlob;
    } else if (IS_ROUTINE(a)) {
        return TypeRoutine;
    } else if (IS_CHANNEL(a)) {
        return TypeChannel;
    } else if (IS_STRING(a)) {
        return TypeString;
    } else if (IS_VALARRAY(a)) {
        return TypeArray;
    } else if (IS_UNIFORMARRAY(a)) {
        return TypeArray;
    } else if (IS_YARGTYPE(a)) {
        return TypeYargType;
    }
    fatalVMError("Unexpected object type");
    return TypeYargType; // This should never happen.
}

bool is_obj_type(ObjYargType* type) {
    switch (type->yt) {
        case TypeBool:
        case TypeDouble:
        case TypeMachineUint32:
        case TypeInteger:
            return false;
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeNativeBlob:
        case TypeRoutine:
        case TypeChannel:
        case TypeArray:
        case TypeYargType:
            return true;
    }
}