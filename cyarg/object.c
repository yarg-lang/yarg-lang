#include <stdio.h>
#include <string.h>
#include <assert.h>

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

    platform_critical_section_enter_blocking(&vm.heap);

    object->next = vm.objects;
    vm.objects = object;
    
    platform_critical_section_exit(&vm.heap);

#ifdef DEBUG_LOG_GC
    PRINTERR("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

ObjInt* allocateIntObject(size_t numDigits) {
    numDigits += numDigits % 2; // numDigits is always even
    assert(numDigits <= 254 && numDigits >= 2);
    ObjInt *i = (ObjInt *) allocateObject(sizeof (ObjInt) + numDigits * sizeof (uint16_t), OBJ_INT);
    i->bigInt.m_ = numDigits;
    return i;
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
    if (array->objectCount > 0) {
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
    closure->cUpvalueCount = function->upvalueCount;
    return closure;
}

ObjFunction* newFunction() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    initFunction(function);
    return function;
}

void initFunction(ObjFunction* function) {
    // may not be called after alloc, so init all fields.
    function->arity = 0;
    function->upvalueCount = 0;
    function->fName = NULL;
    initChunk(&function->chunk);
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

ObjInt* newInt(int64_t value) {
    ObjInt *i = allocateIntObject(sizeof value / sizeof (uint16_t));
    int_set_i(value, &i->bigInt);
    return i;
}

ObjInt* newIntU(uint64_t value) {
    ObjInt *i = allocateIntObject(sizeof value / sizeof (uint16_t));
    int_set_u(value, &i->bigInt);
    return i;
}

Value defaultIntValue() {
    ObjInt *intObj = allocateIntObject(1);
    int_init(&intObj->bigInt);
    return OBJ_VAL(intObj);
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

ObjMap* newMap(ObjConcreteYargTypeMap* type) {
    ObjMap* map = ALLOCATE_OBJ(ObjMap, OBJ_MAP);
    map->type = type;
    initTable(&map->entries);
    return map;
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
    if (IS_INT(val)) {
        ObjInt *i = AS_INTOBJ(val);
        return i->isLiteral;
    } else if (IS_ADDRESS(val)) {
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

ObjString* copyStringWithEscapes(const char* chars, int length)
{
    char* heapChars = ALLOCATE(char, length + 1);

    char const *in = chars;
    char *out = heapChars;
    int lengthOut = 0;
    for (int i = 0; i < length && *in != '\0'; i++)
    {
        if (*in == '\\')
        {
            in++;
            i++;
        }
        *out++ = *in++;
        lengthOut++;
    }
    uint32_t hash = hashString(heapChars, lengthOut);
    ObjString* interned = tableFindString(&vm.strings, heapChars, lengthOut, hash);
    if (interned != NULL)
    {
        FREE(char, heapChars);
        return interned;
    }

    *out = '\0';
    return allocateString(heapChars, lengthOut, hash);
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

static ObjString* functionToString(ObjFunction* function) {
    if (function->fName == NULL) {
        return copyString("<script>", 8);
    }
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "<fn %s>", function->fName->chars);
    return copyString(buffer, (int)strlen(buffer));
}

static ObjString* routineToString(ObjRoutine* routine) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "<R%p>", routine);
    return copyString(buffer, (int)strlen(buffer));
}

static ObjString* arrayToString(ObjPackedUniformArray* array) {
    ObjConcreteYargTypeArray* arrayType = (ObjConcreteYargTypeArray*)array->store.storedType;
    char buffer[1024];
    ObjString* typeStr = valueToString(OBJ_VAL(arrayType));
    snprintf(buffer, sizeof(buffer), "%s:[", typeStr->chars);
    for (int i = 0; i < arrayType->cardinality; i++) {
        PackedValue element = arrayElement(array->store, i);
        Value unpackedValue = unpackValue(element);
        ObjString* candidate = valueToString(unpackedValue);
        snprintf(buffer, sizeof(buffer), "%s%s", buffer, candidate->chars);
        if (i < arrayType->cardinality - 1) {
            snprintf(buffer, sizeof(buffer), "%s, ", buffer);
        }
    }
    snprintf(buffer, sizeof(buffer), "%s]", buffer);
    return copyString(buffer, (int)strlen(buffer));
}

static ObjString* pointerToString(ObjPackedPointer* ptr) {
    Value targetType = ptr->type->target_type == NULL ? NIL_VAL : OBJ_VAL(ptr->type->target_type);
    ObjString* targetTypeStr = valueToString(targetType);
    tempRootPush(OBJ_VAL(targetTypeStr));

    ObjString* prefix = copyString("<*", 2);
    tempRootPush(OBJ_VAL(prefix));
    ObjString* working = concatenateStrings(prefix, targetTypeStr);
    tempRootPush(OBJ_VAL(working));
    
    int length = working->length + 12;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, working->chars, working->length);
    snprintf(chars + working->length, 12 + 1, ":%p>", (void*) ptr->destination);

    ObjString* result = takeString(chars, length);
    tempRootPop();
    tempRootPop();
    tempRootPop();
    return result;
}

static ObjString* structToString(ObjPackedStruct* st) {
    ObjConcreteYargTypeStruct* structType = (ObjConcreteYargTypeStruct*)st->store.storedType;
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "struct{|%zu:%zu|", structType->field_count, structType->storage_size);
    for (size_t i = 0; i < structType->field_count; i++) {
        PackedValue f = structField(st->store, i);
        Value logValue = unpackValue(f);
        ObjString* fieldStr = valueToString(logValue);
        snprintf(buffer, sizeof(buffer), "%s%s; ", buffer, fieldStr->chars);
    }
    snprintf(buffer, sizeof(buffer), "%s}", buffer);
    return copyString(buffer, (int)strlen(buffer));
}

ObjString* mapToString(ObjMap* map) {
    ObjString* typeStr = valueToString(OBJ_VAL(map->type));
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "<map (%d) %s >", map->entries.count, typeStr->chars);
    return copyString(buffer, (int)strlen(buffer));
}

