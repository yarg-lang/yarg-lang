//
//  stringstore.c
//  gh.yarg-lang
//
//  Created by dlm on 03/11/2026.
//

#if defined CYARG_ORDERED_STRING_TREE

#include "stringstore.h"

#include "yargobject.h"
#include "memory.h"

#include <assert.h>
#include <string.h>


void stringStoreInit(StringStore *ss) {
    ss->root = 0;
}

ObjString *stringStoreStore(StringStore *ss, ObjString *newS) {
    assert(newS->sss.l == 0 && newS->sss.l == 0 && newS->sss.t == 0);
//    printf("Add:%s\n", newS->chars);
    ObjString *s = ss->root;
    int cmp;
    ObjString *towardsRoot = 0;
    while (s != 0) {
        towardsRoot = s;
        cmp = strncmp(newS->chars, s->chars, newS->length);
        if (cmp == 0) { // simulate a trailing null terminator in newS
            cmp = '\0' - s->chars[newS->length];
        }
        if (cmp < 0) {
            s = s->sss.l;
        } else if (cmp > 0) {
            s = s->sss.r;
        } else {
            s = 0;
        }
    }
    if (towardsRoot == 0) {
        // empty store attach as root
        ss->root = newS;
    } else {
        if (cmp < 0 && towardsRoot->sss.l == 0) {
            newS->sss.n = 0;
            newS->sss.t = towardsRoot;
            towardsRoot->sss.l = newS;
        } else if (cmp > 0 && towardsRoot->sss.r == 0) {
            newS->sss.n = 0;
            newS->sss.t = towardsRoot;
            towardsRoot->sss.r = newS;
        } else if (cmp < 0) {
            assert(strcmp(newS->chars, towardsRoot->chars) < 0);
            assert(strcmp(towardsRoot->sss.l->chars, newS->chars) < 0);
            newS->sss.t = towardsRoot;
            newS->sss.l = towardsRoot->sss.l;
            towardsRoot->sss.l = newS;
            assert(newS->sss.r == 0);
            // todo - relabel then balance
        } else if (cmp > 0) {
            assert(strcmp(newS->chars, towardsRoot->chars) > 0);
            assert(strcmp(towardsRoot->sss.r->chars, newS->chars) > 0);
            newS->sss.t = towardsRoot;
            newS->sss.r = towardsRoot->sss.r;
            towardsRoot->sss.r = newS;
            assert(newS->sss.l == 0);
            // todo - relabel then balance
        } else {
            // one exists
            assert(strcmp(towardsRoot->chars, newS->chars) == 0);
            newS = towardsRoot;
        }
    }
    return newS;
}

ObjString *stringStoreGet(StringStore *ss, char const *chars, int length) {
    ObjString *s = ss->root;
    while (s != 0) {
        int cmp = strncmp(chars, s->chars, length);
        if (cmp == 0) { // simulate a trailing null terminator in chars
            cmp = '\0' - s->chars[length];
        }
        if (cmp < 0) {
            s = s->sss.l;
        } else if (cmp > 0) {
            s = s->sss.r;
        } else {
            break;
       }
    }
    return s;
}

static void removeNodeIfUnmarked(StringStore *ss, ObjString *);

// Note this doesn’t visit nodes in sort order.
// Nodes are investigated:
// On the way in if they are leaf nodes
// On the way back after both branches have been visited
void stringStoreDeleteUnmarked(StringStore *ss) {
//    printf("parse\n");
    ObjString *s = ss->root;
    while (s != 0) { // going in to the leaves
        if (s->sss.l != 0) {
            s->sss.p = 0;
            s = s->sss.l;
        } else if (s->sss.r != 0) {
            s->sss.p = 1;
            s = s->sss.r;
        } else {
            ObjString *t = s->sss.t;
            removeNodeIfUnmarked(ss, s); // leaf
            while (t != 0) { // going back toward root
                if (t->sss.p == 0 && t->sss.r != 0) { // go right in to right branch
                    t->sss.p = 1;
                    t = t->sss.r;
                    break;
                } else {
                    s = t;
                    t = t->sss.t;
                    removeNodeIfUnmarked(ss, s);
                }
            }
            s = t;
        }
    }
}

// Node to be deleted (s) three cases
//      Left             Right           Root
//        t               t                s
//       s                 s             l   r
//     l   r             l   r

// in Left case
//      if l == 0 => t.l = r
//      elif r == 0 => t.l = l
//      else t.l = longest(l, r); longest.m.m.m = shortest -- m is the same child as shortest
// in Right case
//      if l == 0 => t.r = r
//      elif r == 0 => t.r = l
//      else t.r = longest(l, r); longest.m.m.m = shortest -- m is the same child as shortest
// in Root case
//      if l == 0 => ss.root = r
//      elif r == 0 => ss.root = l
//      else ss.root = longest(l, r); longest.m.m.m = shortest -- m is the same child as shortest
int ssss=0;
void removeNodeIfUnmarked(StringStore *ss, ObjString *s) {
//    printf("Node:%d:%s\n", s->obj.isMarked, s->chars);
    if (!s->obj.isMarked) {
//        if (strcmp("seek", s->chars) == 0) {
//            ssss++;
//        }
        if (s->sss.t == 0) {
            ss->root = s->sss.l != 0 ? s->sss.l : s->sss.r; // todo if both choose max(n)
            if (ss->root != 0) {
                ss->root->sss.t = 0;
            }
            if (s->sss.l != 0 && s->sss.r != 0) { // todo if both choose max(n)
                ObjString *attachToRightEndOf = s->sss.l;
                while (attachToRightEndOf->sss.r != 0) {
                    attachToRightEndOf = attachToRightEndOf->sss.r;
                }
                attachToRightEndOf->sss.r = s->sss.r;
                if (s->sss.r != 0) {
                    s->sss.r->sss.t = attachToRightEndOf;
                }
            }
        } else if (s == s->sss.t->sss.l) {
            s->sss.t->sss.l = s->sss.l;
            if (s->sss.l == 0) {
                s->sss.t->sss.l = s->sss.r;
                if (s->sss.r != 0 ) {
                    s->sss.r->sss.t = s->sss.t;
                }
            } else {
                s->sss.l->sss.t = s->sss.t;
                ObjString *attachToRightEndOf = s->sss.l;
                while (attachToRightEndOf->sss.r != 0) {
                    attachToRightEndOf = attachToRightEndOf->sss.r;
                }
                attachToRightEndOf->sss.r = s->sss.r;
                if (s->sss.r != 0) {
                    s->sss.r->sss.t = attachToRightEndOf;
                }
            }
        } else {
            assert(s == s->sss.t->sss.r);
            s->sss.t->sss.r = s->sss.r;
            if (s->sss.r == 0) {
                s->sss.t->sss.r = s->sss.l;
                if (s->sss.l != 0 ) {
                    s->sss.l->sss.t = s->sss.t;
                }
            } else {
                s->sss.r->sss.t = s->sss.t;
                ObjString *attachToLeftEndOf = s->sss.r;
                while (attachToLeftEndOf->sss.l != 0) {
                    attachToLeftEndOf = attachToLeftEndOf->sss.l;
                }
                attachToLeftEndOf->sss.l = s->sss.l;
                if (s->sss.l != 0) {
                    s->sss.l->sss.t = attachToLeftEndOf;
                }
            }
        }
    }
}

#endif // defined CYARG_ORDERED_STRING_TREE
