#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "yargtype.h"

#define TABLE_MAX_LOAD 0.75

void initTable(ValueTable* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(ValueTable* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash & (capacity - 1);
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // Empty entry.
                return tombstone != NULL ? tombstone : entry;
            } else {
                // We found a tombstone.
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            // We found the key.
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
}

bool tableGet(ValueTable* table, ObjString* key, Value* value) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(ValueTable* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(ValueTable* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(ValueTable* table, ObjString* key) {
    if (table->count == 0) return false;

    // Find the entry.
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // Place a tombstone in the entry.
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void tableAddAll(ValueTable* from, ValueTable* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

ObjString* tableFindString(ValueTable* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash & (table->capacity - 1);
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length &&
                   entry->key->hash == hash &&
                   memcmp(entry->key->chars, chars, length) == 0) {
            // We found it.
            return entry->key;
        }

        index = (index + 1) & (table->capacity - 1);
    }
}

void tableRemoveWhite(ValueTable* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.isMarked) {
            tableDelete(table, entry->key);
        }
    }
}

void markTable(ValueTable* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL) {
            markObject((Obj*)entry->key);
            markValue(entry->value);
        }
    }
}

void initCellTable(ValueCellTable* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeCellTable(ValueCellTable* table) {
    FREE_ARRAY(EntryCell, table->entries, table->capacity);
    initCellTable(table);
}

static EntryCell* findCellEntry(EntryCell* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash & (capacity - 1);
    EntryCell* tombstone = NULL;

    for (;;) {
        EntryCell* entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->cell.value)) {
                // Empty entry.
                return tombstone != NULL ? tombstone : entry;
            } else {
                // We found a tombstone.
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            // We found the key.
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
}

bool tableCellGet(ValueCellTable* table, ObjString* key, ValueCell* value) {
    if (table->count == 0) return false;

    EntryCell* entry = findCellEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->cell;
    return true;
}

bool tableCellGetPlace(ValueCellTable* table, ObjString* key, ValueCell** place) {
    if (table->count == 0) return false;

    EntryCell* entry = findCellEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *place = &entry->cell;
    return true;
}

static void adjustCellCapacity(ValueCellTable* table, int capacity) {
    EntryCell* entries = ALLOCATE(EntryCell, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].cell.value = NIL_VAL;
        entries[i].cell.cellType = NULL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        EntryCell* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        EntryCell* dest = findCellEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->cell= entry->cell;
        table->count++;
    }

    FREE_ARRAY(EntryCell, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableCellSet(ValueCellTable* table, ObjString* key, ValueCell cell) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCellCapacity(table, capacity);
    }

    EntryCell* entry = findCellEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && IS_NIL(entry->cell.value)) table->count++;

    entry->key = key;
    entry->cell = cell;
    return isNewKey;
}

bool tableCellDelete(ValueCellTable* table, ObjString* key) {
    if (table->count == 0) return false;

    // Find the entry.
    EntryCell* entry = findCellEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // Place a tombstone in the entry.
    entry->key = NULL;
    entry->cell.value = BOOL_VAL(true);
    entry->cell.cellType = NULL;
    return true;
}

void tableCellAddAll(ValueCellTable* from, ValueCellTable* to) {
    for (int i = 0; i < from->capacity; i++) {
        EntryCell* entry = &from->entries[i];
        if (entry->key != NULL) {
            tableCellSet(to, entry->key, entry->cell);
        }
    }
}

ObjString* tableCellFindString(ValueCellTable* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash & (table->capacity - 1);
    for (;;) {
        EntryCell* entry = &table->entries[index];
        if (entry->key == NULL) {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->cell.value)) return NULL;
        } else if (entry->key->length == length &&
                   entry->key->hash == hash &&
                   memcmp(entry->key->chars, chars, length) == 0) {
            // We found it.
            return entry->key;
        }

        index = (index + 1) & (table->capacity - 1);
    }
}

void tableCellRemoveWhite(ValueCellTable* table) {
    for (int i = 0; i < table->capacity; i++) {
        EntryCell* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.isMarked) {
            tableCellDelete(table, entry->key);
        }
    }
}

void markCellTable(ValueCellTable* table) {
    for (int i = 0; i < table->capacity; i++) {
        EntryCell* entry = &table->entries[i];
        if (entry->key != NULL) {
            markObject((Obj*)entry->key);
            markValueCell(&entry->cell);
        }
    }
}

void printCellTable(ValueCellTable* table) {
    for (int i = 0; i < table->capacity; i++) {
        EntryCell* entry = &table->entries[i];
        if (entry->key != NULL) {
            printObject(OBJ_VAL((Obj*)entry->key));
            FPRINTMSG(stderr, ":::");
            printValue(entry->cell.value);
            FPRINTMSG(stderr, ":::");
            printType(stderr, entry->cell.cellType);
            FPRINTMSG(stderr, "\n");
        }
    }
}
