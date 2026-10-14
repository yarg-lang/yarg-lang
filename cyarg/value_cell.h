#ifndef cyarg_value_cell_h
#define cyarg_value_cell_h

#include "yargtype.h"
#include "value.h"

typedef struct ObjRoutine ObjRoutine;

typedef struct ValueCell {
    Value value;
    ObjConcreteYargType* cellType;
} ValueCell;

typedef struct {
    Value* value;
    const ObjConcreteYargType* cellType;
} ValueCellTarget;

typedef struct {
    void* value;
    const ObjConcreteYargType* type;
    const ObjConcreteYargType* cellType;
} PlacedValueCellTarget;

bool assignToValueCellTarget(ValueCellTarget lhs, Value rhsValue);
bool assignToPlacedValueCellTarget(PlacedValueCellTarget lhs, Value rhsValue);

ValueCellTarget peekCellTarget(ObjRoutine* routine, int distance);

#endif