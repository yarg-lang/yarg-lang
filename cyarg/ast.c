#include <stdlib.h>
#include <stdio.h>

#include "ast.h"
#include "memory.h"

ObjAst* newObjAst() {
    ObjAst* ast = ALLOCATE_OBJ(ObjAst, OBJ_AST);
    initTable(&ast->constants);
    return ast;
}

ObjStmtExpression* newStmtExpression(ObjExpr* expr, ObjType type, int line) {
    ObjStmtExpression* stmt = ALLOCATE_OBJ(ObjStmtExpression, type);
    stmt->stmt.line = line;
    stmt->expression = expr;

    return stmt;
}

ObjStmtVarDeclaration* newStmtVarDeclaration(const char* name, int nameLength, int line) {
    ObjStmtVarDeclaration* stmt = ALLOCATE_OBJ(ObjStmtVarDeclaration, OBJ_STMT_VARDECLARATION);
    tempRootPush(OBJ_VAL(stmt));
    stmt->stmt.line = line;
    stmt->name = copyString(name, nameLength);
    tempRootPop();
    return stmt;
}

ObjStmtPlaceDeclaration* newStmtPlaceDeclaration(const char* name, int nameLength, int line) {
    ObjStmtPlaceDeclaration* stmt = ALLOCATE_OBJ(ObjStmtPlaceDeclaration, OBJ_STMT_PLACEDECLARATION);
    tempRootPush(OBJ_VAL(stmt));
    stmt->stmt.line = line;
    stmt->name = copyString(name, nameLength);
    tempRootPop();
    return stmt;
}

ObjStmtBlock* newStmtBlock(int line) {
    ObjStmtBlock* block = ALLOCATE_OBJ(ObjStmtBlock, OBJ_STMT_BLOCK);
    block->stmt.nextStmt = NULL;
    block->stmt.line = line;
    block->statements = NULL;
    return block;
}

ObjStmtIf* newStmtIf(int line) {
    ObjStmtIf* ctrl = ALLOCATE_OBJ(ObjStmtIf, OBJ_STMT_IF);
    ctrl->stmt.nextStmt = NULL;
    ctrl->stmt.line = line;
    ctrl->test = NULL;
    ctrl->ifStmt = NULL;
    ctrl->elseStmt = NULL;
    return ctrl;
}

ObjStmtFunDeclaration* newStmtFunDeclaration(const char* name, int nameLength, int line) {
    ObjStmtFunDeclaration* fun = ALLOCATE_OBJ(ObjStmtFunDeclaration, OBJ_STMT_FUNDECLARATION);
    fun->stmt.line = line;
    initDynamicObjArray(&fun->parameters);
    tempRootPush(OBJ_VAL(fun));
    fun->name = copyString(name, nameLength);
    tempRootPop();
    return fun;
}

ObjStmtWhile* newStmtWhile(int line) {
    ObjStmtWhile* loop = ALLOCATE_OBJ(ObjStmtWhile, OBJ_STMT_WHILE);
    loop->stmt.nextStmt = NULL;
    loop->stmt.line = line;
    loop->test = NULL;
    loop->loop = NULL;
    return loop;
}

ObjStmtFor* newStmtFor(int line) {
    ObjStmtFor* loop = ALLOCATE_OBJ(ObjStmtFor, OBJ_STMT_FOR);
    loop->stmt.nextStmt = NULL;
    loop->stmt.line = line;
    loop->condition = NULL;
    loop->initializer = NULL;
    loop->loopExpression = NULL;
    loop->body = NULL;
    return loop;
}

ObjStmtClassDeclaration* newStmtClassDeclaration(const char* name, int nameLength, int line) {
    ObjStmtClassDeclaration* decl = ALLOCATE_OBJ(ObjStmtClassDeclaration, OBJ_STMT_CLASSDECLARATION);
    decl->stmt.line = line;
    initDynamicObjArray(&decl->methods);
    tempRootPush(OBJ_VAL(decl));
    decl->name = copyString(name, nameLength);
    tempRootPop();
    return decl;
}

