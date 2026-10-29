#include "parser.h"

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>

#include "ast.h"
#include "memory.h"
#include "yargobject.h"

#include "scanner.h"
#include "routine.h"
#include "packed_containers.h"

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + - | ^ & %
    PREC_FACTOR,     // * / << >>
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_DEREF,      // []
    PREC_PRIMARY
} Precedence;

typedef struct ObjParser {
    Obj obj;
    Token current;
    Token previous;
    bool panicMode;
    ObjRoutine* context;
    ObjScanner* scanner;
    ObjExpr* prevExpr;
    ObjAst* ast;
    DynamicObjArray errors;
} ObjParser;

ObjParser* newParser(ObjRoutine* context, ObjAst* ast, ObjScanner* scanner) {
    ObjParser* parser = ALLOCATE_OBJ(ObjParser, OBJ_PARSER);
    push(context, IMPL_OBJ_VAL((Obj*)parser));
    parser->panicMode = false;
    parser->context = context;
    parser->ast = ast;
    parser->prevExpr = NULL;
    parser->scanner = scanner;
    initDynamicObjArray(&parser->errors);
    pop(context);
    return parser;
}

void markParser(ObjParser* parser) {
    markObject((Obj*)parser->ast);
    markObject((Obj*)parser->prevExpr);
    markObject((Obj*)parser->scanner);
    markObject((Obj*)parser->context);
    markDynamicObjArray(&parser->errors);
}

void gc_freeParser(ObjParser* parser) {
    gc_freeDynamicObjArray(&parser->errors);
}

void pushWorkingNode(ObjParser* parser, Obj* obj) {
    push(parser->context, IMPL_OBJ_VAL(obj));
}

void popWorkingNode(ObjParser* parser) {
    pop(parser->context);
} 

typedef ObjExpr* (*AstParseFn)(ObjParser* parser, bool canAssign);

typedef struct {
    AstParseFn prefix;
    AstParseFn infix;
    Precedence precedence;
} AstParseRule;

static AstParseRule* getRule(TokenType type);
static ObjExpr* parsePrecedence(ObjParser* parser, Precedence precedence);
static ObjExpr* expression(ObjParser* parser);
static ObjExpr* typeExpression(ObjParser* parser);

void errorAt(ObjParser* parser, Token* token, const char* report) {
    if (parser->panicMode) return;
    parser->panicMode = true;

    size_t messageLength = DECIMAL_DIGITS_BOUND(int) + 13; // strlen("[line ] Error");
    if (token->type == TOKEN_EOF) {
        messageLength += 7; // strlen(" at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        messageLength += 6; // strlen(" at ''")
        messageLength += token->length;
    }
    messageLength += strlen(report) + 2; // ": " and report itself

    char* message = ALLOCATE(char, messageLength + 1);
    snprintf(message, messageLength + 1, "[line %d] Error", token->line);
    if (token->type == TOKEN_EOF) {
        strcat(message, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        strcat(message, " at '");
        strncat(message, token->start, token->length);
        strcat(message, "'");
    }
    strcat(message, ": ");
    strcat(message, report);
    ObjString* errorString = takeString(message, messageLength);

    appendToDynamicObjArray(&parser->errors, (Obj*)errorString);
}

void error(ObjParser* parser, const char* message) {
    errorAt(parser, &parser->previous, message);
}

void errorAtCurrent(ObjParser* parser, const char* message) {
    errorAt(parser, &parser->current, message);
}

bool check(ObjParser* parser, TokenType type) {
    return parser->current.type == type;
}

static void advance(ObjParser* parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = scanToken(parser->scanner);
        if (parser->current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser, parser->current.start);
    }
}

static bool match(ObjParser* parser, TokenType type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static void synchronize(ObjParser* parser) {
    parser->panicMode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON) return;
        switch (parser->current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
            case TOKEN_LOAD:
                return;
            
            default:
                ; // Do nothing.
        }

        advance(parser);
    }
}

void consume(ObjParser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }

    errorAtCurrent(parser, message);
}

static bool checkTypeToken(ObjParser* parser) {
    switch (parser->current.type) {
        case TOKEN_MACHINE_FLOAT64:
        case TOKEN_INT8:
        case TOKEN_UINT8:
        case TOKEN_INT16:
        case TOKEN_UINT16:
        case TOKEN_INT32:
        case TOKEN_UINT32:
        case TOKEN_INT64:
        case TOKEN_UINT64:
        case TOKEN_BOOL:
        case TOKEN_TYPE_STRING:
        case TOKEN_ANY:
        case TOKEN_STRUCT:
        case TOKEN_INT:
            return true;
        default:
            return false;
    }
}


static uint64_t strtoNum(const char* literal, int length, int radix) {
    uint64_t val = 0;
    for (int i = length - 1 ; i >= 0; i--) {
        uint64_t positionVal = 0;
        switch (literal[i]) {
            case '0': positionVal = 0; break;
            case '1': positionVal = 1; break;
            case '2': positionVal = 2; break;
            case '3': positionVal = 3; break;
            case '4': positionVal = 4; break;
            case '5': positionVal = 5; break;
            case '6': positionVal = 6; break;
            case '7': positionVal = 7; break;
            case '8': positionVal = 8; break;
            case '9': positionVal = 9; break;
            case 'A': positionVal = 10; break;
            case 'B': positionVal = 11; break;
            case 'C': positionVal = 12; break;
            case 'D': positionVal = 13; break;
            case 'E': positionVal = 14; break;
            case 'F': positionVal = 15; break;
            case 'a': positionVal = 10; break;
            case 'b': positionVal = 11; break;
            case 'c': positionVal = 12; break;
            case 'd': positionVal = 13; break;
            case 'e': positionVal = 14; break;
            case 'f': positionVal = 15; break;
        }
        uint64_t power = length - 1 - i;
        val += positionVal * pow(radix, power);
    }
    return val;
}

