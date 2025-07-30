#include <stdlib.h>
#include <stdio.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"
#include "yargtype.h"
#include "ast.h"

#ifdef DEBUG_LOG_GC
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    vm.bytesAllocated += newSize - oldSize;
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage();
#endif

        if (vm.bytesAllocated > vm.nextGC) {
            collectGarbage();
        }
    }

    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) {
        PRINTERR("help! no memory.");
        exit(1);
    }
    return result;
}

void tempRootPush(Value value) {
    *vm.tempRootsTop = value;
    vm.tempRootsTop++;

    if (vm.tempRootsTop - &vm.tempRoots[0] >= TEMP_ROOTS_MAX) {
        fatalVMError("Allocation Stash Max Exeeded.");
    }
}

Value tempRootPop() {
    vm.tempRootsTop--;
    return *vm.tempRootsTop;
}

void markObject(Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;

#ifdef DEBUG_LOG_GC
    PRINTERR("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    PRINTERR("\n");
#endif

    object->isMarked = true;

    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);

        if (vm.grayStack == NULL) exit(1);
    }

    vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
    if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

void markValueCell(ValueCell* cell) {
    if (cell == NULL) return;
    markValue(cell->value);
    markValue(cell->type);
}

static void markArray(ValueArray* array) {
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

static void blackenObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    PRINTERR("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
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
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject((Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            markObject((Obj*)function->name);
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
        case OBJ_VALARRAY: {
            ObjValArray* array = (ObjValArray*)object;
            markArray(&array->array);
            break;
        }
        case OBJ_UNIFORMARRAY: {
            ObjUniformArray* array = (ObjUniformArray*)object;
            if (is_obj_type(array->element_type)) {
                for (int i = 0; i < array->count; i++) {
                    Obj** elements = (Obj**) array->array;
                    if (elements[i]) {
                        markObject(elements[i]);
                    }
                }
            }
            markObject((Obj*)array->element_type);
            break;
        }
        case OBJ_YARGTYPE:
            break;
        case OBJ_AST: {
            ObjAst* ast = (ObjAst*)object;
            markObject((Obj*)ast->statements);
            markTable(&ast->constants);
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
        case OBJ_STMT_VARDECLARATION: {
            ObjStmtVarDeclaration* stmt = (ObjStmtVarDeclaration*)object;
            markObject((Obj*)stmt->stmt.nextStmt);
            markObject((Obj*)stmt->name);
            markObject((Obj*)stmt->type);
            markObject((Obj*)stmt->initialiser);
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
        case OBJ_STMT_STRUCTDECLARATION: { 
            ObjStmtStructDeclaration* struct_ = (ObjStmtStructDeclaration*)object;
            markStmt(object);
            markObject((Obj*)struct_->name);
            markObject((Obj*)struct_->address);
            markTable(&struct_->fields);
            break;
        }
        case OBJ_STMT_FIELDDECLARATION: {
            ObjStmtFieldDeclaration* field = (ObjStmtFieldDeclaration*)object;
            markStmt(object);
            markObject((Obj*)field->name);
            markObject((Obj*)field->offset);
            break;
        }
        case OBJ_EXPR_NUMBER: {
            ObjExprNumber* expr = (ObjExprNumber*)object;
            markObject((Obj*)expr->expr.nextExpr);
            break;
        }       
        case OBJ_EXPR_OPERATION: {
            ObjExprOperation* expr = (ObjExprOperation*)object;
            markObject((Obj*)expr->expr.nextExpr);
            markObject((Obj*)expr->rhs);
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
        case OBJ_EXPR_NAMEDCONSTANT: {
            markExpr(object);
            ObjExprNamedConstant* const_ = (ObjExprNamedConstant*)object;
            markObject((Obj*)const_->value);
            markObject((Obj*)const_->name);
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
        case OBJ_EXPR_ARRAYINIT: {
            markExpr(object);
            ObjExprArrayInit* array = (ObjExprArrayInit*)object;
            markDynamicObjArray(&array->initializers);
            break;
        }
        case OBJ_EXPR_ARRAYELEMENT: {
            markExpr(object);
            ObjExprArrayElement* array = (ObjExprArrayElement*)object;
            markObject((Obj*)array->element);
            markObject((Obj*)array->assignment);
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
        case OBJ_NATIVE:
        case OBJ_BLOB:
        case OBJ_CHANNEL:
        case OBJ_STRING:
            break;
    }
}

static void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    PRINTERR("%p free type %d\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJ_BOUND_METHOD:
            FREE(ObjBoundMethod, object);
            break;
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            freeTable(&klass->methods);
            FREE(ObjClass, object);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            freeTable(&instance->fields);
            FREE(ObjInstance, object);
            break;
        }
        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;
        case OBJ_BLOB: {
            ObjBlob* blob = (ObjBlob*)object;
            free(blob->blob);
            FREE(ObjBlob, object);
            break;
        }
        case OBJ_ROUTINE:
            FREE(ObjRoutine, object);
            break;
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
        case OBJ_CHANNEL:
            FREE(ObjChannel, object);
            break;
        case OBJ_VALARRAY: {
            ObjValArray* array = (ObjValArray*)object;
            freeValueArray(&array->array);
            FREE(ObjValArray, object);
            break;
        }
        case OBJ_UNIFORMARRAY: {
            ObjUniformArray* array = (ObjUniformArray*)object;
            reallocate(array->array, array->count * array->element_size, 0);
            array->array = NULL;    
            FREE(ObjUniformArray, object);
            break;
        }
        case OBJ_YARGTYPE: {
            FREE(ObjYargType, object);
            break;
        }
        case OBJ_AST: {
            ObjAst* ast = (ObjAst*)object;
            freeTable(&ast->constants);
            break;
        }
        case OBJ_STMT_RETURN: // fall through
        case OBJ_STMT_YIELD:
        case OBJ_STMT_PRINT:
        case OBJ_STMT_EXPRESSION: FREE(ObjStmtExpression, object); break;
        case OBJ_STMT_VARDECLARATION: FREE(ObjStmtVarDeclaration, object); break;
        case OBJ_STMT_BLOCK: FREE(ObjStmtBlock, object); break;
        case OBJ_STMT_IF: FREE(ObjStmtIf, object); break;
        case OBJ_STMT_FUNDECLARATION: {
            ObjStmtFunDeclaration* fun = (ObjStmtFunDeclaration*)object;
            freeDynamicObjArray(&fun->parameters);
            FREE(ObjStmtFunDeclaration, object); 
            break;
        }
        case OBJ_STMT_WHILE: FREE(ObjStmtWhile, object); break;
        case OBJ_STMT_FOR: FREE(ObjStmtFor, object); break;
        case OBJ_STMT_CLASSDECLARATION: {
            ObjStmtClassDeclaration* decl = (ObjStmtClassDeclaration*)object;
            freeDynamicObjArray(&decl->methods);
            FREE(ObjStmtClassDeclaration, object);
            break;
        }
        case OBJ_STMT_STRUCTDECLARATION: {
            ObjStmtStructDeclaration* struct_ = (ObjStmtStructDeclaration*)object;
            freeTable(&struct_->fields);
            FREE(ObjStmtStructDeclaration, object); 
            break;
        }
        case OBJ_STMT_FIELDDECLARATION: FREE(ObjStmtFieldDeclaration, object); break;
        case OBJ_EXPR_NUMBER: FREE(ObjExprNumber, object); break;
        case OBJ_EXPR_OPERATION: FREE(ObjExprOperation, object); break;
        case OBJ_EXPR_GROUPING: FREE(ObjExprGrouping, object); break;
        case OBJ_EXPR_NAMEDVARIABLE: FREE(ObjExprNamedVariable, object); break;
        case OBJ_EXPR_NAMEDCONSTANT: FREE(ObjExprNamedConstant, object); break;
        case OBJ_EXPR_LITERAL: FREE(ObjExprLiteral, object); break;
        case OBJ_EXPR_STRING: FREE(ObjExprString, object); break;
        case OBJ_EXPR_CALL: {
            ObjExprCall* call = (ObjExprCall*)object;
            freeDynamicObjArray(&call->arguments);
            FREE(ObjExprCall, object); 
            break;
        }
        case OBJ_EXPR_ARRAYINIT: {
            ObjExprArrayInit* init = (ObjExprArrayInit*)object;
            freeDynamicObjArray(&init->initializers);
            FREE(ObjExprArrayInit, object); 
            break;
        }
        case OBJ_EXPR_ARRAYELEMENT: FREE(ObjExprArrayElement, object); break;
        case OBJ_EXPR_BUILTIN: FREE(ObjExprBuiltin, object); break;
        case OBJ_EXPR_DOT: FREE(ObjExprDot, object); break;
        case OBJ_EXPR_SUPER: FREE(ObjExprSuper, object); break;
        case OBJ_EXPR_TYPE: FREE(ObjExprType, object); break;
    }
}

static void markRoots() {

    // Don't use markObject, as this is not on the heap.
    markRoutine(&vm.core0);
    
    markObject((Obj*)vm.core1);
    for (int i = 0; i < MAX_PINNED_ROUTINES; i++) {
        markObject((Obj*)vm.pinnedRoutines[i]);
    }

    for (Value* slot = vm.tempRoots; slot < vm.tempRootsTop; slot++) {
        markValue(*slot);
    }

    markCellTable(&vm.globals);
    markCompilerRoots();
    markObject((Obj*)vm.initString);
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

void collectGarbage() {

    platform_mutex_enter(&vm.heap);

#ifdef DEBUG_LOG_GC
    PRINTERR("-- gc begin\n");
    size_t before = vm.bytesAllocated;
#endif

    markRoots();
    traceReferences();
    tableRemoveWhite(&vm.strings);
    sweep();

    size_t candidateGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;
    vm.nextGC = candidateGC > ALWAYS_GC_ABOVE ? ALWAYS_GC_ABOVE : candidateGC;

#ifdef DEBUG_LOG_GC
    PRINTERR("-- gc end\n");
    PRINTERR("   collected %zu bytes (from %zu to %zu) next at %zu\n",
             before - vm.bytesAllocated, before, vm.bytesAllocated,
             vm.nextGC);
#endif

    platform_mutex_leave(&vm.heap);
}

void freeObjects() {
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }

    free(vm.grayStack);
}