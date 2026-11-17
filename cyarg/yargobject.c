#include "yargobject.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "object_gc.h"
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

void freeFunction(ObjFunction* function) {
    freeChunk(&function->chunk);
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
    keyValueStoreInit(&klass->methods);
    return klass;
}

ObjInstance* newInstance(ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    keyValueStoreInit(&instance->fields);
    return instance;
}

ObjInt* allocateIntObject(size_t numDigits) {
    numDigits += numDigits % 2; // numDigits is always even
    assert(numDigits <= 254 && numDigits >= 2);
    ObjInt *i = (ObjInt *) vm_object_alloc(sizeof (ObjInt) + numDigits * sizeof (uint16_t), OBJ_INT);
    i->bigInt.m_ = numDigits;
    i->bigInt.obj_ = true;
    return i;
}

Value defaultIntValue() {
    Value i = { .type = &yargTypes.integer };
    (void) int_init_concrete2(&i.as.bigSmallInt.i_);
    return i;
}

void noLongerLiteralInt(Value *value) {
    if (IS_INT(*value)) {
        AS_BIG_SMALL_INT(*value).literal_ = false;
        AS_INT(*value)->literal_ = false; // in the small case this will repeat the assignment above, in the large case there is literal_ in the value and the object
    }
}

ObjMap* newMap(ObjConcreteYargTypeMap* type) {
    ObjMap* map = ALLOCATE_OBJ(ObjMap, OBJ_MAP);
    map->type = type;
    keyValueStoreInit(&map->entries);
    return map;
}

ObjString* mapToString(const ObjMap* map) {
    ObjString* typeStr = valueToString(YARGTYPE_VAL(map->type));
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "<map (%d) %s >", keyValueStoreCount(&map->entries), typeStr->chars);
    return copyString(buffer, (int)strlen(buffer));
}

bool isAddressValue(Value val) {
    if (IS_INT(val)) {
        return AS_BIG_SMALL_INT(val).literal_;
    } else if (IS_ADDRESS(val)) {
        return true;
    } else {
        return false;
    }
}

static ObjString* allocateString(char* chars, int length) {
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string = stringStoreStore(&vm.stringStore, string);
    return string;
}

ObjString* takeString(char* chars, int length) {
    ObjString *s = stringStoreGet(&vm.stringStore, chars, length);
    if (s == NULL) {
        s = allocateString(chars, length);
    } else {
        vm_free(chars);
    }
    return s;
}


#if defined CYARG_ORDERED_STRING_TREE
ObjString* copyString(const char* chars, int length) {
    ObjString *s = stringStoreGet(&vm.stringStore, chars, length);
    if (s == NULL) {
        char *heapChars = ALLOCATE(char, length + 1);
        memcpy(heapChars, chars, length);
        heapChars[length] = '\0';
        s = allocateString(heapChars, length);
    }
    return s;
}
#else
static uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash += 16777619;
    }
    return hash;
}
ObjString* tableFindString(ValueTable* table, const char* chars, int length, uint32_t hash);
ObjString* copyString(const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* s = tableFindString(&vm.stringStore, chars, length, hash);
    if (s == 0) {
        char* heapChars = ALLOCATE(char, length + 1);
        memcpy(heapChars, chars, length);
        heapChars[length] = '\0';

        s = ALLOCATE_OBJ(ObjString, OBJ_STRING);
        s->length = length;
        s->chars = heapChars;
        s->hash = hash;

        tempRootPush(STRING_VAL(s));
        tableSet(&vm.stringStore, s, NIL_VAL);
        tempRootPop();
    }
    return s;
}
#endif


ObjString* copyStringWithEscapes(const char* chars, int length)
{
    char* heapChars = ALLOCATE(char, length + 1);

    char const *in = chars;
    char *out = heapChars;
    for (int i = 0; i < length && *in != '\0'; i++)
    {
        if (*in == '\\')
        {
            in++;
            i++;
        }
        *out++ = *in++;
    }
    *out = '\0';

    return takeString(heapChars, (int)(out - heapChars));
}

Value cstring_val(const char *cstring) {
    return STRING_VAL(copyString(cstring, (int) strlen(cstring)));
}
