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
   TypeYargType
} ConcreteYargType;

typedef struct ObjConcreteYargType {
    Obj obj;
    ConcreteYargType yt;
    bool isConst;
    ObjConcreteYargType* element_type;
} ObjConcreteYargType;

ObjConcreteYargType* newYargTypeFromType(ConcreteYargType yt);
ObjConcreteYargType* newYargArrayTypeFromType(Value elementType);

ConcreteYargType yt_typeof(Value a);
bool is_obj_type(ObjConcreteYargType* type);

#endif