static ObjExpr* namedVariable(ObjParser* parser, Token name, bool canAssign) {

    ObjString* nameString = copyString(name.start, name.length);
    pushWorkingNode(parser, (Obj*)nameString);

    ObjExprNamedVariable* expr = newExprNamedVariable(parser->context, name.start, name.length);

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        pushWorkingNode(parser, (Obj*)expr);
        expr->assignment = expression(parser);
        popWorkingNode(parser);
    }

    popWorkingNode(parser);
    return (ObjExpr*)expr;
}

static void expressionList(ObjParser* parser, DynamicObjArray* items) {

    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            appendToDynamicObjArray(items, (Obj*) expression(parser));
            if (items->objectCount > 255) {
                error(parser, "Can't have more than 255 arguments.");
            }
        } while (match(parser, TOKEN_COMMA));
    }
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' aftger arguments.");

}

static void appendInitializer(ObjParser* parser, ObjExprCollectionInitializer* collection) {
    Obj* key_or_item = (Obj*) expression(parser);
    if (match(parser, TOKEN_COLON)) {
        pushWorkingNode(parser, key_or_item);
        Obj* value = (Obj*) expression(parser);
        pushWorkingNode(parser, value);
        ObjExprPair* pair = newExprPair((ObjExpr*)key_or_item, (ObjExpr*)value);
        appendToDynamicObjArray(&collection->initializers, (Obj*)pair);
        collection->isMap = true;
        popWorkingNode(parser);
        popWorkingNode(parser);
    } else {
        appendToDynamicObjArray(&collection->initializers, key_or_item);
    }

}

static void collectionInitializers(ObjParser* parser, ObjExprCollectionInitializer* collection) {

    if (!check(parser, TOKEN_RIGHT_SQUARE_BRACKET)) {
        do {
            appendInitializer(parser, collection);
            if (collection->initializers.objectCount > 255) {
                error(parser, "Can't have more than 255 array initialisers.");
            }

        } while (match(parser, TOKEN_COMMA));
    }
}

static ObjExpr* super_(ObjParser* parser, bool canAssign) {

    consume(parser, TOKEN_DOT, "Expect '.' after 'super'.");
    consume(parser, TOKEN_IDENTIFIER, "Expect superclass method name.");
    ObjExprSuper* super_ = newExprSuper(parser->context, parser->previous.start, parser->previous.length);
    pushWorkingNode(parser, (Obj*)super_);

    if (match(parser, TOKEN_LEFT_PAREN)) {
        super_->call = newExprCall();
        expressionList(parser, &super_->call->arguments);
    }
    popWorkingNode(parser);
    return (ObjExpr*)super_;
}

static ObjExpr* dot(ObjParser* parser, bool canAssign) { 

    consume(parser, TOKEN_IDENTIFIER, "Expect property name after '.'.");
    ObjExprDot* expr = newExprDot(parser->context, parser->previous.start, parser->previous.length);
    pushWorkingNode(parser, (Obj*)expr);
    
    if (canAssign && match(parser, TOKEN_EQUAL)) {
        expr->assignment = expression(parser);
    } else if (match(parser, TOKEN_LEFT_PAREN)) {
        expr->call = newExprCall();
        expressionList(parser, &expr->call->arguments);
    }
    popWorkingNode(parser);
    return (ObjExpr*) expr;
}

static ObjExpr* deref(ObjParser* parser, bool canAssign) {
    
    ObjExprCollectionElement* elmt = newExprCollectionElement();
    pushWorkingNode(parser, (Obj*)elmt);

    elmt->element = parsePrecedence(parser, PREC_ASSIGNMENT);   // prevents assignment within []
    consume(parser, TOKEN_RIGHT_SQUARE_BRACKET, "Expect ] after expression.");

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        elmt->assignment = expression(parser);
    }

    popWorkingNode(parser);
    return (ObjExpr*)elmt;
}

static ObjExpr* collectioninit(ObjParser* parser, bool canAssign) {

    ObjExprCollectionInitializer* collection = newExprCollectionInitializer();
    pushWorkingNode(parser, (Obj*)collection);

    collectionInitializers(parser, collection);
    consume(parser, TOKEN_RIGHT_SQUARE_BRACKET, "Expect ']' initialising map or array.");

    collection->cardinality = (ObjExpr*) newExprNumberFromCint(collection->initializers.objectCount);
    
    popWorkingNode(parser);
    return (ObjExpr*)collection;
}

static ObjExpr* call(ObjParser* parser, bool canAssign) {
    ObjExprCall* call = newExprCall();
    pushWorkingNode(parser, (Obj*)call);
    expressionList(parser, &call->arguments);
    popWorkingNode(parser);
    return (ObjExpr*)call;
}

static ObjExpr* string(ObjParser* parser, bool canAssign) {
    return (ObjExpr*) newExprString(parser->context, parser->previous.start + 1, parser->previous.length - 2);
}

static ObjExpr* literal(ObjParser* parser, bool canAssign) {     
    switch (parser->previous.type) {
        case TOKEN_FALSE: return (ObjExpr*) newExprLiteral(EXPR_LITERAL_FALSE);
        case TOKEN_NIL: return (ObjExpr*) newExprLiteral(EXPR_LITERAL_NIL);
        case TOKEN_TRUE: return (ObjExpr*) newExprLiteral(EXPR_LITERAL_TRUE);
        default: return NULL; // Unreachable.
    } 
}

