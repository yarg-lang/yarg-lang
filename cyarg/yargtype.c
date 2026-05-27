#include "common.h"

#include "yargtype.h"

#include "memory.h"
#include "vm.h"

ObjConcreteYargType* newYargTypeFromType(ConcreteYargType yt) {
    switch (yt) {
        case TypeAny:
        case TypeBool:
        case TypeDouble:
        case TypeInt8:
        case TypeUint8:
        case TypeInt16:
        case TypeUint16:
        case TypeInt32:
        case TypeUint32:
        case TypeInt64:
        case TypeUint64:
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeRoutine:
        case TypeChannel:
        case TypeYargType:
        case TypeInt : {
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
        case TypeMap: {
            ObjConcreteYargTypeMap* m = ALLOCATE_OBJ(ObjConcreteYargTypeMap, OBJ_YARGTYPE_MAP);
            m->core.yt = yt;
            return (ObjConcreteYargType*)m;
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

    ObjConcreteYargType** fieldTypes = ALLOCATE(ObjConcreteYargType*, fieldCount);
    for (size_t i = 0; i < fieldCount; i++) {
        fieldTypes[i] = NULL;
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

size_t addFieldType(ObjConcreteYargTypeStruct* st, size_t index, size_t fieldOffset, Value type, Value offset, Value name) {
    st->field_types[index] = IS_NIL(type) ? NULL : AS_YARGTYPE(type);
    tableSet(&st->field_names, AS_STRING(name), SIZE_T_UI_VAL(index));
    if (IS_NIL(offset)) {
        st->field_indexes[index] = fieldOffset;
    } else if (is_positive_integer32(offset)) {
        fieldOffset = as_positive_integer32(offset);
        st->field_indexes[index] = fieldOffset;
    }
    st->storage_size = fieldOffset + yt_sizeof_type_storage(type);
    return st->storage_size;
}

bool isUint32Pointer(Value val) {
    if (IS_POINTER(val)) {
        ObjConcreteYargTypePointer* pointer = AS_POINTER(val)->type;
        ObjConcreteYargType* dest = pointer->target_type;
        if (dest) {
            return dest->yt == TypeUint32;
        }
    }
    return false;
}

Value concrete_typeof(Value a) {
    if (IS_NIL(a)) {
        return NIL_VAL;
    } else if (IS_BOOL(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeBool));
    } else if (IS_DOUBLE(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeDouble));
    } else if (IS_I8(a)) {
        return (OBJ_VAL(newYargTypeFromType(TypeInt8)));
    } else if (IS_UI8(a)) {
        return (OBJ_VAL(newYargTypeFromType(TypeUint8)));
    } else if (IS_I16(a)) {
        return (OBJ_VAL(newYargTypeFromType(TypeInt16)));
    } else if (IS_UI16(a)) {
        return (OBJ_VAL(newYargTypeFromType(TypeUint16)));
    } else if (IS_I32(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeInt32));
    } else if (IS_UI32(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeUint32));
    } else if (IS_I64(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeInt64));
    } else if (IS_UI64(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeUint64));
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
    } else if (IS_ROUTINE(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeRoutine));
    } else if (IS_CHANNEL(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeChannel));
    } else if (IS_STRING(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeString));
    } else if (IS_UNIFORMARRAY(a)) {
        return OBJ_VAL(AS_UNIFORMARRAY(a)->store.storedType);
    } else if (IS_STRUCT(a)) {
        return OBJ_VAL(AS_STRUCT(a)->store.storedType);
    } else if (IS_YARGTYPE(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeYargType));
    } else if (IS_POINTER(a)) {
        return OBJ_VAL(AS_POINTER(a)->type);
    } else if (IS_MAP(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeMap));
    } else if (IS_INT(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeInt));
    }
    fatalVMError("Unexpected object type");
    return NIL_VAL;
}