ObjExprOperation* newExprOperation(ObjExpr* rhs, ExprOp op) {
    ObjExprOperation* operation = ALLOCATE_OBJ(ObjExprOperation, OBJ_EXPR_OPERATION);
    operation->rhs = rhs;
    operation->operation = op;

    return operation;
}

ObjExprGrouping* newExprGrouping(ObjExpr* expression) {
    ObjExprGrouping* grp = ALLOCATE_OBJ(ObjExprGrouping, OBJ_EXPR_GROUPING);
    grp->expr.nextExpr = NULL;
    grp->expression = expression;
    return grp;
}


ObjExprNumber* newExprNumberDouble(double value) {
    ObjExprNumber* num = ALLOCATE_OBJ(ObjExprNumber, OBJ_EXPR_NUMBER);
    num->expr.nextExpr = NULL;
    num->type = NUMBER_DOUBLE;
    num->val.dbl = value;
    return num;
}

ObjExprNumber* newExprNumberInteger(int value) {
    ObjExprNumber* num = ALLOCATE_OBJ(ObjExprNumber, OBJ_EXPR_NUMBER);
    num->expr.nextExpr = NULL;
    num->type = NUMBER_INTEGER;
    num->val.integer = value;
    return num;
}

ObjExprNumber* newExprNumberUInteger32(uint32_t value) {
    ObjExprNumber* num = ALLOCATE_OBJ(ObjExprNumber, OBJ_EXPR_NUMBER);
    num->expr.nextExpr = NULL;
    num->type = NUMBER_UINTEGER32;
    num->val.uinteger32 = value;
    return num;
}

ObjExprNamedVariable* newExprNamedVariable(const char* name, int nameLength) {
    ObjExprNamedVariable* var = ALLOCATE_OBJ(ObjExprNamedVariable, OBJ_EXPR_NAMEDVARIABLE);
    tempRootPush(OBJ_VAL(var));
    var->name = copyString(name, nameLength);
    tempRootPop();
    return var;
}

ObjExprNamedConstant* newExprNamedConstant(const char* name, int nameLength) {
    ObjExprNamedConstant* var = ALLOCATE_OBJ(ObjExprNamedConstant, OBJ_EXPR_NAMEDCONSTANT);
    tempRootPush(OBJ_VAL(var));
    var->name = copyString(name, nameLength);
    tempRootPop();
    return var;
}


ObjExprLiteral* newExprLiteral(ExprLiteral literal) {
    ObjExprLiteral* lit = ALLOCATE_OBJ(ObjExprLiteral, OBJ_EXPR_LITERAL);
    lit->expr.nextExpr = NULL;
    lit->literal = literal;
    return lit;
}

ObjExprString* newExprString(const char* str, int strLength) {
    ObjExprString* string = ALLOCATE_OBJ(ObjExprString, OBJ_EXPR_STRING);
    string->expr.nextExpr = NULL;
    string->string = NULL;
    tempRootPush(OBJ_VAL(string));
    string->string = copyString(str, strLength);
    tempRootPop();
    return string;
}

ObjExprCall* newExprCall() {
    ObjExprCall* call = ALLOCATE_OBJ(ObjExprCall, OBJ_EXPR_CALL);
    initDynamicObjArray(&call->arguments);
    return call;
}

ObjExprArrayInit* newExprArrayInit() {
    ObjExprArrayInit* array = ALLOCATE_OBJ(ObjExprArrayInit, OBJ_EXPR_ARRAYINIT);
    initDynamicObjArray(&array->initializers);
    return array;
}

ObjExprArrayElement* newExprArrayElement() {
    ObjExprArrayElement* array = ALLOCATE_OBJ(ObjExprArrayElement, OBJ_EXPR_ARRAYELEMENT);
    array->expr.nextExpr = NULL;
    array->element = NULL;
    array->assignment = NULL;
    return array;
}

