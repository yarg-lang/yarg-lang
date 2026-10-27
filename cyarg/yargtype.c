#include "yargtype.h"

#include <string.h>
#include <assert.h>

#include "common.h"
#include "memory.h"
#include "vm.h"
#include "yargobject.h"
#include "vmobject.h"
#include "yargstructtype.h"
#include "chunk.h"


const SimpleYargTypes yargTypes = {
    .boolean = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeBool },
    .address = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeAddress },
    .dbl = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeDouble },
    .int8 = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeInt8 },
    .uint8 = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeUint8 },
    .int16 = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeInt16 },
    .uint16 = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeUint16 },
    .int32 = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeInt32 },
    .uint32 = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeUint32 },
    .int64 = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeInt64 },
    .uint64 = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeUint64 },
    .type = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeYargType },
    .class_ = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeClass },
    .string = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeString },
    .integer = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeInt },
    .function = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeFunction },
    .routine = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeRoutine },
    .channel = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeChannel },
    .syncGroup = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = TypeSyncGroup },
    .implementation_obj = { .obj.isMarked = true, .obj.next = NULL, .obj.type = OBJ_YARGTYPE, .yt = 0 },
};


bool is_obj_yargtype(const ObjConcreteYargType* type) {
    if (type == NULL) return false;
    else if (type == &yargTypes.implementation_obj) return true;
    else if (type == &yargTypes.class_) return true;
    else if (type == &yargTypes.string) return true;
    else if (type == &yargTypes.type) return true;
    else if (type == &yargTypes.integer) return true;
    else if (type == &yargTypes.function) return true;
    else if (type->yt == TypeStruct) return true;
    else if (type->yt == TypeArray) return true;
    else if (type->yt == TypePointer) return true;
    else if (type->yt == TypeChannel) return true;
    else if (type->yt == TypeSyncGroup) return true;
    else if (type->yt == TypeRoutine) return true;
    else if (type->yt == TypeMap) return true;
    return false;
}

bool is_packable_container_type(const ObjConcreteYargType* type) {
    if (type == NULL) return false;
    if (type->yt == TypeArray) return true;
    if (type->yt == TypeStruct) return true;
    if (type->yt == TypePointer) return true;
    return false;
}

const ObjConcreteYargType* newYargTypeFromTypeCode(uint8_t typeCode) {
    BuiltinType type = typeCode;
    const ObjConcreteYargType* typeObj = NULL;
    switch (type)
    {
        case TYPE_LITERAL_BOOL: typeObj = &yargTypes.boolean; break;
        case TYPE_LITERAL_INT8: typeObj = &yargTypes.int8; break;
        case TYPE_LITERAL_UINT8: typeObj = &yargTypes.uint8; break;
        case TYPE_LITERAL_INT16: typeObj = &yargTypes.int16; break;
        case TYPE_LITERAL_UINT16: typeObj = &yargTypes.uint16; break;
        case TYPE_LITERAL_INT32: typeObj = &yargTypes.int32; break;
        case TYPE_LITERAL_UINT32: typeObj = &yargTypes.uint32; break;
        case TYPE_LITERAL_INT64: typeObj = &yargTypes.int64; break;
        case TYPE_LITERAL_UINT64: typeObj = &yargTypes.uint64; break;
        case TYPE_LITERAL_MACHINE_FLOAT64: typeObj = &yargTypes.dbl; break;
        case TYPE_LITERAL_STRING: typeObj = &yargTypes.string; break;
        case TYPE_LITERAL_INT: typeObj = &yargTypes.integer; break;

        default: typeObj = NULL; break;
    }
    return typeObj;
}

ObjConcreteYargType* newYargArrayTypeFromType(const ObjConcreteYargType* elementType, size_t cardinality) {
    ObjConcreteYargTypeArray* t = ALLOCATE_OBJ(ObjConcreteYargTypeArray, OBJ_YARGTYPE_ARRAY);
    t->core.yt = TypeArray;

    t->element_type = elementType;
    t->cardinality = cardinality;
    return (ObjConcreteYargType*)t;
}

