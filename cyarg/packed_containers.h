#ifndef cyarg_packed_containers_h
#define cyarg_packed_containers_h

#include "value.h"

// Yarg has two container types that can be placed if they contain only placeable elements:
//  - Array (fixed-size sequences of elements)
//  - Struct (collections of named fields, accessed by offset at run time)
// Additionally, the Pointer type is built to support placed values, and it acts as a container
// type that has only a single element.
//
// In order to support a predictable memory layout when placed, with only one container implementation, 
// all instances of structs, arrays and pointers refer to elements that are 'packed'. They exist in 
// the underlying storage as a flat sequence of elements. When not 'placeable', the storage currently
// defaults to a Value, but in future it may support more efficient storage for non-placeable elements.
//
// two examples:
// var uint32[2] a; // an array of two uint32 elements
// 0 +----+
//   |    | a[0]
// 4 +----+
//   |    | a[1]
// 8 +----+
// 
// var struct { uint32 a; uint64 b; }[2] s1; // an array of two structs, each with a uint32 and a uint64 field
//  0 +----+
//    |    | s1[0].a
//  8 +----+        // note alignment due to uint64 field
//    |    | s1[0].b
// 16 +----+
//    |    | s1[1].a
// 24 +----+
//    |    | s1[1].b
// 32 +----+
//
// var struct { uint32 a; b; } s2; // a single struct with a uint32 and a field that can hold any value.
//                 0 +----+
//                   |    | s2.a
//                 4 +----+
//                   |    | s2.b
// sizeof(Value) + 4 +----+
// 
// Note that s2 is not of 'placeable' type, so the memory layout is entirely private to the implementation, 
// and may change.
//
// Both s1 and a are'placeable', and could be used with place:
// place uint32[2] @x10000000 rA;
//     rA[0] has address @x10000000
//     rA[1] has address @x10000004
// place struct { uint32 a; uint64 b; }[2] @x10000000 rS;
//     rS[0].a has address @x10000000
//     rS[0].b has address @x10000008
//     rS[1].a has address @x10000016
//     rS[1].b has address @x10000024
//
// Note the recursion into the elements of arrays and the fields of structs when determining their placement.

typedef struct ObjPackedStorage ObjPackedStorage;

typedef struct ObjPackedContainer {
    Obj obj;
    void* loc;
} ObjPackedContainer;

typedef struct ObjOwningPackedContainer {
    ObjPackedContainer container;
    ObjPackedStorage* storage;
} ObjOwningPackedContainer;

#define AS_PACKEDCONTAINER(value) ((ObjPackedContainer*)AS_OBJ(value))

bool is_uniformarray(Value value);
bool is_pointer(Value value);
bool is_struct(Value value);
bool isUint32Pointer(Value val);

#define IS_UNIFORMARRAY(value) is_uniformarray(value)
#define IS_POINTER(value)      is_pointer(value)
#define IS_STRUCT(value)       is_struct(value)

void markPackedStorage(ObjPackedStorage* storage);
void freePackedStorage(ObjPackedStorage* storage);

Value newPackedValueContainer(const ObjConcreteYargType* type);
Value createPackedValueContainerAt(void* location, const ObjConcreteYargType* type);
Value createPackedContainerFor(ObjPackedContainer* container, size_t offset, const ObjConcreteYargType* type);

Value pinPackedValueContainer(Value packedValueContainer);

Value defaultArrayValue(const ObjConcreteYargType* type);
Value defaultStructValue(const ObjConcreteYargType* type);
Value defaultPointerValue(const ObjConcreteYargType* type);

Value duplicateStruct(Value structVal);
Value duplicateArray(Value arrayVal);
Value duplicatePointer(Value pointer);

// Two useful conveniences - a byte array and a value array.
// byte arrays can create a buffer, or be placed over an existing memory location.
Value allocByteArray(size_t length);
Value createByteArrayAt(uint8_t* location, size_t length);
bool  isByteArray(Value value);
// value arrays can contain any value.
Value allocValueArray(size_t length);

Value arrayElement(Value array, size_t index);
Value pointerDestination(Value pointer);
Value structField(Value structVal, size_t fieldIndex);

Value arrayElementPointer(Value array, size_t index);
Value structFieldPointer(Value structVal, size_t fieldIndex);

bool setArrayElement(Value array, size_t index, Value value);
bool setStructField(Value structVal, size_t fieldIndex, Value value);
bool setPointerDestination(Value pointer, Value value);

void offsetPointerDestination(Value pointer, Value offset);

bool isArrayPointer(Value value);
bool isStructPointer(Value value);

bool arraysEqual(Value a, Value b);
bool structsEqual(Value a, Value b);
bool pointersEqual(Value a, Value b);

bool yt_packs_asValue(const ObjConcreteYargType* type);


#endif // cyarg_packed_containers_h