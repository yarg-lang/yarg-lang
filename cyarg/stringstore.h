//
//  stringstore.h
//  gh.yarg-lang
//
//  Created by dlm on 03/11/2026.
//
#ifndef cyarg_stringstore_h
#define cyarg_stringstore_h

typedef struct StringStore StringStore;
typedef struct ObjString ObjString;

void stringStoreInit(StringStore *);
ObjString *stringStoreStore(StringStore *, ObjString *); // return will be the stored string or an identical one
ObjString *stringStoreGet(StringStore *, char const *, int);
void stringStoreDeleteUnmarked(StringStore *);

#endif // !cyarg_stringstore_h