ObjExprBuiltin* newExprBuiltin(ExprBuiltin fn, int arity) {
    ObjExprBuiltin* builtin = ALLOCATE_OBJ(ObjExprBuiltin, OBJ_EXPR_BUILTIN);
    builtin->expr.nextExpr = NULL;
    builtin->builtin = fn;
    builtin->arity = arity;
    return builtin;
}

ObjExprDot* newExprDot(const char* name, int nameLength) {
    ObjExprDot* expr = ALLOCATE_OBJ(ObjExprDot, OBJ_EXPR_DOT);
    tempRootPush(OBJ_VAL(expr));
    expr->name = copyString(name, nameLength);
    tempRootPop();
    return expr;
}

ObjExprSuper* newExprSuper(const char* name, int nameLength) {
    ObjExprSuper* expr = ALLOCATE_OBJ(ObjExprSuper, OBJ_EXPR_SUPER);
    tempRootPush(OBJ_VAL(expr));
    expr->name = copyString(name, nameLength);
    tempRootPop();
    return expr;
}

ObjExprTypeLiteral* newExprType(ExprTypeLiteral type) {
    ObjExprTypeLiteral* expr = ALLOCATE_OBJ(ObjExprTypeLiteral, OBJ_EXPR_TYPE);
    expr->type = type;
    return expr;
}

ObjExprTypeStruct* newExprTypeStruct() {
    ObjExprTypeStruct* expr = ALLOCATE_OBJ(ObjExprTypeStruct, OBJ_EXPR_TYPE_STRUCT);
    tempRootPush(OBJ_VAL(expr));
    initTable(&expr->fieldsByName);
    initValueArray(&expr->fieldsByIndex);
    tempRootPop();
    return expr;
}

ObjStmtFieldDeclaration* newStmtFieldDeclaration(const char* name, int nameLength, int line) {
    ObjStmtFieldDeclaration* decl = ALLOCATE_OBJ(ObjStmtFieldDeclaration, OBJ_STMT_FIELDDECLARATION);
    decl->stmt.line = line;
    tempRootPush(OBJ_VAL(decl));
    decl->name = copyString(name, nameLength);
    tempRootPop();
    return decl;
}

static int indendation = 0;

void printIndentation() {
    for (int i = 0; i < indendation; i++) {
        printf("  ");
    }
}

static void printExprOperation(ObjExprOperation* opexpr) {
    switch (opexpr->operation) {
        case EXPR_OP_EQUAL: printf("=="); break;
        case EXPR_OP_GREATER: printf(">"); break;
        case EXPR_OP_RIGHT_SHIFT: printf(">>"); break;
        case EXPR_OP_LESS: printf("<"); break;
        case EXPR_OP_LEFT_SHIFT: printf("<<"); break;
        case EXPR_OP_ADD: printf("+"); break;
        case EXPR_OP_SUBTRACT: printf("-"); break;
        case EXPR_OP_MULTIPLY: printf("*"); break;
        case EXPR_OP_DIVIDE: printf("/"); break;
        case EXPR_OP_BIT_OR: printf("|"); break;
        case EXPR_OP_BIT_AND: printf("&"); break;
        case EXPR_OP_BIT_XOR: printf("^"); break;
        case EXPR_OP_MODULO: printf("%%"); break;
        case EXPR_OP_NOT_EQUAL: printf("!="); break;
        case EXPR_OP_GREATER_EQUAL: printf(">="); break;
        case EXPR_OP_LESS_EQUAL: printf("<="); break;
        case EXPR_OP_NOT: printf("!"); break;
        case EXPR_OP_NEGATE: printf("-"); break;
        case EXPR_OP_LOGICAL_AND: printf(" and "); break;
        case EXPR_OP_LOGICAL_OR: printf(" or "); break;
        case EXPR_OP_DEREF_PTR: printf("*"); break;
    }
    printf("(");
    printExpr(opexpr->rhs);
    printf(")");
    if (opexpr->assignment) {
        printf(" = ");
        printExpr(opexpr->assignment);
    }
}

