#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include "yargtype.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    memset(object, 0, size);

    object->type = type;
    object->isMarked = false;

    object->next = vm.objects;
    vm.objects = object;

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

StoredValue* arrayElement(ObjConcreteYargTypeArray* arrayType, StoredValue* arrayStart, size_t index) {
    StoredValue* element = (StoredValue*)(((uint8_t*)arrayStart) + arrayElementOffset(arrayType, index));
    return element;
}

ObjPackedUniformArray* newPackedUniformArray(ObjConcreteYargTypeArray* type) {
    ObjPackedUniformArray* array = ALLOCATE_OBJ(ObjPackedUniformArray, OBJ_PACKEDUNIFORMARRAY);
    array->type = type;    

    tempRootPush(OBJ_VAL(array));

    StoredValue* storage = reallocate(NULL, 0, arrayElementSize(type) * type->cardinality);

    for (size_t i = 0; i < array->type->cardinality; i++) {
        StoredValue* element = arrayElement(array->type, storage, i);
        initialisePackedStorage(arrayElementType(array->type), element);
    }

    array->arrayElements = storage;

    tempRootPop();
    return array;
}

ObjPackedUniformArray* newPackedUniformArrayAt(ObjConcreteYargTypeArray* type, void* location) {

    ObjPackedUniformArray* array = ALLOCATE_OBJ(ObjPackedUniformArray, OBJ_UNOWNED_UNIFORMARRAY);
    array->type = type;
    array->arrayElements = (StoredValue*)location;

    return array;
}

Value defaultArrayValue(ObjConcreteYargType* type) {

    ObjConcreteYargTypeArray* arrayType = (ObjConcreteYargTypeArray*)type;
    if (arrayType->cardinality == 0) {
        return NIL_VAL;
    }
    
    return OBJ_VAL(newPackedUniformArray(arrayType));
}

void* createHeapCell(Value type) {
    void* dest = reallocate(NULL, 0, yt_sizeof_type_storage(type));
    return dest;
}

ObjPointer* newPointerForHeapCell(Value target_type, void* location) {

    ObjPointer* ptr = ALLOCATE_OBJ(ObjPointer, OBJ_POINTER);
    ptr->destination_type = target_type;
    ptr->destination = location;
    return ptr;
}

ObjPointer* newPointerAtCell(Value type, StoredValue* location) {
    if (IS_NIL(type) || IS_YARGTYPE(type)) {
        ObjPointer* ptr = ALLOCATE_OBJ(ObjPointer, OBJ_UNOWNED_POINTER);
        ptr->destination_type = type;
        ptr->destination = location;
        return ptr;
    } else {
        return NULL;
    }
}

ObjPointer* newPointerAt(Value type, Value location) {
    if (   IS_YARGTYPE(type)
        && AS_YARGTYPE(type)->yt == TypeMachineUint32
        && IS_UINTEGER(location)) {
        ObjPointer* ptr = ALLOCATE_OBJ(ObjPointer, OBJ_UNOWNED_POINTER);
        ptr->destination_type = type;
        ptr->destination = (void*)(uintptr_t) AS_UINTEGER(location);
        return ptr;
    } else {
        return NULL;
    }
}

bool isArrayPointer(Value value) {
    ObjPointer* pointer = AS_POINTER(value);
    if (IS_POINTER(value)) {
        if (IS_NIL(pointer->destination_type)) {
            return IS_UNIFORMARRAY(pointer->destination->asValue);
        } else if (   IS_YARGTYPE(pointer->destination_type)
                   && AS_YARGTYPE(pointer->destination_type)->yt == TypeArray) {
            return true;
        }
    }
    return false;
}


Value createPointerToObj(Value target_type, Obj* target) {
    if (target == NULL) {
        return NIL_VAL;
    }

    void* location = createHeapCell(target_type);
    Obj** locationObjPtr = (Obj**) location;
    *locationObjPtr = target;

    ObjPointer* pointer = newPointerForHeapCell(target_type, location);

    return OBJ_VAL(pointer);
}

Value placeObjectAt(Value type, Value location) {
    if (is_placeable_type(type)) {
        ObjConcreteYargType* placed_type = AS_YARGTYPE(type);
        void* locationPtr = (void*)(uintptr_t)AS_UINTEGER(location);
        switch (placed_type->yt) {
            case TypeMachineUint32: return OBJ_VAL(newPointerAt(type, location));
            case TypeArray: {
                ObjPackedUniformArray* target_array = newPackedUniformArrayAt((ObjConcreteYargTypeArray*)placed_type, locationPtr);
                tempRootPush(OBJ_VAL(target_array));
                Value result = createPointerToObj(type, (Obj*)target_array);
                tempRootPop();
                return result;
            }
            default:
                return NIL_VAL;
        }
    }
    return NIL_VAL;
}

ObjPackedStruct* newPackedStruct(ObjConcreteYargTypeStruct* type) {
    ObjPackedStruct* object = ALLOCATE_OBJ(ObjPackedStruct, OBJ_PACKEDSTRUCT);
    object->type = type;
    tempRootPush(OBJ_VAL(object));

    StoredValue* storage = reallocate(object->structFields, 0, type->storage_size);

    for (size_t i = 0; i < type->field_count; i++) {
        StoredValue* field = structField(object->type, storage, i);
        initialisePackedStorage(type->field_types[i], field);
    }

    object->structFields = storage;

    tempRootPop();
    return object;
}

