#include <stdio.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "value.h"
#include "yargtype.h"

typedef union PackedValueStore {
    AnyValue as;
    Value asValue;
} PackedValueStore;

static void packValue(PackedValue packedStorageTarget, Value value);

PackedValue allocPackedValue(Value type) {
    void* dest = reallocate(NULL, 0, yt_sizeof_type_storage(type));

    ObjConcreteYargType* ct = IS_NIL(type) ? NULL : AS_YARGTYPE(type);
    PackedValue value = { .storedType = ct, .storedValue = dest };
    return value;
}

static void markPackedStruct(ObjConcreteYargTypeStruct* type, PackedValueStore* fields) {
    if (fields) {
        PackedValue s;
        s.storedType = (ObjConcreteYargType*)type;
        s.storedValue = fields;
        for (int i = 0; i < type->field_count; i++) {
            PackedValue f = structField(s, i);
            markPackedValue(f);
        }
    }
}

static void markPackedArray(ObjConcreteYargTypeArray* type, PackedValueStore* elements) {
    if (elements) {
        PackedValue array = { .storedType = (ObjConcreteYargType*)type, .storedValue = elements };
        for (size_t i = 0; i < type->cardinality; i++) {
            PackedValue el = arrayElement(array, i);
            markPackedValue(el);
        }
    }
}

void markPackedContainer(PackedValue packedContainer) {
    if (packedContainer.storedType) {
        markObject((Obj*)packedContainer.storedType);

        switch (packedContainer.storedType->yt) {
            case TypeStruct: {
                ObjConcreteYargTypeStruct* structType = (ObjConcreteYargTypeStruct*) packedContainer.storedType;
                markPackedStruct(structType, packedContainer.storedValue);
                break;
            }
            case TypeArray: {
                ObjConcreteYargTypeArray* arrayType = (ObjConcreteYargTypeArray*) packedContainer.storedType;
                markPackedArray(arrayType, packedContainer.storedValue);
                break;
            }
            case TypePointer: {
                ObjConcreteYargTypePointer* pointerType = (ObjConcreteYargTypePointer*)packedContainer.storedType;
                PackedValue dest = { .storedType = pointerType->target_type, .storedValue = packedContainer.storedValue };
                if (packedContainer.storedValue && pointerType->target_type) {
                    markPackedValue(dest);
                }
                break;
            }
            default:
                break; // nothing to do.

        }
    }
}

void markPackedValue(PackedValue value) {
    if (value.storedValue == NULL) return;
    if (value.storedType == NULL) {
        markValue(value.storedValue->asValue);
        return;
    } else if (type_packs_as_container(value.storedType)) {
        markPackedContainer(value);
        return;
    } else if (type_packs_as_obj(value.storedType)) {
        markObject((Obj*)value.storedType);
        markObject(value.storedValue->as.obj);
        return;
    }
}

