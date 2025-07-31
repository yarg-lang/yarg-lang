#ifndef cyarg_table_h
#define cyarg_table_h

#include "common.h"
#include "value.h"

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} ValueTable;

void initTable(ValueTable* table);
void freeTable(ValueTable* table);
bool tableGet(ValueTable* table, ObjString* key, Value* value);
bool tableSet(ValueTable* table, ObjString* key, Value value);
bool tableDelete(ValueTable* table, ObjString* key);
void tableAddAll(ValueTable* from, ValueTable* to);
ObjString* tableFindString(ValueTable* table, const char* chars, int length, uint32_t hash);
void tableRemoveWhite(ValueTable* table);
void markTable(ValueTable* table);

#endif