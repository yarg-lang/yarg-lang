#include "common.h"

#include "yargtype.h"

#include "memory.h"
#include "vm.h"

ObjConcreteYargType* newYargTypeFromType(ConcreteYargType yt) {
    switch (yt) {
        case TypeAny:
        case TypeBool:
        case TypeDouble:
        case TypeMachineUint32:
        case TypeInteger:
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeNativeBlob:
        case TypeRoutine:
        case TypeChannel:
        case TypeYargType: {
            ObjConcreteYargType* t = ALLOCATE_OBJ(ObjConcreteYargType, OBJ_YARGTYPE);
            t->yt = yt;
            return t;
        }
        case TypeArray: {
            ObjConcreteYargTypeArray* t = ALLOCATE_OBJ(ObjConcreteYargTypeArray, OBJ_YARGTYPE_ARRAY);
            t->core.yt = yt;
            return (ObjConcreteYargType*)t;
        }
        case TypeStruct: {
            ObjConcreteYargTypeStruct* s = ALLOCATE_OBJ(ObjConcreteYargTypeStruct, OBJ_YARGTYPE_STRUCT);
            s->core.yt = yt;
            initTable(&s->field_names);
            return (ObjConcreteYargType*)s;
        }
    }
}

ObjConcreteYargType* newYargArrayTypeFromType(Value elementType) {
    ObjConcreteYargTypeArray* t = ALLOCATE_OBJ(ObjConcreteYargTypeArray, OBJ_YARGTYPE_ARRAY);
    if (IS_YARGTYPE(elementType)) {
        t->element_type = AS_YARGTYPE(elementType);
    }
    t->core.yt = TypeArray;
    return (ObjConcreteYargType*)t;
}

ObjConcreteYargType* newYargStructType(size_t fieldCount) {
    ObjConcreteYargTypeStruct* t = ALLOCATE_OBJ(ObjConcreteYargTypeStruct, OBJ_YARGTYPE_STRUCT);
    tempRootPush(OBJ_VAL(t));
    t->core.yt = TypeStruct;
    initTable(&t->field_names);

    t->field_types = GROW_ARRAY(Value, t->field_types, 0, fieldCount);
    t->field_count = fieldCount;
    for (size_t i = 0; i < fieldCount; i++) {
        t->field_types[i] = NIL_VAL;
    }

    tempRootPop();
    return (ObjConcreteYargType*)t;
}

void addFieldType(ObjConcreteYargTypeStruct* st, size_t index, Value type, Value name) {
    st->field_types[index] = type;
    tableSet(&st->field_names, AS_STRING(name), UINTEGER_VAL(index));
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
    } else if (IS_STRUCT(a)) {
        return TypeStruct;
    } else if (IS_YARGTYPE(a)) {
        return TypeYargType;
    }
    fatalVMError("Unexpected object type");
    return TypeYargType; // This should never happen.
}

Value concrete_typeof(Value a) {
    if (IS_NIL(a)) {
        return NIL_VAL;
    } else if (IS_BOOL(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeBool));
    } else if (IS_DOUBLE(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeDouble));
    } else if (IS_UINTEGER(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeMachineUint32));
    } else if (IS_INTEGER(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeInteger));
    } else if (IS_FUNCTION(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeFunction));
    } else if (IS_CLOSURE(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeFunction));
    } else if (IS_NATIVE(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeFunction));
    } else if (IS_BOUND_METHOD(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeFunction));
    } else if (IS_CLASS(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeClass));
    } else if (IS_INSTANCE(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeInstance));
    } else if (IS_BLOB(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeNativeBlob));
    } else if (IS_ROUTINE(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeRoutine));
    } else if (IS_CHANNEL(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeChannel));
    } else if (IS_STRING(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeString));
    } else if (IS_VALARRAY(a)) {
        ObjConcreteYargTypeArray* arrayType = (ObjConcreteYargTypeArray*) newYargTypeFromType(TypeArray);
        arrayType->cardinality = AS_VALARRAY(a)->array.count;
        return OBJ_VAL((ObjConcreteYargType*)arrayType);
    } else if (IS_UNIFORMARRAY(a)) {
        ObjConcreteYargTypeArray* arrayType = (ObjConcreteYargTypeArray*) newYargTypeFromType(TypeArray);
        arrayType->cardinality = AS_UNIFORMARRAY(a)->count;
        arrayType->element_type = AS_UNIFORMARRAY(a)->element_type;
        return OBJ_VAL((ObjConcreteYargType*)arrayType);
    } else if (IS_STRUCT(a)) {
        return OBJ_VAL(AS_STRUCT(a)->type);
    } else if (IS_YARGTYPE(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeYargType));
    }
    fatalVMError("Unexpected object type");
    return NIL_VAL;
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
        case TypeStruct:
        case TypeYargType:
            return true;
    }
}

bool is_nil_assignable_type(ObjConcreteYargType* type) {
    switch (type->yt) {
        case TypeAny:
        case TypeBool:
        case TypeDouble:
        case TypeMachineUint32:
        case TypeInteger:
        case TypeStruct:
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
        case TypeStruct:
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
            case TypeStruct: return defaultStructValue(ct);
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

static bool isAssignableCardinality(size_t lhsCardinality, size_t rhsCardinality) {
    if (lhsCardinality == 0) {
        return true;
    } else {
        return lhsCardinality == rhsCardinality;
    }
}

static bool isInitializableArray(ObjConcreteYargType* lhsType, Value rhsValue) {
    if (IS_NIL(rhsValue)) {
        return true;
    }
    if (yt_typeof(rhsValue) != TypeArray) {
        return false;
    }
    ObjConcreteYargTypeArray* lhsArType = (ObjConcreteYargTypeArray*)lhsType;
    ObjConcreteYargTypeArray* rhsArType = (ObjConcreteYargTypeArray*)AS_YARGTYPE(concrete_typeof(rhsValue));

    if (isAssignableCardinality(lhsArType->cardinality, rhsArType->cardinality)) {
        if (lhsArType->element_type == NULL) {
            return true;
        } else if (lhsArType->element_type->yt == TypeAny && rhsArType->element_type == NULL) {
            return true;
        } else if (rhsArType->element_type == NULL) {
            return false;
        } else {
            return lhsArType->element_type->yt == rhsArType->element_type->yt;
        }
    } else {
        return false;
    }
}

bool isInitialisableType(ObjConcreteYargType* lhsType, Value rhsValue) {
    if (lhsType->yt == TypeAny) {
        return true;
    } else if (is_nil_assignable_type(lhsType) && IS_NIL(rhsValue)) { 
        return true;
    } else if (is_obj_type(lhsType) && lhsType->yt == TypeArray) {       
        return isInitializableArray(lhsType, rhsValue); 
    } else if (is_obj_type(lhsType)) {
        return lhsType->yt == yt_typeof(rhsValue);
    } else {
        return !IS_NIL(rhsValue) && lhsType->yt == yt_typeof(rhsValue);
    }
}

bool isCompatibleType(ObjConcreteYargType* lhsType, Value rhsValue) {
    if (lhsType->isConst) {
        return false;
    } else {
        return isInitialisableType(lhsType, rhsValue);
    }
}