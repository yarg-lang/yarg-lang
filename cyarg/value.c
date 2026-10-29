#include "value.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "object.h"
#include "memory.h"
#include "yargtype.h"
#include "yargobject.h"
#include "yargstructtype.h"
#include "value_cell.h"
#include "packed_containers.h"

bool isObjValue(Value value) {
    if (value.type == &yargTypes.implementation_obj && value.as.obj != NULL) return true;
    else if (is_obj_yargtype(value.type) && value.as.obj != NULL) return true;
    return false;
}

bool isObjType(Value value, ObjType type) {
    return isObjValue(value) && AS_OBJ(value)->type == type;
}

bool is_promotable_to_val(Obj* object) {
    if (object == NULL) return true;

    switch (object->type) {
        case OBJ_STRING:
        case OBJ_INT:
        case OBJ_CLASS:
        case OBJ_YARGTYPE:
            return true;
        default:
            return false;
    }
}

Value as_val(Obj* object) {
    if (!is_promotable_to_val(object)) {
        fatalVMError("Object is not promotable to a value.");
    }
    if (object == NULL) {
        return NIL_VAL;
    } else if (object->type == OBJ_STRING) {
        return TYPED_VAL(object, &yargTypes.string);
    } else if (object->type == OBJ_INT) {
        return TYPED_VAL(object, &yargTypes.integer);
    } else if (object->type == OBJ_CLASS) {
        return TYPED_VAL(object, &yargTypes.class_);
    } else if (object->type == OBJ_YARGTYPE) {
        return TYPED_VAL(object, &yargTypes.type);
    } else {
        fatalVMError("Unsupported object type for as_val: %d", object->type);
        return IMPL_OBJ_VAL(object);
    }
}

bool is_nil(Value value) {
    if (value.type == NULL && value.as.obj == NULL) return true;
    if (value.type == &yargTypes.implementation_obj && value.as.obj != NULL) return false;
    assert(value.type != NULL);
    switch (value.type->yt) {
        case TypePointer:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeRoutine:
        case TypeChannel:
        case TypeSyncGroup:
        case TypeMap:
        case TypeString:
            return value.as.obj == NULL;
        default:
            return false;
    }
}

Value getPlacedValue(PlacedValue placedStorage) {
    
    if (is_placeable_type(placedStorage.type)) {
        Value value = NIL_VAL;
        value.type = placedStorage.type;
        if (placedStorage.type == &yargTypes.dbl) {
            value.as.dbl = *(double*) placedStorage.value;
        } else if (placedStorage.type == &yargTypes.int8) {
            value.as.i8 = *(int8_t*) placedStorage.value;
        } else if (placedStorage.type == &yargTypes.uint8) {
            value.as.ui8 = *(uint8_t*) placedStorage.value;
        } else if (placedStorage.type == &yargTypes.int16) {
            value.as.i16 = *(int16_t*) placedStorage.value;
        } else if (placedStorage.type == &yargTypes.uint16) {
            value.as.ui16 = *(uint16_t*) placedStorage.value;
        } else if (placedStorage.type == &yargTypes.int32) {
            value.as.i32 = *(int32_t*) placedStorage.value;
        } else if (placedStorage.type == &yargTypes.uint32) {
            value.as.ui32 = *(uint32_t*) placedStorage.value;
        } else if (placedStorage.type == &yargTypes.int64) {
            value.as.i64 = *(int64_t*) placedStorage.value;
        } else if (placedStorage.type == &yargTypes.uint64) {
            value.as.ui64 = *(uint64_t*) placedStorage.value;
        } else if (placedStorage.type == &yargTypes.address) {
            value.as.address = *(uintptr_t*) placedStorage.value;
        } else {
            fatalVMError("Unsupported placeable type: %s", placedStorage.type->yt);
        }
        return value;
    } else {
        fatalVMError("Unsupported non-placeable type: %s", placedStorage.type->yt);
    }
    return NIL_VAL;
}