const ObjConcreteYargType* arrayElementType(const ObjConcreteYargType* arrayType) {
    return ((const ObjConcreteYargTypeArray*)arrayType)->element_type;
}

size_t arrayElementOffset(const ObjConcreteYargTypeArray* arrayType, size_t index) {
    return index * yt_sizeof_type_storage(arrayElementType((const ObjConcreteYargType*)arrayType));
}

ObjConcreteYargType* newYargStructType(size_t fieldCount) {
    ObjConcreteYargTypeStruct* t = ALLOCATE_OBJ(ObjConcreteYargTypeStruct, OBJ_YARGTYPE_STRUCT);
    t->core.yt = TypeStruct;
    t->alignment = 1;
    initTable(&t->field_names);
    tempObjRootPush((Obj*)t);

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

ObjConcreteYargType* newYargPointerType(const ObjConcreteYargType* targetType) {
    ObjConcreteYargTypePointer* p = ALLOCATE_OBJ(ObjConcreteYargTypePointer, OBJ_YARGTYPE_POINTER);
    p->core.yt = TypePointer;

    p->target_type = targetType;
    return (ObjConcreteYargType*)p;
}

void appendFieldType(ObjConcreteYargTypeStruct* st, ObjConcreteYargType* type, ObjString* name) {
    size_t index = st->field_names.count;

    assert(st->field_types[index] == NULL);

    st->field_types[index] = type;
    tableSet(&st->field_names, name, SIZE_T_UI_VAL(index));

    size_t fieldAlignment = yt_alignmentfor_type_storage(type);
    size_t fieldOffset = st->storage_size;
    size_t alignmentPadding = (fieldAlignment - (fieldOffset % fieldAlignment)) % fieldAlignment;
    
    st->field_indexes[index] = fieldOffset + alignmentPadding;
    st->storage_size = fieldOffset + alignmentPadding + yt_sizeof_type_storage(type);

    if (fieldAlignment > st->alignment) {
        st->alignment = fieldAlignment;
    }
}

void appendFieldTypeAtOffset(ObjConcreteYargTypeStruct* st, ObjConcreteYargType* type, size_t offset, ObjString* name) {
    size_t index = st->field_names.count;

    assert(st->field_types[index] == NULL);

    st->field_types[index] = type;
    tableSet(&st->field_names, name, SIZE_T_UI_VAL(index));
    st->field_indexes[index] = offset;
    st->storage_size = offset + yt_sizeof_type_storage(type);
}

size_t structFieldOffset(const ObjConcreteYargTypeStruct* type, size_t fieldIndex) {
    return type->field_indexes[fieldIndex];
}


bool is_nil_assignable_type(const ObjConcreteYargType* type) {
    if (type == NULL) {
        return true;
    } else {
        switch (type->yt) {
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
            case TypeAddress:
                return false;
            case TypeString:
            case TypeClass:
            case TypeInstance:
            case TypeFunction:
            case TypeRoutine:
            case TypeChannel:
            case TypeSyncGroup:
            case TypePointer:
            case TypeMap:
            case TypeYargType:

            // TODO: reconsider if Array should be nil-assignable
            case TypeArray:

                return true;
        } 
    }
    return false;
}

bool is_placeable_type(const ObjConcreteYargType* type) {
    if (type != NULL) {
        switch(type->yt) {
            case TypeInt8: return true;
            case TypeUint8: return true;
            case TypeInt16: return true;
            case TypeUint16: return true;
            case TypeInt32: return true;
            case TypeUint32: return true;
            case TypeInt64: return true;
            case TypeUint64: return true;
            case TypeDouble: return true;
            case TypeArray: {
                const ObjConcreteYargType* elementType = arrayElementType(type);
                return is_placeable_type(elementType);
            }
            case TypeStruct: {
                const ObjConcreteYargTypeStruct* ct = (const ObjConcreteYargTypeStruct*)type;
                bool is_placeable = true;
                for (size_t i = 0; i < ct->field_count; i++) {
                    ObjConcreteYargType* fieldType = ct->field_types[i];
                    is_placeable &= is_placeable_type(fieldType);
                }
                return is_placeable;
            }
            default: return false;
        }
    }
    return false;
}

size_t yt_sizeof_type_storage(const ObjConcreteYargType* type) {
    if (type == NULL) {
        return sizeof(Value);
    } else {
        switch (type->yt) {
        case TypeBool:
            return sizeof(Value);
        case TypeDouble:
            return sizeof(double);
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
        case TypeAddress:
            return sizeof(uintptr_t);
        case TypeStruct: {
            const ObjConcreteYargTypeStruct* st = (const ObjConcreteYargTypeStruct*)type;
            return st->storage_size;
        }
        case TypeArray: {
            const ObjConcreteYargTypeArray* array = (const ObjConcreteYargTypeArray*)type;
            return yt_sizeof_type_storage(arrayElementType(type)) * array->cardinality;
        }
        case TypePointer:
        case TypeInt:
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeRoutine:
        case TypeChannel:
        case TypeSyncGroup:
        case TypeMap:
        case TypeYargType:
            return sizeof(Value);
        }
    }
}

size_t yt_alignmentfor_type_storage(const ObjConcreteYargType* type) {
    if (type == NULL) {
        return sizeof(Value);
    } else {
        switch (type->yt) {
        case TypeBool:
            return sizeof(Value);
        case TypeDouble:
            return sizeof(double);
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
        case TypeAddress:
            return sizeof(uintptr_t);
        case TypeStruct: {
            const ObjConcreteYargTypeStruct* st = (const ObjConcreteYargTypeStruct*)type;
            return st->alignment;
        }
        case TypeArray: {
            const ObjConcreteYargType* elementType = arrayElementType(type);
            return yt_alignmentfor_type_storage(elementType);
        }
        case TypeInt:
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeRoutine:
        case TypeChannel:
        case TypeSyncGroup:
        case TypePointer:
        case TypeMap:
        case TypeYargType:
            return sizeof(Value);
        }
    }
}

static bool isInitializableArray(ObjConcreteYargTypeArray* lhsConcreteType, ObjConcreteYargTypeArray* rhsConcreteType) {

    if (lhsConcreteType->cardinality == rhsConcreteType->cardinality) {
        if (lhsConcreteType->element_type == NULL) {
            return true;
        } else if (lhsConcreteType->element_type == NULL && rhsConcreteType->element_type == NULL) {
            return true;
        } else if (rhsConcreteType->element_type == NULL) {
            return false;
        } else {
            return typesEqual(lhsConcreteType->element_type, rhsConcreteType->element_type);
        }
    } else {
        return false;
    }
}

bool isInitialisableType(const ObjConcreteYargType* lhsType, Value rhsValue, Value *promotedRhs) {

    promotedRhs->type = NULL;

    if (lhsType == NULL) {
        return true;
    }
    
    if (IS_NIL(rhsValue)) {
        return is_nil_assignable_type(lhsType);
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
        return typesEqual(lhsType, rhsConcreteType);
    }
}

// this is a temporary measure, until we have a more complete hashing setup.
bool isSupportedMapKeyType(const ObjConcreteYargType* type) {
    if (type != NULL) {
        switch (type->yt) {
            case TypeMap: {
                const ObjConcreteYargTypeMap* mt = (const ObjConcreteYargTypeMap*)type;
                return isSupportedMapKeyType(mt->key_type);
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

static ObjString* typeLiteralToString(const ObjConcreteYargType* type) {
    if (type == NULL) {
        return copyString("any", 3);
    }

    switch (type->yt) {
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
        case TypeAddress: return copyString("address", 7);
        case TypeString: return copyString("string", 6);
        case TypeClass: return copyString("Class", 5);
        case TypeInstance: return copyString("Instance", 8);
        case TypeFunction: return copyString("Function", 8);
        case TypeRoutine: return copyString("Routine", 7);
        case TypeChannel: return copyString("Channel", 7);
        case TypeSyncGroup: return copyString("SyncGroup", 9);
        case TypeYargType: return copyString("Type", 4);
        case TypeArray: {
            const ObjConcreteYargTypeArray* array = (const ObjConcreteYargTypeArray*) type;
            ObjString* typeStr = typeLiteralToString(array->element_type);
            tempRootPush(STRING_VAL(typeStr));
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
            const ObjConcreteYargTypeStruct* st = (const ObjConcreteYargTypeStruct*) type;
            char buffer[1024];
            snprintf(buffer, sizeof(buffer), "struct{|%zu:%zu| ", st->field_count, st->storage_size);
            size_t cursor = strlen(buffer);
            for (size_t i = 0; i < st->field_count; i++) {
                ObjString* fieldTypeStr = typeLiteralToString(st->field_types[i]);
                tempRootPush(STRING_VAL(fieldTypeStr));
                snprintf(buffer + cursor, sizeof(buffer) - cursor, "%s; ", fieldTypeStr->chars);
                cursor = strlen(buffer);
                tempRootPop();
            }
            snprintf(buffer + cursor, sizeof(buffer) - cursor, "}");
            return copyString(buffer, (int)strlen(buffer));
        }
        case TypePointer: {
            const ObjConcreteYargTypePointer* st = (const ObjConcreteYargTypePointer*) type;
            ObjString* typeStr = typeLiteralToString(st->target_type);
            tempRootPush(STRING_VAL(typeStr));
            char buffer[128];
            snprintf(buffer, sizeof(buffer), "*%s", typeStr->chars);
            ObjString* result = copyString(buffer, (int)strlen(buffer));
            tempRootPop();
            return result;
        }
        case TypeMap: {
            const ObjConcreteYargTypeMap* mt = (const ObjConcreteYargTypeMap*) type;
            ObjString* typeStr = typeLiteralToString(mt->value_type);
            tempRootPush(STRING_VAL(typeStr));
            ObjString* keyTypeStr = typeLiteralToString(mt->key_type);
            tempRootPush(STRING_VAL(keyTypeStr));
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

ObjString* typeToString(const ObjConcreteYargType* type) {
    ObjString* literalStr = typeLiteralToString(type);
    tempRootPush(STRING_VAL(literalStr));
    ObjString* prefix = copyString("Type:", 5);
    tempRootPush(STRING_VAL(prefix));
    ObjString* result = concatenateStrings(prefix, literalStr);
    tempRootPop();
    tempRootPop();
    return result;
}


bool structFieldIndex(const ObjConcreteYargTypeStruct* type, ObjString* name, size_t* index) {
    Value indexVal;
    if (tableGet(&type->field_names, name, &indexVal)) {
        *index = AS_SIZE_T(indexVal);
        return true;
    }
    return false;
}

bool typesEqual(const ObjConcreteYargType* a, const ObjConcreteYargType* b) {
    if (a == b) return true;
    if (a != NULL) {
        if (b == NULL) return false;
        if (a->yt != b->yt) return false;
        if (a->yt == TypePointer) {
            assert(b->yt == TypePointer);
            const ObjConcreteYargTypePointer* pointer_a = (const ObjConcreteYargTypePointer*)a;
            const ObjConcreteYargTypePointer* pointer_b = (const ObjConcreteYargTypePointer*)b;
            return typesEqual(pointer_a->target_type, pointer_b->target_type);
        }
        if (a->yt == TypeArray) {
            assert(b->yt == TypeArray);
            const ObjConcreteYargTypeArray* array_a = (const ObjConcreteYargTypeArray*)a;
            const ObjConcreteYargTypeArray* array_b = (const ObjConcreteYargTypeArray*)b;
            if (!typesEqual(array_a->element_type, array_b->element_type)) return false;
            return array_a->cardinality == array_b->cardinality;
        }
        if (a->yt == TypeStruct) {
            assert(b->yt == TypeStruct);
            const ObjConcreteYargTypeStruct* struct_a = (const ObjConcreteYargTypeStruct*)a;
            const ObjConcreteYargTypeStruct* struct_b = (const ObjConcreteYargTypeStruct*)b;
            if (struct_a->field_count != struct_b->field_count) return false;
            for (size_t i = 0; i < struct_a->field_count; i++) {
                if (!typesEqual(struct_a->field_types[i], struct_b->field_types[i])) return false;
            }
            return true;
        }
        // TODO: maps, other complex types
        return a->yt == b->yt;
    }
    return false;
}
