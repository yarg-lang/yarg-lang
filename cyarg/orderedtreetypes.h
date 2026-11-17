//
//  orderedtreetypes.h
//  gh.yarg-lang
//
//  Created by dlm on 17/11/2026.
//

#ifndef cyarg_orderedtreetypes_h
#define cyarg_orderedtreetypes_h

#include "object.h"

#if defined CYARG_ORDERED_STRING_TREE

typedef struct ValueCellStoreNode ValueCellStoreNode;

typedef struct ObjString {
    Obj obj;
    char *chars; // vm_alloc’d
    uint32_t length;
    struct {
        struct ObjString *t; // toward root or zero if root
        struct ObjString *l; // prior
        struct ObjString *r; // subsequent
        int16_t n; // distance to leaf: 0 if leaf, max(l->n, r->n) + 1 otherwise
        uint16_t p : 1; // 0 if parsing/ed left, 1 if parsing/ed right
    } sss;
} ObjString;

typedef struct KeyValueStore {
    ValueCellStoreNode *root;
} KeyValueStore;

typedef struct StringStore {
    ObjString *root;
} StringStore;

#else

typedef struct Entry Entry;
typedef struct EntryCell EntryCell;

typedef struct ObjString {
    Obj obj;
    char *chars; // vm_alloc’d
    uint32_t length;
    uint32_t hash;
} ObjString;

typedef struct StringStore {
    int count;
    int capacity;
    Entry* entries;
} StringStore, ValueTable;

typedef struct KeyValueStore {
    int count;
    int capacity;
    EntryCell* entries;
} KeyValueStore, ValueCellTable;

#endif // defined CYARG_ORDERED_STRING_TREE

#endif // !cyarg_orderedtreetypes_h
