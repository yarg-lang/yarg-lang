#ifndef cyarg_yargstructtype_h
#define cyarg_yargstructtype_h

#include "yargtype.h"
#include "value.h"
#include "table.h"

#include <stddef.h>

typedef struct ObjConcreteYargTypeStruct {
    ObjConcreteYargType core;
    ValueTable field_names;
    size_t* field_indexes;
    ObjConcreteYargType** field_types;
    size_t field_count;
    size_t storage_size;
    size_t alignment;
} ObjConcreteYargTypeStruct;

ObjConcreteYargType* newYargStructType(size_t fieldCount);
size_t addFieldType(ObjConcreteYargTypeStruct* st, size_t index, size_t fieldOffset, ObjConcreteYargType* type, ObjString* name);
size_t addFieldTypeAtOffset(ObjConcreteYargTypeStruct* st, size_t index, ObjConcreteYargType* type, size_t offset, ObjString* name);

bool structFieldIndex(const ObjConcreteYargTypeStruct* type, ObjString* name, size_t* index);
size_t structFieldOffset(const ObjConcreteYargTypeStruct* type, size_t fieldIndex);

#endif