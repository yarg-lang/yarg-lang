#ifndef cyarg_value_h
#define cyarg_value_h

#include <string.h>
#include <stdio.h>

#include "common.h"
#include "big-int/big-int.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjRoutine ObjRoutine;
typedef struct ObjConcreteYargType ObjConcreteYargType;

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

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_DOUBLE,
    VAL_I8,
    VAL_UI8,
    VAL_I16,
    VAL_UI16,
    VAL_I32,
    VAL_UI32,
    VAL_UI64,
    VAL_I64,
    VAL_ADDRESS,
    VAL_OBJ,
} ValueType;

typedef struct {
    ValueType type;
    AnyValue as;
} Value;

#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
#define IS_64BIT 1
#define IS_32BIT 0
#else
#define IS_64BIT 0
#define IS_32BIT 1
#endif

#define IS_BOOL(value)     ((value).type == VAL_BOOL)
#define IS_NIL(value)      ((value).type == VAL_NIL)
#define IS_DOUBLE(value)   ((value).type == VAL_DOUBLE)
#define IS_I8(value)       ((value).type == VAL_I8)
#define IS_UI8(value)      ((value).type == VAL_UI8)
#define IS_I16(value)      ((value).type == VAL_I16)
#define IS_UI16(value)     ((value).type == VAL_UI16)
#define IS_I32(value)      ((value).type == VAL_I32)
#define IS_UI32(value)     ((value).type == VAL_UI32)
#define IS_UI64(value)     ((value).type == VAL_UI64)
#define IS_I64(value)      ((value).type == VAL_I64)
#define IS_ADDRESS(value)  ((value).type == VAL_ADDRESS)
#define IS_OBJ(value)      ((value).type == VAL_OBJ)
#define IS_INT(value)      ((value).type == VAL_OBJ && (value).as.obj->type == OBJ_INT)

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

#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value }})
#define NIL_VAL             ((Value){VAL_NIL, {.i32 = 0 }})
#define DOUBLE_VAL(value)   ((Value){VAL_DOUBLE, {.dbl = value }})
#define I8_VAL(value)       ((Value){VAL_I8, {.i8 = value}})
#define UI8_VAL(value)      ((Value){VAL_UI8, {.ui8 = value}})
#define I16_VAL(value)      ((Value){VAL_I16, {.i16 = value}})
#define UI16_VAL(value)     ((Value){VAL_UI16, {.ui16 = value}})
#define I32_VAL(value)      ((Value){VAL_I32, {.i32 = value }})
#define UI32_VAL(value)     ((Value){VAL_UI32, {.ui32 = value }})
#define I64_VAL(a)          ((Value){VAL_I64, {.i64 = a}})
#define UI64_VAL(a)         ((Value){VAL_UI64, {.ui64 = a}})
#define ADDRESS_VAL(value)  ((Value){VAL_ADDRESS, { .address = value}})
#define OBJ_VAL(object)     ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#if IS_64BIT
#define SIZE_T_UI_VAL(value)   UI64_VAL(value)
#elif IS_32BIT
#define SIZE_T_UI_VAL(value)   UI32_VAL(value)
#endif

bool is_positive_integer(Value a);
uint32_t as_positive_integer(Value a);

bool valuesEqual(Value a, Value b);

void printValue(Value value);
void fprintValue(FILE* op, Value value);

typedef union PackedValueStore PackedValueStore;

typedef struct {
    PackedValueStore* storedValue;
    ObjConcreteYargType* storedType;
} PackedValue;

void initialisePackedValue(PackedValue packedValue);
Value unpackValue(PackedValue packedValue);
PackedValue allocPackedValue(Value type);
void markPackedValue(PackedValue packedValue);

bool assignToPackedValue(PackedValue lhs, Value rhsValue);

bool is_uniformarray(PackedValue val);
bool is_struct(PackedValue val);
bool is_nil(PackedValue val);
bool is_channel(PackedValue val);

typedef struct {
    Value value;
    ObjConcreteYargType* cellType;
} ValueCell;

typedef struct {
    Value* value;
    ObjConcreteYargType* cellType;
} ValueCellTarget;

bool assignToValueCellTarget(ValueCellTarget lhs, Value rhsValue);
bool initialiseValueCellTarget(ValueCellTarget lhs, Value rhsValue);

typedef struct {
    int capacity;
    int count;
    Value* values;
} DynamicValueArray;

void initDynamicValueArray(DynamicValueArray* array);
void appendToDynamicValueArray(DynamicValueArray* array, Value value);
void freeDynamicValueArray(DynamicValueArray* array);

PackedValueStore* storedAddressof(Value value);

#endif
