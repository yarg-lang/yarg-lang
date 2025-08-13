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
   TypeYargType
} ConcreteYargType;

typedef struct ObjConcreteYargType {
    Obj obj;
    ConcreteYargType yt;
    bool isConst;
} ObjConcreteYargType;

typedef struct {
    ObjConcreteYargType core;
    ObjConcreteYargType* element_type;
} ObjConcreteYargTypeArray;

typedef struct {
    ObjConcreteYargType core;
    ValueTable field_names;
    Value* field_types;
    size_t field_count;
} ObjConcreteYargTypeStruct;

ObjConcreteYargType* newYargTypeFromType(ConcreteYargType yt);
ObjConcreteYargType* newYargArrayTypeFromType(Value elementType);
ObjConcreteYargType* newYargStructType(size_t fieldCount);

void addFieldType(ObjConcreteYargTypeStruct* st, size_t index, Value type, Value name);

ConcreteYargType yt_typeof(Value a);
Value concrete_typeof(Value a);
bool is_obj_type(ObjConcreteYargType* type);
bool is_nil_assignable_type(ObjConcreteYargType* type);
ObjConcreteYargType* array_element_type(Value val);
size_t yt_sizeof_type(Value type);

Value defaultValue(Value type);

bool isInitialisableType(ObjConcreteYargType* lhsType, Value rhsValue);
bool isCompatibleType(ObjConcreteYargType* lhsType, Value rhsValue);

#endif