void setPlacedValue(PlacedValue lhs, Value rhs) {
    if (IS_I8(rhs)) {
        *(int8_t*)lhs.value = rhs.as.i8;
    } else if (IS_UI8(rhs)) {
        *(uint8_t*)lhs.value = rhs.as.ui8;
    } else if (IS_I16(rhs)) {
        *(int16_t*)lhs.value = rhs.as.i16;
    } else if (IS_UI16(rhs)) {
        *(uint16_t*)lhs.value = rhs.as.ui16;
    } else if (IS_I32(rhs)) {
        *(int32_t*)lhs.value = rhs.as.i32;
    } else if (IS_UI32(rhs)) {
        *(uint32_t*)lhs.value = rhs.as.ui32;
    } else if (IS_I64(rhs)) {
        *(int64_t*)lhs.value = rhs.as.i64;
    } else if (IS_UI64(rhs)) {
        *(uint64_t*)lhs.value = rhs.as.ui64;
    } else if (IS_ADDRESS(rhs)) {
        *(uintptr_t*)lhs.value = rhs.as.address;
    } else if (IS_DOUBLE(rhs)) {
        *(double*)lhs.value = rhs.as.dbl;
    } else {
        // TODO: review struct, array, and pointer handling
        fatalVMError("Unhandled type in setPlacedValue");
    }
}

Value concrete_typeof(Value a) {
    if (IS_NIL(a)) {
        return NIL_VAL;
    } else if (IS_IMPL_OBJ(a)) {
        fatalVMError("Unexpected object type");
        return NIL_VAL;
    } else {
        return YARGTYPE_VAL(a.type);
    }
}

Value duplicateValue(Value src) {
    if (IS_INT(src)) {
        ObjInt* rhs = AS_INTOBJ(src);
        ObjInt* lhs = allocateIntObject(rhs->bigInt.m_);
        uint16_t* lhs_w = (uint16_t*) &lhs->bigInt.w_[0];
        uint16_t* rhs_w = (uint16_t*) &rhs->bigInt.w_[0];
        memcpy(lhs_w, rhs_w, rhs->bigInt.m_ * sizeof(uint16_t));
        lhs->bigInt.neg_ = rhs->bigInt.neg_;
        lhs->bigInt.d_ = rhs->bigInt.d_;
        lhs->isLiteral = rhs->isLiteral;
        return INTOBJ_VAL(lhs);
    } else if (IS_STRUCT(src)) {
        return duplicateStruct(src);
    } else if (IS_UNIFORMARRAY(src)) {
        return duplicateArray(src);
    } else if (IS_POINTER(src)) {
        return duplicatePointer(src);
    } else {
        return src;
    }
}

Value defaultValue(Value type) {
    if (IS_NIL(type)) {
        return NIL_VAL;
    } else {
        ObjConcreteYargType* ct = AS_YARGTYPE(type);
        switch (ct->yt) {
            case TypeBool: return BOOL_VAL(false);
            case TypeInt: return defaultIntValue();
            case TypeDouble: return DOUBLE_VAL(0);
            case TypeInt8: return I8_VAL(0);
            case TypeUint8: return UI8_VAL(0);
            case TypeInt16: return I16_VAL(0);
            case TypeUint16: return UI16_VAL(0);
            case TypeInt32: return I32_VAL(0);
            case TypeUint32: return UI32_VAL(0);
            case TypeInt64: return I64_VAL(0);
            case TypeUint64: return UI64_VAL(0);
            case TypeAddress: return ADDRESS_VAL(0);
            case TypeStruct: return defaultStructValue(ct);
            case TypeArray: return defaultArrayValue(ct);
            case TypePointer: return defaultPointerValue(ct);
            case TypeMap:
            case TypeString:
            case TypeClass:
            case TypeInstance:
            case TypeFunction:
            case TypeRoutine:
            case TypeChannel:
            case TypeSyncGroup:
            case TypeYargType:
                return NIL_VAL;
        }
    }
}

void initDynamicValueArray(DynamicValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
    array->stash = NIL_VAL;
}

void appendToDynamicValueArray(DynamicValueArray* array, Value value) {
    array->stash = value;

    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
    array->stash = NIL_VAL;
}

void freeDynamicValueArray(DynamicValueArray* array) {
    vm_free(array->values);
    initDynamicValueArray(array);
}

