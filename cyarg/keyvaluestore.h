//
//  keyvaluestore.h
//  gh.yarg-lang
//
//  Created by dlm on 03/11/2026.
//
#ifndef cyarg_keyvaluestore_h
#define cyarg_keyvaluestore_h

#include <stdbool.h>

typedef struct KeyValueStore KeyValueStore;
typedef struct ObjString ObjString;
typedef struct ValueCell ValueCell;
typedef struct Value Value;

void keyValueStoreInit(KeyValueStore *);
int keyValueStoreCount(KeyValueStore const *);
void keyValueStoreFree(KeyValueStore *); // vm_mutex_enter_blocking()/vm_mutex_exit() must be called around this function
bool keyValueStoreGet(KeyValueStore const *, ObjString const *key, ValueCell *);
bool keyValueStoreGetValue(KeyValueStore const *, ObjString const *key, Value *);
bool keyValueStorePlace(const KeyValueStore *, ObjString const *key, ValueCell **);
bool keyValueStoreSet(KeyValueStore *, ObjString const *key, const ValueCell *);
bool keyValueStoreSetValue(KeyValueStore *, ObjString const *key, Value const *);
void keyValueStoreAddAll(KeyValueStore const *from, KeyValueStore *to);
void keyValueStoreMark(KeyValueStore *);

#endif // !cyarg_keyvaluestore_h
