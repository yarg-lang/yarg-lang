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
        case TypePointer: {
            ObjConcreteYargTypePointer* p = ALLOCATE_OBJ(ObjConcreteYargTypePointer, OBJ_YARGTYPE_POINTER);
            p->core.yt = yt;
            return (ObjConcreteYargType*)p;
        }
    }
}

ObjConcreteYargType* newYargArrayTypeFromType(Value elementType) {
    ObjConcreteYargTypeArray* t = (ObjConcreteYargTypeArray*) newYargTypeFromType(TypeArray);
    if (IS_YARGTYPE(elementType)) {
        t->element_type = AS_YARGTYPE(elementType);
    }
    t->core.yt = TypeArray;
    return (ObjConcreteYargType*)t;
}

ObjConcreteYargType* newYargStructType(size_t fieldCount) {
    ObjConcreteYargTypeStruct* t = (ObjConcreteYargTypeStruct*) newYargTypeFromType(TypeStruct);
    tempRootPush(OBJ_VAL(t));

    t->field_types = GROW_ARRAY(Value, t->field_types, 0, fieldCount);
    t->field_count = fieldCount;
    for (size_t i = 0; i < fieldCount; i++) {
        t->field_types[i] = NIL_VAL;
    }

    tempRootPop();
    return (ObjConcreteYargType*)t;
}

ObjConcreteYargType* newYargPointerType(Value targetType) {
    ObjConcreteYargTypePointer* p = (ObjConcreteYargTypePointer*) newYargTypeFromType(TypePointer);
    if (IS_YARGTYPE(targetType)) {
        p->target_type = AS_YARGTYPE(targetType);
    }
    return (ObjConcreteYargType*)p;
}

void addFieldType(ObjConcreteYargTypeStruct* st, size_t index, Value type, Value name) {
    st->field_types[index] = type;
    tableSet(&st->field_names, AS_STRING(name), UINTEGER_VAL(index));
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
    } else if (IS_POINTER(a)) {
        return OBJ_VAL(newYargPointerType(AS_POINTER(a)->destination_type));
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
        case TypePointer:
        case TypeYargType:
            return true;
    }
}

bool is_nil_assignable_type(Value type) {
    if (IS_NIL(type)) {
        return true;
    } else if (IS_YARGTYPE(type)) {
        ObjConcreteYargType* ct = AS_YARGTYPE(type);
        switch (ct->yt) {
            case TypeBool:
            case TypeDouble:
            case TypeMachineUint32:
            case TypeInteger:
            case TypeStruct:
                return false;
            case TypeAny:
            case TypeString:
            case TypeClass:
            case TypeInstance:
            case TypeFunction:
            case TypeNativeBlob:
            case TypeRoutine:
            case TypeChannel:
            case TypeArray:
            case TypePointer:
            case TypeYargType:
                return true;
        } 
    } else {
        return false;
    }
}

bool is_placeable_type(Value typeVal) {
    if (IS_YARGTYPE(typeVal)) {
        switch(AS_YARGTYPE(typeVal)->yt) {
            case TypeMachineUint32: return true;
            case TypeArray: {
                ObjConcreteYargTypeArray* ct = (ObjConcreteYargTypeArray*)AS_YARGTYPE(typeVal);
                if (ct->element_type) {
                    return is_placeable_type(OBJ_VAL(ct->element_type));
                }
                return false;
            }
            default: return false;
        }
    }
    return false;
}

size_t yt_sizeof_type_storage(Value type) {
    if (IS_NIL(type)) {
        return sizeof(Value);
    } else {
        ObjConcreteYargType* t = AS_YARGTYPE(type);
        switch (t->yt) {
        case TypeAny:
        case TypeBool:
        case TypeDouble:
        case TypeInteger:
            return sizeof(Value);
        case TypeMachineUint32:
            return sizeof(uint32_t);
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeNativeBlob:
        case TypeRoutine:
        case TypeChannel:
        case TypeArray:
        case TypeStruct:
        case TypePointer:
        case TypeYargType:
            return sizeof(uintptr_t);            
        }
    }
}

