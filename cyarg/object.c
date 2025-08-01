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

ObjValArray* newValArray(size_t capacity) {
    ObjValArray* array = ALLOCATE_OBJ(ObjValArray, OBJ_VALARRAY);
    initValueArray(&array->array);
    tempRootPush(OBJ_VAL(array));

    for (int i = 0; i < capacity; i++) {
        appendToValueArray(&array->array, NIL_VAL);
    }

    tempRootPop();
    return array;
}

ObjUniformArray* newUniformArray(ObjYargType* element_type, size_t capacity) {
    ObjUniformArray* array = ALLOCATE_OBJ(ObjUniformArray, OBJ_UNIFORMARRAY);
    array->array = NULL;
    array->element_size = 0;
    array->element_type = element_type;
    array->count = 0;

    tempRootPush(OBJ_VAL(array));

    if (is_obj_type(element_type)) {
        array->element_size = sizeof(Obj*);
        array->count = capacity;
        array->array = reallocate(array->array, 0, capacity * array->element_size);

        for (int i = 0; i < capacity; i++) {
            Obj** elements = (Obj**) array->array;
            elements[i] = NULL;
        }
    } else if (element_type->yt == TypeMachineUint32) {
        array->element_size = sizeof(uint32_t);
        array->count = capacity;
        array->array = reallocate(array->array, 0, capacity * array->element_size);

        for (int i = 0; i < capacity; i++) {
            uint32_t* elements = (uint32_t*) array->array;
            elements[i] = 0;
        }
    }

    tempRootPop();
    return array;
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

static void printArray(FILE* op, Value a) {
    if (IS_VALARRAY(a)) {
        FPRINTMSG(op, "any[%d]", AS_VALARRAY(a)->array.count);
    } else if (IS_UNIFORMARRAY(a)) {
        FPRINTMSG(op, "muint32[%.zd]", AS_UNIFORMARRAY(a)->count);
    }
}

static void printType(FILE* op, ObjYargType* type) {
    switch (type->yt) {
        case TypeBool: fprintf(op, "bool"); break;
        case TypeDouble: fprintf(op, "mfloat64"); break;
        case TypeMachineUint32: fprintf(op, "muint32"); break;
        case TypeInteger: fprintf(op, "integer"); break;
        case TypeString: fprintf(op, "string"); break;
        case TypeClass: fprintf(op, "Type:Class"); break;
        case TypeInstance: fprintf(op, "Type:Instance"); break;
        case TypeFunction: fprintf(op, "Type:Function"); break;
        case TypeNativeBlob: fprintf(op, "Type:NativeBlob"); break;
        case TypeRoutine: fprintf(op, "Type:Routine"); break;
        case TypeChannel: fprintf(op, "Type:Channel"); break;
        case TypeArray: {
            if (type->element_type == NULL) {
                fprintf(op, "any");
            } else {
                printType(op, type->element_type);
            }
            fprintf(op, "[]");
            break;
        }
        case TypeYargType: fprintf(op, "Type:Type"); break;
        default: fprintf(op, "Type:Unknown"); break;
    }
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
        case OBJ_VALARRAY:
            printArray(op, value);
            break;
        case OBJ_UNIFORMARRAY:
            printArray(op, value);
            break;
        case OBJ_YARGTYPE:
            printType(op, AS_YARGTYPE(value));
            break;
        default:
            FPRINTMSG(op, "<implementation object %d>", OBJ_TYPE(value));
            break;
    }
}

void printObject(Value value) {
    fprintObject(stdout, value);
}