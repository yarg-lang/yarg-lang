#include "packed_containers.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "common.h"
#include "memory.h"
#include "yargtype.h"
#include "yargstructtype.h"
#include "value_cell.h"

typedef struct ObjPackedStorage {
    Obj obj;
    size_t storage_size;
    size_t value_elements;
    Value** values;
    uint8_t storage[];
} ObjPackedStorage;

ObjPackedStorage* newPackedStorage(const ObjConcreteYargType* type);

size_t yt_packable_element_count(const ObjConcreteYargType* type) {
    if (type && type->yt == TypeArray) {
        const ObjConcreteYargTypeArray* arrayType = (const ObjConcreteYargTypeArray*) type;
        return arrayType->cardinality * yt_packable_element_count(arrayType->element_type);
    } else if (type && type->yt == TypeStruct) {
        const ObjConcreteYargTypeStruct* structType = (const ObjConcreteYargTypeStruct*) type;
        size_t total = 0;
        for (size_t i = 0; i < structType->field_count; i++) {
            total += yt_packable_element_count(structType->field_types[i]);
        }
        return total;
    } else {
        return 1;
    }
}

size_t yt_packable_value_element_count(const ObjConcreteYargType* type) {
    if (type && type->yt == TypeArray) {
        const ObjConcreteYargTypeArray* arrayType = (const ObjConcreteYargTypeArray*) type;
        return arrayType->cardinality * yt_packable_value_element_count(arrayType->element_type);
    } else if (type && type->yt == TypeStruct) {
        const ObjConcreteYargTypeStruct* structType = (const ObjConcreteYargTypeStruct*) type;
        size_t total = 0;
        for (size_t i = 0; i < structType->field_count; i++) {
            total += yt_packable_value_element_count(structType->field_types[i]);
        }
        return total;
    } else if (yt_packs_asValue(type)) {
        return 1;
    } else {
        return 0;
    }
}

typedef struct FlattenedElements {
    size_t offset;
    const ObjConcreteYargType* type;
} FlattenedElements;

size_t yt_flatten_types(FlattenedElements* elements, size_t start_cursor, size_t start_offset, const ObjConcreteYargType* rootType) {
    size_t cursor = start_cursor;
    if (rootType && rootType->yt == TypeArray) {
        const ObjConcreteYargTypeArray* arrayType = (const ObjConcreteYargTypeArray*) rootType;
        for (size_t i = 0; i < arrayType->cardinality; i++) {
            size_t elementOffset = arrayElementOffset(arrayType, i);
            cursor = yt_flatten_types(elements, cursor, start_offset + elementOffset, arrayType->element_type);
        }
    } else if (rootType && rootType->yt == TypeStruct) {
        const ObjConcreteYargTypeStruct* structType = (const ObjConcreteYargTypeStruct*) rootType;
        for (size_t i = 0; i < structType->field_count; i++) {
            size_t fieldOffset = structFieldOffset(structType, i);
            cursor = yt_flatten_types(elements, cursor, start_offset + fieldOffset, structType->field_types[i]);
        }
    } else {
        elements[cursor++] = (FlattenedElements){.offset = start_offset, .type = rootType};
    }
    return cursor;
}

ObjPackedStorage* newPackedStorage(const ObjConcreteYargType* type) {
    size_t storage_required = yt_sizeof_type_storage(type);
    ObjPackedStorage* storage = (ObjPackedStorage*) allocateObject(sizeof(ObjPackedStorage) + storage_required, OBJ_PACKEDSTORAGE);
    storage->storage_size = storage_required;
    tempObjRootPush((Obj*)storage);

    size_t packed_elements = yt_packable_element_count(type);
    FlattenedElements* elements = ALLOCATE(FlattenedElements, packed_elements);
    size_t flattened_count = yt_flatten_types(elements, 0, 0, type);
    assert(flattened_count == packed_elements);

    size_t value_elements = yt_packable_value_element_count(type);
    assert(value_elements <= packed_elements);

    storage->values = ALLOCATE(Value*, value_elements);
    for (size_t i = 0; i < packed_elements; i++) {        
        if (yt_packs_asValue(elements[i].type)) {
            Value* valueAddr = (Value*)((uintptr_t)&storage->storage[0] + elements[i].offset);
            assert(storage->value_elements < value_elements);
            storage->values[storage->value_elements++] = valueAddr;
            Value elementType = elements[i].type ? YARGTYPE_VAL(elements[i].type) : NIL_VAL;
            *valueAddr = defaultValue(elementType);
        }
    }

    vm_free(elements);
    tempRootPop();
    return storage;
}

