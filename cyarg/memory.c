#include "memory.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdalign.h>
#include <assert.h>

#include "compiler.h"
#include "vm.h"
#include "yargtype.h"
#include "ast.h"
#include "channel.h"
#include "sync_group.h"
#include "vm_mutex.h"
#include "debug.h"

#include "yargobject.h"
#include "vmobject.h"
#include "yargstructtype.h"
#include "builtin.h"
#include "value_cell.h"
#include "packed_containers.h"

#include "../external/o1heap/o1heap/o1heap.h"

#ifdef DEBUG_LOG_GC
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void init_heap_instance(O1HeapInstance** instance) {

#if defined(CYARG_SELF_HOSTED)
    static alignas(O1HEAP_ALIGNMENT)uint8_t heapArena[190 * 1024];
#else
    static alignas(O1HEAP_ALIGNMENT)uint8_t heapArena[10000 * 1024];
#endif

    *instance = o1heapInit(heapArena, sizeof(heapArena));
    if (*instance == NULL) {
        PRINTERR("Failed to initialize heap instance.\n");
        exit(1);
    }
}

size_t vm_bytesAllocated() {
    O1HeapDiagnostics diagnostics = o1heapGetDiagnostics(vm.heap_instance);
    return diagnostics.allocated;
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    vm_mutex_enter_blocking(&vm.heap);

    size_t before = vm_bytesAllocated();

    before += newSize - oldSize;
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage();
#endif

        if (before > vm.nextGC) {
            collectGarbage();
        }
    }

    if (newSize == 0) {
        o1heapFree(vm.heap_instance, pointer);
        vm_mutex_exit(&vm.heap);
        return NULL;
    }

    void* result = o1heapReallocate(vm.heap_instance, pointer, newSize);
    if (result == NULL) {
        PRINTERR("help! no memory.");
        exit(1);
    }
    vm_mutex_exit(&vm.heap);
    return result;
}

void vm_free(void* pointer) {
    vm_mutex_enter_blocking(&vm.heap);
    o1heapFree(vm.heap_instance, pointer);
    vm_mutex_exit(&vm.heap);
    return;
}

// can only be called during gc, so the heap critical section is already held.
void gc_free(void* pointer) {
    o1heapFree(vm.heap_instance, pointer);
    return;
}


void tempRootPush(Value value) {

    vm_mutex_enter_blocking(&vm.heap);

    *vm.tempRootsTop = value;
    vm.tempRootsTop++;

    if (vm.tempRootsTop - &vm.tempRoots[0] >= TEMP_ROOTS_MAX) {
        fatalVMError("Allocation Stash Max Exeeded.");
    }

    vm_mutex_exit(&vm.heap);
}

void tempObjRootPush(Obj* object) {

    tempRootPush(IMPL_OBJ_VAL(object));
}

Value tempRootPop() {
    vm_mutex_enter_blocking(&vm.heap);
    vm.tempRootsTop--;
    Value result = *vm.tempRootsTop;
    vm_mutex_exit(&vm.heap);
    return result;
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

    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack = (Obj**)o1heapReallocate(vm.heap_instance, vm.grayStack, sizeof(Obj*) * vm.grayCapacity);

        if (vm.grayStack == NULL) exit(1);
    }

    vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
    if (isObjValue(value)) {
        markObject(value.as.obj);
    }
    markObject((Obj*)value.type);
}

void markValueCell(ValueCell* cell) {
    if (cell == NULL) return;
    markValue(cell->value);
    markObject((Obj*)cell->cellType);
}

static void markArray(DynamicValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(array->values[i]);
    }
}

static void markExpr(Obj* expr) {
    markObject((Obj*)((ObjExpr*)expr)->nextExpr);
}

static void markStmt(Obj* stmt) {
    markObject((Obj*)((ObjStmt*)stmt)->nextStmt);
}

void markDynamicObjArray(DynamicObjArray* array) {
    markObject(array->stash);
    for (int i = 0; i < array->objectCount; i++) {
        markObject(array->objects[i]);
    }
}

void markFunction(ObjFunction* function) {
    markObject((Obj*)function->fName);
    markArray(&function->chunk.constants);
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
            markArray(&function->chunk.constants);
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
            markArray(&expr->fieldsByIndex);
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
    }
}