ObjString* concatenateStrings(ObjString* a, ObjString* b) {

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    return result;
}

ObjString* doubleToString(double value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%#g", value);
    return copyString(buffer, (int)strlen(buffer));
}

ObjString* i8ToString(int8_t value) {
    char* buffer = ALLOCATE(char, DECIMAL_DIGITS_BOUND(int8_t) + 2); // +1 for possible '-' sign, +1 for null terminator
    snprintf(buffer, DECIMAL_DIGITS_BOUND(int8_t) + 2, "%d", value);
    return takeString(buffer, (int)strlen(buffer));
}

ObjString* ui8ToString(uint8_t value) {
    char* buffer = ALLOCATE(char, DECIMAL_DIGITS_BOUND(uint8_t) + 1);
    snprintf(buffer, DECIMAL_DIGITS_BOUND(uint8_t) + 1, "%u", value);
    return takeString(buffer, (int)strlen(buffer));
}

ObjString* i16ToString(int16_t value) {
    char* buffer = ALLOCATE(char, DECIMAL_DIGITS_BOUND(int16_t) + 2);  // +1 for possible '-' sign, +1 for null terminator
    snprintf(buffer, DECIMAL_DIGITS_BOUND(int16_t) + 2, "%d", value);
    return takeString(buffer, (int)strlen(buffer));
}

ObjString* ui16ToString(uint16_t value) {
    char* buffer = ALLOCATE(char, DECIMAL_DIGITS_BOUND(uint16_t) + 1);
    snprintf(buffer, DECIMAL_DIGITS_BOUND(uint16_t) + 1, "%u", value);
    return takeString(buffer, (int)strlen(buffer));
}

ObjString* i32ToString(int32_t value) {
    char* buffer = ALLOCATE(char, DECIMAL_DIGITS_BOUND(int32_t) + 2); // +1 for possible '-' sign, +1 for null terminator
    snprintf(buffer, DECIMAL_DIGITS_BOUND(int32_t) + 2, "%d", value);
    return takeString(buffer, (int)strlen(buffer));
}

ObjString* ui32ToString(uint32_t value) {
    char* buffer = ALLOCATE(char, DECIMAL_DIGITS_BOUND(uint32_t) + 1);
    snprintf(buffer, DECIMAL_DIGITS_BOUND(uint32_t) + 1, "%u", value);
    return takeString(buffer, (int)strlen(buffer));
}

ObjString* i64ToString(int64_t value) {
    char* buffer = ALLOCATE(char, DECIMAL_DIGITS_BOUND(int64_t) + 2); // +1 for possible '-' sign, +1 for null terminator
    snprintf(buffer, DECIMAL_DIGITS_BOUND(int64_t) + 2, "%" PRId64, value);
    return takeString(buffer, (int)strlen(buffer));
}

ObjString* ui64ToString(uint64_t value) {
    char* buffer = ALLOCATE(char, DECIMAL_DIGITS_BOUND(uint64_t) + 1);
    snprintf(buffer, DECIMAL_DIGITS_BOUND(uint64_t) + 1, "%" PRIu64, value);
    return takeString(buffer, (int)strlen(buffer));
}

ObjString* addressToString(uintptr_t value) {
    char* buffer = ALLOCATE(char, 18 + 1);
    snprintf(buffer, 18 + 1, "%p", (void*)value);
    return takeString(buffer, (int)strlen(buffer));
}

static ObjString* arrayToString(Value arrayVal) {
    const ObjConcreteYargTypeArray* arrayType = (const ObjConcreteYargTypeArray*)arrayVal.type;

    char buffer[1024];
    ObjString* typeStr = valueToString(YARGTYPE_VAL(arrayType));
    snprintf(buffer, sizeof(buffer), "%s:[", typeStr->chars);
    size_t cursor = strlen(buffer);
    for (int i = 0; i < arrayType->cardinality; i++) {
        Value element = arrayElement(arrayVal, i);
        tempRootPush(element);
        ObjString* candidate = valueToString(element);
        snprintf(buffer + cursor, sizeof(buffer) - cursor, "%s", candidate->chars);
        cursor = strlen(buffer);
        if (i < arrayType->cardinality - 1) {
            snprintf(buffer + cursor, sizeof(buffer) - cursor, ", ");
            cursor = strlen(buffer);
        }
        tempRootPop();
    }
    snprintf(buffer + cursor, sizeof(buffer) - cursor, "]");
    return copyString(buffer, (int)strlen(buffer));
}

