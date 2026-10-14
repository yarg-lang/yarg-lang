#include "yargobject.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "memory.h"
#include "yargstructtype.h"
#include "packed_containers.h"
#include "vm.h"

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

ObjString* functionToString(const ObjFunction* function) {
    if (function->fName == NULL) {
        return copyString("<script>", 8);
    }
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "<fn %s>", function->fName->chars);
    return copyString(buffer, (int)strlen(buffer));
}

ObjClass* newClass(ObjString* name) {
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    klass->name = name;
    initTable(&klass->methods);
    return klass;
}

ObjInstance* newInstance(ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}

bool intsEqual(const ObjInt* a, const ObjInt* b) {
    if (a == b) return true;

    return int_is(&a->bigInt, &b->bigInt) == INT_EQ;
}

ObjInt* allocateIntObject(size_t numDigits) {
    numDigits += numDigits % 2; // numDigits is always even
    assert(numDigits <= 254 && numDigits >= 2);
    ObjInt *i = (ObjInt *) allocateObject(sizeof (ObjInt) + numDigits * sizeof (uint16_t), OBJ_INT);
    i->bigInt.m_ = numDigits;
    return i;
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
    return INTOBJ_VAL(intObj);
}

void noLongerLiteralInt(Value value) {
    if (IS_INT(value)) {
        ((ObjInt *) value.as.obj)->isLiteral = false;
    }
}

ObjMap* newMap(ObjConcreteYargTypeMap* type) {
    ObjMap* map = ALLOCATE_OBJ(ObjMap, OBJ_MAP);
    map->type = type;
    initTable(&map->entries);
    return map;
}

ObjString* mapToString(const ObjMap* map) {
    ObjString* typeStr = valueToString(YARGTYPE_VAL(map->type));
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "<map (%d) %s >", map->entries.count, typeStr->chars);
    return copyString(buffer, (int)strlen(buffer));
}

bool isAddressValue(Value val) {
    if (IS_INT(val)) {
        ObjInt *i = AS_INTOBJ(val);
        return i->isLiteral;
    } else if (IS_ADDRESS(val)) {
        return true;
    } else {
        return false;
    }
}

static ObjString* allocateString(char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    tempRootPush(STRING_VAL(string));
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
