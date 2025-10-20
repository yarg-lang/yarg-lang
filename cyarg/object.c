#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include "yargtype.h"
#include "channel.h"
#include "sync_group.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    memset(object, 0, size);

    object->type = type;
    object->isMarked = false;

    platform_mutex_enter(&vm.heap);

    object->next = vm.objects;
    vm.objects = object;
    
    platform_mutex_leave(&vm.heap);

#ifdef DEBUG_LOG_GC
    PRINTERR("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

void initDynamicObjArray(DynamicObjArray* array) {
    array->objects = NULL;
    array->stash = NULL;
    array->objectCapacity = 0;
    array->objectCount = 0;
}

void freeDynamicObjArray(DynamicObjArray* array) {
    FREE_ARRAY(Obj*, array->objects, array->objectCapacity);
    initDynamicObjArray(array);
}

void appendToDynamicObjArray(DynamicObjArray* array, Obj* obj) {
    array->stash = obj;
    if (array->objectCapacity < array->objectCount + 1) {
        int oldCapacity = array->objectCapacity;
        array->objectCapacity = GROW_CAPACITY(oldCapacity);
        array->objects = GROW_ARRAY(Obj*, array->objects, oldCapacity, array->objectCapacity);
    }
    array->objects[array->objectCount] = obj;
    array->objectCount++;
    array->stash = NULL;
}

Obj* removeLastFromDynamicObjArray(DynamicObjArray* array) {
    Obj* end = NULL;
    if (array->objectCount > 1) {
        end = array->objects[array->objectCount - 1];
        array->objectCount--;
    }
    return end;
}


ObjBoundMethod* newBoundMethod(Value reciever, ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod,
                                         OBJ_BOUND_METHOD);
    bound->reciever = reciever;
    bound->method = method;
    return bound;
}

ObjClass* newClass(ObjString* name) {
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    klass->name = name;
    initTable(&klass->methods);
    return klass;
}

