#include "object.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "heap.h"
#include "memory.h"
#include "object_gc.h"
#include "object_gc_impl.h"
#include "debug.h"

#include "yargobject.h"
#include "vmobject.h"
#include "builtin.h"
#include "channel.h"
#include "sync_group.h"

// configuration
#define TEMP_ROOTS_MAX 8
#define FIRST_GC_AT 50 * 1024
#define ALWAYS_GC_ABOVE 100 * 1024
#define GC_HEAP_GROW_FACTOR 2

typedef struct objects {
    vm_mutex* mutex;

    Value tempRoots[TEMP_ROOTS_MAX];
    Value* tempRootsTop;

    size_t nextGC;
    Obj* objects;
    int grayCount;
    int grayCapacity;
    Obj** grayStack;
} objects;

objects vm_objects;

void vm_objects_init(vm_mutex* mutex) {
    memset(&vm_objects, 0, sizeof(vm_objects));
    vm_objects.mutex = mutex;
    vm_objects.tempRootsTop = &vm_objects.tempRoots[0];
    vm_objects.nextGC = FIRST_GC_AT;
}

static void collectGarbage();

void vm_objects_free() {
    Obj* object = vm_objects.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }

    heap_free(vm_objects.grayStack);
    assert(heap_bytes_allocated() == 0);
}

void tempRootPush(Value value) {

    vm_mutex_enter_blocking(vm_objects.mutex);

    *vm_objects.tempRootsTop = value;
    vm_objects.tempRootsTop++;

    if (vm_objects.tempRootsTop - &vm_objects.tempRoots[0] >= TEMP_ROOTS_MAX) {
        fatalVMError("Allocation Stash Max Exeeded.");
    }

    vm_mutex_exit(vm_objects.mutex);
}

void tempObjRootPush(Obj* object) {

    tempRootPush(IMPL_OBJ_VAL(object));
}

Value tempRootPop() {
    vm_mutex_enter_blocking(vm_objects.mutex);
    vm_objects.tempRootsTop--;
    Value result = *vm_objects.tempRootsTop;
    vm_mutex_exit(vm_objects.mutex);
    return result;
}

Obj* vm_object_alloc(size_t size, ObjType type) {
    Obj* object = (Obj*)vm_allocate(size);
    memset(object, 0, size);
    object->type = type;

    vm_mutex_enter_blocking(vm_objects.mutex);

    object->next = vm_objects.objects;
    vm_objects.objects = object;
    
    vm_mutex_exit(vm_objects.mutex);

#ifdef DEBUG_LOG_GC
    PRINTERR("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

void pinObj(Obj* object) {
    ; // currently a nop.
}

void gc_objects(size_t newSize) {
    size_t before = heap_bytes_allocated();
#ifdef DEBUG_STRESS_GC
    collectGarbage();
#endif

    before += newSize;
    if (before > vm_objects.nextGC) {
        collectGarbage();
    }
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

void markObject(Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;

#ifdef DEBUG_LOG_GC
    PRINTERR("%p mark ", (void*)object);
    printObj(object);
    PRINTERR("\n");
#endif

    object->isMarked = true;

    if (vm_objects.grayCapacity < vm_objects.grayCount + 1) {
        vm_objects.grayCapacity = GROW_CAPACITY(vm_objects.grayCapacity);
        vm_objects.grayStack = (Obj**)heap_reallocate(vm_objects.grayStack, sizeof(Obj*) * vm_objects.grayCapacity);

        if (vm_objects.grayStack == NULL) fatalVMError("Failed to allocate gray stack.");
    }

    vm_objects.grayStack[vm_objects.grayCount++] = object;
}

void markValue(Value value) {
    if (isObjValue(value)) {
        markObject(value.as.obj);
    }
    else if (IS_BIG_INT(value)) {
        markObject((Obj *)AS_OBJINT(value));
    }
    markObject((Obj*)value.type);
}

void gc_free(void* ptr) {
    heap_free(ptr);
}

static void markRoots() {
    for (Value* slot = vm_objects.tempRoots; slot < vm_objects.tempRootsTop; slot++) {
        markValue(*slot);
    }
    
    markVMRoots();
    markCompilerRoots();
}

static void traceReferences() {
    while (vm_objects.grayCount > 0) {
        Obj* object = vm_objects.grayStack[--vm_objects.grayCount];
        blackenObject(object);
    }
}

static void sweep() {
    Obj* previous = NULL;
    Obj* object = vm_objects.objects;
    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm_objects.objects = object;
            }

            freeObject(unreached);
        }
    }
}

static void collectGarbage() {

#ifdef DEBUG_LOG_GC
    FPRINTMSG(stderr, "-- gc begin\n");
    heap_log_diagnostics();
    size_t before = heap_bytes_allocated();
#endif

    if (!heap_invariant()) {
        fatalVMError("heap invariants do not hold as gc starts");
    }

    markRoots();
    traceReferences();
    vmRemoveUnmarkedRoots();
    sweep();

    size_t after = heap_bytes_allocated();

    size_t candidateGC = after * GC_HEAP_GROW_FACTOR;
    vm_objects.nextGC = candidateGC > ALWAYS_GC_ABOVE ? ALWAYS_GC_ABOVE : candidateGC;

#ifdef DEBUG_LOG_GC
    FPRINTMSG(stderr, "-- gc end\n");
    FPRINTMSG(stderr, "   collected %zu bytes (from %zu to %zu) next at %zu\n",
             before - after, before, after,
             vm_objects.nextGC);
#endif

}

void printObjects() {
    PRINTERR("=== Objects ===\n");
    Obj* object = vm_objects.objects;
    size_t count = 0;
    while (object != NULL) {
        PRINTERR("%p ", (void*)object);
        fprintObj(stderr, object);
        PRINTERR("\n");
        object = object->next;
        count++;
    }
    PRINTERR("=== End Objects (%zu) ===\n", count);
}