static ObjExpr* builtin(ObjParser* parser, bool canAssign) {     
    switch (parser->previous.type) {
        case TOKEN_PEEK: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_PEEK, 1);
        case TOKEN_READ_YARG_SOURCE: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_READ_YARG_SOURCE, 1);
        case TOKEN_VM_READ_XIP_FILE: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_READ_XIP_FILE, 1);
        case TOKEN_COMPILE: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_COMPILE, 1);
        case TOKEN_MAKE_ROUTINE: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_MAKE_ROUTINE, 1);
        case TOKEN_MAKE_CHANNEL: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_MAKE_CHANNEL, 1);
        case TOKEN_MAKE_SYNCGROUP: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_MAKE_SYNCGROUP, 1);
        case TOKEN_RESUME: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_RESUME, 1);
        case TOKEN_SEND: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_SEND, 1);
        case TOKEN_RECEIVE: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_RECEIVE, 1);      
        case TOKEN_SHARE: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_SHARE, 1);
        case TOKEN_START: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_START, 1);
        case TOKEN_CPEEK: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_CPEEK, 1);
        case TOKEN_LEN: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_LEN, 1);
        case TOKEN_PIN: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_PIN, 1);
        case TOKEN_NEW: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_NEW, 1);
        case TOKEN_INT8: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_INT8, 1);
        case TOKEN_UINT8: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_UINT8, 1);
        case TOKEN_INT16: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_INT16, 1);
        case TOKEN_UINT16: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_UINT16, 1);
        case TOKEN_INT32: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_INT32, 1);
        case TOKEN_UINT32: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_UINT32, 1);
        case TOKEN_INT64: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_INT64, 1);
        case TOKEN_UINT64: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_UINT64, 1);
        case TOKEN_TS_SET: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_TS_SET, 1);
        case TOKEN_TS_READ: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_TS_READ, 1);
        case TOKEN_TS_WRITE: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_TS_WRITE, 1);
        case TOKEN_TS_INTERRUPT: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_TS_INTERRUPT, 1);
        case TOKEN_TS_SYNC: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_TS_SYNC, 1);
        case TOKEN_INT: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_INT, 1);
        case TOKEN_MACHINE_FLOAT64: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_MFLOAT64, 1);
        case TOKEN_TYPE_STRING: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_STRING, 1);
        case TOKEN_LOAD: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_LOAD, 1);
        default: return NULL; // Unreachable.
    } 
}

static ObjStmtFieldDeclaration* fieldStmt(ObjParser* parser) {
    ObjExpr* type = NULL; 
    if (checkTypeToken(parser)) {
        type = (ObjExpr*) typeExpression(parser);
    } else {
        type = (ObjExpr*) newExprLiteral(EXPR_LITERAL_NIL);
    }
    pushWorkingNode(parser, (Obj*)type);
    ObjExpr* offset = NULL;
    if (match(parser, TOKEN_AT)) {
        offset = expression(parser);
    } else {
        offset = (ObjExpr*) newExprLiteral(EXPR_LITERAL_NIL);
    }
    pushWorkingNode(parser, (Obj*)offset);

    consume(parser, TOKEN_IDENTIFIER, "Expect name of field.");
    ObjStmtFieldDeclaration* field = (ObjStmtFieldDeclaration*) pushNewStmtDeclaration(parser->context, OBJ_STMT_FIELDDECLARATION, parser->previous.start, parser->previous.length, parser->previous.line);
    field->type = type;
    field->offset = offset;
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after field declaration.");

    popWorkingNode(parser);
    popWorkingNode(parser);
    popWorkingNode(parser);
    return field;
}

static ObjExprTypeStruct* structExpression(ObjParser* parser) {
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' to start struct type.");

    ObjExprTypeStruct* struct_declaration = newExprTypeStruct();
    pushWorkingNode(parser, (Obj*)struct_declaration);

    while (!check(parser, TOKEN_RIGHT_BRACE)) {
        ObjStmtFieldDeclaration* field = fieldStmt(parser);
        appendToDynamicValueArray(&struct_declaration->fieldsByIndex, IMPL_OBJ_VAL(field));
    }
    
    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after field declarations.");
    popWorkingNode(parser);
    return struct_declaration;
}

static void consumeCollectionIndex(ObjParser* parser, ObjExpr* cursor) {
    if (match(parser, TOKEN_LEFT_SQUARE_BRACKET)) {

        cursor->nextExpr = (ObjExpr*) ALLOCATE_OBJ(ObjExprTypeIndexedCollection, OBJ_EXPR_TYPE_INDEXED_COLLECTION);
        ObjExprTypeIndexedCollection* collection = (ObjExprTypeIndexedCollection*)cursor->nextExpr;
            
        if (!check(parser, TOKEN_RIGHT_SQUARE_BRACKET)) {
            collection->indexing = parsePrecedence(parser, PREC_ASSIGNMENT); // no assignment in this expression.
        }

        if (!collection->indexing) {
            error(parser, "Collection type must have size or index type.");
        }

        consume(parser, TOKEN_RIGHT_SQUARE_BRACKET, "Expect ']' after array type.");
    }
}

static ObjExpr* type(ObjParser* parser, bool canAssign) {

    if (check(parser, TOKEN_LEFT_PAREN)) {
        return builtin(parser, canAssign);
    } else {
        ObjExpr* expression = NULL;

        switch (parser->previous.type) {
            case TOKEN_ANY: expression = (ObjExpr*) newExprLiteral(EXPR_LITERAL_NIL); break;
            case TOKEN_BOOL: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_BOOL); break;
            case TOKEN_INT8: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_INT8); break;
            case TOKEN_UINT8: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_UINT8); break;
            case TOKEN_INT16: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_INT16); break;
            case TOKEN_UINT16: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_UINT16); break;
            case TOKEN_INT32: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_INT32); break;
            case TOKEN_UINT32: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_UINT32); break;
            case TOKEN_INT64: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_INT64); break;
            case TOKEN_UINT64: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_UINT64); break;
            case TOKEN_MACHINE_FLOAT64: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_MFLOAT64); break;
            case TOKEN_STRUCT: expression = (ObjExpr*) structExpression(parser); break;
            case TOKEN_INT: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_INT); break;
            case TOKEN_TYPE_STRING: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_STRING); break;
            default: expression = NULL; // Unreachable
        }
        pushWorkingNode(parser, (Obj*)expression);
        consumeCollectionIndex(parser, expression);

        popWorkingNode(parser);
        return expression;
    }

}

static ObjExpr* variable(ObjParser* parser, bool canAssign) {

    return namedVariable(parser, parser->previous, canAssign);
}

