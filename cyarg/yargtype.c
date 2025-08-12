#include "common.h"

#include "yargtype.h"

#include "memory.h"
#include "vm.h"

ObjConcreteYargType* newYargTypeFromType(ConcreteYargType yt) {
    ObjConcreteYargType* t = ALLOCATE_OBJ(ObjConcreteYargType, OBJ_YARGTYPE);
    t->yt = yt;
    return t;
}

ObjConcreteYargType* newYargArrayTypeFromType(Value elementType) {
    ObjConcreteYargType* t = ALLOCATE_OBJ(ObjConcreteYargType, OBJ_YARGTYPE);
    if (IS_YARGTYPE(elementType)) {
        t->element_type = AS_YARGTYPE(elementType);
    }
    t->yt = TypeArray;
    return t;
}

ConcreteYargType yt_typeof(Value a) {
    if (IS_NIL(a)) {
        return TypeAny;
    } else if (IS_BOOL(a)) {
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

bool is_obj_type(ObjConcreteYargType* type) {
    switch (type->yt) {
        case TypeAny:
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

ObjConcreteYargType* array_element_type(Value val) {
    if (IS_UNIFORMARRAY(val)) {
        ObjUniformArray* array = AS_UNIFORMARRAY(val);
        return array->element_type;
    } else {
        return NULL;
    }
}

size_t yt_sizeof_type(Value type) {
    if (IS_NIL(type)) {
        return 8;
    } else {
        ObjConcreteYargType* t = AS_YARGTYPE(type);
        switch (t->yt) {
        case TypeAny:
        case TypeBool:
        case TypeDouble:
        case TypeInteger:
            return 8;
        case TypeMachineUint32:
            return 4;
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeNativeBlob:
        case TypeRoutine:
        case TypeChannel:
        case TypeArray:
        case TypeYargType:
            return 4;            
        }
    }
}

Value defaultValue(Value type) {
    if (IS_NIL(type)) {
        return NIL_VAL;
    } else {
        ObjConcreteYargType* ct = AS_YARGTYPE(type);
        switch (ct->yt) {
            case TypeBool: return FALSE_VAL;
            case TypeInteger: return INTEGER_VAL(0);
            case TypeDouble: return DOUBLE_VAL(0);
            case TypeMachineUint32: return UINTEGER_VAL(0);
            case TypeAny:
            case TypeString:
            case TypeClass:
            case TypeInstance:
            case TypeFunction:
            case TypeNativeBlob:
            case TypeRoutine:
            case TypeChannel:
            case TypeArray:
            case TypeYargType:
                return NIL_VAL;
        }
    }
}