#ifndef cyarg_value_h
#define cyarg_value_h

#include "yargtype.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjRoutine ObjRoutine;
typedef struct ObjConcreteYargType ObjConcreteYargType;

// All Yarg values (including nil) can be stored in a Value, and all
// Yarg values are typed (except nil which exists outside the type system)
typedef struct {
    const ObjConcreteYargType* type;
    AnyValue as;
} Value;

// placed values exist with a defined memory layout, so the
// type information is stored separately from the value itself.
// Note that placed values are a subset of all values, and 
// specifically exclude nil.
// Note: elsewhere, a NULL 'const ObjConcreteYargType*' indicates 
/// the 'any' type, and is invalid for placed values.
typedef struct {
    void* value;
    const ObjConcreteYargType* type;
} PlacedValue;

#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
#define IS_64BIT 1
#define IS_32BIT 0
#else
#define IS_64BIT 0
#define IS_32BIT 1
#endif

#define IS_NIL(value)      is_nil(value)

#define IS_BOOL(value)     ((value).type == &yargTypes.boolean)
#define IS_DOUBLE(value)   ((value).type == &yargTypes.dbl)
#define IS_I8(value)       ((value).type == &yargTypes.int8)
#define IS_UI8(value)      ((value).type == &yargTypes.uint8)
#define IS_I16(value)      ((value).type == &yargTypes.int16)
#define IS_UI16(value)     ((value).type == &yargTypes.uint16)
#define IS_I32(value)      ((value).type == &yargTypes.int32)
#define IS_UI32(value)     ((value).type == &yargTypes.uint32)
#define IS_I64(value)      ((value).type == &yargTypes.int64)
#define IS_UI64(value)     ((value).type == &yargTypes.uint64)
#define IS_ADDRESS(value)  ((value).type == &yargTypes.address)
#define IS_INT(value)      ((value).type == &yargTypes.integer && (value).as.obj->type == OBJ_INT)
#define IS_IMPL_OBJ(value) ((value).type == &yargTypes.implementation_obj && (value).as.obj != NULL)
#define IS_OBJ(value)      isObjValue(value)

#define AS_OBJ(value)      ((value).as.obj)
#define AS_BOOL(value)     ((value).as.boolean)
#define AS_I8(value)       ((value).as.i8)
#define AS_UI8(value)      ((value).as.ui8)
#define AS_I16(value)      ((value).as.i16)
#define AS_UI16(value)     ((value).as.ui16)
#define AS_I32(value)      ((value).as.i32)
#define AS_UI32(value)     ((value).as.ui32)
#define AS_UI64(value)     ((value).as.ui64)
#define AS_I64(value)      ((value).as.i64)
#define AS_ADDRESS(value)  ((value).as.address)
#define AS_DOUBLE(value)   ((value).as.dbl)

#define NIL_VAL             ((Value){.type = NULL, .as.obj = NULL})

#define BOOL_VAL(value)     ((Value){.type = &yargTypes.boolean, {.boolean = value }})
#define DOUBLE_VAL(value)   ((Value){.type = &yargTypes.dbl, {.dbl = value }})
#define I8_VAL(value)       ((Value){.type = &yargTypes.int8, {.i8 = value}})
#define UI8_VAL(value)      ((Value){.type = &yargTypes.uint8, {.ui8 = value}})
#define I16_VAL(value)      ((Value){.type = &yargTypes.int16, {.i16 = value}})
#define UI16_VAL(value)     ((Value){.type = &yargTypes.uint16, {.ui16 = value}})
#define I32_VAL(value)      ((Value){.type = &yargTypes.int32, {.i32 = value }})
#define UI32_VAL(value)     ((Value){.type = &yargTypes.uint32, {.ui32 = value }})
#define I64_VAL(a)          ((Value){.type = &yargTypes.int64, {.i64 = a}})
#define UI64_VAL(a)         ((Value){.type = &yargTypes.uint64, {.ui64 = a}})
#define ADDRESS_VAL(value)  ((Value){.type = &yargTypes.address, { .address = value}})
#define IMPL_OBJ_VAL(object)     ((Value){.type = &yargTypes.implementation_obj, {.obj = (Obj*)object}})
#define TYPED_VAL(object, type) ((Value){.type = ((const ObjConcreteYargType*)type), .as.obj = (Obj*) object})


#if IS_64BIT
#define AS_SIZE_T(value)       ((value).as.ui64)
#define SIZE_T_UI_VAL(value)   UI64_VAL(value)
#elif IS_32BIT
#define AS_SIZE_T(value)       ((value).as.ui32)
#define SIZE_T_UI_VAL(value)   UI32_VAL(value)
#endif

// not implemented - all placed values have 
// default value of 0, and our policy of
// zeroing all memory ensures that placed values are 
// initialized to their default state.
void setDefaultPlacedValue(PlacedValue value);

Value getPlacedValue(PlacedValue placedStorage);
void setPlacedValue(PlacedValue placedStorage, Value rhs);

Value defaultValue(Value type);

bool is_nil(Value value);

bool is_positive_integer(Value a);
size_t as_positive_integer(Value a);

bool valuesEqual(Value a, Value b);

ObjString* valueToString(Value value);

typedef struct {
    int capacity;
    int count;
    Value* values;
} DynamicValueArray;

void initDynamicValueArray(DynamicValueArray* array);
void appendToDynamicValueArray(DynamicValueArray* array, Value value);
void freeDynamicValueArray(DynamicValueArray* array);


Value duplicateValue(Value src);
Value concrete_typeof(Value a);

bool isAddressValue(Value value);

bool isObjType(Value value, ObjType type);
bool isObjValue(Value value);

bool isInitialisableType(const ObjConcreteYargType* lhsType, Value rhsValue, Value *promotedRhs); // promotedRhs will be VAL_NIL if no promotion

#endif
