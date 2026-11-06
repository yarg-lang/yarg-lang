#include "object.h"

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "heap.h"
#include "memory.h"
#include "object_gc.h"
#include "object_gc_impl.h"
#include "vm.h"
#include "debug.h"

#include "ast.h"
#include "yargobject.h"
#include "vmobject.h"
#include "builtin.h"
#include "channel.h"
#include "sync_group.h"
#include "packed_containers.h"
#include "yargtype.h"
#include "yargstructtype.h"


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

static void freeObject(Obj* object);

void vm_objects_free() {
    Obj* object = vm_objects.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }

    heap_free(vm_objects.grayStack);
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

static void collectGarbage();

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

void markValueCell(ValueCell* cell) {
    if (cell == NULL) return;
    markValue(cell->value);
    markObject((Obj*)cell->cellType);
}

static void markDynamicValueArray(DynamicValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(array->values[i]);
    }
    markValue(array->stash);
}

static void markExpr(Obj* expr) {
    markObject((Obj*)((ObjExpr*)expr)->nextExpr);
}

static void markStmt(Obj* stmt) {
    markObject((Obj*)((ObjStmt*)stmt)->nextStmt);
}

void markFunction(ObjFunction* function) {
    markObject((Obj*)function->fName);
    markDynamicValueArray(&function->chunk.constants);
}

static void blackenObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    PRINTERR("%p blacken ", (void*)object);
    printObj(object);
    PRINTERR("\n");
