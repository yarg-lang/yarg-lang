#ifndef cyarg_object_dyn_array_h
#define cyarg_object_dyn_array_h

typedef struct Obj Obj;

// Dynamic array of Object pointers. Takes ownership of the passed object before growing the array.
typedef struct DynamicObjArray {
    Obj* stash;
    Obj** objects;
    int objectCapacity;
    int objectCount;
} DynamicObjArray;

void initDynamicObjArray(DynamicObjArray* array);
void freeDynamicObjArray(DynamicObjArray* array);
void gc_freeDynamicObjArray(DynamicObjArray* array);

void appendToDynamicObjArray(DynamicObjArray* array, Obj* obj);

#endif
