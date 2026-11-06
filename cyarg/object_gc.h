#ifndef cyarg_object_gc_h
#define cyarg_object_gc_h

#include "value.h"
typedef struct Obj Obj;

// API for various GC features: temporary root management & pinning of objects

// temporary root management for the garbage collector
void tempRootPush(Value value);
void tempObjRootPush(Obj* object);
Value tempRootPop();

void pinObj(Obj* object);

#endif // cyarg_object_gc_h
