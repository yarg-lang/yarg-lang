#ifndef cyarg_yargtype_h
#define cyarg_yargtype_h

#include "object.h"
#include "stdio.h"

typedef enum {
   TypeAny,
   TypeBool,
   TypeDouble,
   TypeInt8,
   TypeUint8,
   TypeInt16,
   TypeUint16,
   TypeInt32,
   TypeUint32,
   TypeInt64,
   TypeUint64,
   TypeString,
   TypeClass,
   TypeInstance,
   TypeFunction,
   TypeNativeBlob,
   TypeRoutine,
   TypeChannel,
   TypeArray,
   TypeStruct,
   TypePointer,
   TypeYargType
} ConcreteYargType;

typedef struct ObjConcreteYargType {
    Obj obj;
    ConcreteYargType yt;
    bool isConst;
} ObjConcreteYargType;

typedef struct ObjConcreteYargTypeArray {
    ObjConcreteYargType core;
    size_t cardinality;
    ObjConcreteYargType* element_type;
} ObjConcreteYargTypeArray;

typedef struct ObjConcreteYargTypeStruct {
    ObjConcreteYargType core;
    ValueTable field_names;
    size_t* field_indexes;
    ObjConcreteYargType** field_types;
    size_t field_count;
    size_t storage_size;
} ObjConcreteYargTypeStruct;

typedef struct ObjConcreteYargTypePointer {
    ObjConcreteYargType core;
    ObjConcreteYargType* target_type;
} ObjConcreteYargTypePointer;

ObjConcreteYargType* newYargTypeFromType(ConcreteYargType yt);

ObjConcreteYargType* newYargArrayTypeFromType(Value elementType);
ObjConcreteYargType* newYargStructType(size_t fieldCount);
ObjConcreteYargType* newYargPointerType(Value targetType);

size_t arrayElementOffset(ObjConcreteYargTypeArray* arrayType, size_t index);
size_t arrayElementSize(ObjConcreteYargTypeArray* arrayType);
Value arrayElementType(ObjConcreteYargTypeArray* arrayType);

size_t addFieldType(ObjConcreteYargTypeStruct* st, size_t index, size_t fieldOffset, Value type, Value offset, Value name);

bool isUint32Pointer(Value val);

Value concrete_typeof(Value a);
bool type_packs_as_obj(ObjConcreteYargType* type);
bool type_packs_as_container(ObjConcreteYargType* type);
bool is_nil_assignable_type(Value type);
bool is_placeable_type(Value type);
bool is_stored_type(Value type);
size_t yt_sizeof_type_storage(Value type);

Value defaultValue(Value type);

bool isInitialisableType(ObjConcreteYargType* lhsType, Value rhsValue);
bool isCompatibleType(ObjConcreteYargType* lhsType, Value rhsValue);

void printType(FILE* op, ObjConcreteYargType* type);

#endif