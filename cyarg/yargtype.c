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

Value arrayElementType(ObjConcreteYargTypeArray* arrayType) {
    return arrayType->element_type ? OBJ_VAL(arrayType->element_type) : NIL_VAL;
}

size_t arrayElementOffset(ObjConcreteYargTypeArray* arrayType, size_t index) {
    return index * arrayElementSize(arrayType);
}

size_t arrayElementSize(ObjConcreteYargTypeArray* arrayType) {
    return yt_sizeof_type_storage(arrayElementType(arrayType));
}

ObjConcreteYargType* newYargStructType(size_t fieldCount) {
    ObjConcreteYargTypeStruct* t = (ObjConcreteYargTypeStruct*) newYargTypeFromType(TypeStruct);
    tempRootPush(OBJ_VAL(t));

    Value* fieldTypes = ALLOCATE(Value, fieldCount);
    for (size_t i = 0; i < fieldCount; i++) {
        fieldTypes[i] = NIL_VAL;
    }

    size_t* fieldIndexes = ALLOCATE(size_t, fieldCount);
    for (size_t i = 0; i < fieldCount; i++) {
        fieldIndexes[i] = 0;
    }

    t->field_indexes = fieldIndexes;
    t->field_types = fieldTypes;
    t->field_count = fieldCount;

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

size_t addFieldType(ObjConcreteYargTypeStruct* st, size_t index, size_t fieldOffset, Value type, Value name) {
    st->field_types[index] = type;
    tableSet(&st->field_names, AS_STRING(name), UINTEGER_VAL(index));
    st->field_indexes[index] = fieldOffset;
    st->storage_size = fieldOffset + yt_sizeof_type_storage(type);
    return st->storage_size;
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
    } else if (IS_UNIFORMARRAY(a)) {
        return OBJ_VAL(AS_UNIFORMARRAY(a)->type);
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

bool type_packs_as_obj(ObjConcreteYargType* type) {
    switch (type->yt) {
        case TypeAny:
        case TypeBool:
        case TypeDouble:
        case TypeMachineUint32:
        case TypeInteger:
        case TypeArray:
        case TypeStruct:
            return false;
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeNativeBlob:
        case TypeRoutine:
        case TypeChannel:
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
                Value elementType = arrayElementType(ct);
                return is_placeable_type(elementType);
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
        case TypeStruct: {
            ObjConcreteYargTypeStruct* st = (ObjConcreteYargTypeStruct*)t;
            return st->storage_size;
        }
        case TypeArray: {
            ObjConcreteYargTypeArray* array = (ObjConcreteYargTypeArray*)t;
            return arrayElementSize(array) * array->cardinality;
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
            return sizeof(Obj*);
        }
    }
}

void initialisePackedStorage(Value type, StoredValue* packedStorage) {

    if (IS_NIL(type)) {
        packedStorage->asValue = NIL_VAL;
    } else {
        ObjConcreteYargType* ct = AS_YARGTYPE(type);
        switch (ct->yt) {
            case TypeAny: packedStorage->asValue = NIL_VAL; break;
            case TypeBool: packedStorage->asValue = BOOL_VAL(false); break;
            case TypeDouble: packedStorage->asValue = DOUBLE_VAL(0); break;
            case TypeInteger:  packedStorage->asValue = INTEGER_VAL(0); break;
            case TypeMachineUint32: {
                packedStorage->as.uinteger = 0;
                break;
            }
            case TypeArray: {
                ObjConcreteYargTypeArray* at = (ObjConcreteYargTypeArray*)ct;
                Value elementType = arrayElementType(at);
                if (at->cardinality > 0) {
                    for (size_t i = 0; i < at->cardinality; i++) {
                        initialisePackedStorage(elementType, arrayElement(at, packedStorage, i));
                    }
                }
                break;
            }
            case TypeStruct: {
                ObjConcreteYargTypeStruct* st = (ObjConcreteYargTypeStruct*)ct;
                for (size_t i = 0; i < st->field_count; i++) {
                    StoredValue* field = structField(st, packedStorage, i);
                    initialisePackedStorage(st->field_types[i], field);
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
                packedStorage->as.obj = NULL;
                break;
            }
        }
    }
}

Value unpackStoredValue(Value type, StoredValue* packedStorage) {
    if (IS_NIL(type)) {
        return packedStorage->asValue;
    } else {
        ObjConcreteYargType* ct = AS_YARGTYPE(type);
        switch (ct->yt) {
            case TypeAny: return packedStorage->asValue;
            case TypeBool: return packedStorage->asValue;
            case TypeDouble: return packedStorage->asValue;
            case TypeInteger: return packedStorage->asValue;
            case TypeMachineUint32: return UINTEGER_VAL(packedStorage->as.uinteger);
            case TypeStruct: {
                return OBJ_VAL(newPackedStructAt((ObjConcreteYargTypeStruct*)ct, packedStorage));
            }
            case TypeArray: {
                return OBJ_VAL(newPackedUniformArrayAt((ObjConcreteYargTypeArray*)ct, packedStorage));
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
                if (packedStorage->as.obj) {
                    return OBJ_VAL(packedStorage->as.obj);
                } else {
                    return NIL_VAL;
                }
            }
        }
    }
}

void packValueStorage(StoredValueCellTarget* packedStorageCell, Value value) {
    if (IS_NIL(*packedStorageCell->type)) {
        packedStorageCell->storedValue->asValue = value;
    } else {
        ObjConcreteYargType* ct = AS_YARGTYPE(*packedStorageCell->type);
        switch (ct->yt) {
            case TypeAny: packedStorageCell->storedValue->asValue = value; break;
            case TypeBool: packedStorageCell->storedValue->asValue = value; break;
            case TypeDouble: packedStorageCell->storedValue->asValue = value; break;
            case TypeInteger: packedStorageCell->storedValue->asValue = value; break;
            case TypeMachineUint32: packedStorageCell->storedValue->as.uinteger = AS_UINTEGER(value); break;
            case TypePointer:
            case TypeString:
            case TypeClass:
            case TypeInstance:
            case TypeFunction:
            case TypeNativeBlob:
            case TypeRoutine:
            case TypeChannel:
            case TypeYargType: {
                packedStorageCell->storedValue->as.obj = AS_OBJ(value);
                break;
            }
            case TypeStruct:
            case TypeArray:
                break;
        }
    }
}

Value defaultValue(Value type) {
    if (IS_NIL(type)) {
        return NIL_VAL;
    } else {
        ObjConcreteYargType* ct = AS_YARGTYPE(type);
        switch (ct->yt) {
            case TypeBool: return BOOL_VAL(false);
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