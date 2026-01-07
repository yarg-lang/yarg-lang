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
    } else if (is_positive_integer(offset)) {
        fieldOffset = as_positive_integer(offset);
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
    } else if (IS_BLOB(a)) {
        return OBJ_VAL(newYargTypeFromType(TypeNativeBlob));
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

bool type_packs_as_container(ObjConcreteYargType* type) {
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
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeNativeBlob:
        case TypeRoutine:
        case TypeChannel:
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

Value defaultValue(Value type) {
    if (IS_NIL(type)) {
        return NIL_VAL;
    } else {
        ObjConcreteYargType* ct = AS_YARGTYPE(type);
        switch (ct->yt) {
            case TypeBool: return BOOL_VAL(false);
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

static void printTypeLiteral(FILE* op, ObjConcreteYargType* type) {
    if (type == NULL) {
        FPRINTMSG(op, "any");
        return;
    }

    if (type->isConst) {
        FPRINTMSG(op, "const ");
    }
    switch (type->yt) {
        case TypeAny: FPRINTMSG(op, "any"); break;
        case TypeBool: FPRINTMSG(op, "bool"); break;
        case TypeDouble: FPRINTMSG(op, "mfloat64"); break;
        case TypeInt8: FPRINTMSG(op, "int8"); break;
        case TypeUint8: FPRINTMSG(op, "uint8"); break;
        case TypeInt16: FPRINTMSG(op, "int16"); break;
        case TypeUint16: FPRINTMSG(op, "uint16"); break;
        case TypeInt32: FPRINTMSG(op, "int32"); break;
        case TypeUint32: FPRINTMSG(op, "uint32"); break;
        case TypeInt64: FPRINTMSG(op, "int64"); break;
        case TypeUint64: FPRINTMSG(op, "uint64"); break;
        case TypeString: FPRINTMSG(op, "string"); break;
        case TypeClass: FPRINTMSG(op, "Class"); break;
        case TypeInstance: FPRINTMSG(op, "Instance"); break;
        case TypeFunction: FPRINTMSG(op, "Function"); break;
        case TypeNativeBlob: FPRINTMSG(op, "NativeBlob"); break;
        case TypeRoutine: FPRINTMSG(op, "Routine"); break;
        case TypeChannel: FPRINTMSG(op, "Channel"); break;  
        case TypeYargType: FPRINTMSG(op, "Type"); break;
        case TypeArray: {
            ObjConcreteYargTypeArray* array = (ObjConcreteYargTypeArray*) type;
            Value type = arrayElementType(array);
            if (IS_NIL(type)) {
                FPRINTMSG(op, "any");
            } else {
                printTypeLiteral(op, array->element_type);
            }
            FPRINTMSG(op, "[");
            if (array->cardinality > 0) {
                FPRINTMSG(op, "%zu", array->cardinality);
            }
            FPRINTMSG(op, "]");
            break;
        }
        case TypeStruct: {
            ObjConcreteYargTypeStruct* st = (ObjConcreteYargTypeStruct*) type;
            FPRINTMSG(op, "struct{|%zu:%zu| ", st->field_count, st->storage_size);
            for (size_t i = 0; i < st->field_count; i++) {
                printTypeLiteral(op, st->field_types[i]);
                FPRINTMSG(op, "; ");
            }
            FPRINTMSG(op, "}");
            break;
        }
        case TypePointer: {
            ObjConcreteYargTypePointer* st = (ObjConcreteYargTypePointer*) type;
            if (type->isConst) {
                FPRINTMSG(op, "const ");
            }
            FPRINTMSG(op, "*");
            if (st->target_type) {
                printTypeLiteral(op, st->target_type);
            } else {
                FPRINTMSG(op, "any");
            }
            break;
        }
        default: FPRINTMSG(op, "Unknown"); break;
    }
}

void printType(FILE* op, ObjConcreteYargType* type) {
    FPRINTMSG(op, "Type:");
    printTypeLiteral(op, type);
}