ObjString* objectToString(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD:
            return functionToString(AS_BOUND_METHOD(value)->method->function);
        case OBJ_CLASS:
            return copyString(AS_CLASS(value)->name->chars, AS_CLASS(value)->name->length);
        case OBJ_CLOSURE:
            return functionToString(AS_CLOSURE(value)->function);
        case OBJ_FUNCTION:
            return functionToString(AS_FUNCTION(value));
        case OBJ_INSTANCE: {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%s instance", AS_INSTANCE(value)->klass->name->chars);
            return copyString(buffer, (int)strlen(buffer));
            }
        case OBJ_NATIVE:
            return copyString("<native fn>", 11);
        case OBJ_ROUTINE:
            return routineToString(AS_ROUTINE(value));
            break;
        case OBJ_CHANNELCONTAINER:
            return channelToString(AS_CHANNEL(value));
            break;
        case OBJ_SYNCGROUP:
            return syncGroupToString(AS_SYNCGROUP(value));
            break;
        case OBJ_STRING:
            return copyString(AS_CSTRING(value), AS_STRING(value)->length);
            break;
        case OBJ_UPVALUE:
            return copyString("upvalue", 7);
            break;
        case OBJ_UNOWNED_UNIFORMARRAY:
        case OBJ_PACKEDUNIFORMARRAY:
            return arrayToString(AS_UNIFORMARRAY(value));
            break;
        case OBJ_YARGTYPE:
        case OBJ_YARGTYPE_ARRAY:
        case OBJ_YARGTYPE_STRUCT:
        case OBJ_YARGTYPE_MAP:
            return typeToString(AS_YARGTYPE(value));
            break;
        case OBJ_UNOWNED_PACKEDPOINTER:
        case OBJ_PACKEDPOINTER:
            return pointerToString(AS_POINTER(value));
            break;
        case OBJ_UNOWNED_PACKEDSTRUCT:
        case OBJ_PACKEDSTRUCT:
            return structToString(AS_STRUCT(value));
            break;
        case OBJ_INT: {
            Int *i = AS_INT(value);
            char sb[INT_STRLEN_FOR_INT254];
            char const* s = int_to_s(i, sb, INT_STRLEN_FOR_INT254);
            return copyString(s, (int)strlen(s));
        }
        case OBJ_MAP:
            return mapToString(AS_MAP(value));
        default: {
                char buffer[64];
                snprintf(buffer, sizeof(buffer), "<implementation object %d>", OBJ_TYPE(value));
                return copyString(buffer, (int)strlen(buffer));
            }
    }
}