ObjClosure* newClosure(ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjFunction* newFunction() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjInstance* newInstance(ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}

ObjNative* newNative(NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

ObjBlob* newBlob(size_t count) {
    ObjBlob* blob = ALLOCATE_OBJ(ObjBlob, OBJ_BLOB);
    blob->blob = NULL;
    
    tempRootPush(OBJ_VAL(blob));
    blob->blob = reallocate(NULL, 0, count);
    tempRootPop();
    return blob;
}

PackedValue arrayElement(PackedValue array, size_t index) {
    ObjConcreteYargTypeArray* array_type = (ObjConcreteYargTypeArray*) array.storedType;
    
    PackedValue el;
    el.storedType = array_type->element_type;
    el.storedValue = (PackedValueStore*)(((uint8_t*)array.storedValue) + arrayElementOffset(array_type, index));
    return el;
}

size_t arrayCardinality(PackedValue array) {
    ObjConcreteYargTypeArray* array_type = (ObjConcreteYargTypeArray*) array.storedType;
    return array_type->cardinality;
}

ObjPackedUniformArray* newPackedUniformArray(ObjConcreteYargTypeArray* type) {
    ObjPackedUniformArray* array = ALLOCATE_OBJ(ObjPackedUniformArray, OBJ_PACKEDUNIFORMARRAY);
    tempRootPush(OBJ_VAL(array));

    PackedValue new_array = { .storedType = (ObjConcreteYargType*) type, .storedValue = NULL };
    new_array.storedValue = reallocate(NULL, 0, arrayElementSize(type) * type->cardinality);

    for (size_t i = 0; i < type->cardinality; i++) {
        PackedValue el = arrayElement(new_array, i);
        initialisePackedValue(el);
    }

    array->store = new_array;
    tempRootPop();
    return array;
}

ObjPackedUniformArray* newPackedUniformArrayAt(PackedValue location) {

    ObjPackedUniformArray* array = ALLOCATE_OBJ(ObjPackedUniformArray, OBJ_UNOWNED_UNIFORMARRAY);
    array->store = location;

    return array;
}

Value defaultArrayValue(ObjConcreteYargType* type) {

    ObjConcreteYargTypeArray* arrayType = (ObjConcreteYargTypeArray*)type;
    if (arrayType->cardinality == 0) {
        return NIL_VAL;
    }
    
    return OBJ_VAL(newPackedUniformArray(arrayType));
}

ObjPackedPointer* newPointerForHeapCell(PackedValue location) {

    ObjPackedPointer* ptr = ALLOCATE_OBJ(ObjPackedPointer, OBJ_PACKEDPOINTER);
    tempRootPush(OBJ_VAL(ptr));
    ptr->type = (ObjConcreteYargTypePointer*) newYargTypeFromType(TypePointer);
    ptr->type->target_type = location.storedType;
    ptr->destination = location.storedValue;
    tempRootPop();
    return ptr;
}

ObjPackedPointer* newPointerAtHeapCell(PackedValue location) {
    ObjPackedPointer* ptr = ALLOCATE_OBJ(ObjPackedPointer, OBJ_UNOWNED_PACKEDPOINTER);
    tempRootPush(OBJ_VAL(ptr));
    ptr->type = (ObjConcreteYargTypePointer*) newYargTypeFromType(TypePointer);
    ptr->type->target_type = location.storedType;
    ptr->destination = location.storedValue;
    tempRootPop();
    return ptr;
}

void offsetPointerDestination(ObjPackedPointer* pointer, size_t offset) {
    uintptr_t addr = (uintptr_t)(pointer->destination);
    addr += offset;
    pointer->destination = (PackedValueStore*) addr;
}

bool isAddressValue(Value val) {
    if (IS_ADDRESS(val)) {
        return true;
    } else if (IS_POINTER(val)) {
        return true;
    } else {
        return false;
    }
}

bool isArrayPointer(Value value) {
    ObjPackedPointer* pointer = AS_POINTER(value);
    if (IS_POINTER(value)) {
        PackedValue target = { 
            .storedType = pointer->type->target_type,
            .storedValue = pointer->destination
        };
        return is_uniformarray(target);
    }
    return false;
}

bool isStructPointer(Value value) {
    ObjPackedPointer* pointer = AS_POINTER(value);
    if (IS_POINTER(value)) {
        PackedValue target = { 
            .storedType = pointer->type->target_type,
            .storedValue = pointer->destination
        };
        return is_struct(target);
    }
    return false;
}

Obj* destinationObject(Value pointer) {
    if (IS_POINTER(pointer)) {
        ObjPackedPointer* p = AS_POINTER(pointer);
        PackedValue dest;
        dest.storedType = p->type->target_type;
        dest.storedValue = p->destination;
        Value target = unpackValue(dest);
        if (IS_OBJ(target)) {
            return AS_OBJ(target);
        }
    }
    return NULL;
}

Value placeObjectAt(Value placedType, Value location) {
    if (is_placeable_type(placedType) && IS_ADDRESS(location)) {
        PackedValue loc;
        loc.storedType = IS_NIL(placedType) ? NULL : AS_YARGTYPE(placedType);
        loc.storedValue = (PackedValueStore*) AS_ADDRESS(location);
        switch (loc.storedType->yt) {
            case TypeArray:  // fall through
            case TypeStruct:
            case TypeInt8:
            case TypeUint8:
            case TypeInt16:
            case TypeUint16:
            case TypeInt32:
            case TypeUint32:
            case TypeInt64:
            case TypeUint64: {
                ObjPackedPointer* result = newPointerAtHeapCell(loc);
                return OBJ_VAL(result);
            }
            default:
                return NIL_VAL;
        }
    }
    return NIL_VAL;
}

ObjPackedStruct* newPackedStruct(ObjConcreteYargTypeStruct* type) {
    ObjPackedStruct* object = ALLOCATE_OBJ(ObjPackedStruct, OBJ_PACKEDSTRUCT);
    tempRootPush(OBJ_VAL(object));

    PackedValue new_struct = { .storedType = (ObjConcreteYargType*) type, .storedValue = NULL };
    new_struct.storedValue = reallocate(new_struct.storedValue, 0, type->storage_size);

    for (size_t i = 0; i < type->field_count; i++) {
        PackedValue f = structField(new_struct, i);
        initialisePackedValue(f);
    }

    object->store = new_struct;

    tempRootPop();
    return object;
}

ObjPackedStruct* newPackedStructAt(PackedValue location) {
    ObjPackedStruct* object = ALLOCATE_OBJ(ObjPackedStruct, OBJ_UNOWNED_PACKEDSTRUCT);
    object->store = location;

    return object;
}

bool structFieldIndex(ObjConcreteYargType* type, ObjString* name, size_t* index) {
    ObjConcreteYargTypeStruct* structType = (ObjConcreteYargTypeStruct*)type;
    Value indexVal;
    if (tableGet(&structType->field_names, name, &indexVal)) {
        *index = AS_UI32(indexVal);
        return true;
    }
    return false;
}

PackedValue structField(PackedValue struct_, size_t index) {
    ObjConcreteYargTypeStruct* typeStruct = (ObjConcreteYargTypeStruct*)struct_.storedType;

    PackedValue f;
    f.storedType = typeStruct->field_types[index];
    f.storedValue = (PackedValueStore*)((uint8_t*)struct_.storedValue + typeStruct->field_indexes[index]);
    return f;
}

Value defaultStructValue(ObjConcreteYargType* type) {
    ObjConcreteYargTypeStruct* typeStruct = (ObjConcreteYargTypeStruct*)type;

    ObjPackedStruct* object = newPackedStruct(typeStruct);
    tempRootPush(OBJ_VAL(object));

    return tempRootPop();
}

static ObjString* allocateString(char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    tempRootPush(OBJ_VAL(string));
    tableSet(&vm.strings, string, NIL_VAL);
    tempRootPop();
    return string;
}

static uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash += 16777619;
    }
    return hash;
}