static ObjString* pointerToString(Value ptrVal) {
    const ObjConcreteYargTypePointer* ptrType = (const ObjConcreteYargTypePointer*)ptrVal.type;
    const ObjPackedContainer* ptr = AS_PACKEDCONTAINER(ptrVal);

    Value targetType = ptrType->target_type ? YARGTYPE_VAL(ptrType->target_type) : NIL_VAL;
    ObjString* targetTypeStr = valueToString(targetType);
    tempRootPush(STRING_VAL(targetTypeStr));

    size_t string_length = targetTypeStr->length;
    string_length += DECIMAL_DIGITS_BOUND(uintptr_t) + 4; // "<*%s:%p>"

    char* chars = ALLOCATE(char, string_length + 1);
    snprintf(chars, string_length + 1, "<*%s:%p>", targetTypeStr->chars, ptr->loc);

    ObjString* result = takeString(chars, strlen(chars));
    tempRootPop();
    return result;
}

static ObjString* structToString(Value stVal) {
    const ObjConcreteYargTypeStruct* structType = (const ObjConcreteYargTypeStruct*)stVal.type;
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "struct{|%zu:%zu|", structType->field_count, structType->storage_size);
    size_t cursor = strlen(buffer);
    for (size_t i = 0; i < structType->field_count; i++) {
        Value f = structField(stVal, i);
        tempRootPush(f);
        ObjString* fieldStr = valueToString(f);
        tempRootPush(STRING_VAL(fieldStr));
        snprintf(buffer + cursor, sizeof(buffer) - cursor, "%s; ", fieldStr->chars);
        cursor = strlen(buffer);
        tempRootPop();
        tempRootPop();
    }
    snprintf(buffer + cursor, sizeof(buffer) - cursor, "}");
    return copyString(buffer, (int)strlen(buffer));
}


ObjString* valueToString(Value value) {

    if (is_nil(value)) {
        return copyString("nil", 3);
    } else if (value.type == &yargTypes.implementation_obj) {
        // implementation object
        return objectToString(value.as.obj);
    } else {
        switch (value.type->yt) {
            case TypeBool:     return copyString(AS_BOOL(value) ? "true" : "false", AS_BOOL(value) ? 4 : 5);
            case TypeDouble:   return doubleToString(AS_DOUBLE(value));
            case TypeInt8:     return i8ToString(AS_I8(value));
            case TypeUint8:    return ui8ToString(AS_UI8(value));
            case TypeInt16:    return i16ToString(AS_I16(value));
            case TypeUint16:   return ui16ToString(AS_UI16(value));
            case TypeInt32:    return i32ToString(AS_I32(value));
            case TypeUint32:   return ui32ToString(AS_UI32(value));
            case TypeInt64:    return i64ToString(AS_I64(value));
            case TypeUint64:   return ui64ToString(AS_UI64(value));
            case TypeAddress:  return addressToString(AS_ADDRESS(value));
            case TypeArray:    return arrayToString(value);
            case TypeStruct:   return structToString(value);
            case TypeYargType: return objectToString(value.as.obj);
            case TypePointer:  return pointerToString(value);
            case TypeInt:      return objectToString(value.as.obj);
            case TypeMap:      return objectToString(value.as.obj);
            case TypeString:   return objectToString(value.as.obj);
            case TypeRoutine:  return objectToString(value.as.obj);
            case TypeClass:    return objectToString(value.as.obj);
            case TypeInstance: return objectToString(value.as.obj);
            case TypeFunction: return objectToString(value.as.obj);
            case TypeChannel:  return objectToString(value.as.obj);
            case TypeSyncGroup: return objectToString(value.as.obj);
        }
    }
    return NULL;
}

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;
    if (is_nil(a) && is_nil(b)) return true;

    switch (a.type->yt) {
        case TypeBool:     return AS_BOOL(a) == AS_BOOL(b);
        case TypeDouble:   return AS_DOUBLE(a) == AS_DOUBLE(b);
        case TypeInt8:     return AS_I8(a) == AS_I8(b);
        case TypeUint8:    return AS_UI8(a) == AS_UI8(b);
        case TypeInt16:    return AS_I16(a) == AS_I16(b);
        case TypeUint16:   return AS_UI16(a) == AS_UI16(b);
        case TypeInt32:    return AS_I32(a) == AS_I32(b);
        case TypeUint32:   return AS_UI32(a) == AS_UI32(b);
        case TypeInt64:    return AS_I64(a) == AS_I64(b);
        case TypeUint64:   return AS_UI64(a) == AS_UI64(b);
        case TypeAddress:  return AS_ADDRESS(a) == AS_ADDRESS(b);
        case TypeArray:    return arraysEqual(a, b);
        case TypeStruct:   return structsEqual(a, b);
        case TypeYargType: return typesEqual(AS_YARGTYPE(a), AS_YARGTYPE(b));
        case TypePointer:  return pointersEqual(a, b);
        case TypeInt:      return intsEqual(AS_INTOBJ(a), AS_INTOBJ(b));

        case TypeMap:
        case TypeString:
        case TypeRoutine:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeChannel:
        case TypeSyncGroup:
            return AS_OBJ(a) == AS_OBJ(b);
    }
    assert(false && "Unhandled value type in valuesEqual");
    return false;
}