void printCallArgs(DynamicObjArray* args) {
    printf("(");
    for (int i = 0; i < args->objectCount; i++) {
        printExpr((ObjExpr*)args->objects[i]);
        if (i < args->objectCount - 1) {
            printf(", ");
        }
    }
    printf(")");
}

void printExprDot(ObjExprDot* dot) {
    printf(".");
    printObject(OBJ_VAL(dot->name));
    if (dot->assignment) {
        printf(" = ");
        printExpr(dot->assignment);
    } else if (dot->call) {
        printExpr((ObjExpr*)dot->call);
    }
}

void printExprSuper(ObjExprSuper* expr) {
    printf("super.");
    printObject(OBJ_VAL(expr->name));
    if (expr->call) {
        printExpr((ObjExpr*)expr->call);
    }
}

void printExprBuiltin(ObjExprBuiltin* fn) {
    switch (fn->builtin) {
        case EXPR_BUILTIN_RPOKE: printf("rpoke"); break;
        case EXPR_BUILTIN_IMPORT: printf("import"); break;
        case EXPR_BUILTIN_MAKE_ARRAY: printf("make_array"); break;
        case EXPR_BUILTIN_MAKE_ROUTINE: printf("make_routine"); break;
        case EXPR_BUILTIN_MAKE_CHANNEL: printf("make_channel"); break;
        case EXPR_BUILTIN_RESUME: printf("resume"); break;
        case EXPR_BUILTIN_START: printf("start"); break;
        case EXPR_BUILTIN_RECEIVE: printf("receive"); break;
        case EXPR_BUILTIN_SEND: printf("send"); break;
        case EXPR_BUILTIN_PEEK: printf("peek"); break;
        case EXPR_BUILTIN_SHARE: printf("share"); break;
        case EXPR_BUILTIN_RPEEK: printf("rpeek"); break;
        case EXPR_BUILTIN_LEN: printf("len"); break;
        case EXPR_BUILTIN_PIN: printf("pin"); break;
        case EXPR_BUILTIN_NEW: printf("new"); break;
    }
}

void printType(ObjExpr* type) {
    if (type->obj.type == OBJ_EXPR_LITERAL) {
        ObjExprLiteral* literal = (ObjExprLiteral*)type;
        if (literal->literal == EXPR_LITERAL_NIL) {
            printf("any");
        }
        return;
    }
    else if (type->obj.type == OBJ_EXPR_TYPE) {
        ObjExprTypeLiteral* typeObject = (ObjExprTypeLiteral*)type;

        switch (typeObject->type) {
            case EXPR_TYPE_LITERAL_MFLOAT64: printf("mfloat64"); break;
            case EXPR_TYPE_LITERAL_MUINT32: printf("muint32"); break;
            case EXPR_TYPE_LITERAL_INTEGER: printf("integer"); break;
            case EXPR_TYPE_LITERAL_BOOL: printf("bool"); break;
            case EXPR_TYPE_LITERAL_STRING: printf("string"); break;
            case EXPR_TYPE_MODIFIER_ARRAY: printf("[]"); break;
            case EXPR_TYPE_MODIFIER_CONST: printf("<const>"); break;
            default: printf("<unknown>"); break;
        }
    } else {
        printf("<unexpected type>");
    }
}

void printStructType(ObjExprTypeStruct* struct_) {
    printf("struct {\n");
    for (int i = 0; i < struct_->fieldsByIndex.count; i++) {
        Value x = struct_->fieldsByIndex.values[i];
        Obj* val = AS_OBJ(x);
        printStmts((ObjStmt*) val);
    }
    printf("}");
}