ObjPackedStruct* newPackedStructAt(ObjConcreteYargTypeStruct* type, StoredValue* packedStorage) {
    ObjPackedStruct* object = ALLOCATE_OBJ(ObjPackedStruct, OBJ_UNOWNED_PACKEDSTRUCT);
    object->type = type;
    object->structFields = packedStorage;

    return object;
}

bool structFieldIndex(ObjConcreteYargTypeStruct* structType, ObjString* name, size_t* index) {
    Value indexVal;
    if (tableGet(&structType->field_names, name, &indexVal)) {
        *index = AS_UINTEGER(indexVal);
        return true;
    }
    return false;
}

StoredValue* structField(ObjConcreteYargTypeStruct* structType, StoredValue* structStart, size_t index) {

    StoredValue* field = (StoredValue*)((uint8_t*)structStart + structType->field_indexes[index]);
    return field;
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

ObjUpvalue* newUpvalue(ValueCell* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed.value = NIL_VAL;
    upvalue->closed.type = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

static void printType(FILE* op, ObjConcreteYargType* type);

static void printFunction(FILE* op, ObjFunction* function) {
    if (function->name == NULL) {
        FPRINTMSG(op, "<script>");
        return;
    }
    FPRINTMSG(op, "<fn %s>", function->name->chars);
}

static void printRoutine(FILE* op, ObjRoutine* routine) {
    FPRINTMSG(op, "<R%s %p>"
          , routine->type == ROUTINE_ISR ? "i" : "n"
          , routine);
}

static void printChannel(FILE* op, ObjChannel* channel) {
    FPRINTMSG(op, "<ch ");
    if (channel->present) {
        fprintValue(op, channel->data);
    }
    else {
        FPRINTMSG(op, " NIL");
    }
    FPRINTMSG(op, ">");
}

static void printArray(FILE* op, ObjPackedUniformArray* array) {
    printType(op, (ObjConcreteYargType*) array->type);
    fprintf(op, ":[");
    for (int i = 0; i < array->type->cardinality; i++) {
        StoredValue* element = arrayElement(array->type, array->arrayElements, i);
        Value unpackedValue = unpackStoredValue(arrayElementType(array->type), element);
        fprintValue(op, unpackedValue);
        if (i < array->type->cardinality - 1) {
            fprintf(op, ", ");
        }
    }
    fprintf(op, "]");
}

static void printType(FILE* op, ObjConcreteYargType* type) {
    if (type->isConst) {
        fprintf(op, "const ");
    }
    switch (type->yt) {
        case TypeAny: fprintf(op, "Type:any"); break;
        case TypeBool: fprintf(op, "Type:bool"); break;
        case TypeDouble: fprintf(op, "Type:mfloat64"); break;
        case TypeMachineUint32: fprintf(op, "Type:muint32"); break;
        case TypeInteger: fprintf(op, "Type:integer"); break;
        case TypeString: fprintf(op, "Type:string"); break;
        case TypeClass: fprintf(op, "Type:Class"); break;
        case TypeInstance: fprintf(op, "Type:Instance"); break;
        case TypeFunction: fprintf(op, "Type:Function"); break;
        case TypeNativeBlob: fprintf(op, "Type:NativeBlob"); break;
        case TypeRoutine: fprintf(op, "Type:Routine"); break;
        case TypeChannel: fprintf(op, "Type:Channel"); break;
        case TypeArray: {
            ObjConcreteYargTypeArray* array = (ObjConcreteYargTypeArray*) type;
            fprintf(op, "Type:");
            Value type = arrayElementType(array);
            if (IS_NIL(type)) {
                fprintf(op, "any");
            } else {
                printType(op, array->element_type);
            }
            fprintf(op, "[");
            if (array->cardinality > 0) {
                fprintf(op, "%zu", array->cardinality);
            }
            fprintf(op, "]");
            break;
        }
        case TypeStruct: {
            ObjConcreteYargTypeStruct* st = (ObjConcreteYargTypeStruct*) type;
            fprintf(op, "Type:struct{%zu:", st->field_count);
            for (size_t i = 0; i < st->field_count; i++) {
                fprintValue(op, st->field_types[i]);
                fprintf(op, "; ");
            }
            fprintf(op, "}");
            break;
        }
        case TypeYargType: fprintf(op, "Type:Type"); break;
        default: fprintf(op, "Type:Unknown"); break;
    }
}

static void printPointer(FILE* op, ObjPointer* ptr) {
    fprintf(op, "<*");
    fprintValue(op, ptr->destination_type);
    fprintf(op, ":%p>", (void*) ptr->destination);
}

static void printStruct(FILE* op, ObjPackedStruct* st) {
    fprintf(op, "struct{|%zu:%zu|", st->type->field_count, st->type->storage_size);
    for (size_t i = 0; i < st->type->field_count; i++) {
        StoredValue* field = structField(st->type, st->structFields, i);
        Value logValue = unpackStoredValue(st->type->field_types[i], field);
        fprintValue(op, logValue);
        fprintf(op, "; ");
    }
    fprintf(op, "}");
}

void fprintObject(FILE* op, Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD:
            printFunction(op, AS_BOUND_METHOD(value)->method->function);
            break;
        case OBJ_CLASS:
            fprintf(op, "%s", AS_CLASS(value)->name->chars);
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
        case OBJ_CHANNEL:
            printChannel(op, AS_CHANNEL(value));
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
        case OBJ_UNOWNED_POINTER:
        case OBJ_POINTER:
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