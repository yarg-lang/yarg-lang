#ifndef cyarg_table_h
#define cyarg_table_h

#if !defined CYARG_ORDERED_STRING_TREE

#include "value.h"
#include "value_cell.h"
#include "orderedtreetypes.h"
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
bool tableGet(const ValueTable* table, ObjString* key, Value* value);
bool tableSet(ValueTable* table, ObjString* key, Value value);
void tableRemoveWhite(ValueTable* table);
ObjString* tableFindString(ValueTable* table, const char* chars, int length, uint32_t hash);

inline static void stringStoreInit(StringStore *ss) {
    return initTable(ss);
}
inline static ObjString *stringStoreStore(StringStore *ss, ObjString *s) {
    char const *key = s->chars;
    int length = 0;
    uint32_t hash = 2166136261u;
    while (*key != 0) {
        hash ^= (uint8_t)*key++;
        hash += 16777619;
        length++;
    }
    s->hash = hash;

    tempRootPush(STRING_VAL(s));
    tableSet(ss, s, NIL_VAL);
    tempRootPop();
    return s;
}
inline static void stringStoreDeleteUnmarked(StringStore *ss) {
    return tableRemoveWhite(ss);
}
inline static ObjString *stringStoreGet(StringStore *ss, char const *cs, int length) {
    char const *key = cs;
    uint32_t hash = 2166136261u;
    while (*key != 0) {
        hash ^= (uint8_t)*key++;
        hash += 16777619;
    }
    ObjString* s = tableFindString(ss, cs, length, hash);
    return s;
}

typedef struct EntryCell {
    ObjString* key;
    ValueCell cell;
} EntryCell;

typedef struct KeyValueStore ValueCellTable;

void initCellTable(ValueCellTable* table);
void freeCellTable(ValueCellTable* table);
bool tableCellGet(ValueCellTable* table, ObjString* key, ValueCell* value);
bool tableCellGetPlace(ValueCellTable* table, ObjString* key, ValueCell** place);
bool tableCellSet(ValueCellTable* table, ObjString* key, ValueCell value);
void tableCellAddAll(ValueCellTable* from, ValueCellTable* to);
ObjString* tableCellFindString(ValueCellTable* table, const char* chars, int length, uint32_t hash);
void tableCellRemoveWhite(ValueCellTable* table);
void markCellTable(ValueCellTable* table);

inline static void keyValueStoreInit(KeyValueStore *kvs) {
    return initCellTable(kvs);
}
inline static int keyValueStoreCount(KeyValueStore const *kvs) {
    return kvs->count;
}
inline static void keyValueStoreFree(KeyValueStore *kvs) {
    return freeCellTable(kvs);
}
inline static bool keyValueStoreGet(KeyValueStore const *kvs, ObjString const *key, ValueCell *vc) {
    return tableCellGet((KeyValueStore *)kvs, (ObjString *)key, vc);
}
inline static bool keyValueStoreGetValue(KeyValueStore const *kvs, ObjString const *key, Value *v) {
    ValueCell vc;
    bool r = tableCellGet((KeyValueStore *)kvs, (ObjString *)key, &vc);
    if (r) {
        *v = vc.value;
    }
    return r;
}
inline static bool keyValueStorePlace(KeyValueStore const *kvs, ObjString const *key, ValueCell **vc) {
    return tableCellGetPlace((KeyValueStore *)kvs, (ObjString *)key, vc);
}
inline static bool keyValueStoreSet(KeyValueStore *kvs, ObjString const *key, ValueCell const *vc) {
    return tableCellSet(kvs, (ObjString *)key, *vc);
}
inline static bool keyValueStoreSetValue(KeyValueStore *kvs, ObjString const *key, Value const *v) {
    ValueCell vc = { .value = *v };
    return tableCellSet(kvs, (ObjString *)key, vc);
}
inline static void keyValueStoreAddAll(KeyValueStore const *from, KeyValueStore *to) {
    return tableCellAddAll((KeyValueStore *)from, to);
}
inline static void keyValueStoreMark(KeyValueStore *kvs) {
    return markCellTable(kvs);
}

#endif // !defined CYARG_ORDERED_STRING_TREE
#endif // !cyarg_table_h