#define GC_FREE_ARRAY(type, pointer, oldCount) \
    gc_free(pointer)

static void gc_freeDynamicObjArray(DynamicObjArray* array) {
    gc_free(array->objects);
}

static void gc_freeDynamicValueArray(DynamicValueArray* array) {
    gc_free(array->values);
}

static void gc_freeTable(ValueTable* table) {
    gc_free(table->entries);
}

void gc_freeChunk(Chunk* chunk) {
    if (!chunk->xip) {
        gc_free(chunk->code);
    }
    gc_free(chunk->lines);
    gc_freeDynamicValueArray(&chunk->constants);
}

static void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    PRINTERR("%p free type %d\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJ_BOUND_METHOD: gc_free(object); break;
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            gc_freeTable(&klass->methods);
            gc_free(object);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            gc_free(closure->upvalues);
            gc_free(object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            gc_freeChunk(&function->chunk);
            gc_free(object);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            gc_freeTable(&instance->fields);
            gc_free(object);
            break;
        }
        case OBJ_NATIVE: gc_free(object); break;
        case OBJ_BUILTIN: gc_free(object); break;
        case OBJ_ROUTINE:
            gc_free(object);
            break;
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            gc_free(string->chars);
            gc_free(object);
            break;
        }
        case OBJ_UPVALUE: gc_free(object); break;
        case OBJ_CHANNELCONTAINER: gc_freeChannelObject((ObjChannelContainer*)object); break;
        case OBJ_PACKEDSTORAGE: gc_freePackedStorage((ObjPackedStorage*)object); break;
        case OBJ_UNOWNED_PACKEDCONTAINER: gc_free(object); break;
        case OBJ_PACKEDCONTAINER: gc_free(object); break;
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)object;
            gc_freeTable(&map->entries);
            gc_free(object);
            break;
        }
        case OBJ_YARGTYPE: gc_free(object); break;
        case OBJ_YARGTYPE_ARRAY: gc_free(object); break;
        case OBJ_YARGTYPE_STRUCT: {
            ObjConcreteYargTypeStruct* t = (ObjConcreteYargTypeStruct*)object;
            GC_FREE_ARRAY(ObjConcreteYargType*, t->field_types, t->field_count);
            GC_FREE_ARRAY(size_t, t->field_indexes, t->field_count);
            gc_freeTable(&t->field_names);
            gc_free(object);
            break;
        }
        case OBJ_YARGTYPE_MAP: gc_free(object); break;
        case OBJ_YARGTYPE_POINTER: gc_free(object); break;
        case OBJ_SYNCGROUP: gc_freeSyncGroup(object); break;
        case OBJ_STACKSLICE: gc_free(object); break;
        case OBJ_AST: gc_free(object); break;
        case OBJ_PLACEALIAS: gc_free(object); break;
        case OBJ_STMT_RETURN: // fall through
        case OBJ_STMT_YIELD:
        case OBJ_STMT_PRINT:
        case OBJ_STMT_EXPRESSION: gc_free(object); break;
        case OBJ_STMT_POKE: gc_free(object); break;
        case OBJ_STMT_VARDECLARATION: gc_free(object); break;
        case OBJ_STMT_FIELDDECLARATION: gc_free(object); break;
        case OBJ_STMT_PLACEDECLARATION: {
            ObjStmtPlaceDeclaration* stmt = (ObjStmtPlaceDeclaration*)object;
            gc_freeDynamicObjArray(&stmt->aliases);
            gc_free(object);
            break;
        }
        case OBJ_STMT_BLOCK: gc_free(object); break;
        case OBJ_STMT_IF: gc_free(object); break;
        case OBJ_STMT_FUNDECLARATION: {
            ObjStmtFunDeclaration* fun = (ObjStmtFunDeclaration*)object;
            gc_freeDynamicObjArray(&fun->parameters);
            gc_free(object); 
            break;
        }
        case OBJ_STMT_WHILE: gc_free(object); break;
        case OBJ_STMT_FOR: gc_free(object); break;
        case OBJ_STMT_CLASSDECLARATION: {
            ObjStmtClassDeclaration* decl = (ObjStmtClassDeclaration*)object;
            gc_freeDynamicObjArray(&decl->methods);
            gc_free(object);
            break;
        }
        case OBJ_EXPR_NUMBER: gc_free(object); break;
        case OBJ_EXPR_ADDRESS: gc_free(object); break;
        case OBJ_EXPR_OPERATION: gc_free(object); break;
        case OBJ_EXPR_GROUPING: gc_free(object); break;
        case OBJ_EXPR_NAMEDVARIABLE: gc_free(object); break;
        case OBJ_EXPR_LITERAL: gc_free(object); break;
        case OBJ_EXPR_STRING: gc_free(object); break;
        case OBJ_EXPR_CALL: {
            ObjExprCall* call = (ObjExprCall*)object;
            gc_freeDynamicObjArray(&call->arguments);
            gc_free(object); 
            break;
        }
        case OBJ_EXPR_COLLECTION_INITIALIZER: {
            ObjExprCollectionInitializer* init = (ObjExprCollectionInitializer*)object;
            gc_freeDynamicObjArray(&init->initializers);
            gc_free(object); 
            break;
        }
        case OBJ_EXPR_COLLECTION_ELEMENT: gc_free(object); break;
        case OBJ_EXPR_PAIR: gc_free(object); break;
        case OBJ_EXPR_BUILTIN: gc_free(object); break;
        case OBJ_EXPR_DOT: gc_free(object); break;
        case OBJ_EXPR_SUPER: gc_free(object); break;
        case OBJ_EXPR_TYPE: gc_free(object); break;
        case OBJ_EXPR_TYPE_STRUCT: {
            ObjExprTypeStruct* expr = (ObjExprTypeStruct*)object;
            gc_freeDynamicValueArray(&expr->fieldsByIndex);
            gc_free(object);
            break;
        }
        case OBJ_EXPR_TYPE_INDEXED_COLLECTION: gc_free(object); break;
        case OBJ_INT: gc_free(object); break;
    }
}