static ObjExpr* this_(ObjParser* parser, bool canAssign) {     
    return variable(parser, false); 
}

static ObjExpr* unary(ObjParser* parser, bool canAssign) {
    TokenType operatorType = parser->previous.type;

    ObjExpr* rhs = parsePrecedence(parser, PREC_UNARY);
    pushWorkingNode(parser, (Obj*)rhs);

    ExprOp op;
    switch (operatorType) {
        case TOKEN_BANG: op = EXPR_OP_NOT; break;
        case TOKEN_MINUS: op = EXPR_OP_NEGATE; break;
        default: break; // Unreachable.
    }

    ObjExprOperation* expr = newExprOperation(rhs, op);
    popWorkingNode(parser);
    return (ObjExpr*) expr; 
}

static ObjExpr* unaryPlace(ObjParser* parser, bool canAssign) {
    TokenType operatorType = parser->previous.type;

    ObjExpr* rhs = parsePrecedence(parser, PREC_UNARY);
    pushWorkingNode(parser, (Obj*)rhs);

    ExprOp op;
    switch (operatorType) {
        case TOKEN_STAR: op = EXPR_OP_DEREF_PTR; break;
        default: break; // Unreachable.
    }

    ObjExprOperation* expr = newExprOperation(rhs, op);
    popWorkingNode(parser);
    pushWorkingNode(parser, (Obj*)expr);
    if (op == EXPR_OP_DEREF_PTR) {

        if (canAssign && match(parser, TOKEN_EQUAL)) {
            expr->assignment = expression(parser);
        }
    }
    popWorkingNode(parser);
    return (ObjExpr*) expr; 
}

static ObjExpr* grouping(ObjParser* parser, bool canAssign) {
    ObjExpr* expr = expression(parser);
    pushWorkingNode(parser, (Obj*)expr);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
    ObjExprGrouping* grp = newExprGrouping(expr);
    popWorkingNode(parser);
    return (ObjExpr*)grp; 
}

static ObjExpr* and_(ObjParser* parser, bool canAssign) { 
    ObjExpr* rhs = parsePrecedence(parser, PREC_AND);
    pushWorkingNode(parser, (Obj*)rhs);
    ObjExprOperation* expr = newExprOperation(rhs, EXPR_OP_LOGICAL_AND);
    popWorkingNode(parser);
    return (ObjExpr*) expr;
}

static ObjExpr* or_(ObjParser* parser, bool canAssign) {
    ObjExpr* rhs = parsePrecedence(parser, PREC_OR);
    pushWorkingNode(parser, (Obj*)rhs);
    ObjExprOperation* expr = newExprOperation(rhs, EXPR_OP_LOGICAL_OR);
    popWorkingNode(parser);
    return (ObjExpr*) expr;
}

static ObjExpr* binary(ObjParser* parser, bool canAssign) {
    TokenType operatorType = parser->previous.type;
    AstParseRule* rule = getRule(operatorType);
    ObjExpr* rhs = parsePrecedence(parser, (Precedence)(rule->precedence + 1));
    pushWorkingNode(parser, (Obj*)rhs);

    ExprOp op;
    switch (operatorType) {
        case TOKEN_EQUAL_EQUAL:   op = EXPR_OP_EQUAL; break;
        case TOKEN_GREATER:       op = EXPR_OP_GREATER; break;
        case TOKEN_RIGHT_SHIFT:   op = EXPR_OP_RIGHT_SHIFT; break;
        case TOKEN_LESS:          op = EXPR_OP_LESS; break;
        case TOKEN_LEFT_SHIFT:    op = EXPR_OP_LEFT_SHIFT; break;
        case TOKEN_PLUS:          op = EXPR_OP_ADD; break;
        case TOKEN_MINUS:         op = EXPR_OP_SUBTRACT; break;
        case TOKEN_STAR:          op = EXPR_OP_MULTIPLY; break;
        case TOKEN_SLASH:         op = EXPR_OP_DIVIDE; break;
        case TOKEN_BAR:           op = EXPR_OP_BIT_OR; break;
        case TOKEN_AMP:           op = EXPR_OP_BIT_AND; break;
        case TOKEN_CARET:         op = EXPR_OP_BIT_XOR; break;
        case TOKEN_PERCENT:       op = EXPR_OP_MODULO; break;
        case TOKEN_BANG_EQUAL:    op = EXPR_OP_NOT_EQUAL; break;
        case TOKEN_GREATER_EQUAL: op = EXPR_OP_GREATER_EQUAL; break;
        case TOKEN_LESS_EQUAL:    op = EXPR_OP_LESS_EQUAL; break;
        default:
            return NULL; // Unreachable.
    }

    ObjExprOperation* expr = newExprOperation(rhs, op);
    popWorkingNode(parser);
    return (ObjExpr*) expr;
}

