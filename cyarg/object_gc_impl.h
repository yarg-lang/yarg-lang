#ifndef cyarg_object_gc_impl_h
#define cyarg_object_gc_impl_h

#include "value.h"
#include "object.h"

// functions that are internal to heap management & gc operations.

typedef struct ObjSyncGroup ObjSyncGroup;
void markSyncGroup(ObjSyncGroup* group);
void gc_freeSyncGroup(ObjSyncGroup* group);

typedef struct ObjChannelContainer ObjChannelContainer;
void markChannel(ObjChannelContainer* channel);
void gc_freeChannelObject(ObjChannelContainer* channel);

typedef struct ObjScanner ObjScanner;
void markScanner(ObjScanner* scanner);

typedef struct ObjParser ObjParser;
void markParser(ObjParser* parser);
void gc_freeParser(ObjParser* parser);

void markVMRoots();
void markCompilerRoots();
void vmRemoveUnmarkedRoots();

void gc_objects(size_t size_requested);

typedef struct ObjFunction ObjFunction;
typedef struct ValueCell ValueCell;
typedef struct DynamicObjArray DynamicObjArray;

void gc_free(void* ptr);
void markValue(Value value);
void markValueCell(ValueCell* value);
void markObject(Obj* object);
void markDynamicObjArray(DynamicObjArray* array);
void markFunction(ObjFunction* function);

#endif // cyarg_object_gc_impl_h
