#include "value_cell.h"

#include <assert.h>

#include "common.h"
#include "debug.h"
#include "yargobject.h"

bool assignToValueCellTarget(ValueCellTarget lhs, Value rhsValue) {
    if (lhs.cellType == NULL) {
        noLongerLiteralInt(&rhsValue);
        *lhs.value = rhsValue;
        return true;
    } else {
        Value promoted;
        if (isInitialisableType(lhs.cellType, rhsValue, &promoted)) {
            if (promoted.type == NULL)
            {
                noLongerLiteralInt(&rhsValue);
                *(lhs.value) = rhsValue;
            }
            else
            {
                *(lhs.value) = promoted;
            }
            return true;
        } else {
            return false;
        }
    }
}

bool canInitialiseType(Value targetType, Value rhsValue, Value* resultOut) {
    if (IS_NIL(targetType)) {
        noLongerLiteralInt(&rhsValue);
        *resultOut = rhsValue;
        return true;
    } else {
        Value promoted;
        const ObjConcreteYargType* targetTypeConcrete = AS_YARGTYPE(targetType);
        if (isInitialisableType(targetTypeConcrete, rhsValue, &promoted)) {
            if (promoted.type == NULL)
            {
                noLongerLiteralInt(&rhsValue);
                *resultOut = rhsValue;
            }
            else
            {
                *resultOut = promoted;
            }
            return true;
        } else {
            return false;
        }
    }
}

bool assignToPlacedValueCellTarget(PlacedValueCellTarget lhs, Value rhsValue) {
    assert(lhs.cellType != NULL);

    Value promoted;
    if (isInitialisableType(lhs.cellType, rhsValue, &promoted)) {
        assert(promoted.type != NULL || rhsValue.type != NULL);

        if (promoted.type == NULL) {
            noLongerLiteralInt(&rhsValue);
            assert(is_placeable_type(rhsValue.type));
            setPlacedValue((PlacedValue){.value = lhs.value, .type = lhs.cellType}, rhsValue);
        } else {
            assert(is_placeable_type(promoted.type));
            setPlacedValue((PlacedValue){.value = lhs.value, .type = lhs.cellType}, promoted);
        }

        return true;
    } else {
        return false;
    }
}

void fprintValueCell(FILE* op, ValueCell* cell) {
    FPRINTMSG(op, "<vc:");
    fprintValue(op, cell->value);
    FPRINTMSG(op, "<");
    Value cellTypeValue = cell->cellType ? YARGTYPE_VAL((Obj*)cell->cellType) : NIL_VAL;
    fprintValue(op, cellTypeValue);
    FPRINTMSG(op, ">>");
}
