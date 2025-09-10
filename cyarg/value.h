#ifndef cyarg_value_h
#define cyarg_value_h

#include <string.h>
#include <stdio.h>

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjRoutine ObjRoutine;
typedef struct ObjValArray ObjValArray;

typedef union {
    bool boolean;
    double dbl;
    uint32_t uinteger;
    int32_t integer;
    uint64_t ui64;
    int64_t i64;
    uintptr_t address;
    Obj* obj;
} AnyValue;

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_DOUBLE,
    VAL_UINTEGER,
    VAL_INTEGER,
    VAL_UI64,
    VAL_I64,
    VAL_ADDRESS,
    VAL_OBJ,
} ValueType;

typedef struct {
    ValueType type;
    AnyValue as;
} Value;

#define IS_BOOL(value)     ((value).type == VAL_BOOL)
#define IS_NIL(value)      ((value).type == VAL_NIL)
#define IS_DOUBLE(value)   ((value).type == VAL_DOUBLE)
#define IS_UINTEGER(value) ((value).type == VAL_UINTEGER)
#define IS_INTEGER(value)  ((value).type == VAL_INTEGER)
#define IS_UI64(value)     ((value).type == VAL_UI64)
#define IS_I64(value)      ((value).type == VAL_I64)
#define IS_ADDRESS(value)  ((value).type == VAL_ADDRESS)
#define IS_OBJ(value)      ((value).type == VAL_OBJ)

#define AS_OBJ(value)      ((value).as.obj)
#define AS_BOOL(value)     ((value).as.boolean)
#define AS_UINTEGER(value) ((value).as.uinteger)
#define AS_INTEGER(value)  ((value).as.integer)
#define AS_UI64(value)     ((value).as.ui64)
#define AS_I64(value)      ((value).as.i64)
#define AS_ADDRESS(value)  ((value).as.address)
#define AS_DOUBLE(value)   ((value).as.dbl)

#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value }})
#define NIL_VAL             ((Value){VAL_NIL, {.integer = 0 }})
#define DOUBLE_VAL(value)   ((Value){VAL_DOUBLE, {.dbl = value }})
#define UINTEGER_VAL(value) ((Value){VAL_UINTEGER, {.uinteger = value }})
#define INTEGER_VAL(value)  ((Value){VAL_INTEGER, {.integer = value }})
#define UI64_VAL(a)         ((Value){VAL_UI64, { .ui64 = a}})
#define I64_VAL(a)          ((Value){VAL_I64, { .i64 = a}})
#define ADDRESS_VAL(value)  ((Value){VAL_ADDRESS, { .address = value}})
#define OBJ_VAL(object)     ((Value){VAL_OBJ, {.obj = (Obj*)object}})

typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void appendToValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

typedef union {
    AnyValue as;
    Value asValue;
} StoredValue;

void printValue(Value value);
void fprintValue(FILE* op, Value value);

bool is_positive_integer(Value a);
uint32_t as_positive_integer(Value a);

#endif