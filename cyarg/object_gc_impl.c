#include "object_gc_impl.h"

#include "common.h"
#include "heap.h"
#include "object.h"
#include "keyvaluestore.h"

#include "chunk.h"
#include "value_cell.h"
#include "vmobject.h"
#include "ast.h"
#include "yargobject.h"
#include "channel.h"
#include "packed_containers.h"
#include "yargtype.h"
#include "yargstructtype.h"
#include "routine.h"

// Our GC implementation needs to reach into the details of every object.
// This file implements those details for objects that declare their structure
// in their headers.
// Objects that declare their structure in a .c must provide (if needed) a mark & free
// function.

void markValueCell(ValueCell* cell) {
    if (cell == NULL) return;
    markValue(cell->value);
    markObject((Obj*)cell->cellType);
}

void markDynamicObjArray(DynamicObjArray* array) {
    markObject(array->stash);
    for (int i = 0; i < array->objectCount; i++) {
        markObject(array->objects[i]);
    }
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

void blackenObject(Obj* object) {
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
            keyValueStoreMark(&klass->methods);
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
            keyValueStoreMark(&instance->fields);
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
            keyValueStoreMark(&map->entries);
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
            keyValueStoreMark(&type->field_names);
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
        case OBJ_EXPR_CALL_BUILTIN: {
            markExpr(object);
            ObjExprCallBuiltin* call = (ObjExprCallBuiltin*)object;
            markObject((Obj*)call->callee);
            markDynamicObjArray(&call->arguments);
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

void gc_freeDynamicObjArray(DynamicObjArray* array) {
    gc_free(array->objects);
}

inline static void gc_freeDynamicValueArray(DynamicValueArray* array) {
    heap_free(array->values);
}

static void gc_freeChunk(Chunk* chunk) {
    if (!chunk->xip) {
        heap_free(chunk->code);
    }
    heap_free(chunk->lines);
    gc_freeDynamicValueArray(&chunk->constants);
}


void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    PRINTERR("%p freeObject type %d\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJ_BOUND_METHOD: break;
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            keyValueStoreGCFree(&klass->methods);
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
            keyValueStoreGCFree(&instance->fields);
            break;
        }
        case OBJ_NATIVE: break;
        case OBJ_BUILTIN: break;
        case OBJ_ROUTINE: {
            ObjRoutine* routine = (ObjRoutine*)object;
            gc_free(routine->stackSlices);
            gc_freeDynamicObjArray(&routine->additionalSlicesArray);
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
            keyValueStoreGCFree(&map->entries);
            break;
        }
        case OBJ_YARGTYPE: break;
        case OBJ_YARGTYPE_ARRAY: break;
        case OBJ_YARGTYPE_STRUCT: {
            ObjConcreteYargTypeStruct* t = (ObjConcreteYargTypeStruct*)object;
            heap_free(t->field_types);
            heap_free(t->field_indexes);
            keyValueStoreGCFree(&t->field_names);
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
        case OBJ_EXPR_CALL_BUILTIN: {
            ObjExprCallBuiltin* call = (ObjExprCallBuiltin*)object;
            gc_freeDynamicObjArray(&call->arguments);
            break;
        }
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