void printExpr(ObjExpr* expr) {
    ObjExpr* cursor = expr;
    while (cursor) {
        switch (cursor->obj.type) {
            case OBJ_EXPR_OPERATION: printExprOperation((ObjExprOperation*)cursor); break;
            case OBJ_EXPR_GROUPING: {
                ObjExprGrouping* expr = (ObjExprGrouping*)cursor;
                printf("(");
                printExpr(expr->expression);
                printf(")");
                break;
            }
            case OBJ_EXPR_NUMBER: {
                ObjExprNumber* num = (ObjExprNumber*)cursor;
                switch (num->type) {
                    case NUMBER_DOUBLE:
                        printf("%f", num->val.dbl);
                        break;
                    case NUMBER_INTEGER:
                        printf("%d", num->val.integer);
                        break;
                    case NUMBER_UINTEGER32:
                        printf("u%u", num->val.uinteger32);
                        break;
                }
                break;
            }
            case OBJ_EXPR_NAMEDVARIABLE: {
                ObjExprNamedVariable* var = (ObjExprNamedVariable*)cursor;
                printObject(OBJ_VAL(var->name));
                if (var->assignment) {
                    printf(" = ");
                    printExpr(var->assignment);
                }
                break;
            }
            case OBJ_EXPR_LITERAL: {
                ObjExprLiteral* lit = (ObjExprLiteral*)cursor;
                switch (lit->literal) {
                    case EXPR_LITERAL_FALSE: printf("false"); break;
                    case EXPR_LITERAL_TRUE: printf("true"); break;
                    case EXPR_LITERAL_NIL: printf("nil"); break;
                }
                break;
            }
            case OBJ_EXPR_STRING: {
                ObjExprString* str = (ObjExprString*)cursor;
                printf("\"");
                printObject(OBJ_VAL(str->string));
                printf("\"");
                break;
            }
            case OBJ_EXPR_CALL: {
                ObjExprCall* call = (ObjExprCall*)cursor;
                printCallArgs(&call->arguments);
                break;
            }
            case OBJ_EXPR_ARRAYINIT: {
                ObjExprArrayInit* array = (ObjExprArrayInit*)cursor;
                printf("[");
                for (int i = 0; i < array->initializers.objectCount; i++) {
                    printExpr((ObjExpr*)array->initializers.objects[i]);
                    printf(", ");
                }
                printf("]");
                break;
            }
            case OBJ_EXPR_ARRAYELEMENT: {
                ObjExprArrayElement* array = (ObjExprArrayElement*)cursor;
                printf("[");
                printExpr(array->element);
                printf("]");
                if (array->assignment) {
                    printf(" = ");
                    printExpr(array->assignment);
                }
                break;
            }
            case OBJ_EXPR_BUILTIN: printExprBuiltin((ObjExprBuiltin*)cursor); break;
            case OBJ_EXPR_DOT: printExprDot((ObjExprDot*)cursor); break;
            case OBJ_EXPR_SUPER: printExprSuper((ObjExprSuper*)cursor); break;
            case OBJ_EXPR_TYPE: printType(cursor); break;
            case OBJ_EXPR_TYPE_STRUCT: printStructType((ObjExprTypeStruct*)cursor); break;
            default: printf("<unknown>"); break;
        }
        cursor = cursor->nextExpr;
    }
}

void printStmtIf(ObjStmtIf* ctrl) {
    printf("if (");
    printExpr(ctrl->test);
    printf(")\n");
    printStmts(ctrl->ifStmt);
    if (ctrl->elseStmt) {
        printf("else\n");
        printStmts(ctrl->elseStmt);
    }
}

void printFunDeclaration(ObjStmtFunDeclaration* decl) {
    printf("fun ");
    printObject(OBJ_VAL(decl->name));
    printCallArgs(&decl->parameters);
    printf("\n");
    printIndentation();
    printf("{\n");
    indendation++;
    printStmts(decl->body);
    indendation--;
    printIndentation();
    printf("}");
}

void printStmtWhile(ObjStmtWhile* loop) {
    printf("while (");
    printExpr(loop->test);
    printf(")\n");
    printStmts(loop->loop);
}

void printStmtFor(ObjStmtFor* loop) {
    printf("for (");
    printStmts(loop->initializer);
    printExpr(loop->condition);
    printf("; ");
    printExpr(loop->loopExpression);
    printf(")\n");
    printStmts(loop->body);
}

