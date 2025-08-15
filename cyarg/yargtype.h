#ifndef cyarg_yargtype_h
#define cyarg_yargtype_h

#include "object.h"

typedef enum {
   TypeAny,
   TypeBool,
   TypeDouble,
   TypeMachineUint32,
   TypeInteger,
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

typedef struct {
    ObjConcreteYargType core;
    size_t cardinality;
    ObjConcreteYargType* element_type;
} ObjConcreteYargTypeArray;

typedef struct {
    ObjConcreteYargType core;
    ValueTable field_names;
    Value* field_types;
    size_t field_count;
} ObjConcreteYargTypeStruct;

typedef struct {
    ObjConcreteYargType core;
    ObjConcreteYargType* target_type;
} ObjConcreteYargTypePointer;

ObjConcreteYargType* newYargTypeFromType(ConcreteYargType yt);

ObjConcreteYargType* newYargArrayTypeFromType(Value elementType);
ObjConcreteYargType* newYargStructType(size_t fieldCount);
ObjConcreteYargType* newYargPointerType(Value targetType);

void addFieldType(ObjConcreteYargTypeStruct* st, size_t index, Value type, Value name);

Value concrete_typeof(Value a);
bool is_obj_type(ObjConcreteYargType* type);
bool is_nil_assignable_type(Value type);
bool is_placeable_type(Value type);
size_t yt_sizeof_type_storage(Value type);

Value defaultValue(Value type);

bool isInitialisableType(ObjConcreteYargType* lhsType, Value rhsValue);
bool isCompatibleType(ObjConcreteYargType* lhsType, Value rhsValue);

#endif