static ObjExpr* number(ObjParser* parser, bool canAssign) {
    int radix = 0;
    const char* number_start = parser->previous.start;
    int number_len = parser->previous.length;
    if (number_len > 2 && (number_start[0] == '0')) {
        switch (number_start[1]) {
        case 'x': case 'X':
            radix = 16;
            break;
        case 'b': case 'B':
            radix = 2;
            break;
        case 'd': case 'D':
            radix = 10;
            break;
        }
    }
    if (radix != 0) {
        number_start = &number_start[2];
        number_len -= 2;
    } else {
        radix = 10;
    }

    ObjExprNumber* val = NULL;

    if (radix == 10) {
        // N == [0-9]
        // N+ (int)
        // N+.N+ (float)
        // N+[eE]N+ (float)
        // N+.N*[eE]N+ (float)
        // N+[eE]-N+ (float)
        // N+.N*[eE]-N+ (float)
        enum {NUMBER_START, NUMBER_ZERO, NUMBER_MSB, NUMBER_DOT, NUMBER_DOT_AND_MSB, NUMBER_FRAC, NUMBER_E, NUMBER_EN, NUMBER_EXPONENT} state = NUMBER_START;
        const char *number = number_start;
        const char *msb = number;
        while (number < number_start + number_len) {
            switch (*number) {
            case '0':
                if (state == NUMBER_START) {
                    state = NUMBER_ZERO;
                } else if (state == NUMBER_DOT || state == NUMBER_DOT_AND_MSB) {
                    state = NUMBER_FRAC;
                } else if (state == NUMBER_E || state == NUMBER_EN) {
                    state = NUMBER_EXPONENT;
                }
                break;
            case '.':
                if (state == NUMBER_START || state == NUMBER_ZERO) {
                    state = NUMBER_DOT;
                } else if (state == NUMBER_MSB) {
                    state = NUMBER_DOT_AND_MSB;
                } else {
                    errorAtCurrent(parser, "Only one decimal point allowed");
                    return 0;
                }
                break;
            case 'e': case 'E':
                if (state == NUMBER_ZERO || state == NUMBER_MSB || state == NUMBER_DOT || state == NUMBER_DOT_AND_MSB || state == NUMBER_FRAC) {
                    state = NUMBER_E;
                }  else {
                    errorAtCurrent(parser, "Only one E|e allowed");
                    return 0;
                }
                break;
            case '-':
                if (state == NUMBER_E) {
                    state = NUMBER_EN;
                } else {
                    errorAtCurrent(parser, "- only allowed after e or E");
                    return 0;
                }
                break;
            default:
                if (state == NUMBER_START || state == NUMBER_ZERO) {
                    state = NUMBER_MSB;
                    msb = number;
                } else if (state == NUMBER_DOT ||state == NUMBER_DOT_AND_MSB) {
                    state = NUMBER_FRAC;
                } else if (state == NUMBER_E || state == NUMBER_EN) {
                    state = NUMBER_EXPONENT;
                }
                break;
            }

            number++;
        }
        if (state == NUMBER_ZERO || state == NUMBER_MSB) {
            char *heapChars = ALLOCATE(char, number_len + 1);
            heapChars[number - msb] = '\0';
            memcpy(heapChars, msb, number - msb);
            val = newExprNumberInt((int)(msb - number_start + number_len));
            int_set_s(heapChars, &val->bigInt);
            vm_free(heapChars);
        } else if (state == NUMBER_DOT_AND_MSB || state == NUMBER_FRAC || state == NUMBER_EXPONENT) {
            char *end;
            val = newExprNumberDouble(strtod(number_start, &end));
            if (end - number_start != number_len) {
                errorAtCurrent(parser, "misformed float literal");
                return 0;
            }
        } else {
            errorAtCurrent(parser, "misformed number");
            return 0;
        }
    } else {
        uint64_t value = strtoNum(number_start, number_len, radix);
        int decimalDigits;
        if (value <= UINT8_MAX)
            decimalDigits = 3;
        else if (value <= UINT16_MAX)
            decimalDigits = 5;
        else if (value <= UINT32_MAX)
            decimalDigits = 10;
        else
            decimalDigits = 20;

        val = newExprNumberInt(decimalDigits);
        int_set_u(value, &val->bigInt);
    }
    return (ObjExpr*) val;
}

static ObjExpr* address(ObjParser* parser, bool canAssign) {

    const char* hex_digit_start = &parser->previous.start[2];
    int hex_digit_len = parser->previous.length - 2;
    uint64_t value = strtoNum(hex_digit_start, hex_digit_len, 16);

    ObjExprAddress* val = newExprAddress(value);
    return (ObjExpr*) val;
}

static ObjExpr* reserved(ObjParser* parser, bool canAssign) {
    error(parser, "Can't use reserved word as expression.");
    return NULL;
}

