#ifndef cyarg_value_cell_h
#define cyarg_value_cell_h

#include "value.h"

typedef struct {
    Value value;
    Value type;
} ValueCell;

typedef struct {
    Value* value;
    Value* type;
} ValueCellTarget;

typedef struct {
    StoredValue* storedValue;
    Value* type;
} StoredValueCellTarget;

#endif