void markPackedStorage(ObjPackedStorage* storage) {
    for (size_t i = 0; i < storage->value_elements; i++) {
        if (storage->values[i] != NULL) {
            markValue(*storage->values[i]);
        }
    }
}

void gc_freePackedStorage(ObjPackedStorage* storage) {
    gc_free(storage->values);
}

bool is_uniformarray(Value value) {
    if (value.type == NULL) return false;
    return value.type->yt == TypeArray;
}
bool is_pointer(Value value) {
    if (value.type == NULL) return false;
    return value.type->yt == TypePointer;
}

bool is_struct(Value value) {
    if (value.type == NULL) return false;
    return value.type->yt == TypeStruct;
}

bool yt_packs_asValue(const ObjConcreteYargType* type) {
    if (type == NULL) {
        assert(yt_sizeof_type_storage(type) == sizeof(Value));
        return true;
    } else {
        switch (type->yt) {
        case TypeBool:
            assert(yt_sizeof_type_storage(type) == sizeof(Value));
            return true;
        case TypeDouble:
            return false;
        case TypeInt8:
            return false;
        case TypeUint8:
            return false;
        case TypeInt16:
            return false;
        case TypeUint16:
            return false;
        case TypeInt32:
            return false;
        case TypeUint32:
            return false;
        case TypeInt64:
            return false;
        case TypeUint64:
            return false;
        case TypeAddress:
            return false;
        case TypeStruct:
            return false;
        case TypeArray:
            return false;
        case TypePointer:
            return false;
        
        case TypeInt:
        case TypeString:
        case TypeClass:
        case TypeInstance:
        case TypeFunction:
        case TypeRoutine:
        case TypeChannel:
        case TypeSyncGroup:
        case TypeMap:
        case TypeYargType:
            assert(yt_sizeof_type_storage(type) == sizeof(Value));
            return true;
        }
    }
}


bool isUint32Pointer(Value val) {
    if (IS_POINTER(val)) {
        const ObjConcreteYargTypePointer* pointer = (const ObjConcreteYargTypePointer*) val.type;
        if (pointer->target_type) {
            return pointer->target_type->yt == TypeUint32;
        }
    }
    return false;
}

bool arraysEqual(Value a, Value b) {
    if (!typesEqual(a.type, b.type)) return false;
    if (a.as.obj == b.as.obj) return true;
    return false;
}

bool structsEqual(Value a, Value b) {
    if (!typesEqual(a.type, b.type)) return false;
    if (a.as.obj == b.as.obj) return true;
    return false;
}

bool pointersEqual(Value a, Value b) {
    if (!typesEqual(a.type, b.type)) return false;
    ObjPackedContainer* container_a = AS_PACKEDCONTAINER(a);
    ObjPackedContainer* container_b = AS_PACKEDCONTAINER(b);
    return container_a->loc == container_b->loc;
}

Value defaultArrayValue(const ObjConcreteYargType* type) {

    return newPackedValueContainer(type);
}

Value defaultStructValue(const ObjConcreteYargType* type) {

    return newPackedValueContainer(type);
}

Value defaultPointerValue(const ObjConcreteYargType* type) {

    return NIL_VAL;
}

Value duplicateArray(Value arrayVal) {
    return arrayVal; // TODO: revisit when arrays become value types
}

Value duplicateStruct(Value rhsStructVal) {

    ObjOwningPackedContainer* container = ALLOCATE_OBJ(ObjOwningPackedContainer, OBJ_PACKEDCONTAINER);
    tempObjRootPush((Obj*)container);
    const ObjConcreteYargType* type = rhsStructVal.type;
    ObjPackedStorage* storage = newPackedStorage(type);
    tempObjRootPush((Obj*)storage);
    ObjPackedContainer* rhsContainer = AS_PACKEDCONTAINER(rhsStructVal);

    memcpy(&storage->storage[0], rhsContainer->loc, storage->storage_size);

    container->storage = storage;
    container->container.loc = &container->storage->storage[0];
    tempRootPop();
    tempRootPop();
    return TYPED_VAL(container, type);
}

Value duplicatePointer(Value pointer) {
    return pointer;
}

Value newPackedValueContainer(const ObjConcreteYargType* type) {
    ObjOwningPackedContainer* container = ALLOCATE_OBJ(ObjOwningPackedContainer, OBJ_PACKEDCONTAINER);
    tempObjRootPush((Obj*)container);
    ObjPackedStorage* storage = newPackedStorage(type);
    container->storage = storage;
    container->container.loc = &container->storage->storage[0];
    tempRootPop();
    return TYPED_VAL(container, type);
}

