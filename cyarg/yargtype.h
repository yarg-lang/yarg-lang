#ifndef cyarg_yargtype_h
#define cyarg_yargtype_h

#include "object.h"

typedef enum {
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
} YargType;

typedef struct ObjYargType {
    Obj obj;
    YargType yt;
} ObjYargType;

ObjYargType* newYargTypeFromType(YargType yt);

YargType yt_typeof(Value a);
bool is_obj_type(ObjYargType* type);

#endif