void initialisePackedValue(PackedValue packedValue) {

    if (packedValue.storedType == NULL) {
        packedValue.storedValue->asValue = NIL_VAL;
    } else {
        switch (packedValue.storedType->yt) {
            case TypeAny: packedValue.storedValue->asValue = NIL_VAL; break;
            case TypeBool: packedValue.storedValue->asValue = BOOL_VAL(false); break;
            case TypeDouble: packedValue.storedValue->asValue = DOUBLE_VAL(0); break;
            case TypeInt8: packedValue.storedValue->as.i8 = 0; break;
            case TypeUint8: packedValue.storedValue->as.ui8 = 0; break;
            case TypeInt16: packedValue.storedValue->as.i16 = 0; break;
            case TypeUint16: packedValue.storedValue->as.ui16 = 0; break;
            case TypeInt32: packedValue.storedValue->as.i32 = 0; break;
            case TypeUint32: packedValue.storedValue->as.ui32 = 0; break;
            case TypeInt64: packedValue.storedValue->as.i64 = 0; break;
            case TypeUint64: packedValue.storedValue->as.ui64 = 0; break;
            case TypeArray: {
                ObjConcreteYargTypeArray* at = (ObjConcreteYargTypeArray*)packedValue.storedType;
                Value elementTypeVal = arrayElementType(at);
                ObjConcreteYargType* elementType = IS_NIL(elementTypeVal) ? NULL : AS_YARGTYPE(elementTypeVal);
                if (at->cardinality > 0) {
                    for (size_t i = 0; i < at->cardinality; i++) {
                        PackedValue el = arrayElement(packedValue, i);
                        initialisePackedValue(el);
                    }
                }
                break;
            }
            case TypeStruct: {
                ObjConcreteYargTypeStruct* st = (ObjConcreteYargTypeStruct*)packedValue.storedType;
                for (size_t i = 0; i < st->field_count; i++) {
                    PackedValue f = structField(packedValue, i);
                    initialisePackedValue(f);
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
                packedValue.storedValue->as.obj = NULL;
                break;
            }
        }
    }
}

Value unpackValue(PackedValue packedValue) {
    if (packedValue.storedType == NULL) {
        return packedValue.storedValue->asValue;
    } else {
        switch (packedValue.storedType->yt) {
            case TypeAny: return packedValue.storedValue->asValue;
            case TypeBool: return packedValue.storedValue->asValue;
            case TypeDouble: return packedValue.storedValue->asValue;
            case TypeInt8: return I8_VAL(packedValue.storedValue->as.i8);
            case TypeUint8: return UI8_VAL(packedValue.storedValue->as.ui8);
            case TypeInt16: return I16_VAL(packedValue.storedValue->as.i16);
            case TypeUint16: return UI16_VAL(packedValue.storedValue->as.ui16);
            case TypeInt32: return I32_VAL(packedValue.storedValue->as.i32);
            case TypeUint32: return UI32_VAL(packedValue.storedValue->as.ui32);
            case TypeInt64: return I64_VAL(packedValue.storedValue->as.i64);
            case TypeUint64: return UI64_VAL(packedValue.storedValue->as.ui64);
            case TypeStruct: {
                return OBJ_VAL(newPackedStructAt(packedValue));
            }
            case TypeArray: {
                return OBJ_VAL(newPackedUniformArrayAt(packedValue));
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
                if (packedValue.storedValue->as.obj) {
                    return OBJ_VAL(packedValue.storedValue->as.obj);
                } else {
                    return NIL_VAL;
                }
            }
        }
    }
}

static void packValue(PackedValue packedStorageTarget, Value value) {
    if (packedStorageTarget.storedType == NULL) {
        packedStorageTarget.storedValue->asValue = value;
    } else {
        switch (packedStorageTarget.storedType->yt) {
            case TypeAny: packedStorageTarget.storedValue->asValue = value; break;
            case TypeBool: packedStorageTarget.storedValue->asValue = value; break;
            case TypeDouble: packedStorageTarget.storedValue->asValue = value; break;
            case TypeInt8: packedStorageTarget.storedValue->as.i8 = AS_I8(value); break;
            case TypeUint8: packedStorageTarget.storedValue->as.ui8 = AS_UI8(value); break;
            case TypeInt16: packedStorageTarget.storedValue->as.i16 = AS_I16(value); break;
            case TypeUint16: packedStorageTarget.storedValue->as.ui16 = AS_UI16(value); break;
            case TypeInt32: packedStorageTarget.storedValue->as.i32 = AS_I32(value); break;
            case TypeUint32: packedStorageTarget.storedValue->as.ui32 = AS_UI32(value); break;
            case TypeInt64: packedStorageTarget.storedValue->as.i64 = AS_I64(value); break;
            case TypeUint64: packedStorageTarget.storedValue->as.ui64 = AS_UI64(value); break;
            case TypePointer:
            case TypeString:
            case TypeClass:
            case TypeInstance:
            case TypeFunction:
            case TypeNativeBlob:
            case TypeRoutine:
            case TypeChannel:
            case TypeYargType: {
                packedStorageTarget.storedValue->as.obj = AS_OBJ(value);
                break;
            }
            case TypeStruct:
            case TypeArray:
                break;
        }
    }
}

bool assignToPackedValue(PackedValue lhs, Value rhsValue) {

    if (lhs.storedType == NULL) {
        lhs.storedValue->asValue = rhsValue;
        return true;
    } else {
        if (isCompatibleType(lhs.storedType, rhsValue)) {
            packValue(lhs, rhsValue);
            return true;
        } else {
            return false;
        }
    }
}

bool assignToValueCellTarget(ValueCellTarget lhs, Value rhsValue) {
    if (lhs.cellType == NULL) {
        *lhs.value = rhsValue;
        return true;
    } else {
        if (isCompatibleType(lhs.cellType, rhsValue)) {
            *(lhs.value) = rhsValue;
            return true;
        } else {
            return false;
        }
    }
}

bool initialiseValueCellTarget(ValueCellTarget lhs, Value rhsValue) {
    if (lhs.cellType == NULL) {
        *lhs.value = rhsValue;
        return true;
    } else {
        if (isInitialisableType(lhs.cellType, rhsValue)) {
            *(lhs.value) = rhsValue;
            return true;
        } else {
            return false;
        }
    }
}

bool is_uniformarray(PackedValue val) {
    if (val.storedType == NULL) {
        return IS_UNIFORMARRAY(val.storedValue->asValue);
    } else if (val.storedType->yt == TypeArray) {
        return true;
    } else {
        return false;
    }
}

bool is_struct(PackedValue val) {
    if (val.storedType == NULL) {
        return IS_STRUCT(val.storedValue->asValue);
    } else if (val.storedType->yt == TypeStruct) {
        return true;
    } else {
        return false;
    }
}

bool is_nil(PackedValue val) {
    if (val.storedType == NULL) {
        return IS_NIL(val.storedValue->asValue);
    } else if (val.storedType->yt == TypeAny) {
        return IS_NIL(val.storedValue->asValue);
    } else if (type_packs_as_obj(val.storedType)
               && val.storedValue == NULL) {
        return true;
    } else {
        return false;
    }
}

bool is_channel(PackedValue val) {
    if (val.storedType == NULL) {
        return IS_CHANNEL(val.storedValue->asValue);
    } else if (val.storedType->yt == TypeChannel) {
        return true;
    } else {
        return false;
    }
}

void initDynamicValueArray(DynamicValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void appendToDynamicValueArray(DynamicValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeDynamicValueArray(DynamicValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initDynamicValueArray(array);
}

void printValue(Value value) {
    fprintValue(stdout, value);
}

void fprintValue(FILE* op, Value value) {
    switch (value.type) {
        case VAL_BOOL:
            FPRINTMSG(op, AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL: FPRINTMSG(op, "nil"); break;
        case VAL_DOUBLE: FPRINTMSG(op, "%#g", AS_DOUBLE(value)); break;
        case VAL_I8: FPRINTMSG(op, "%d", AS_I8(value)); break;
        case VAL_UI8: FPRINTMSG(op, "%u", AS_UI8(value)); break;
        case VAL_I16: FPRINTMSG(op, "%d", AS_I16(value)); break;
        case VAL_UI16: FPRINTMSG(op, "%u", AS_UI16(value)); break;
        case VAL_I32: FPRINTMSG(op, "%d", AS_I32(value)); break;
        case VAL_UI32: FPRINTMSG(op, "%u", AS_UI32(value)); break;
        case VAL_I64: FPRINTMSG(op, "%ld", AS_I64(value)); break;
        case VAL_UI64: FPRINTMSG(op, "%lu", AS_UI64(value)); break;
        case VAL_ADDRESS: FPRINTMSG(op, "%p", (void*) AS_ADDRESS(value)); break;
        case VAL_OBJ: fprintObject(op, value); break;
    }
}

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL:     return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:      return true;
        case VAL_DOUBLE:   return AS_DOUBLE(a) == AS_DOUBLE(b);
        case VAL_I8:       return AS_I8(a) == AS_I8(b);
        case VAL_UI8:      return AS_UI8(a) == AS_UI8(b);
        case VAL_I16:      return AS_I16(a) == AS_I16(b);
        case VAL_UI16:     return AS_UI16(a) == AS_UI16(b);
        case VAL_I32:      return AS_I32(a) == AS_I32(b);
        case VAL_UI32:     return AS_UI32(a) == AS_UI32(b);
        case VAL_I64:      return AS_I64(a) == AS_I64(b);
        case VAL_UI64:     return AS_UI64(a) == AS_UI64(b);
        case VAL_ADDRESS:  return AS_ADDRESS(a) == AS_ADDRESS(b);
        case VAL_OBJ:      return AS_OBJ(a) == AS_OBJ(b);
        default:           return false; // Unreachable.
    }
}

bool is_positive_integer(Value a) {
    if (IS_UI32(a) || IS_UI16(a) || IS_UI8(a)) {
        return true;
    } else if (IS_UI64(a) && AS_UI64(a) <= UINT32_MAX) {
        return true;
    } else if (IS_I32(a) && AS_I32(a) >= 0) {
        return true;
    } else if (IS_I16(a) && AS_I16(a) >= 0) {
        return true;
    } else if (IS_I8(a) && AS_I8(a) >= 0) {
        return true;
    } else if (IS_I64(a) && AS_I64(a) >= 0) {
        return true;
    }
    return false;
}

uint32_t as_positive_integer(Value a) {
    if (IS_I32(a)) {
        return AS_I32(a);
    } else if (IS_I8(a)) {
        return AS_I8(a);
    } else if (IS_I16(a)) {
        return AS_I16(a);
    } else if (IS_I64(a) && AS_I64(a) <= UINT32_MAX) {
        return AS_I64(a);
    } else if (IS_UI32(a)) {
        return AS_UI32(a);
    } else if (IS_UI8(a)) {
        return AS_UI8(a);
    } else if (IS_UI16(a)) {
        return AS_UI16(a);
    } else if (IS_UI64(a) && AS_UI64(a) <= UINT32_MAX) {
        return AS_UI64(a);
    }
    return 0;
}

PackedValueStore* storedAddressof(Value value) {
    if (IS_POINTER(value)) {
        ObjPackedPointer* pointer = AS_POINTER(value);
        return pointer->destination;
    } else if (IS_STRUCT(value)) {
        ObjPackedStruct* structObj = AS_STRUCT(value);
        return structObj->store.storedValue;
    } else if (IS_UNIFORMARRAY(value)) {
        ObjPackedUniformArray* arrayObj = AS_UNIFORMARRAY(value);
        return arrayObj->store.storedValue;
    }
    return NULL;
}