static void markRoots() {
    markVMRoots();
    markCompilerRoots();
}

static void traceReferences() {
    while (vm.grayCount > 0) {
        Obj* object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
}

static void sweep() {
    Obj* previous = NULL;
    Obj* object = vm.objects;
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
                vm.objects = object;
            }

            freeObject(unreached);
        }
    }
}

void fprintHeapDiagnostics(FILE* out) {
    O1HeapDiagnostics diagnostics = o1heapGetDiagnostics(vm.heap_instance);
    fprintf(out, "Heap Diagnostics:\n");
    fprintf(out, "  Bytes Allocated: %zu\n", diagnostics.allocated);
    fprintf(out, "  Capacity: %zu\n", diagnostics.capacity);
    fprintf(out, "  Peak Allocation: %zu\n", diagnostics.peak_allocated);
    fprintf(out, "  Peak Requested: %zu\n", diagnostics.peak_request_size);
    fprintf(out, "  Failed Allocations: %llu\n", diagnostics.oom_count);
}

void collectGarbage() {

#ifdef DEBUG_LOG_GC
    FPRINTMSG(stderr, "-- gc begin\n");
    fprintHeapDiagnostics(stderr);
    O1HeapDiagnostics diagnostics = o1heapGetDiagnostics(vm.heap_instance);
    size_t before = diagnostics.allocated;
#endif

    if (!o1heapDoInvariantsHold(vm.heap_instance)) {
        fatalVMError("heap invariants do not hold as gc starts");
    }

    markRoots();
    traceReferences();
    tableRemoveWhite(&vm.strings);
    sweep();

    size_t after = vm_bytesAllocated();

    size_t candidateGC = after * GC_HEAP_GROW_FACTOR;
    vm.nextGC = candidateGC > ALWAYS_GC_ABOVE ? ALWAYS_GC_ABOVE : candidateGC;

#ifdef DEBUG_LOG_GC
    FPRINTMSG(stderr, "-- gc end\n");
    FPRINTMSG(stderr, "   collected %zu bytes (from %zu to %zu) next at %zu\n",
             before - after, before, after,
             vm.nextGC);
#endif

}

void freeObjects() {
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }

    o1heapFree(vm.heap_instance, vm.grayStack);
}

void printObjects() {
    PRINTERR("=== Objects ===\n");
    Obj* object = vm.objects;
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

void pinObj(Obj* object) {
    ; // currently a nop.
}