ObjString* takeString(char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

ObjString* copyString(const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

ObjUpvalue* newUpvalue(ValueCell* slot, size_t stackOffset) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed.value = NIL_VAL;
    upvalue->closed.cellType = NULL;
    upvalue->contents = slot;
    upvalue->stackOffset = stackOffset;
    upvalue->next = NULL;
    return upvalue;
}

static void printFunction(FILE* op, ObjFunction* function) {
    if (function->name == NULL) {
        FPRINTMSG(op, "<script>");
        return;
    }
    FPRINTMSG(op, "<fn %s>", function->name->chars);
}

static void printRoutine(FILE* op, ObjRoutine* routine) {
    FPRINTMSG(op, "<R%p>", routine);
}

static void printArray(FILE* op, ObjPackedUniformArray* array) {
    ObjConcreteYargTypeArray* arrayType = (ObjConcreteYargTypeArray*)array->store.storedType;
    printType(op, array->store.storedType);
    FPRINTMSG(op, ":[");
    for (int i = 0; i < arrayType->cardinality; i++) {
        PackedValue element = arrayElement(array->store, i);
        Value unpackedValue = unpackValue(element);
        fprintValue(op, unpackedValue);
        if (i < arrayType->cardinality - 1) {
            FPRINTMSG(op, ", ");
        }
    }
    FPRINTMSG(op, "]");
}

static void printPointer(FILE* op, ObjPackedPointer* ptr) {
    FPRINTMSG(op, "<*");
    Value targetType = ptr->type->target_type == NULL ? NIL_VAL : OBJ_VAL(ptr->type->target_type);
    fprintValue(op, targetType);
    FPRINTMSG(op, ":%p>", (void*) ptr->destination);
}

static void printStruct(FILE* op, ObjPackedStruct* st) {
    ObjConcreteYargTypeStruct* structType = (ObjConcreteYargTypeStruct*)st->store.storedType;
    FPRINTMSG(op, "struct{|%zu:%zu|", structType->field_count, structType->storage_size);
    for (size_t i = 0; i < structType->field_count; i++) {
        PackedValue f = structField(st->store, i);
        Value logValue = unpackValue(f);
        fprintValue(op, logValue);
        FPRINTMSG(op, "; ");
    }
    FPRINTMSG(op, "}");
}

void fprintObject(FILE* op, Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD:
            printFunction(op, AS_BOUND_METHOD(value)->method->function);
            break;
        case OBJ_CLASS:
            FPRINTMSG(op, "%s", AS_CLASS(value)->name->chars);
            break;
        case OBJ_CLOSURE:
            printFunction(op, AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            printFunction(op, AS_FUNCTION(value));
            break;
        case OBJ_INSTANCE:
            FPRINTMSG(op, "%s instance", AS_INSTANCE(value)->klass->name->chars);
            break;
        case OBJ_NATIVE:
            FPRINTMSG(op, "<native fn>");
            break;
        case OBJ_BLOB:
            FPRINTMSG(op, "<blob %p>", AS_BLOB(value)->blob);
            break;
        case OBJ_ROUTINE:
            printRoutine(op, AS_ROUTINE(value));
            break;
        case OBJ_CHANNELCONTAINER:
            printChannel(op, AS_CHANNEL(value));
            break;
        case OBJ_SYNCGROUP:
            printSyncGroup(op, AS_SYNCGROUP(value));
            break;
        case OBJ_STRING:
            FPRINTMSG(op, "%s", AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            FPRINTMSG(op, "upvalue");
            break;
        case OBJ_UNOWNED_UNIFORMARRAY:
        case OBJ_PACKEDUNIFORMARRAY:
            printArray(op, AS_UNIFORMARRAY(value));
            break;
        case OBJ_YARGTYPE:
        case OBJ_YARGTYPE_ARRAY:
        case OBJ_YARGTYPE_STRUCT:
            printType(op, AS_YARGTYPE(value));
            break;
        case OBJ_UNOWNED_PACKEDPOINTER:
        case OBJ_PACKEDPOINTER:
            printPointer(op, AS_POINTER(value));
            break;
        case OBJ_UNOWNED_PACKEDSTRUCT:
        case OBJ_PACKEDSTRUCT:
            printStruct(op, AS_STRUCT(value));
            break;
        default:
            FPRINTMSG(op, "<implementation object %d>", OBJ_TYPE(value));
            break;
    }
}

void printObject(Value value) {
    fprintObject(stdout, value);
}