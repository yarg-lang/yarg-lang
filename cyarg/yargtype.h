#ifndef cyarg_yargtype_h
#define cyarg_yargtype_h

//               | Value | Placeable | Container | Parameterised | Obj | nil |
// TypeBool      | x     |           |           |               |     |     |
// TypeInt       | x     |           |           |               | x   |     |
// TypePointer   |       | Packable* | x         | x             | x   | x   |
// TypeClass     |       |           |           |               | x   | x   |
// TypeInstance  |       |           |           | x             | x   | x   |
// TypeFunction  |       |           |           | x             | x   | x   |
// TypeRoutine   |       |           | %         | x             | x   | x   |
// TypeChannel   |       |           | x         | x             | x   | x   |
// TypeSyncGroup |       |           | x         | x             | x   | x   |
// TypeMap       | +     |           | x         | x             | x   | +   |
// TypeYargType  |       |           |           |               | x   | x   |
// TypeDouble    | x     | x         |           |               |     |     |
// TypeInt8      | x     | x         |           |               |     |     |
// TypeUint8     | x     | x         |           |               |     |     |
// TypeInt16     | x     | x         |           |               |     |     |
// TypeUint16    | x     | x         |           |               |     |     |
// TypeInt32     | x     | x         |           |               |     |     |
// TypeUint32    | x     | x         |           |               |     |     |
// TypeInt64     | x     | x         |           |               |     |     |
// TypeUint64    | x     | x         |           |               |     |     |
// TypeAddress   | x     | x         |           |               |     | $   |
// TypeString    |       | !         |           |               | x   | x   |
// TypeArray     | +     | Packable  | x         | x             | x   | +   |
// TypeStruct    | x     | Packable  | x         | x             | x   |     |
//
// nil exists outside the set of types as an untyped special value. A location
// that can contain any type can contain nil, and the 'nil' types above can also
// take nil as a valid value.
//
// Value - values are copied when passed. other types are 'pass by reference'
// Placeable - has a platform defined layout in memory. Container types
//             occupy linear memory suffient for all elements
//  Packable - laid out linearly in memory, will be placeable if its contained 
//             types are placeable.
//  Packable* - Pointers will refer to packed type memory, or other types,
//              similarly to a single element array, without allocating storage.
// Container - storage for one-or-more elements of another type.
// Parameterised - requires information to fully define the type, eg the
//                 size of an array, or the types of its elements.
// Obj - Always stored as a heap Obj(ect).
//
// ! will have a c-style string available
// % routines contain only functions
// $ address 0 is distinct from nil, and nil cannot be assigned to address.
// + both Array and Map will become value types as the implementation is completed.

#include "object.h"

typedef enum {
   TypeBool,
   TypeInt,
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
   TypeAddress,
   TypeClass,
   TypeInstance,
   TypeFunction,
   TypeRoutine,
   TypeChannel,
   TypeSyncGroup,
   TypeArray,
   TypeStruct,
   TypePointer,
   TypeMap,
   TypeYargType
} ConcreteYargType;

typedef union {
    bool boolean;
    double dbl;
    uint8_t ui8;
    int8_t i8;
    uint16_t ui16;
    int16_t i16;
    uint32_t ui32;
    int32_t i32;
    uint64_t ui64;
    int64_t i64;
    uintptr_t address;
    Obj* obj;
} AnyValue;

typedef struct ObjConcreteYargType {
    Obj obj;
    ConcreteYargType yt;
} ObjConcreteYargType;

typedef struct {
    ObjConcreteYargType boolean;
    ObjConcreteYargType address;
    ObjConcreteYargType dbl;
    ObjConcreteYargType int8;
    ObjConcreteYargType uint8;
    ObjConcreteYargType int16;
    ObjConcreteYargType uint16;
    ObjConcreteYargType int32;
    ObjConcreteYargType uint32;
    ObjConcreteYargType int64;
    ObjConcreteYargType uint64;
    ObjConcreteYargType type;
    ObjConcreteYargType class_;
    ObjConcreteYargType string;
    ObjConcreteYargType integer;

    ObjConcreteYargType implementation_obj; // only the address matters.

    // TODO: review as more types have complete implementations.
    ObjConcreteYargType function;
    ObjConcreteYargType routine;
    ObjConcreteYargType channel;
    ObjConcreteYargType syncGroup;
} SimpleYargTypes;

extern const SimpleYargTypes yargTypes;

typedef struct ObjConcreteYargTypeArray {
    ObjConcreteYargType core;
    size_t cardinality;
    const ObjConcreteYargType* element_type;
} ObjConcreteYargTypeArray;

typedef struct ObjConcreteYargTypePointer {
    ObjConcreteYargType core;
    const ObjConcreteYargType* target_type;
} ObjConcreteYargTypePointer;

typedef struct ObjConcreteYargTypeMap {
    ObjConcreteYargType core;
    const ObjConcreteYargType* key_type;
    const ObjConcreteYargType* value_type;
} ObjConcreteYargTypeMap;

#define AS_YARGTYPE(value)     ((ObjConcreteYargType*)AS_OBJ(value))

#define YARGTYPE_VAL(object)   ((Value){.type = &yargTypes.type, .as.obj = (Obj*) object})

const ObjConcreteYargType* newYargTypeFromTypeCode(uint8_t typeCode);

ObjConcreteYargType* newYargArrayTypeFromType(const ObjConcreteYargType* elementType, size_t cardinality);
ObjConcreteYargType* newYargPointerType(const ObjConcreteYargType* targetType);

size_t arrayElementOffset(const ObjConcreteYargTypeArray* arrayType, size_t index);
const ObjConcreteYargType* arrayElementType(const ObjConcreteYargType* arrayType);

bool is_nil_assignable_type(const ObjConcreteYargType* type);
bool is_placeable_type(const ObjConcreteYargType* type);
bool is_packable_container_type(const ObjConcreteYargType* type);
bool is_obj_yargtype(const ObjConcreteYargType* type);
size_t yt_sizeof_type_storage(const ObjConcreteYargType* type);
size_t yt_alignmentfor_type_storage(const ObjConcreteYargType* type);

bool isSupportedMapKeyType(const ObjConcreteYargType* type);

ObjString* typeToString(const ObjConcreteYargType* type);

bool typesEqual(const ObjConcreteYargType* a, const ObjConcreteYargType* b);

#endif
