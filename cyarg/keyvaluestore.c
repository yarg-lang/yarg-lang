//
//  keyvaluestore.c
//  gh.yarg-lang
//
//  Created by dlm on 03/11/2026.
//

#if defined CYARG_ORDERED_STRING_TREE

#include "keyvaluestore.h"

#include "yargobject.h"
#include "value_cell.h"
#include "memory.h"

#include <assert.h>
#include <string.h>

typedef struct ValueCellStoreNode { // todo move into valuecellstore.c
    ValueCell cell;
    ObjString const *key;

    struct ValueCellStoreNode *t; // toward root or zero if root
    struct ValueCellStoreNode *l; // prior
    struct ValueCellStoreNode *r; // subsequent
    int16_t n; // distance to leaf: 0 if leaf, max(l->n, r->n) + 1 otherwise
    uint16_t p : 1; // 0 if parsing/ed left, 1 if parsing/ed right
} ValueCellStoreNode;

static ValueCellStoreNode *kvsGet(KeyValueStore const *, char const *);
static void markNode(ValueCellStoreNode *);

void keyValueStoreInit(KeyValueStore *kvs) {
    kvs->root = 0;
}

// Note this doesn’t visit nodes in sort order.
// Nodes are investigated:
// On the way in if they are leaf nodes
// On the way back after both branches have been visited
int keyValueStoreCount(KeyValueStore const *kvs) {
    int r = 0;
    ValueCellStoreNode *s = kvs->root;
    while (s != 0) { // going in to the leaves
        if (s->l != 0) {
            s->p = 0;
            s = s->l;
        } else if (s->r != 0) {
            s->p = 1;
            s = s->r;
        } else {
            ValueCellStoreNode *t = s->t;
            r++;

            while (t != 0) { // going back toward root
                if (t->p == 0 && t->r != 0) { // go right in to right branch
                    t->p = 1;
                    t = t->r;
                    break;
                } else {
                    s = t;
                    t = t->t;
                    r++;
                }
            }
            s = t;
        }
    }
    return r;
}

// Note this doesn’t visit nodes in sort order.
// Nodes are investigated:
// On the way in if they are leaf nodes
// On the way back after both branches have been visited
void keyValueStoreFree(KeyValueStore *kvs) {
    int n = keyValueStoreCount(kvs); // todo:debug only
    ValueCellStoreNode *s = kvs->root;
    while (s != 0) { // going in to the leaves
        if (s->l != 0) {
            s->p = 0;
            s = s->l;
        } else if (s->r != 0) {
            s->p = 1;
            s = s->r;
        } else {
            ValueCellStoreNode *t = s->t;
            gc_free(s); n--;

            while (t != 0) { // going back toward root
                if (t->p == 0 && t->r != 0) { // go right in to right branch
                    t->p = 1;
                    t = t->r;
                    break;
                } else {
                    s = t;
                    t = t->t;
                    gc_free(s); n--;
                }
            }
            s = t;
        }
    }
    assert(n == 0);
}

bool keyValueStoreGet(KeyValueStore const *kvs, ObjString const *key, ValueCell *c) {
    ValueCellStoreNode *n = kvsGet(kvs, key->chars);
    if (n != 0) {
        *c = n->cell;
    }
    return (n != 0);
}

bool keyValueStoreGetValue(KeyValueStore const *kvs, ObjString const *key, Value *v) {
    ValueCellStoreNode *n = kvsGet(kvs, key->chars);
    if (n != 0) {
        *v = n->cell.value;
    }
    return (n != 0);
}

bool keyValueStorePlace(KeyValueStore const *kvs, ObjString const *key, ValueCell **c) {
    ValueCellStoreNode *n = kvsGet(kvs, key->chars);
    if (n != 0) {
        *c = &n->cell;
    }
    return (n != 0);
}

