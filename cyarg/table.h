#ifndef cyarg_table_h
#define cyarg_table_h

#include "common.h"
#include "value.h"
#include "value_cell.h"

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

typedef struct {
    ObjString* key;
    ValueCell cell;
} EntryCell;

typedef struct {
    int count;
    int capacity;
    EntryCell* entries;
} ValueCellTable;

void initCellTable(ValueCellTable* table);
void freeCellTable(ValueCellTable* table);
bool tableCellGet(ValueCellTable* table, ObjString* key, ValueCell* value);
bool tableCellSet(ValueCellTable* table, ObjString* key, ValueCell value);
bool tableCellDelete(ValueCellTable* table, ObjString* key);
void tableCellAddAll(ValueCellTable* from, ValueCellTable* to);
ObjString* tableCellFindString(ValueCellTable* table, const char* chars, int length, uint32_t hash);
void tableCellRemoveWhite(ValueCellTable* table);
void markCellTable(ValueCellTable* table);

#endif