#endif

    switch (object->type) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            markValue(bound->reciever);
            markObject((Obj*)bound->method);
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            markObject((Obj*)klass->name);
            markTable(&klass->methods);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            markObject((Obj*)closure->function);
            for (int i = 0; i < closure->cUpvalueCount; i++) {
                markObject((Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            markObject((Obj*)function->fName);
            markDynamicValueArray(&function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            markObject((Obj*)instance->klass);
            markTable(&instance->fields);
            break;
        }
        case OBJ_UPVALUE:
            markValueCell(&((ObjUpvalue*)object)->closed);
            break;
        case OBJ_ROUTINE: {
            ObjRoutine* stack = (ObjRoutine*)object;
            markRoutine(stack);
            break;
        }
        case OBJ_PACKEDSTORAGE: {
            ObjPackedStorage* storage = (ObjPackedStorage*)object;
            markPackedStorage(storage);
            break;
        }
        case OBJ_PACKEDCONTAINER: {
            ObjOwningPackedContainer* container = (ObjOwningPackedContainer*)object;
            markObject((Obj*)container->storage);
            break;
        }
        case OBJ_UNOWNED_PACKEDCONTAINER: break;

        case OBJ_NATIVE: break;
        case OBJ_BUILTIN: break;
        case OBJ_CHANNELCONTAINER: {
            ObjChannelContainer* channel = (ObjChannelContainer*)object;
            markChannel(channel);
            break;
        }
        case OBJ_STRING: break;
        case OBJ_INT: break;
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)object;
            markObject((Obj*)map->type);
            markTable(&map->entries);
            break;
        }
        case OBJ_YARGTYPE: break;
        case OBJ_YARGTYPE_ARRAY: {
            ObjConcreteYargTypeArray* type = (ObjConcreteYargTypeArray*)object;
            markObject((Obj*)type->element_type);
            break;
        }
        case OBJ_YARGTYPE_STRUCT: {
            ObjConcreteYargTypeStruct* type = (ObjConcreteYargTypeStruct*)object;
            markTable(&type->field_names);
            for (int i = 0; i < type->field_count; i++) {
                ObjConcreteYargType* field_type = type->field_types[i];
                markObject((Obj*)field_type);
            }
            break;
        }
        case OBJ_YARGTYPE_POINTER: {
            ObjConcreteYargTypePointer* type = (ObjConcreteYargTypePointer*)object;
            markObject((Obj*)type->target_type);
            break;
        }
        case OBJ_YARGTYPE_MAP: {
            ObjConcreteYargTypeMap* type = (ObjConcreteYargTypeMap*)object;
            markObject((Obj*)type->key_type);
            markObject((Obj*)type->value_type);
            break;
        }
        case OBJ_SYNCGROUP: markSyncGroup((ObjSyncGroup*)object); break;
        case OBJ_STACKSLICE: break;
        case OBJ_AST: {
            ObjAst* ast = (ObjAst*)object;
            markObject((Obj*)ast->statements);
            break;
        }
        case OBJ_PLACEALIAS: {
            ObjPlaceAlias* alias = (ObjPlaceAlias*)object;
            markObject((Obj*)alias->name);
            markObject((Obj*)alias->location);
            break;
        }
        case OBJ_STMT_YIELD: // fall through
        case OBJ_STMT_RETURN:
        case OBJ_STMT_PRINT:
        case OBJ_STMT_ERROR:
        case OBJ_STMT_EXPRESSION: {
            markStmt(object);
            markObject((Obj*)((ObjStmtExpression*)object)->expression);
            break;
        }
        case OBJ_STMT_POKE: {
            markStmt(object);
            ObjStmtPoke* stmt = (ObjStmtPoke*)object;
            markObject((Obj*)stmt->location);
            markObject((Obj*)stmt->offset);
            markObject((Obj*)stmt->assignment);
            break;
        }
        case OBJ_STMT_VARDECLARATION: {
            markStmt(object);
            ObjStmtVarDeclaration* stmt = (ObjStmtVarDeclaration*)object;
            markObject((Obj*)stmt->name);
            markObject((Obj*)stmt->type);
            markObject((Obj*)stmt->initialiser);
            break;
        }
        case OBJ_STMT_FIELDDECLARATION: {
            markStmt(object);
            ObjStmtFieldDeclaration* stmt = (ObjStmtFieldDeclaration*)object;
            markObject((Obj*)stmt->name);
            markObject((Obj*)stmt->type);
            markObject((Obj*)stmt->offset);
            break;
        }
        case OBJ_STMT_PLACEDECLARATION: {
            markStmt(object);
            ObjStmtPlaceDeclaration* stmt = (ObjStmtPlaceDeclaration*)object;
            markObject((Obj*)stmt->type);
            markDynamicObjArray(&stmt->aliases);
            break;
        }
        case OBJ_STMT_BLOCK: {
            ObjStmtBlock* block = (ObjStmtBlock*)object;
            markObject((Obj*)block->stmt.nextStmt);
            markObject((Obj*)block->statements);
            break;
        }
        case OBJ_STMT_IF: {
            ObjStmtIf* ctrl = (ObjStmtIf*)object;
            markObject((Obj*)ctrl->stmt.nextStmt);
            markObject((Obj*)ctrl->test);
            markObject((Obj*)ctrl->ifStmt);
            markObject((Obj*)ctrl->elseStmt);
            break;
        }
        case OBJ_STMT_FUNDECLARATION: {
            ObjStmtFunDeclaration* fun = (ObjStmtFunDeclaration*)object;
            markStmt(object);
            markObject((Obj*)fun->name);
            markObject((Obj*)fun->body);
            markDynamicObjArray(&fun->parameters);
            break;
        }
        case OBJ_STMT_WHILE: {
            ObjStmtWhile* loop = (ObjStmtWhile*)object;
            markObject((Obj*)loop->stmt.nextStmt);
            markObject((Obj*)loop->test);
            markObject((Obj*)loop->loop);
            break;
        }
        case OBJ_STMT_FOR: {
            ObjStmtFor* loop = (ObjStmtFor*)object;
            markObject((Obj*)loop->stmt.nextStmt);
            markObject((Obj*)loop->condition);
            markObject((Obj*)loop->initializer);
            markObject((Obj*)loop->loopExpression);
            markObject((Obj*)loop->body);
            break;
        }
        case OBJ_STMT_CLASSDECLARATION: {
            ObjStmtClassDeclaration* decl = (ObjStmtClassDeclaration*)object;
            markStmt(object);
            markObject((Obj*)decl->name);
            markObject((Obj*)decl->superclass);
            markDynamicObjArray(&decl->methods);
            break;
        }
        case OBJ_EXPR_NUMBER: {
            ObjExprNumber* expr = (ObjExprNumber*)object;
            markObject((Obj*)expr->expr.nextExpr);
            break;
        }
        case OBJ_EXPR_ADDRESS: {
            ObjExprAddress* addr = (ObjExprAddress*)object;
            markObject((Obj*)addr->expr.nextExpr);
            break;
        }       
        case OBJ_EXPR_OPERATION: {
            markExpr(object);
            ObjExprOperation* expr = (ObjExprOperation*)object;
            markObject((Obj*)expr->rhs);
            markObject((Obj*)expr->assignment);
            break;
        }
        case OBJ_EXPR_GROUPING: {
            ObjExprGrouping* expr = (ObjExprGrouping*)object;
            markObject((Obj*)expr->expr.nextExpr);
            markObject((Obj*)expr->expression);
            break;
        }
        case OBJ_EXPR_NAMEDVARIABLE: {
            markExpr(object);
            ObjExprNamedVariable* var = (ObjExprNamedVariable*)object;
            markObject((Obj*)var->assignment);
            markObject((Obj*)var->name);
            break;
        }
        case OBJ_EXPR_LITERAL: {
            ObjExprLiteral* lit = (ObjExprLiteral*)object;
            markObject((Obj*)lit->expr.nextExpr);
            break;
        }
        case OBJ_EXPR_STRING: {
            ObjExprString* str = (ObjExprString*)object;
            markObject((Obj*)str->expr.nextExpr);
            markObject((Obj*)str->string);
            break;
        }
        case OBJ_EXPR_CALL: {
            markExpr(object);
            ObjExprCall* call = (ObjExprCall*)object;
            markDynamicObjArray(&call->arguments);
            break;
        }
        case OBJ_EXPR_COLLECTION_INITIALIZER: {
            markExpr(object);
            ObjExprCollectionInitializer* collection = (ObjExprCollectionInitializer*)object;
            markDynamicObjArray(&collection->initializers);
            markObject((Obj*)collection->cardinality);
            break;
        }
        case OBJ_EXPR_COLLECTION_ELEMENT: {
            markExpr(object);
            ObjExprCollectionElement* collection = (ObjExprCollectionElement*)object;
            markObject((Obj*)collection->element);
            markObject((Obj*)collection->assignment);
            break;
        }
        case OBJ_EXPR_BUILTIN: {
            markExpr(object);
            break;
        }
        case OBJ_EXPR_DOT: {
            markExpr(object);
            ObjExprDot* expr = (ObjExprDot*)object;
            markObject((Obj*)expr->name);
            markObject((Obj*)expr->assignment);
            markObject((Obj*)expr->offset);
            markObject((Obj*)expr->call);
            break;
        }
        case OBJ_EXPR_SUPER: {
            markExpr(object);
            ObjExprSuper* expr = (ObjExprSuper*)object;
            markObject((Obj*)expr->name);
            markObject((Obj*)expr->call);
            break;
        }
        case OBJ_EXPR_TYPE: {
            markExpr(object);
            break;
        }
        case OBJ_EXPR_TYPE_STRUCT: {
            markExpr(object);
            ObjExprTypeStruct* expr = (ObjExprTypeStruct*)object;
            markDynamicValueArray(&expr->fieldsByIndex);
            break;
        }
        case OBJ_EXPR_TYPE_INDEXED_COLLECTION: {
            markExpr(object);
            ObjExprTypeIndexedCollection* collection = (ObjExprTypeIndexedCollection*)object;
            markObject((Obj*)collection->indexing);
            break;
        }
        case OBJ_EXPR_PAIR: {
            markExpr(object);
            ObjExprPair* pair = (ObjExprPair*)object;
            markObject((Obj*)pair->a);
            markObject((Obj*)pair->b);
            break;
        }
        case OBJ_SCANNER: markScanner((ObjScanner*)object); break;
        case OBJ_PARSER: markParser((ObjParser*)object); break;
    }
}