bool type_packs_as_obj(ObjConcreteYargType* type) {
    switch (type->yt) {
        case TypeAny:
        case TypeBool:
        case TypeDouble:
        case TypeInt8:
        case TypeUint8:
        case TypeInt16:
        case TypeUint16:
        case TypeInt32:
        case TypeUint32:
        case TypeInt64:
        case TypeUint64:
        case TypeArray:
        case TypeStruct:
            return false;
        case TypeInt:
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeRoutine:
        case TypeChannel:
        case TypePointer:
        case TypeMap:
        case TypeYargType:
            return true;
    }
}

bool type_packs_as_container(ObjConcreteYargType* type) {
    switch (type->yt) {
        case TypeAny:
        case TypeBool:
        case TypeInt:
        case TypeDouble:
        case TypeInt8:
        case TypeUint8:
        case TypeInt16:
        case TypeUint16:
        case TypeInt32:
        case TypeUint32:
        case TypeInt64:
        case TypeUint64:
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeRoutine:
        case TypeChannel:
        case TypeMap:
        case TypeYargType:
            return false;
        case TypePointer:
        case TypeArray:
        case TypeStruct:
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
            case TypeInt:
            case TypeDouble:
            case TypeInt8:
            case TypeUint8:
            case TypeInt16:
            case TypeUint16:
            case TypeInt32:
            case TypeUint32:
            case TypeInt64:
            case TypeUint64:
            case TypeStruct:
                return false;
            case TypeAny:
            case TypeString:
            case TypeClass:
            case TypeInstance:
            case TypeFunction:
            case TypeRoutine:
            case TypeChannel:
            case TypeArray:
            case TypePointer:
            case TypeMap:
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
            case TypeInt8: return true;
            case TypeUint8: return true;
            case TypeInt16: return true;
            case TypeUint16: return true;
            case TypeInt32: return true;
            case TypeUint32: return true;
            case TypeInt64: return true;
            case TypeUint64: return true;
            case TypeArray: {
                ObjConcreteYargTypeArray* ct = (ObjConcreteYargTypeArray*)AS_YARGTYPE(typeVal);
                Value elementType = arrayElementType(ct);
                return is_placeable_type(elementType);
            }
            case TypeStruct: {
                ObjConcreteYargTypeStruct* ct = (ObjConcreteYargTypeStruct*)AS_YARGTYPE(typeVal);
                bool is_placeable = true;
                for (size_t i = 0; i < ct->field_count; i++) {
                    Value fieldType = ct->field_types[i] == NULL ? NIL_VAL : OBJ_VAL(ct->field_types[i]);
                    is_placeable &= is_placeable_type(fieldType);
                }
                return is_placeable;
            }
            default: return false;
        }
    }
    return false;
}