Value newPackedValueContainerFor(ObjOwningPackedContainer* owningContainer, size_t offset, const ObjConcreteYargType* type) {
    ObjOwningPackedContainer* newContainer = ALLOCATE_OBJ(ObjOwningPackedContainer, OBJ_PACKEDCONTAINER);
    newContainer->storage = owningContainer->storage;
    void* targetLocation = (void*) ((uintptr_t) owningContainer->container.loc + offset);
    newContainer->container.loc = targetLocation;
    return TYPED_VAL(newContainer, type);
}

Value createPackedValueContainerAt(void* location, const ObjConcreteYargType* type) {
    ObjPackedContainer* container = ALLOCATE_OBJ(ObjPackedContainer, OBJ_UNOWNED_PACKEDCONTAINER);
    container->loc = location;
    return TYPED_VAL(container, type);
}

Value createPackedContainerFor(ObjPackedContainer* container, size_t offset, const ObjConcreteYargType* type) {
    Obj* object = (Obj*) container;
    assert(object && (object->type == OBJ_PACKEDCONTAINER || object->type == OBJ_UNOWNED_PACKEDCONTAINER));
    if (object->type == OBJ_UNOWNED_PACKEDCONTAINER) {
        return createPackedValueContainerAt((void*) ((uintptr_t) container->loc + offset), type);
    } else {
        assert(object->type == OBJ_PACKEDCONTAINER);
        ObjOwningPackedContainer* owningContainer = (ObjOwningPackedContainer*) container;
        return newPackedValueContainerFor(owningContainer, offset, type);
    }
}

Value pinPackedValueContainer(Value packedValueContainer) {
    ObjPackedContainer* container = AS_PACKEDCONTAINER(packedValueContainer);
    void* address = container->loc;
    return ADDRESS_VAL((uintptr_t) address);
}

bool  isByteArray(Value value) {
    if (!is_uniformarray(value)) return false;
    const ObjConcreteYargTypeArray* arrayType = (const ObjConcreteYargTypeArray*) value.type;
    if (arrayType->element_type) {
        return arrayType->element_type->yt == TypeUint8;
    }
    return false;
}

Value allocByteArray(size_t length) {
    const ObjConcreteYargType* element = &yargTypes.uint8;
    tempRootPush(YARGTYPE_VAL(element));
    const ObjConcreteYargType* arrayType = newYargArrayTypeFromType(element, length);
    tempRootPush(YARGTYPE_VAL(arrayType));

    Value result = defaultArrayValue(arrayType);
    tempRootPop(); // arrayType
    tempRootPop(); // element
    return result;
}

Value createByteArrayAt(uint8_t* location, size_t length) {
    const ObjConcreteYargType* element = &yargTypes.uint8;
    tempRootPush(YARGTYPE_VAL(element));
    const ObjConcreteYargType* arrayType = newYargArrayTypeFromType(element, length);
    tempRootPush(YARGTYPE_VAL(arrayType));

    Value result = createPackedValueContainerAt(location, arrayType);
    tempRootPop(); // arrayType
    tempRootPop(); // element
    return result;
}

Value allocValueArray(size_t length) {
    const ObjConcreteYargType* arrayType = newYargArrayTypeFromType(NULL, length);
    tempRootPush(YARGTYPE_VAL(arrayType));

    Value result = defaultArrayValue(arrayType);
    tempRootPop(); // arrayType
    return result;
}

void offsetPointerDestination(Value pointerVal, Value offsetVal) {
    size_t offset = as_positive_integer(offsetVal);
    ObjPackedContainer* pointer = AS_PACKEDCONTAINER(pointerVal);
    uintptr_t addr = (uintptr_t) pointer->loc;
    addr += offset;
    pointer->loc = (void*) addr;
}

Value containerElement(ObjPackedContainer* container, const ObjConcreteYargType* element_type, size_t element_offset) {
    void* elementAddr = (void*) ((uintptr_t) container->loc + element_offset);

    if (is_placeable_type(element_type) && is_packable_container_type(element_type)) {
        return createPackedContainerFor(container, element_offset, element_type);
    } else if (is_placeable_type(element_type)) {
        PlacedValue store;
        store.value = (void*) elementAddr;
        store.type = element_type;
        return getPlacedValue(store);
    } else if (is_packable_container_type(element_type)) {
        return createPackedContainerFor(container, element_offset, element_type);
    } else {
        return *(Value*) elementAddr;
    }
}

