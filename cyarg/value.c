#include <stdio.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "value.h"

void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void appendToValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

void printValue(Value value) {
    fprintValue(stdout, value);
}

void fprintValue(FILE* op, Value value) {
    switch (value.type) {
        case VAL_BOOL:
            FPRINTMSG(op, AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL: FPRINTMSG(op, "nil"); break;
        case VAL_DOUBLE: FPRINTMSG(op, "%#g", AS_DOUBLE(value)); break;
        case VAL_UINTEGER: FPRINTMSG(op, "%u", AS_UINTEGER(value)); break;
        case VAL_INTEGER: FPRINTMSG(op, "%d", AS_INTEGER(value)); break;
        case VAL_ADDRESS: FPRINTMSG(op, "%p", (void*) AS_ADDRESS(value)); break;
        case VAL_OBJ: fprintObject(op, value); break;
    }
}

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL:     return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:      return true;
        case VAL_DOUBLE:   return AS_DOUBLE(a) == AS_DOUBLE(b);
        case VAL_UINTEGER: return AS_UINTEGER(a) == AS_UINTEGER(b);
        case VAL_INTEGER:  return AS_INTEGER(a) == AS_INTEGER(b);
        case VAL_ADDRESS:  return AS_ADDRESS(a) == AS_ADDRESS(b);
        case VAL_OBJ:      return AS_OBJ(a) == AS_OBJ(b);
        default:           return false; // Unreachable.
    }
}

bool is_positive_integer(Value a) {
    if (IS_UINTEGER(a)) {
        return true;
    }
    int32_t aa = AS_INTEGER(a);
    if (IS_INTEGER(a) && aa >= 0) {
        return true;
    }
    return false;
}

uint32_t as_positive_integer(Value a) {
    if (IS_INTEGER(a)) {
        return AS_INTEGER(a);
    }
    return AS_UINTEGER(a);;
}