static AstParseRule rules[] = {
    [TOKEN_LEFT_PAREN]           = {grouping,  call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]          = {NULL,      NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]           = {NULL,      NULL,   PREC_NONE},
    [TOKEN_RIGHT_BRACE]          = {NULL,      NULL,   PREC_NONE},
    [TOKEN_LEFT_SQUARE_BRACKET]  = {collectioninit, deref, PREC_DEREF},
    [TOKEN_RIGHT_SQUARE_BRACKET] = {NULL,      NULL,   PREC_NONE},
    [TOKEN_COMMA]                = {NULL,      NULL,   PREC_NONE},
    [TOKEN_DOT]                  = {NULL,      dot,    PREC_CALL},
    [TOKEN_MINUS]                = {unary,     binary, PREC_TERM},
    [TOKEN_PLUS]                 = {NULL,      binary, PREC_TERM},
    [TOKEN_SEMICOLON]            = {NULL,      NULL,   PREC_NONE},
    [TOKEN_SLASH]                = {NULL,      binary, PREC_FACTOR},
    [TOKEN_STAR]                 = {unaryPlace,binary, PREC_FACTOR},
    [TOKEN_BAR]                  = {NULL,      binary, PREC_TERM},
    [TOKEN_AMP]                  = {NULL,      binary, PREC_TERM},
    [TOKEN_CARET]                = {NULL,      binary, PREC_TERM},
    [TOKEN_PERCENT]              = {NULL,      binary, PREC_TERM},
    [TOKEN_BANG]                 = {unary,     NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]           = {NULL,      binary, PREC_EQUALITY},
    [TOKEN_EQUAL]                = {NULL,      NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]          = {NULL,      binary, PREC_EQUALITY},
    [TOKEN_GREATER]              = {NULL,      binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]        = {NULL,      binary, PREC_COMPARISON},
    [TOKEN_RIGHT_SHIFT]          = {NULL,      binary, PREC_FACTOR},
    [TOKEN_LESS]                 = {NULL,      binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]           = {NULL,      binary, PREC_COMPARISON},
    [TOKEN_LEFT_SHIFT]           = {NULL,      binary, PREC_FACTOR},
    [TOKEN_ADDRESS]              = {address,   NULL,   PREC_NONE},
    [TOKEN_IDENTIFIER]           = {variable,  NULL,   PREC_NONE},
    [TOKEN_STRING]               = {string,    NULL,   PREC_NONE},
    [TOKEN_NUMBER]               = {number,    NULL,   PREC_NONE},
    [TOKEN_AND]                  = {NULL,      and_,   PREC_AND},
    [TOKEN_ANY]                  = {type,      NULL,   PREC_NONE},
    [TOKEN_BOOL]                 = {type,      NULL,   PREC_NONE},
    [TOKEN_BREAK]                = {reserved,  reserved, PREC_NONE},
    [TOKEN_CLASS]                = {NULL,      NULL,   PREC_NONE},
    [TOKEN_COMPILE]              = {builtin,   NULL,   PREC_NONE},
    [TOKEN_CONST]                = {reserved,  reserved, PREC_NONE},
    [TOKEN_CONTINUE]             = {reserved,  reserved, PREC_NONE},
    [TOKEN_CPEEK]                = {builtin,   NULL,   PREC_NONE},
    [TOKEN_ELSE]                 = {NULL,      NULL,   PREC_NONE},
    [TOKEN_FALSE]                = {literal,   NULL,   PREC_NONE},
    [TOKEN_FOR]                  = {NULL,      NULL,   PREC_NONE},
    [TOKEN_FUN]                  = {NULL,      NULL,   PREC_NONE},
    [TOKEN_IF]                   = {NULL,      NULL,   PREC_NONE},
    [TOKEN_INT8]                 = {type,      NULL,   PREC_NONE},
    [TOKEN_INT16]                = {type,      NULL,   PREC_NONE},
    [TOKEN_INT32]                = {type,      NULL,   PREC_NONE},
    [TOKEN_INT64]                = {type,      NULL,   PREC_NONE},
    [TOKEN_LEN]                  = {builtin,   NULL,   PREC_NONE},
    [TOKEN_MACHINE_FLOAT64]      = {type,      NULL,   PREC_NONE},
    [TOKEN_MAKE_CHANNEL]         = {builtin,   NULL,   PREC_NONE},
    [TOKEN_MAKE_ROUTINE]         = {builtin,   NULL,   PREC_NONE},
    [TOKEN_MAKE_SYNCGROUP]       = {builtin,   NULL,   PREC_NONE},
    [TOKEN_NEW]                  = {builtin,   NULL,   PREC_NONE},
    [TOKEN_NIL]                  = {literal,   NULL,   PREC_NONE},
    [TOKEN_OR]                   = {NULL,      or_,    PREC_OR},
    [TOKEN_PEEK]                 = {builtin,   NULL,   PREC_NONE},
    [TOKEN_PIN]                  = {builtin,   NULL,   PREC_NONE},
    [TOKEN_PLACE]                = {NULL,      NULL,   PREC_NONE},
    [TOKEN_POKE]                 = {NULL,      NULL,   PREC_NONE},
    [TOKEN_PRINT]                = {NULL,      NULL,   PREC_NONE},
    [TOKEN_READ_YARG_SOURCE]     = {builtin,   NULL,   PREC_NONE},
    [TOKEN_RECEIVE]              = {builtin,   NULL,   PREC_NONE},
    [TOKEN_RESUME]               = {builtin,   NULL,   PREC_NONE},
    [TOKEN_RETURN]               = {NULL,      NULL,   PREC_NONE},
    [TOKEN_SEND]                 = {builtin,   NULL,   PREC_NONE},
    [TOKEN_SHARE]                = {builtin,   NULL,   PREC_NONE},
    [TOKEN_START]                = {builtin,   NULL,   PREC_NONE},
    [TOKEN_TYPE_STRING]          = {type,      NULL,   PREC_NONE},
    [TOKEN_STRUCT]               = {type,      NULL,   PREC_NONE},
    [TOKEN_SUPER]                = {super_,    NULL,   PREC_NONE},
    [TOKEN_THIS]                 = {this_,     NULL,   PREC_NONE},
    [TOKEN_TRUE]                 = {literal,   NULL,   PREC_NONE},
    [TOKEN_UINT8]                = {type,      NULL,   PREC_NONE},
    [TOKEN_UINT16]               = {type,      NULL,   PREC_NONE},
    [TOKEN_UINT32]               = {type,      NULL,   PREC_NONE},
    [TOKEN_UINT64]               = {type,      NULL,   PREC_NONE},
    [TOKEN_VAR]                  = {NULL,      NULL,   PREC_NONE},
    [TOKEN_VM_READ_XIP_FILE]     = {builtin,   NULL,   PREC_NONE},
    [TOKEN_WHILE]                = {NULL,      NULL,   PREC_NONE},
    [TOKEN_YIELD]                = {NULL,      NULL,   PREC_NONE},
    [TOKEN_TS_SET]               = {builtin,   NULL,   PREC_NONE},
    [TOKEN_TS_READ]              = {builtin,   NULL,   PREC_NONE},
    [TOKEN_TS_WRITE]             = {builtin,   NULL,   PREC_NONE},
    [TOKEN_TS_INTERRUPT]         = {builtin,   NULL,   PREC_NONE},
    [TOKEN_TS_SYNC]              = {builtin,   NULL,   PREC_NONE},
    [TOKEN_INT]                  = {type,      NULL,   PREC_NONE},
    [TOKEN_LOAD]                 = {builtin,   NULL,   PREC_NONE},

    [TOKEN_ERROR]                = {NULL,      NULL,   PREC_NONE},
    [TOKEN_EOF]                  = {NULL,      NULL,   PREC_NONE},
};

static AstParseRule* getRule(TokenType type) {
    return &rules[type];
}