static void gc_freeDynamicValueArray(DynamicValueArray* array) {
    heap_free(array->values);
}

static void gc_freeTable(ValueTable* table) {
    heap_free(table->entries);
}

void gc_freeChunk(Chunk* chunk) {
    if (!chunk->xip) {
        heap_free(chunk->code);
    }
    heap_free(chunk->lines);
    gc_freeDynamicValueArray(&chunk->constants);
}

void gc_free(void* ptr) {
    heap_free(ptr);
}

static void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    PRINTERR("%p freeObject type %d\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJ_BOUND_METHOD: break;
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            gc_freeTable(&klass->methods);
            break;
        }
        case OBJ_CLOSURE: heap_free(((ObjClosure*)object)->upvalues); break;
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            gc_freeChunk(&function->chunk);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            gc_freeTable(&instance->fields);
            break;
        }
        case OBJ_NATIVE: break;
        case OBJ_BUILTIN: break;
        case OBJ_ROUTINE: {
            ObjRoutine* routine = (ObjRoutine*)object;
            gc_freeDynamicObjArray(&routine->runtimeErrors);
            break;
        }
        case OBJ_STRING: heap_free(((ObjString*)object)->chars); break;
        case OBJ_UPVALUE: break;
        case OBJ_CHANNELCONTAINER: gc_freeChannelObject((ObjChannelContainer*)object); break;
        case OBJ_PACKEDSTORAGE: gc_freePackedStorage((ObjPackedStorage*)object); break;
        case OBJ_UNOWNED_PACKEDCONTAINER: break;
        case OBJ_PACKEDCONTAINER: break;
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)object;
            gc_freeTable(&map->entries);
            break;
        }
        case OBJ_YARGTYPE: break;
        case OBJ_YARGTYPE_ARRAY: break;
        case OBJ_YARGTYPE_STRUCT: {
            ObjConcreteYargTypeStruct* t = (ObjConcreteYargTypeStruct*)object;
            heap_free(t->field_types);
            heap_free(t->field_indexes);
            gc_freeTable(&t->field_names);
            break;
        }
        case OBJ_YARGTYPE_MAP: break;
        case OBJ_YARGTYPE_POINTER: break;
        case OBJ_SYNCGROUP: gc_freeSyncGroup((ObjSyncGroup*)object); break;
        case OBJ_STACKSLICE: break;
        case OBJ_AST: break;
        case OBJ_PLACEALIAS: break;
        case OBJ_STMT_RETURN: break;
        case OBJ_STMT_YIELD: break;
        case OBJ_STMT_PRINT: break;
        case OBJ_STMT_EXPRESSION: break;
        case OBJ_STMT_ERROR: break;
        case OBJ_STMT_POKE: break;
        case OBJ_STMT_VARDECLARATION: break;
        case OBJ_STMT_FIELDDECLARATION: break;
        case OBJ_STMT_PLACEDECLARATION: {
            ObjStmtPlaceDeclaration* stmt = (ObjStmtPlaceDeclaration*)object;
            gc_freeDynamicObjArray(&stmt->aliases);
            break;
        }
        case OBJ_STMT_BLOCK: break;
        case OBJ_STMT_IF: break;
        case OBJ_STMT_FUNDECLARATION: {
            ObjStmtFunDeclaration* fun = (ObjStmtFunDeclaration*)object;
            gc_freeDynamicObjArray(&fun->parameters);
            break;
        }
        case OBJ_STMT_WHILE: break;
        case OBJ_STMT_FOR: break;
        case OBJ_STMT_CLASSDECLARATION: {
            ObjStmtClassDeclaration* decl = (ObjStmtClassDeclaration*)object;
            gc_freeDynamicObjArray(&decl->methods);
            break;
        }
        case OBJ_EXPR_NUMBER: break;
        case OBJ_EXPR_ADDRESS: break;
        case OBJ_EXPR_OPERATION: break;
        case OBJ_EXPR_GROUPING: break;
        case OBJ_EXPR_NAMEDVARIABLE: break;
        case OBJ_EXPR_LITERAL: break;
        case OBJ_EXPR_STRING: break;
        case OBJ_EXPR_CALL: {
            ObjExprCall* call = (ObjExprCall*)object;
            gc_freeDynamicObjArray(&call->arguments);
            break;
        }
        case OBJ_EXPR_COLLECTION_INITIALIZER: {
            ObjExprCollectionInitializer* init = (ObjExprCollectionInitializer*)object;
            gc_freeDynamicObjArray(&init->initializers);
            break;
        }
        case OBJ_EXPR_COLLECTION_ELEMENT: break;
        case OBJ_EXPR_PAIR: break;
        case OBJ_EXPR_BUILTIN: break;
        case OBJ_EXPR_DOT: break;
        case OBJ_EXPR_SUPER: break;
        case OBJ_EXPR_TYPE: break;
        case OBJ_EXPR_TYPE_STRUCT: {
            ObjExprTypeStruct* expr = (ObjExprTypeStruct*)object;
            gc_freeDynamicValueArray(&expr->fieldsByIndex);
            break;
        }
        case OBJ_EXPR_TYPE_INDEXED_COLLECTION: break;
        case OBJ_INT: break;
        case OBJ_SCANNER: break;
        case OBJ_PARSER: gc_freeParser((ObjParser*)object); break;
    }
    heap_free(object);
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