void printStmtClassDeclaration(ObjStmtClassDeclaration* class_) {
    printf("class ");
    printObject(OBJ_VAL(class_->name));
    if (class_->superclass) {
        printf(" < ");
        printExpr(class_->superclass);
    }
    printf("\n{\n");
    indendation++;
    for (int i = 0; i < class_->methods.objectCount; i++) {
        printf("  ");
        printFunDeclaration((ObjStmtFunDeclaration*)class_->methods.objects[i]);
        printf("\n");
    }
    indendation--;
    printIndentation();
    printf("}");
}

void printStmtExpression(ObjStmtExpression* stmt) {
    switch (stmt->stmt.obj.type) {
        case OBJ_STMT_RETURN: printf("return "); break;
        case OBJ_STMT_YIELD: printf("yield "); break;
        case OBJ_STMT_PRINT: printf("print "); break;
        case OBJ_STMT_EXPRESSION: break;
        default: break;
    }
    printExpr(stmt->expression);
    printf(";");
}

void printStmtVarDeclaration(ObjStmtVarDeclaration* decl) {
    printf("var ");
    if (decl->type) {
        printExpr((ObjExpr*)decl->type);
    }
    printf(" ");
    printObject(OBJ_VAL(decl->name));
    if (decl->initialiser) {
        printf(" = ");
        printExpr(decl->initialiser);
    }
    printf(";");
}

static void printStmtPlaceDeclaration(ObjStmtPlaceDeclaration* decl) {
    printf("place ");
    printExpr(decl->type);
    printf(" ");
    printExpr(decl->location);
    printf(" ");
    printObject(OBJ_VAL(decl->name));
    printf(";");
}

static void printStmtFieldDeclaration(ObjStmtFieldDeclaration* decl) {
    if (   decl->type->obj.type != OBJ_EXPR_LITERAL
        && ((ObjExprLiteral*)decl->type)->literal != EXPR_LITERAL_NIL) {
        printExpr(decl->type);
        printf(" ");
    }

    printObject(OBJ_VAL(decl->name));
    printf(";");
}

void printStmtBlock(ObjStmtBlock* block) {
    printf("{\n");
    indendation++;
    printStmts(block->statements);
    indendation--;
    printf("}\n");
}

void printStmts(ObjStmt* stmts) {
    ObjStmt* cursor = stmts;
    while (cursor) {
        printIndentation();
        switch (cursor->obj.type) {
            case OBJ_STMT_RETURN: // fall through
            case OBJ_STMT_YIELD:
            case OBJ_STMT_PRINT:
            case OBJ_STMT_EXPRESSION: printStmtExpression((ObjStmtExpression*)cursor); break;
            case OBJ_STMT_VARDECLARATION: printStmtVarDeclaration((ObjStmtVarDeclaration*)cursor); break;
            case OBJ_STMT_PLACEDECLARATION: printStmtPlaceDeclaration((ObjStmtPlaceDeclaration*)cursor); break;
            case OBJ_STMT_BLOCK: printStmtBlock((ObjStmtBlock*)cursor); break;
            case OBJ_STMT_IF: printStmtIf((ObjStmtIf*)cursor); break;
            case OBJ_STMT_FUNDECLARATION: printFunDeclaration((ObjStmtFunDeclaration*)cursor); break;
            case OBJ_STMT_WHILE: printStmtWhile((ObjStmtWhile*)cursor); break;
            case OBJ_STMT_FOR: printStmtFor((ObjStmtFor*)cursor); break;
            case OBJ_STMT_CLASSDECLARATION: printStmtClassDeclaration((ObjStmtClassDeclaration*)cursor); break;
            case OBJ_STMT_FIELDDECLARATION: printStmtFieldDeclaration((ObjStmtFieldDeclaration*)cursor); break;
            default: printf("Unknown stmt;"); break;
        }
        printf("\n");
        cursor = cursor->nextStmt;
    }
}

void printSourceValue(FILE* op, Value value) {
    if (IS_STRING(value)) {
        fprintf(op, "\"%s\"", AS_CSTRING(value));
        return;
    } else {
        fprintValue(op, value);
    }
}
