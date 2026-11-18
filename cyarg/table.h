#ifndef cyarg_table_h
#define cyarg_table_h

#if !defined CYARG_ORDERED_STRING_TREE

#include "value.h"
#include "value_cell.h"
#include "orderedtreetypes.h"
#include "object_gc.h"
#include "yargobject.h"
#include "memory.h"

#include <string.h>

typedef struct ObjString ObjString;

typedef struct Entry {
    ObjString* key;
    Value value;
} Entry;

void initTable(ValueTable* table);
void freeTable(ValueTable* table);
void gc_freeTable(ValueTable* table);
bool tableGet(const ValueTable* table, ObjString* key, Value* value);
bool tableSet(ValueTable* table, ObjString* key, Value value);
void tableRemoveWhite(ValueTable* table);
ObjString* tableFindString(ValueTable* table, const char* chars, int length, uint32_t hash);

void stringStoreInit(StringStore *ss);
ObjString *stringStoreStore(StringStore *ss, ObjString *s);
void stringStoreDeleteUnmarked(StringStore *ss);
ObjString *stringStoreGet(StringStore *ss, char const *cs, int length);

typedef struct EntryCell {
    ObjString* key;
    ValueCell cell;
} EntryCell;

typedef struct KeyValueStore ValueCellTable;

void initCellTable(ValueCellTable* table);
void freeCellTable(ValueCellTable* table);
void gc_freeCellTable(ValueCellTable* table);
bool tableCellGet(ValueCellTable* table, ObjString* key, ValueCell* value);
bool tableCellGetPlace(ValueCellTable* table, ObjString* key, ValueCell** place);
bool tableCellSet(ValueCellTable* table, ObjString* key, ValueCell value);
void tableCellAddAll(ValueCellTable* from, ValueCellTable* to);
ObjString* tableCellFindString(ValueCellTable* table, const char* chars, int length, uint32_t hash);
void tableCellRemoveWhite(ValueCellTable* table);
void markCellTable(ValueCellTable* table);

#endif // !defined CYARG_ORDERED_STRING_TREE
#endif // !cyarg_table_h
