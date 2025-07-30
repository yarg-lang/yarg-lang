#ifndef cyarg_value_cell_h
#define cyarg_value_cell_h

#include "value.h"

typedef struct {
    Value value;
    Value type;
} ValueCell;

#endif