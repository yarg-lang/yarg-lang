#include "object.h"

#include "common.h"
#include "memory.h"
#include "object_gc.h"

#include "yargobject.h"
#include "vmobject.h"
#include "builtin.h"
#include "channel.h"
#include "sync_group.h"

#include <string.h>

Obj* vm_object_alloc(size_t size, ObjType type) {
    Obj* object = (Obj*)vm_allocate(size);
    memset(object, 0, size);
    object->type = type;

    gc_register_object(object);

#ifdef DEBUG_LOG_GC
    PRINTERR("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

ObjString* objectToString(const Obj* value) {
    switch (value->type) {
        case OBJ_BOUND_METHOD: {
            const ObjBoundMethod* boundMethod = (const ObjBoundMethod*)value;
            return functionToString(boundMethod->method->function);
        }
        case OBJ_CLASS: {
            const ObjClass* klass = (const ObjClass*)value;
            return copyString(klass->name->chars, klass->name->length);
        }
        case OBJ_CLOSURE: {
            const ObjClosure* closure = (const ObjClosure*)value;
            return functionToString(closure->function);
        }
        case OBJ_FUNCTION: {
            const ObjFunction* function = (const ObjFunction*)value;
            return functionToString(function);
        }
        case OBJ_INSTANCE: {
            char buffer[64];
            const ObjInstance* instance = (const ObjInstance*)value;
            snprintf(buffer, sizeof(buffer), "%s instance", instance->klass->name->chars);
            return copyString(buffer, (int)strlen(buffer));
            }
        case OBJ_NATIVE:
            return copyString("<native fn>", 11);
        case OBJ_BUILTIN:
            return builtinToString((const ObjBuiltin*)value);
        case OBJ_CHANNELCONTAINER: {
            const ObjChannelContainer* channel = (const ObjChannelContainer*)value;
            return channelToString(channel);
        }
        case OBJ_SYNCGROUP: {
            const ObjSyncGroup* syncGroup = (const ObjSyncGroup*)value;
            return syncGroupToString(syncGroup);
        }
        case OBJ_STRING: {
            const ObjString* string = (const ObjString*)value;
            return copyString(string->chars, string->length);
        }
        case OBJ_UPVALUE: {
            const ObjUpvalue* upvalue = (const ObjUpvalue*)value;
            return copyString("upvalue", 7);
        }
        case OBJ_YARGTYPE:
        case OBJ_YARGTYPE_ARRAY:
        case OBJ_YARGTYPE_STRUCT:
        case OBJ_YARGTYPE_MAP: {
            const ObjConcreteYargType* type = (const ObjConcreteYargType*)value;
            return typeToString(type);
        }
        case OBJ_INT: {
            const ObjInt* intObj = (const ObjInt*)value;
            char sb[INT_STRLEN_FOR_INT254];
            char const* s = int_to_s(&intObj->bigInt, sb, INT_STRLEN_FOR_INT254);
            return copyString(s, (int)strlen(s));
        }
        case OBJ_MAP: {
            const ObjMap* map = (const ObjMap*)value;
            return mapToString(map);
        }
        default: {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "<implementation object %d>", value->type);
            return copyString(buffer, (int)strlen(buffer));
        }
    }
}