bool keyValueStoreSet(KeyValueStore *kvs, ObjString const *key, ValueCell const *v) {
    if (strcmp(key->chars, "reps") == 0) {
        v = v;
    }
    ValueCellStoreNode *newN = ALLOCATE(ValueCellStoreNode, 1);
    newN->l = newN->r = newN->t = 0;

    ValueCellStoreNode *n = kvs->root;
    int cmp;
    ValueCellStoreNode *towardsRoot = 0;
    while (n != 0) {
        towardsRoot = n;
        cmp = strcmp(key->chars, n->key->chars);
        if (cmp < 0) {
            n = n->l;
        } else if (cmp > 0) {
            n = n->r;
        } else {
            n = 0;
        }
    }
    bool exists = false;
    if (towardsRoot == 0) {
        // empty store attach as root
        kvs->root = newN;
    } else {
        if (cmp < 0 && towardsRoot->l == 0) {
            newN->n = 0;
            newN->t = towardsRoot;
            towardsRoot->l = newN;
        } else if (cmp > 0 && towardsRoot->r == 0) {
            newN->n = 0;
            newN->t = towardsRoot;
            towardsRoot->r = newN;
        } else if (cmp < 0) {
            assert(strcmp(key->chars, towardsRoot->key->chars) < 0);
            assert(strcmp(towardsRoot->l->key->chars, key->chars) < 0);
            newN->t = towardsRoot;
            newN->l = towardsRoot->l;
            towardsRoot->l = newN;
            assert(newN->r == 0);
            // todo - relabel then balance
        } else if (cmp > 0) {
            assert(strcmp(key->chars, towardsRoot->key->chars) > 0);
            assert(strcmp(towardsRoot->r->key->chars, key->chars) > 0);
            newN->t = towardsRoot;
            newN->r = towardsRoot->r;
            towardsRoot->r = newN;
            assert(newN->l == 0);
            // todo - relabel then balance
        } else {
            // one exists
            vm_free(newN);
            assert(strcmp(towardsRoot->key->chars, key->chars) == 0);
            newN = towardsRoot;
            exists = true;
        }
    }
    newN->key = key;
    newN->cell = *v;
    return !exists;
}

bool keyValueStoreSetValue(KeyValueStore *kvs, ObjString const *key, Value const *v) {
    return keyValueStoreSet(kvs, key, &(ValueCell){ .value = *v });
}

// Note this doesn’t visit nodes in sort order.
// Nodes are investigated:
// On the way in if they are leaf nodes
// On the way back after both branches have been visited
void keyValueStoreAddAll(KeyValueStore const *from, KeyValueStore *to) {
    ValueCellStoreNode *s = from->root;
    while (s != 0) { // going in to the leaves
        if (s->l != 0) {
            s->p = 0;
            s = s->l;
        } else if (s->r != 0) {
            s->p = 1;
            s = s->r;
        } else {
            ValueCellStoreNode *t = s->t;
            keyValueStoreSet(to, s->key, &s->cell); // leaf
            while (t != 0) { // going back toward root
                if (t->p == 0 && t->r != 0) { // go right in to right branch
                    t->p = 1;
                    t = t->r;
                    break;
                } else {
                    s = t;
                    t = t->t;
                    keyValueStoreSet(to, s->key, &s->cell);
                }
            }
            s = t;
        }
    }
}

// Note this doesn’t visit nodes in sort order.
// Nodes are investigated:
// On the way in if they are leaf nodes
// On the way back after both branches have been visited
void keyValueStoreMark(KeyValueStore *kvs) {
    ValueCellStoreNode *s = kvs->root;
    while (s != 0) { // going in to the leaves
        if (s->l != 0) {
            s->p = 0;
            s = s->l;
        } else if (s->r != 0) {
            s->p = 1;
            s = s->r;
        } else {
            ValueCellStoreNode *t = s->t;
            markNode(s); // leaf
            while (t != 0) { // going back toward root
                if (t->p == 0 && t->r != 0) { // go right in to right branch
                    t->p = 1;
                    t = t->r;
                    break;
                } else {
                    s = t;
                    t = t->t;
                    markNode(s);
                }
            }
            s = t;
        }
    }
}

ValueCellStoreNode *kvsGet(KeyValueStore const *kvs, char const *s) {
    if (strcmp(s, "reps") == 0) {
        s = s;
    }
    ValueCellStoreNode *n = kvs->root;
    while (n != 0) {
        int cmp = strcmp(s, n->key->chars);
        if (cmp < 0) {
            n = n->l;
        } else if (cmp > 0) {
            n = n->r;
        } else {
            break;
        }
    }
    return n;
}

void markNode(ValueCellStoreNode *n) {
    markObject((Obj*)n->key);
    markValueCell(&n->cell);
}

#endif // defined CYARG_ORDERED_STRING_TREE