static ObjExpr* parsePrecedence(ObjParser* parser, Precedence precedence) {
    advance(parser);
    AstParseFn prefixRule = getRule(parser->previous.type)->prefix;
    if (prefixRule == NULL) {
        error(parser, "Expect expression.");
        return NULL;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;

    ObjExpr* expr = prefixRule(parser, canAssign);
    parser->prevExpr = expr;
    pushWorkingNode(parser, (Obj*)expr);

    ObjExpr** cursor = &expr->nextExpr;
    while (precedence <= getRule(parser->current.type)->precedence) {
        advance(parser);
        AstParseFn infixRule = getRule(parser->previous.type)->infix;
        *cursor = infixRule(parser, canAssign);
        parser->prevExpr = *cursor;
        cursor = &(*cursor)->nextExpr;
    }

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        error(parser, "Invalid assignment target.");
    }

    popWorkingNode(parser);
    parser->prevExpr = NULL;
    return expr;
}

static ObjExpr* expression(ObjParser* parser) {
    return parsePrecedence(parser, PREC_ASSIGNMENT);
}

static ObjStmtExpression* expressionStatement(ObjParser* parser, ObjType type, bool expectExpression, const char* msg) {
    ObjStmtExpression* stmt = (ObjStmtExpression*) pushNewStmt(parser->context, type, parser->current.line);
    
    if (match(parser, TOKEN_SEMICOLON)) {
        if (expectExpression) {
            error(parser, "Expression expected.");
        }
    } else {
        stmt->expression = expression(parser);
        consume(parser, TOKEN_SEMICOLON, msg);
    }
    pop(parser->context);
    return stmt;
}

static ObjStmtPoke* pokeStatement(ObjParser* parser) {
    ObjStmtPoke* stmt = (ObjStmtPoke*) pushNewStmt(parser->context, OBJ_STMT_POKE, parser->current.line);
    stmt->location = expression(parser);
    consume(parser, TOKEN_COMMA, "Expect ',' after location.");
    stmt->assignment = expression(parser);
    if (check(parser, TOKEN_COMMA)) {
        consume(parser, TOKEN_COMMA, "Expect ',' after offset.");
        stmt->offset = stmt->assignment;
        stmt->assignment = NULL;
        stmt->assignment = expression(parser);
    }
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after value.");
    pop(parser->context);
    return stmt;
}

static ObjExpr* typeExpression(ObjParser* parser) {
    ObjExpr* expression = NULL;

    if (checkTypeToken(parser)) {
        advance(parser);
        switch (parser->previous.type) {
            case TOKEN_MACHINE_FLOAT64: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_MFLOAT64); break;
            case TOKEN_INT8: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_INT8); break;
            case TOKEN_UINT8: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_UINT8); break;
            case TOKEN_INT16: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_INT16); break;
            case TOKEN_UINT16: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_UINT16); break;
            case TOKEN_INT32: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_INT32); break;
            case TOKEN_UINT32: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_UINT32); break;
            case TOKEN_INT64: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_INT64); break;
            case TOKEN_UINT64: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_UINT64); break;
            case TOKEN_BOOL: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_BOOL); break;
            case TOKEN_TYPE_STRING: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_STRING); break;
            case TOKEN_ANY: expression = (ObjExpr*) newExprLiteral(EXPR_LITERAL_NIL); break;
            case TOKEN_STRUCT: expression = (ObjExpr*) structExpression(parser); break;
            case TOKEN_INT: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_INT); break;
            default:
                errorAtCurrent(parser, "Invalid type in expression.");
        }
    }
    
    if (expression) {
        ObjExpr* cursor = expression;
        pushWorkingNode(parser, (Obj*)expression);
        consumeCollectionIndex(parser, cursor);

        if (cursor->nextExpr) {
            cursor = cursor->nextExpr;
        }

        popWorkingNode(parser);
    }

    return expression;
}

static ObjStmt* varDeclaration(ObjParser* parser) {

    ObjExpr* typeExpr = typeExpression(parser);
    if (typeExpr) {
        pushWorkingNode(parser, (Obj*)typeExpr);
    }
    
    consume(parser, TOKEN_IDENTIFIER, "Expect variable name.");
    ObjStmtVarDeclaration* decl = (ObjStmtVarDeclaration*) pushNewStmtDeclaration(parser->context, OBJ_STMT_VARDECLARATION, parser->previous.start, parser->previous.length, parser->previous.line);

    decl->type = typeExpr;

    if (match(parser, TOKEN_EQUAL)) {
        decl->initialiser = expression(parser);
    }
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration");

    if (typeExpr) {
        popWorkingNode(parser);
    }
    popWorkingNode(parser);

    return (ObjStmt*) decl;
}

static ObjStmt* declaration(ObjParser* parser);
static ObjStmt* statement(ObjParser* parser);

static void block(ObjParser* parser, ObjStmt** statements) {

    ObjStmt** cursor = statements;

    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        *cursor = declaration(parser);
        cursor = &(*cursor)->nextStmt;
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

ObjStmt* ifStatement(ObjParser* parser) {
    ObjStmtIf* ctrl = (ObjStmtIf*) pushNewStmt(parser->context, OBJ_STMT_IF, parser->previous.line);

    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    ctrl->test = expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    ctrl->ifStmt = statement(parser);
    if (match(parser, TOKEN_ELSE)) {
        ctrl->elseStmt = statement(parser);
    }
    
    pop(parser->context);
    return (ObjStmt*)ctrl;
}

ObjStmtWhile* whileStatement(ObjParser* parser) {
    ObjStmtWhile* loop = (ObjStmtWhile*) pushNewStmt(parser->context, OBJ_STMT_WHILE, parser->previous.line);

    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    loop->test = expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    loop->loop = statement(parser);

    pop(parser->context);
    return loop;
}

static ObjStmtFor* forStatement(ObjParser* parser) {

    ObjStmtFor* loop = (ObjStmtFor*) pushNewStmt(parser->context, OBJ_STMT_FOR, parser->previous.line);

    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(parser, TOKEN_SEMICOLON)) {
        // No initializer.
    } else if (match(parser, TOKEN_VAR)) {
        loop->initializer = varDeclaration(parser);
    } else {
        loop->initializer = (ObjStmt*) expressionStatement(parser, OBJ_STMT_EXPRESSION, true, "Expect ';' after expression.");
    }

    if (!match(parser, TOKEN_SEMICOLON)) {
        loop->condition = expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expect ';' after loop condition.");
    }

    if (!match(parser, TOKEN_RIGHT_PAREN)) {
        loop->loopExpression = expression(parser);
        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
    }

    loop->body = statement(parser);

    pop(parser->context);
    return loop;
}