bool is_positive_integer(Value a) {
    if (IS_UI32(a) || IS_UI16(a) || IS_UI8(a)) {
        return true;
    } else if (IS_I32(a) && AS_I32(a) >= 0) {
        return true;
    } else if (IS_I16(a) && AS_I16(a) >= 0) {
        return true;
    } else if (IS_I8(a) && AS_I8(a) >= 0) {
        return true;
#if IS_64BIT
    } else if (IS_UI64(a)) {
        return true;
    } else if (IS_I64(a) && AS_I64(a) >= 0) {
        return true;
    } else if (IS_INT(a)) {
        return int_is_range(AS_INT(a), 0, UINT64_MAX) == INT_WITHIN;
    }
#else
    } else if (IS_UI64(a) && AS_UI64(a) <= UINT32_MAX) {
        return true;
    } else if (IS_I64(a) && AS_I64(a) >= 0 && AS_I64(a) <= UINT32_MAX) {
        return true;
    } else if (IS_INT(a)) {
        return int_is_range(AS_INT(a), 0, UINT32_MAX) == INT_WITHIN;
    }
#endif
    return false;
}

size_t as_positive_integer(Value a) {
    if (IS_I32(a)) {
        return AS_I32(a);
    } else if (IS_I8(a)) {
        return AS_I8(a);
    } else if (IS_I16(a)) {
        return AS_I16(a);
#if IS_64BIT
    } else if (IS_I64(a)) {
        return AS_I64(a);
#else
    } else if (IS_I64(a) && AS_I64(a) <= UINT32_MAX) {
        return (uint32_t) AS_I64(a);
#endif
    } else if (IS_UI32(a)) {
        return AS_UI32(a);
    } else if (IS_UI8(a)) {
        return AS_UI8(a);
    } else if (IS_UI16(a)) {
        return AS_UI16(a);
#if IS_64BIT
    } else if (IS_UI64(a)) {
        return AS_UI64(a);
    } else if (IS_INT(a)) {
        if (int_is_range(AS_INT(a), 0, UINT64_MAX) == INT_WITHIN) {
            return int_to_u64(AS_INT(a));
        }
    }
#else
    } else if (IS_UI64(a) && AS_UI64(a) <= UINT32_MAX) {
        return (uint32_t) AS_UI64(a);
    } else if (IS_INT(a)) {
        if (int_is_range(AS_INT(a), 0, UINT32_MAX) == INT_WITHIN) {
            return int_to_u32(AS_INT(a));
        }
    }
#endif
    return 0;
}
