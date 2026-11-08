#ifndef cyarg_object_gc_h
#define cyarg_object_gc_h

#include "value.h"
#include "vm_mutex.h"
typedef struct Obj Obj;

// GC startup
void vm_objects_init(vm_mutex* mutex);

void gc_register_object(Obj* object);

// shutdown, freeing all GC managed objects
void vm_objects_free();

// API for various GC features: temporary root management & pinning of objects

// temporary root management for the garbage collector
void tempRootPush(Value value);
void tempObjRootPush(Obj* object);
Value tempRootPop();

void pinObj(Obj* object);

#endif // cyarg_object_gc_h