bool is_stored_type(Value type) {
    if (IS_YARGTYPE(type)) {
        switch(AS_YARGTYPE(type)->yt) {
            case TypeArray:
            case TypeStruct:
            case TypePointer:
                return true;
            default:
                return false;
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
            return sizeof(Value);
        case TypeInt8:
            return sizeof(int8_t);
        case TypeUint8:
            return sizeof(uint8_t);
        case TypeInt16:
            return sizeof(int16_t);
        case TypeUint16:
            return sizeof(uint16_t);
        case TypeInt32:
            return sizeof(int32_t);
        case TypeUint32:
            return sizeof(uint32_t);
        case TypeInt64:
            return sizeof(int64_t);
        case TypeUint64:
            return sizeof(uint64_t);
        case TypeStruct: {
            ObjConcreteYargTypeStruct* st = (ObjConcreteYargTypeStruct*)t;
            return st->storage_size;
        }
        case TypeArray: {
            ObjConcreteYargTypeArray* array = (ObjConcreteYargTypeArray*)t;
            return arrayElementSize(array) * array->cardinality;
        }
        case TypeInt:
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeRoutine:
        case TypeChannel:
        case TypePointer:
        case TypeMap:
        case TypeYargType:
            return sizeof(Obj*);
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
            case TypeStruct: return defaultStructValue(ct);
            case TypeArray: return defaultArrayValue(ct);
            case TypePointer:
            case TypeAny:
            case TypeString:
            case TypeClass:
            case TypeInstance:
            case TypeFunction:
            case TypeRoutine:
            case TypeChannel:
            case TypeMap:
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

bool isInitialisableType(ObjConcreteYargType* lhsType, Value rhsValue, Value *promotedRhs) {

    promotedRhs->type = VAL_NIL;

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
        if (IS_INT(rhsValue))
        {
            ObjInt *i = (ObjInt *) rhsValue.as.obj;
            if (i->isLiteral)
            {
                switch (lhsType->yt)
                {
                case TypeInt8:
                    if (int_is_range(&i->bigInt, INT8_MIN, INT8_MAX) == INT_WITHIN)
                    {
                        *promotedRhs = I8_VAL(int_to_i32(&i->bigInt));
                        return true;
                    }
                    break;
                case TypeUint8:
                    if (int_is_range(&i->bigInt, 0, UINT8_MAX) == INT_WITHIN)
                    {
                        *promotedRhs = UI8_VAL(int_to_u32(&i->bigInt));
                        return true;
                    }
                    break;
                case TypeInt16:
                    if (int_is_range(&i->bigInt, INT16_MIN, INT16_MAX) == INT_WITHIN)
                    {
                        *promotedRhs = I16_VAL(int_to_i32(&i->bigInt));
                        return true;
                    }
                    break;
                case TypeUint16:
                    if (int_is_range(&i->bigInt, 0, UINT16_MAX) == INT_WITHIN)
                    {
                        *promotedRhs = UI16_VAL(int_to_u32(&i->bigInt));
                        return true;
                    }
                    break;
                case TypeInt32:
                    if (int_is_range(&i->bigInt, INT32_MIN, INT32_MAX) == INT_WITHIN)
                    {
                        *promotedRhs = I32_VAL(int_to_i32(&i->bigInt));
                        return true;
                    }
                    break;
                case TypeUint32:
                    if (int_is_range(&i->bigInt, 0, UINT32_MAX) == INT_WITHIN)
                    {
                        *promotedRhs = UI32_VAL(int_to_u32(&i->bigInt));
                        return true;
                    }
                    break;
                case TypeInt64:
                    if (int_is_range(&i->bigInt, INT64_MIN, INT64_MAX) == INT_WITHIN)
                    {
                        *promotedRhs = I64_VAL(int_to_i64(&i->bigInt));
                        return true;
                    }
                    break;
                case TypeUint64:
                    if (int_is_range(&i->bigInt, 0, UINT64_MAX) == INT_WITHIN)
                    {
                        *promotedRhs = UI64_VAL(int_to_u64(&i->bigInt));
                        return true;
                    }
                    break;
                default:
                    break;
                }
            }
        }
        return lhsType->yt == rhsConcreteType->yt;
    }
}

// this is a temporary measure, until we have a more complete hashing setup.
bool isSupportedMapKeyType(Value type) {
    if (IS_YARGTYPE(type)) {
        switch (AS_YARGTYPE(type)->yt) {
            case TypeMap: {
                ObjConcreteYargTypeMap* mt = (ObjConcreteYargTypeMap*)AS_YARGTYPE(type);
                return isSupportedMapKeyType(mt->key_type ? OBJ_VAL(mt->key_type) : NIL_VAL);
            }
            case TypeString:
                return true;
            default:
                return false;
        }
    } else {
         return false;
    }
}

static ObjString* typeLiteralToString(ObjConcreteYargType* type) {
    if (type == NULL) {
        return copyString("any", 3);
    }

    switch (type->yt) {
        case TypeAny: return copyString("any", 3);
        case TypeBool: return copyString("bool", 4);
        case TypeDouble: return copyString("mfloat64", 8);
        case TypeInt: return copyString("int", 3);
        case TypeInt8: return copyString("int8", 4);
        case TypeUint8: return copyString("uint8", 5);
        case TypeInt16: return copyString("int16", 5);
        case TypeUint16: return copyString("uint16", 6);
        case TypeInt32: return copyString("int32", 5);
        case TypeUint32: return copyString("uint32", 6);
        case TypeInt64: return copyString("int64", 5);
        case TypeUint64: return copyString("uint64", 6);
        case TypeString: return copyString("string", 6);
        case TypeClass: return copyString("Class", 5);
        case TypeInstance: return copyString("Instance", 8);
        case TypeFunction: return copyString("Function", 8);
        case TypeRoutine: return copyString("Routine", 7);
        case TypeChannel: return copyString("Channel", 7);
        case TypeYargType: return copyString("Type", 4);
        case TypeArray: {
            ObjConcreteYargTypeArray* array = (ObjConcreteYargTypeArray*) type;
            ObjString* typeStr = typeLiteralToString(array->element_type);
            tempRootPush(OBJ_VAL(typeStr));
            char buffer[128];
            if (array->cardinality > 0) {
                snprintf(buffer, sizeof(buffer), "%s[%zu]", typeStr->chars, array->cardinality);
            } else {
                snprintf(buffer, sizeof(buffer), "%s[]", typeStr->chars);
            }
            ObjString* result = copyString(buffer, (int)strlen(buffer));
            tempRootPop();
            return result;
        }
        case TypeStruct: {
            ObjConcreteYargTypeStruct* st = (ObjConcreteYargTypeStruct*) type;
            char buffer[1024];
            snprintf(buffer, sizeof(buffer), "struct{|%zu:%zu| ", st->field_count, st->storage_size);
            size_t cursor = strlen(buffer);
            for (size_t i = 0; i < st->field_count; i++) {
                ObjString* fieldTypeStr = typeLiteralToString(st->field_types[i]);
                tempRootPush(OBJ_VAL(fieldTypeStr));
                snprintf(buffer + cursor, sizeof(buffer) - cursor, "%s; ", fieldTypeStr->chars);
                cursor = strlen(buffer);
                tempRootPop();
            }
            snprintf(buffer + cursor, sizeof(buffer) - cursor, "}");
            return copyString(buffer, (int)strlen(buffer));
        }
        case TypePointer: {
            ObjConcreteYargTypePointer* st = (ObjConcreteYargTypePointer*) type;
            ObjString* typeStr = typeLiteralToString(st->target_type);
            tempRootPush(OBJ_VAL(typeStr));
            char buffer[128];
            snprintf(buffer, sizeof(buffer), "*%s", typeStr->chars);
            ObjString* result = copyString(buffer, (int)strlen(buffer));
            tempRootPop();
            return result;
        }
        case TypeMap: {
            ObjConcreteYargTypeMap* mt = (ObjConcreteYargTypeMap*) type;
            ObjString* typeStr = typeLiteralToString(mt->value_type);
            tempRootPush(OBJ_VAL(typeStr));
            ObjString* keyTypeStr = typeLiteralToString(mt->key_type);
            tempRootPush(OBJ_VAL(keyTypeStr));
            char buffer[128];
            snprintf(buffer, sizeof(buffer), "%s[%s]", typeStr->chars, keyTypeStr->chars);
            ObjString* result = copyString(buffer, (int)strlen(buffer));
            tempRootPop();
            tempRootPop();
            return result;
        }
        default: {
            return copyString("Unknown", 7);
        }
    }
}

ObjString* typeToString(ObjConcreteYargType* type) {
    ObjString* literalStr = typeLiteralToString(type);
    tempRootPush(OBJ_VAL(literalStr));
    ObjString* prefix = copyString("Type:", 5);
    tempRootPush(OBJ_VAL(prefix));
    ObjString* result = concatenateStrings(prefix, literalStr);
    tempRootPop();
    tempRootPop();
    return result;
}