void initialisePackedStorage(Value type, void* storage) {
    if (IS_NIL(type)) {
        *(Value*)storage = NIL_VAL;
    } else {
        ObjConcreteYargType* ct = AS_YARGTYPE(type);
        Value* valueStorage = (Value*)storage;
        switch (ct->yt) {
            case TypeAny: *valueStorage = NIL_VAL; break;
            case TypeBool: *valueStorage = FALSE_VAL; break;
            case TypeDouble: *valueStorage = DOUBLE_VAL(0); break;
            case TypeInteger:  *valueStorage = INTEGER_VAL(0); break;
            case TypeMachineUint32: {
                *(uint32_t*)storage = 0; 
                break;
            }
            case TypeStruct: {
                ObjConcreteYargTypeStruct* st = (ObjConcreteYargTypeStruct*)ct;
                for (size_t i = 0; i < st->field_count; i++) {
                    valueStorage[i] = defaultValue(st->field_types[i]);
                }
                break;
            }
            case TypeArray: {
                ObjConcreteYargTypeArray* at = (ObjConcreteYargTypeArray*)ct;
                uint8_t* byteStorage = (uint8_t*)storage;
                if (at->cardinality > 0) {
                    Value* arrayStorage = (Value*)storage;
                    for (size_t i = 0; i < at->cardinality; i++) {
                        initialisePackedStorage(OBJ_VAL(at->element_type), byteStorage + i * yt_sizeof_type_storage(OBJ_VAL(at->element_type)));
                    }
                }
                break;
            }
            case TypePointer:
            case TypeString:
            case TypeClass:
            case TypeInstance:
            case TypeFunction:
            case TypeNativeBlob:
            case TypeRoutine:
            case TypeChannel:
            case TypeYargType: {
                Obj** location = (Obj**)storage;
                *location = NULL;
                break;
            }
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
            case TypeArray: return defaultArrayValue(ct);
            case TypePointer:
            case TypeAny:
            case TypeString:
            case TypeClass:
            case TypeInstance:
            case TypeFunction:
            case TypeNativeBlob:
            case TypeRoutine:
            case TypeChannel:
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

static bool isInitializableArray(ObjConcreteYargTypeArray* lhsConcreteType, ObjConcreteYargTypeArray* rhsConcreteType) {

    if (isAssignableCardinality(lhsConcreteType->cardinality, rhsConcreteType->cardinality)) {
        if (lhsConcreteType->element_type == NULL) {
            return true;
        } else if (lhsConcreteType->element_type->yt == TypeAny && rhsConcreteType->element_type == NULL) {
            return true;
        } else if (rhsConcreteType->element_type == NULL) {
            return false;
        } else {
            return lhsConcreteType->element_type->yt == rhsConcreteType->element_type->yt;
        }
    } else {
        return false;
    }
}

bool isInitialisableType(ObjConcreteYargType* lhsType, Value rhsValue) {
    if (lhsType->yt == TypeAny) {
        return true;
    }
    
    if (IS_NIL(rhsValue)) {
        return is_nil_assignable_type(OBJ_VAL(lhsType));
    }

    Value rhsType = concrete_typeof(rhsValue);
    ObjConcreteYargType* rhsConcreteType = AS_YARGTYPE(rhsType);

    if (lhsType->yt == TypeArray && rhsConcreteType->yt == TypeArray) {       
        return isInitializableArray((ObjConcreteYargTypeArray*)lhsType, (ObjConcreteYargTypeArray*)rhsConcreteType); 
    } else {
        return lhsType->yt == rhsConcreteType->yt;
    }
}

bool isCompatibleType(ObjConcreteYargType* lhsType, Value rhsValue) {
    if (lhsType->isConst) {
        return false;
    } else {
        return isInitialisableType(lhsType, rhsValue);
    }
}