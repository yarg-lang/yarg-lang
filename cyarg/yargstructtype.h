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
void appendFieldType(ObjConcreteYargTypeStruct* st, ObjConcreteYargType* type, ObjString* name);
void appendFieldTypeAtOffset(ObjConcreteYargTypeStruct* st, ObjConcreteYargType* type, size_t offset, ObjString* name);

bool structFieldIndex(const ObjConcreteYargTypeStruct* type, ObjString* name, size_t* index);
size_t structFieldOffset(const ObjConcreteYargTypeStruct* type, size_t fieldIndex);

#endif