Value arrayElement(Value array, size_t index) {
    const ObjConcreteYargTypeArray* arrayType = (const ObjConcreteYargTypeArray*) array.type;
    size_t element_offset = arrayElementOffset(arrayType, index);

    return containerElement(AS_PACKEDCONTAINER(array), arrayType->element_type, element_offset);
}

Value containerElementPointer(ObjPackedContainer* container, const ObjConcreteYargType* element_type, size_t element_offset) {
    const ObjConcreteYargType* ptrType = newYargPointerType(element_type);
    tempRootPush(YARGTYPE_VAL(ptrType));

    Value result = createPackedContainerFor(container, element_offset, ptrType);
    tempRootPop();
    return result;
}

Value arrayElementPointer(Value array, size_t index) {
    const ObjConcreteYargTypeArray* arrayType = (const ObjConcreteYargTypeArray*) array.type;
    size_t element_offset = arrayElementOffset(arrayType, index);
    return containerElementPointer(AS_PACKEDCONTAINER(array), arrayType->element_type, element_offset);
}

Value structFieldPointer(Value structVal, size_t fieldIndex) {
    const ObjConcreteYargTypeStruct* structType = (const ObjConcreteYargTypeStruct*) structVal.type;
    size_t field_offset = structFieldOffset(structType, fieldIndex);
    return containerElementPointer(AS_PACKEDCONTAINER(structVal), structType->field_types[fieldIndex], field_offset);
}

Value pointerDestination(Value pointer) {
    const ObjConcreteYargTypePointer* pointerType = (const ObjConcreteYargTypePointer*) pointer.type;

    return containerElement(AS_PACKEDCONTAINER(pointer), pointerType->target_type, 0);
}

Value structField(Value structVal, size_t fieldIndex) {
    const ObjConcreteYargTypeStruct* structType = (const ObjConcreteYargTypeStruct*) structVal.type;
    size_t field_offset = structFieldOffset(structType, fieldIndex);

    return containerElement(AS_PACKEDCONTAINER(structVal), structType->field_types[fieldIndex], field_offset);
}

static bool setPlacedContainerElement(const ObjPackedContainer* container, const ObjConcreteYargType* element_type, size_t element_offset, Value value) {

    void* elementAddr = (void*) ((uintptr_t) container->loc + element_offset);

    if (is_placeable_type(element_type)) {
        PlacedValueCellTarget pvct = {
            .value = (void*) elementAddr,
            .type = element_type,
            .cellType = element_type
        };

        return assignToPlacedValueCellTarget(pvct, value);
    } else {
        ValueCellTarget vct = { 
            .cellType = element_type, 
            .value = (Value*) elementAddr 
        };

        return assignToValueCellTarget(vct, value);
    }
}

bool setArrayElement(Value array, size_t index, Value value) {
    const ObjConcreteYargTypeArray* arrayType = (const ObjConcreteYargTypeArray*) array.type;
    size_t element_offset = arrayElementOffset(arrayType, index);
    return setPlacedContainerElement(AS_PACKEDCONTAINER(array), arrayType->element_type, element_offset, value);
}

bool setStructField(Value structVal, size_t fieldIndex, Value value) {
    const ObjConcreteYargTypeStruct* structType = (const ObjConcreteYargTypeStruct*) structVal.type;
    size_t field_offset = structFieldOffset(structType, fieldIndex);
    return setPlacedContainerElement(AS_PACKEDCONTAINER(structVal), structType->field_types[fieldIndex], field_offset, value);
}

bool setPointerDestination(Value pointer, Value value) {
    const ObjConcreteYargTypePointer* pointerType = (const ObjConcreteYargTypePointer*) pointer.type;
    return setPlacedContainerElement(AS_PACKEDCONTAINER(pointer), pointerType->target_type, 0, value);
}


bool isArrayPointer(Value value) {
    if (!IS_POINTER(value)) return false;
    const ObjConcreteYargTypePointer* pointerType = (const ObjConcreteYargTypePointer*) value.type;
    if (pointerType->target_type == NULL) return false;
    return pointerType->target_type->yt == TypeArray;
}

bool isStructPointer(Value value) {
    if (!IS_POINTER(value)) return false;
    const ObjConcreteYargTypePointer* pointerType = (const ObjConcreteYargTypePointer*) value.type;
    if (pointerType->target_type == NULL) return false;
    return pointerType->target_type->yt == TypeStruct;
}