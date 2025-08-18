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
    Obj* obj;
} AnyValue;

#ifdef NAN_BOXING

/*
 * See Crafting Interpreters, NaN Boxing for the origins of this code.
 * https://craftinginterpreters.com/optimization.html#nan-boxing
 * 
 * For Yarg, we have two distinct worlds. The device/microcontroller builds 
 * that are 32 bit, and our host which is a 64 bit machine.
 * 
 * This appears most in pointers, where the original implementation claims 'at
 * most' 48 bits are typically used in modern 64 bit chipsets.
 * 
 * For simplicity, Yarg only supports 32 bit integers (signed or unsigned)
 * and these will appear in the lower 32 bits of our 64bit Value type.
 * 
 * Our 'Quiet NaN' mask is designed to include an Intel flag that is presumably 
 * used by 64bit Intel compilers & runtimes. So we have the sign bit + the 
 * lower 50 bits to use for type information and payloads.
 * 
 * Our sign bit signals the presence of a 48 bit pointer. There are two more bits
 * we use to signal type information.
 * With 0's outside of QNAN we have a payload space for value tags (currently only 
 * three). The three values are NIL, TRUE and FALSE.
 * With bit 48 set we signal the presence of an unsigned integer in the lower 32 
 * bits, and bit 49 signals a signed integer there.
 */

#define SIGN_BIT      ((uint64_t)0x8000000000000000)
#define QNAN          ((uint64_t)0x7ffc000000000000)
#define UINTEGER_TAG  ((uint64_t)0x0001000000000000)
#define INTEGER_TAG   ((uint64_t)0x0002000000000000)
#define PTR64_48_MASK ((uint64_t)0x0000FFFFFFFFFFFF)
#define L32_MASK      ((uint64_t)0x00000000FFFFFFFF)


#define TAG_NIL   1 // 01.
#define TAG_FALSE 2 // 10.
#define TAG_TRUE  3 // 11.

typedef uint64_t Value;

#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)
#define IS_NIL(value)       ((value) == NIL_VAL)
#define IS_DOUBLE(value)    (((value) & QNAN) != QNAN)
#define IS_UINTEGER(value) \
    (((value) & (QNAN | UINTEGER_TAG)) == (QNAN | UINTEGER_TAG))
#define IS_INTEGER(value) \
    (((value) & (QNAN | INTEGER_TAG)) == (QNAN | INTEGER_TAG))
#define IS_OBJ(value) \
    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)      ((value) == TRUE_VAL)
#define AS_DOUBLE(value)    valueToDbl(value)
#define AS_UINTEGER(value) \
     ((uint32_t)((uint64_t)(value) & (L32_MASK)))
#define AS_INTEGER(value) \
     ((uint32_t)((uint64_t)(value) & (L32_MASK)))
#define AS_OBJ(value) \
     ((Obj*)(uintptr_t)((value) & (PTR64_48_MASK)))

#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL))
#define DOUBLE_VAL(dbl) dblToValue(dbl)
#define UINTEGER_VAL(uinteger) \
    (Value)(QNAN | UINTEGER_TAG | (L32_MASK & (uint64_t)(uinteger)))
#define INTEGER_VAL(integer) \
    (Value)(QNAN | INTEGER_TAG | (L32_MASK & (uint64_t)(integer)))
#define OBJ_VAL(obj) \
    (Value)(SIGN_BIT | QNAN | (PTR64_48_MASK & (uint64_t)(uintptr_t)(obj)))

static inline double valueToDbl(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

static inline Value dblToValue(double dbl) {
    Value value;
    memcpy(&value, &dbl, sizeof(double));
    return value;
}

#else 

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_DOUBLE,
    VAL_UINTEGER,
    VAL_INTEGER,
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
#define IS_OBJ(value)      ((value).type == VAL_OBJ)

#define AS_OBJ(value)      ((value).as.obj)
#define AS_BOOL(value)     ((value).as.boolean)
#define AS_UINTEGER(value) ((value).as.uinteger)
#define AS_INTEGER(value)  ((value).as.integer)
#define AS_DOUBLE(value)   ((value).as.dbl)

#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value }})
#define NIL_VAL             ((Value){VAL_NIL, {.integer = 0 }})
#define DOUBLE_VAL(value)   ((Value){VAL_DOUBLE, {.dbl = value }})
#define UINTEGER_VAL(value) ((Value){VAL_UINTEGER, {.uinteger = value }})
#define INTEGER_VAL(value)  ((Value){VAL_INTEGER, {.integer = value }})
#define OBJ_VAL(object)     ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#endif

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