ObjStmtBlock* blockStatement(ObjParser* parser) {
    ObjStmtBlock* stmt = (ObjStmtBlock*) pushNewStmt(parser->context, OBJ_STMT_BLOCK, parser->previous.line);
    block(parser, &stmt->statements);
    pop(parser->context);
    return stmt;
}

ObjStmt* statement(ObjParser* parser) {
    if (match(parser, TOKEN_PRINT)) {
        return (ObjStmt*) expressionStatement(parser, OBJ_STMT_PRINT, true, "Expect ';' after value.");
    } else if (match(parser, TOKEN_POKE)) {
        return (ObjStmt*) pokeStatement(parser);
    } else if (match(parser, TOKEN_FOR)) {
        return (ObjStmt*) forStatement(parser);
    } else if (match(parser, TOKEN_IF)) {
        return ifStatement(parser);
    } else if (match(parser, TOKEN_YIELD)) {
        return (ObjStmt*) expressionStatement(parser, OBJ_STMT_YIELD, false, "Expect ';' after yield value.");
    } else if (match(parser, TOKEN_RETURN)) {
        return (ObjStmt*) expressionStatement(parser, OBJ_STMT_RETURN, false, "Expect ';' after return value.");
    } else if (match(parser, TOKEN_WHILE)) {
        return (ObjStmt*) whileStatement(parser);
    } else if (match(parser, TOKEN_LEFT_BRACE)) {
        return (ObjStmt*) blockStatement(parser);
    } else {
        return (ObjStmt*) expressionStatement(parser, OBJ_STMT_EXPRESSION, true, "Expect ';' after expression.");
    }
}

static ObjStmtFunDeclaration* funDeclaration(ObjParser* parser, const char* msg) {
    consume(parser, TOKEN_IDENTIFIER, msg);
    ObjStmtFunDeclaration* fun = (ObjStmtFunDeclaration*) pushNewStmtDeclaration(parser->context, OBJ_STMT_FUNDECLARATION, parser->previous.start, parser->previous.length, parser->previous.line);

    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            appendToDynamicObjArray(&fun->parameters, (Obj*)expression(parser));
            if (fun->parameters.objectCount > 255) {
                errorAt(parser, &parser->previous, "Can't have more than 255 parameters.");
            }
        } while (match(parser, TOKEN_COMMA));
    }
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before function body.");

    block(parser, &fun->body);
    
    popWorkingNode(parser);
    return fun;
}

static bool tokenIdentifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static ObjStmtClassDeclaration* classDeclaration(ObjParser* parser) {
    consume(parser, TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser->previous;

    ObjStmtClassDeclaration* decl = (ObjStmtClassDeclaration*) pushNewStmtDeclaration(parser->context, OBJ_STMT_CLASSDECLARATION, className.start, className.length, parser->previous.line);

    if (match(parser, TOKEN_LESS)) {
        consume(parser, TOKEN_IDENTIFIER, "Expect superclass name.");
        decl->superclass = variable(parser, false);

        if (tokenIdentifiersEqual(&className, &parser->previous)) {
            error(parser, "A class can't inherit from itself.");
        }

    }

    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        appendToDynamicObjArray(&decl->methods, (Obj*)funDeclaration(parser, "Expect method name."));
    }
    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");

    popWorkingNode(parser);
    return decl;
}

static void placeAliasDeclaration(ObjParser* parser, ObjStmtPlaceDeclaration* place) {

    ObjExpr* location = expression(parser);
    push(parser->context, IMPL_OBJ_VAL(location));

    consume(parser, TOKEN_IDENTIFIER, "Expect place name.");
    appendPlaceAlias(parser->context, place, location, parser->previous.start, parser->previous.length);

    pop(parser->context);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after place declaration.");
}

static ObjStmtPlaceDeclaration* placeDeclaration(ObjParser* parser) {

    ObjStmtPlaceDeclaration* place = (ObjStmtPlaceDeclaration*) pushNewStmt(parser->context, OBJ_STMT_PLACEDECLARATION, parser->previous.line);
    place->type = expression(parser);

    if (match(parser, TOKEN_LEFT_BRACE)) {
        while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
            placeAliasDeclaration(parser, place);
        }
        consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after place declarations.");
    } else {
        placeAliasDeclaration(parser, place);
    }

    pop(parser->context);
    return place;
}

ObjStmt* declaration(ObjParser* parser) {
    ObjStmt* stmt = NULL;

    if (match(parser, TOKEN_CLASS)) {
        stmt = (ObjStmt*) classDeclaration(parser);
    } else if (match(parser, TOKEN_FUN)) {
        stmt = (ObjStmt*) funDeclaration(parser, "Expect function name.");
    } else if (match(parser, TOKEN_VAR) || checkTypeToken(parser)) {
        stmt = varDeclaration(parser);
    } else if (match(parser, TOKEN_PLACE)) {
        stmt = (ObjStmt*) placeDeclaration(parser);
    } else {
        stmt = statement(parser);
    }

    if (parser->panicMode) synchronize(parser);
    return stmt;
}

Value parse(ObjRoutine* context) {

    ObjParser* parser = (ObjParser*)AS_OBJ(peek(context, 0));
    advance(parser);

    ObjStmt** cursor = &parser->ast->statements;
    while (!match(parser, TOKEN_EOF)) {
        *cursor = declaration(parser);
        if (*cursor) {
            cursor = &(*cursor)->nextStmt;
        }
    }

    if (parser->errors.objectCount > 0) {
        return duplicateAsValueArray(&parser->errors);
    } else {
        return NIL_VAL;
    }
}
