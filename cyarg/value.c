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
        case VAL_I8: FPRINTMSG(op, "%d", AS_I8(value)); break;
        case VAL_UI8: FPRINTMSG(op, "%u", AS_UI8(value)); break;
        case VAL_INTEGER: FPRINTMSG(op, "%d", AS_INTEGER(value)); break;
        case VAL_UI32: FPRINTMSG(op, "%u", AS_UI32(value)); break;
        case VAL_I64: FPRINTMSG(op, "%lld", AS_I64(value)); break;
        case VAL_UI64: FPRINTMSG(op, "%llu", AS_UI64(value)); break;
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
        case VAL_I8:       return AS_I8(a) == AS_I8(b);
        case VAL_UI8:      return AS_UI8(a) == AS_UI8(b);
        case VAL_INTEGER:  return AS_INTEGER(a) == AS_INTEGER(b);
        case VAL_UI32:     return AS_UI32(a) == AS_UI32(b);
        case VAL_I64:      return AS_I64(a) == AS_I64(b);
        case VAL_UI64:     return AS_UI64(a) == AS_UI64(b);
        case VAL_ADDRESS:  return AS_ADDRESS(a) == AS_ADDRESS(b);
        case VAL_OBJ:      return AS_OBJ(a) == AS_OBJ(b);
        default:           return false; // Unreachable.
    }
}

bool is_positive_integer(Value a) {
    if (IS_UI32(a) || IS_UI8(a)) {
        return true;
    } else if (IS_UI64(a) && AS_UI64(a) <= UINT32_MAX) {
        return true;
    } else if (IS_INTEGER(a) && AS_INTEGER(a) >= 0) {
        return true;
    } else if (IS_I8(a) && AS_I8(a) >= 0) {
        return true;
    } else if (IS_I64(a) && AS_I64(a) >= 0) {
        return true;
    }
    return false;
}

uint32_t as_positive_integer(Value a) {
    if (IS_INTEGER(a)) {
        return AS_INTEGER(a);
    } else if (IS_I8(a)) {
        return AS_I8(a);
    } else if (IS_I64(a) && AS_I64(a) <= UINT32_MAX) {
        return AS_I64(a);
    } else if (IS_UI32(a)) {
        return AS_UI32(a);
    } else if (IS_UI8(a)) {
        return AS_UI8(a);
    } else if (IS_UI64(a) && AS_UI64(a) <= UINT32_MAX) {
        return AS_UI64(a);
    }
    return 0;
}