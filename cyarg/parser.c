#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#include "parser.h"
#include "ast.h"
#include "memory.h"

#include "scanner.h"
#include <stdbool.h>
#include <limits.h>

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

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
    DynamicObjArray workingNodes;
    ObjExpr* prevExpr;
    ObjAst* ast;
} Parser;

Parser parser;

void initParser(ObjAst* ast) {
    parser.hadError = false;
    parser.panicMode = false;
    parser.ast = ast;
    parser.prevExpr = NULL;
    initDynamicObjArray(&parser.workingNodes);
}

void endParser() {
    parser.ast = NULL;
    freeDynamicObjArray(&parser.workingNodes);
}

void markParserRoots() {
    markObject((Obj*)parser.ast);
    markObject((Obj*)parser.prevExpr);
    markDynamicObjArray(&parser.workingNodes);
}

void pushWorkingNode(Obj* obj) {
    appendToDynamicObjArray(&parser.workingNodes, obj);

}

void popWorkingNode() {
    removeLastFromDynamicObjArray(&parser.workingNodes);
} 

typedef ObjExpr* (*AstParseFn)(bool canAssign);

typedef struct {
    AstParseFn prefix;
    AstParseFn infix;
    Precedence precedence;
} AstParseRule;

static AstParseRule* getRule(TokenType type);
static ObjExpr* parsePrecedence(Precedence precedence);
static ObjExpr* expression();
static ObjExpr* typeExpression();

void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;
    FPRINTMSG(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        FPRINTMSG(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        FPRINTMSG(stderr, " at '%.*s'", token->length, token->start);
    }

    FPRINTMSG(stderr, ": %s\n", message);
    parser.hadError = true;
}

void error(const char* message) {
    errorAt(&parser.previous, message);
}

void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

bool check(TokenType type) {
    return parser.current.type == type;
}

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_IMPORT:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            
            default:
                ; // Do nothing.
        }

        advance();
    }
}

void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static bool checkTypeToken() {
    switch (parser.current.type) {
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



static ObjExpr* namedVariable(Token name, bool canAssign) {

    ObjString* nameString = copyString(name.start, name.length);
    tempRootPush(OBJ_VAL(nameString));

    ObjExprNamedVariable* expr = newExprNamedVariable(name.start, name.length);

    if (canAssign && match(TOKEN_EQUAL)) {
        pushWorkingNode((Obj*)expr);
        expr->assignment = expression();
        popWorkingNode();
    }

    tempRootPop();
    return (ObjExpr*)expr;
}

static void expressionList(DynamicObjArray* items) {

    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            appendToDynamicObjArray(items, (Obj*) expression());
            if (items->objectCount > 255) {
                error("Can't have more than 255 arguments.");
            }
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' aftger arguments.");

}

static void arrayInitExpressionsList(DynamicObjArray* initializers) {

    if (!check(TOKEN_RIGHT_SQUARE_BRACKET)) {
        do {
            appendToDynamicObjArray(initializers, (Obj*)expression());
            if (initializers->objectCount > 255) {
                error("Can't have more than 255 array initialisers.");
            }

        } while (match(TOKEN_COMMA));
    }
}

static ObjExpr* super_(bool canAssign) {

    consume(TOKEN_DOT, "Expect '.' after 'super'.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    ObjExprSuper* super_ = newExprSuper(parser.previous.start, parser.previous.length);
    pushWorkingNode((Obj*)super_);

    if (match(TOKEN_LEFT_PAREN)) {
        super_->call = newExprCall();
        expressionList(&super_->call->arguments);
    }
    popWorkingNode();
    return (ObjExpr*)super_;
}

static ObjExpr* dot(bool canAssign) { 

    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    ObjExprDot* expr = newExprDot(parser.previous.start, parser.previous.length);
    pushWorkingNode((Obj*)expr);
    
    if (canAssign && match(TOKEN_EQUAL)) {
        expr->assignment = expression();
    } else if (match(TOKEN_LEFT_PAREN)) {
        expr->call = newExprCall();
        expressionList(&expr->call->arguments);
    }
    popWorkingNode();
    return (ObjExpr*) expr;
}

static ObjExpr* deref(bool canAssign) {
    
    ObjExprArrayElement* elmt = newExprArrayElement();
    pushWorkingNode((Obj*)elmt);

    elmt->element = parsePrecedence(PREC_ASSIGNMENT);   // prevents assignment within []
    consume(TOKEN_RIGHT_SQUARE_BRACKET, "Expect ] after expression.");

    if (canAssign && match(TOKEN_EQUAL)) {
        elmt->assignment = expression();
    }

    popWorkingNode();
    return (ObjExpr*)elmt;
}

static ObjExpr* arrayinit(bool canAssign) {

    ObjExprArrayInit* array = newExprArrayInit();
    pushWorkingNode((Obj*)array);

    arrayInitExpressionsList(&array->initializers);
    consume(TOKEN_RIGHT_SQUARE_BRACKET, "Expect ']' initialising array.");
    
    popWorkingNode();
    return (ObjExpr*)array;
}

static ObjExpr* call(bool canAssign) {
    ObjExprCall* call = newExprCall();
    pushWorkingNode((Obj*)call);
    expressionList(&call->arguments);
    popWorkingNode();
    return (ObjExpr*)call;
}

static ObjExpr* string(bool canAssign) {
    return (ObjExpr*) newExprString(parser.previous.start + 1, parser.previous.length - 2);
}

static ObjExpr* literal(bool canAssign) {     
    switch (parser.previous.type) {
        case TOKEN_FALSE: return (ObjExpr*) newExprLiteral(EXPR_LITERAL_FALSE);
        case TOKEN_NIL: return (ObjExpr*) newExprLiteral(EXPR_LITERAL_NIL);
        case TOKEN_TRUE: return (ObjExpr*) newExprLiteral(EXPR_LITERAL_TRUE);
        default: return NULL; // Unreachable.
    } 
}

static ObjExpr* builtin(bool canAssign) {     
    switch (parser.previous.type) {
        case TOKEN_PEEK: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_PEEK, 1);
        case TOKEN_IMPORT: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_IMPORT, 1);
        case TOKEN_READ_SOURCE: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_READ_SOURCE, 1);
        case TOKEN_EXEC: return (ObjExpr*) newExprBuiltin(EXPR_BUILTIN_EXEC, 1);
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
        default: return NULL; // Unreachable.
    } 
}

static ObjStmtFieldDeclaration* fieldStmt() {
    ObjExpr* type = NULL; 
    if (checkTypeToken()) {
        type = (ObjExpr*) typeExpression();
    } else {
        type = (ObjExpr*) newExprLiteral(EXPR_LITERAL_NIL);
    }
    pushWorkingNode((Obj*)type);
    ObjExpr* offset = NULL;
    if (match(TOKEN_AT)) {
        offset = expression();
    } else {
        offset = (ObjExpr*) newExprLiteral(EXPR_LITERAL_NIL);
    }
    pushWorkingNode((Obj*)offset);

    consume(TOKEN_IDENTIFIER, "Expect name of field.");
    ObjStmtFieldDeclaration* field = newStmtFieldDeclaration(parser.previous.start, parser.previous.length, parser.previous.line);
    field->type = type;
    field->offset = offset;
    consume(TOKEN_SEMICOLON, "Expect ';' after field declaration.");

    popWorkingNode();
    popWorkingNode();
    return field;
}

static ObjExprTypeStruct* structExpression() {
    consume(TOKEN_LEFT_BRACE, "Expect '{' to start struct type.");

    ObjExprTypeStruct* struct_declaration = newExprTypeStruct();
    pushWorkingNode((Obj*)struct_declaration);

    while (!check(TOKEN_RIGHT_BRACE)) {
        ObjStmtFieldDeclaration* field = fieldStmt();
        pushWorkingNode((Obj*)field);
        appendToDynamicValueArray(&struct_declaration->fieldsByIndex, OBJ_VAL(field));
        popWorkingNode();
    }
    
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after field declarations.");
    popWorkingNode();
    return struct_declaration;
}

static ObjExpr* type(bool canAssign) {

    if (check(TOKEN_LEFT_PAREN)) {
        return builtin(canAssign);
    } else {
        ObjExpr* expression = NULL;

        switch (parser.previous.type) {
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
            case TOKEN_STRUCT: expression = (ObjExpr*) structExpression(); break;
            case TOKEN_INT: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_INT); break;
           default: expression = NULL; // Unreachable
        }
        pushWorkingNode((Obj*)expression);
    
        if (match(TOKEN_LEFT_SQUARE_BRACKET)) {

            expression->nextExpr = (ObjExpr*) newExprTypeArray();
            
            if (!check(TOKEN_RIGHT_SQUARE_BRACKET)) {
                ObjExprTypeArray* array = (ObjExprTypeArray*)expression->nextExpr;
                array->cardinality = parsePrecedence(PREC_ASSIGNMENT); // no assignment in this expression.
            }

            consume(TOKEN_RIGHT_SQUARE_BRACKET, "Expect ']' after array type.");
        }

        popWorkingNode();

        return expression;
    }

}

static ObjExpr* variable(bool canAssign) {

    return namedVariable(parser.previous, canAssign);
}

static ObjExpr* this_(bool canAssign) {     
    return variable(false); 
}

static ObjExpr* unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    ObjExpr* rhs = parsePrecedence(PREC_UNARY);
    pushWorkingNode((Obj*)rhs);

    ExprOp op;
    switch (operatorType) {
        case TOKEN_BANG: op = EXPR_OP_NOT; break;
        case TOKEN_MINUS: op = EXPR_OP_NEGATE; break;
        default: break; // Unreachable.
    }

    ObjExprOperation* expr = newExprOperation(rhs, op);
    popWorkingNode();
    return (ObjExpr*) expr; 
}

static ObjExpr* unaryPlace(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    ObjExpr* rhs = parsePrecedence(PREC_UNARY);
    pushWorkingNode((Obj*)rhs);

    ExprOp op;
    switch (operatorType) {
        case TOKEN_STAR: op = EXPR_OP_DEREF_PTR; break;
        default: break; // Unreachable.
    }

    ObjExprOperation* expr = newExprOperation(rhs, op);
    popWorkingNode();
    pushWorkingNode((Obj*)expr);
    if (op == EXPR_OP_DEREF_PTR) {

        if (canAssign && match(TOKEN_EQUAL)) {
            expr->assignment = expression();
        }
    }
    popWorkingNode();
    return (ObjExpr*) expr; 
}

static ObjExpr* grouping(bool canAssign) {
    ObjExpr* expr = expression();
    pushWorkingNode((Obj*)expr);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
    ObjExprGrouping* grp = newExprGrouping(expr);
    popWorkingNode();
    return (ObjExpr*)grp; 
}

static ObjExpr* and_(bool canAssign) { 
    ObjExpr* rhs = parsePrecedence(PREC_AND);
    pushWorkingNode((Obj*)rhs);
    ObjExprOperation* expr = newExprOperation(rhs, EXPR_OP_LOGICAL_AND);
    popWorkingNode();
    return (ObjExpr*) expr;
}

static ObjExpr* or_(bool canAssign) {
    ObjExpr* rhs = parsePrecedence(PREC_OR);
    pushWorkingNode((Obj*)rhs);
    ObjExprOperation* expr = newExprOperation(rhs, EXPR_OP_LOGICAL_OR);
    popWorkingNode();
    return (ObjExpr*) expr;
}

static ObjExpr* binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    AstParseRule* rule = getRule(operatorType);
    ObjExpr* rhs = parsePrecedence((Precedence)(rule->precedence + 1));
    pushWorkingNode((Obj*)rhs);

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
    popWorkingNode();
    return (ObjExpr*) expr;
}

static ObjExpr* number(bool canAssign) {
    int radix = 0;
    bool sign_bit = true;
    const char* number_start = parser.previous.start;
    int number_len = parser.previous.length;
    if (parser.previous.length > 1) {
        switch (parser.previous.start[1]) {
            case 'x': radix = 16; sign_bit = false; break;
            case 'X': radix = 16; sign_bit = false; break;
            case 'b': radix = 2; sign_bit = false; break;
            case 'B': radix = 2; sign_bit = false; break;
            case 'd': radix = 10; sign_bit = false; break;
            case 'D': radix = 10; sign_bit = false; break;
        }
        if (radix != 0) {
            number_start = &parser.previous.start[2];
            number_len -= 2;
        }
    }
    if (radix == 0) {
        radix = 10;
    }

    ObjExprNumber* val = NULL;

    if (radix == 10 && sign_bit) {
        if (memchr(number_start, '.', number_len)) {
            // for now, use C's stdlib to reuse double formatting.
            double value = strtod(number_start, NULL);
            val = newExprNumberDouble(value);
        } else {
            val = newExprNumberInt(number_start, number_len);
            if (int_is_range(&val->val.bigInt, INT32_MIN, INT32_MAX) == INT_WITHIN) // this won’t work for INT32_MIN or INT64_MIN as the - is not parsed here
            {
                val = newExprNumberInteger32(int_to_i32(&val->val.bigInt)); // todo will GC sort this?
            } // todo what about i64?
        }
    }
    else {
        uint64_t value = strtoNum(number_start, number_len, radix);
        if (value <= UINT32_MAX) {
            val = newExprNumberUInteger32((uint32_t)value);
        } else {
            val = newExprNumberUInteger64(value);
        }
    }
    return (ObjExpr*) val;
}

static ObjExpr* address(bool canAssign) {
    uintptr_t value = strtoNum(parser.previous.start, parser.previous.length, 16);
    ObjExprNumber* addr = newExprNumberAddress(value);
    return (ObjExpr*) addr;
}

static AstParseRule rules[] = {
    [TOKEN_LEFT_PAREN]           = {grouping,  call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]          = {NULL,      NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]           = {NULL,      NULL,   PREC_NONE},
    [TOKEN_RIGHT_BRACE]          = {NULL,      NULL,   PREC_NONE},
    [TOKEN_LEFT_SQUARE_BRACKET]  = {arrayinit, deref,  PREC_DEREF},
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
    [TOKEN_CLASS]                = {NULL,      NULL,   PREC_NONE},
    [TOKEN_COMPILE]              = {builtin,   NULL,   PREC_NONE},
    [TOKEN_CONST]                = {NULL,      NULL,   PREC_NONE},
    [TOKEN_CPEEK]                = {builtin,   NULL,   PREC_NONE},
    [TOKEN_ELSE]                 = {NULL,      NULL,   PREC_NONE},
    [TOKEN_EXEC]                 = {builtin,   NULL,   PREC_NONE},
    [TOKEN_FALSE]                = {literal,   NULL,   PREC_NONE},
    [TOKEN_FOR]                  = {NULL,      NULL,   PREC_NONE},
    [TOKEN_FUN]                  = {NULL,      NULL,   PREC_NONE},
    [TOKEN_IF]                   = {NULL,      NULL,   PREC_NONE},
    [TOKEN_IMPORT]               = {builtin,   NULL,   PREC_NONE},
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
    [TOKEN_READ_SOURCE]          = {builtin,   NULL,   PREC_NONE},
    [TOKEN_RECEIVE]              = {builtin,   NULL,   PREC_NONE},
    [TOKEN_RESUME]               = {builtin,   NULL,   PREC_NONE},
    [TOKEN_RETURN]               = {NULL,      NULL,   PREC_NONE},
    [TOKEN_SEND]                 = {builtin,   NULL,   PREC_NONE},
    [TOKEN_SHARE]                = {builtin,   NULL,   PREC_NONE},
    [TOKEN_START]                = {builtin,   NULL,   PREC_NONE},
    [TOKEN_TYPE_STRING]          = {NULL,      NULL,   PREC_NONE},
    [TOKEN_STRUCT]               = {type,      NULL,   PREC_NONE},
    [TOKEN_SUPER]                = {super_,    NULL,   PREC_NONE},
    [TOKEN_THIS]                 = {this_,     NULL,   PREC_NONE},
    [TOKEN_TRUE]                 = {literal,   NULL,   PREC_NONE},
    [TOKEN_UINT8]                = {type,      NULL,   PREC_NONE},
    [TOKEN_UINT16]               = {type,      NULL,   PREC_NONE},
    [TOKEN_UINT32]               = {type,      NULL,   PREC_NONE},
    [TOKEN_UINT64]               = {type,      NULL,   PREC_NONE},
    [TOKEN_VAR]                  = {NULL,      NULL,   PREC_NONE},
    [TOKEN_WHILE]                = {NULL,      NULL,   PREC_NONE},
    [TOKEN_YIELD]                = {NULL,      NULL,   PREC_NONE},
    [TOKEN_TS_SET]               = {builtin,   NULL,   PREC_NONE},
    [TOKEN_TS_READ]              = {builtin,   NULL,   PREC_NONE},
    [TOKEN_TS_WRITE]             = {builtin,   NULL,   PREC_NONE},
    [TOKEN_TS_INTERRUPT]         = {builtin,   NULL,   PREC_NONE},
    [TOKEN_TS_SYNC]              = {builtin,   NULL,   PREC_NONE},
    [TOKEN_INT]                  = {type,      NULL,   PREC_NONE},

    [TOKEN_ERROR]                = {NULL,      NULL,   PREC_NONE},
    [TOKEN_EOF]                  = {NULL,      NULL,   PREC_NONE},
};

static AstParseRule* getRule(TokenType type) {
    return &rules[type];
}

static ObjExpr* parsePrecedence(Precedence precedence) {
    advance();
    AstParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return NULL;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;

    ObjExpr* expr = prefixRule(canAssign);
    parser.prevExpr = expr;
    pushWorkingNode((Obj*)expr);

    ObjExpr** cursor = &expr->nextExpr;
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        AstParseFn infixRule = getRule(parser.previous.type)->infix;
        *cursor = infixRule(canAssign);
        parser.prevExpr = *cursor;
        cursor = &(*cursor)->nextExpr;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }

    popWorkingNode();
    parser.prevExpr = NULL;
    return expr;
}

static ObjExpr* expression() {
    return parsePrecedence(PREC_ASSIGNMENT);
}

static ObjStmtExpression* printStatement() {
    ObjExpr* expr = expression();
    pushWorkingNode((Obj*)expr);
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    ObjStmtExpression* print = newStmtExpression(expr, OBJ_STMT_PRINT, parser.previous.line);
    popWorkingNode();
    return print;
}

static ObjStmtPoke* pokeStatement() {
    ObjStmtPoke* stmt = newStmtPoke(parser.previous.line);
    pushWorkingNode((Obj*)stmt);
    stmt->location = expression();
    consume(TOKEN_COMMA, "Expect ',' after location.");
    stmt->assignment = expression();
    if (check(TOKEN_COMMA)) {
        consume(TOKEN_COMMA, "Expect ',' after offset.");
        stmt->offset = stmt->assignment;
        stmt->assignment = NULL;
        stmt->assignment = expression();
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    popWorkingNode();
    return stmt;
}

static ObjStmtExpression* expressionStatement() {
    ObjExpr* expr = expression();
    pushWorkingNode((Obj*)expr);
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    ObjStmtExpression* expressionStatement = newStmtExpression(expr, OBJ_STMT_EXPRESSION, parser.previous.line);
    popWorkingNode();
    return expressionStatement;
}

static ObjExpr* typeExpression() {
    ObjExpr* expression = NULL;

    bool isConst = match(TOKEN_CONST);

    if (checkTypeToken()) {
        advance();
        switch (parser.previous.type) {
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
            case TOKEN_STRUCT: expression = (ObjExpr*) structExpression(); break;
            case TOKEN_INT: expression = (ObjExpr*) newExprType(EXPR_TYPE_LITERAL_INT); break;
            default:
                error("Invalid type in expression.");
        }
    }
    
    if (isConst && expression == NULL) {
        expression = (ObjExpr*) newExprLiteral(EXPR_LITERAL_NIL);
    }

    if (expression) {
        ObjExpr* cursor = expression;
        pushWorkingNode((Obj*)expression);

        if (match(TOKEN_LEFT_SQUARE_BRACKET)) {
            ObjExprTypeArray* array_expr = newExprTypeArray();
            cursor->nextExpr = (ObjExpr*) array_expr;
            cursor = cursor->nextExpr;

            if (!check(TOKEN_RIGHT_SQUARE_BRACKET)) {
                array_expr->cardinality = parsePrecedence(PREC_ASSIGNMENT); // no assignment in this expression.
            }

            consume(TOKEN_RIGHT_SQUARE_BRACKET, "Expect ']' after array type.");
        }

        if (isConst) {
            cursor->nextExpr = (ObjExpr*) newExprType(EXPR_TYPE_MODIFIER_CONST);
        }

        popWorkingNode();
    }

    return expression;
}

static ObjStmt* varDeclaration() {

    ObjExpr* typeExpr = typeExpression();
    if (typeExpr) {
        pushWorkingNode((Obj*)typeExpr);
    }
    
    consume(TOKEN_IDENTIFIER, "Expect variable name.");
    ObjStmtVarDeclaration* decl = newStmtVarDeclaration((char*)parser.previous.start, parser.previous.length, parser.previous.line);
    pushWorkingNode((Obj*)decl);

    decl->type = typeExpr;

    if (match(TOKEN_EQUAL)) {
        decl->initialiser = expression();
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration");

    if (typeExpr) {
        popWorkingNode();
    }
    popWorkingNode();

    return (ObjStmt*) decl;
}

static ObjStmt* declaration();
static ObjStmt* statement();

static void block(ObjStmt** statements) {

    ObjStmt** cursor = statements;

    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        *cursor = declaration();
        cursor = &(*cursor)->nextStmt;
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

ObjStmt* ifStatement() {
    ObjStmtIf* ctrl = newStmtIf(parser.previous.line);
    pushWorkingNode((Obj*)ctrl);

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    ctrl->test = expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    ctrl->ifStmt = statement();
    if (match(TOKEN_ELSE)) {
        ctrl->elseStmt = statement();
    }
    popWorkingNode();
    return (ObjStmt*)ctrl;
}

ObjStmtWhile* whileStatement() {
    ObjStmtWhile* loop = newStmtWhile(parser.previous.line);
    pushWorkingNode((Obj*)loop);

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    loop->test = expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    loop->loop = statement();

    popWorkingNode();
    return loop;
}

static ObjStmtExpression* returnOrYieldStatement(ObjType type, const char* msg) {
    ObjStmtExpression* stmt = newStmtExpression(NULL, type, parser.previous.line);
    pushWorkingNode((Obj*)stmt);
    
    if (match(TOKEN_SEMICOLON)) {
        popWorkingNode();
        return stmt;
    } else {
        stmt->expression = expression();
        consume(TOKEN_SEMICOLON, msg);
        
        popWorkingNode();
        return stmt;
    }
}

static ObjStmtFor* forStatement() {

    ObjStmtFor* loop = newStmtFor(parser.previous.line);
    pushWorkingNode((Obj*)loop);

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)) {
        // No initializer.
    } else if (match(TOKEN_VAR)) {
        loop->initializer = varDeclaration();
    } else {
        loop->initializer = (ObjStmt*) expressionStatement();
    }

    if (!match(TOKEN_SEMICOLON)) {
        loop->condition = expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
    }

    if (!match(TOKEN_RIGHT_PAREN)) {
        loop->loopExpression = expression();
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
    }

    loop->body = statement();

    popWorkingNode();
    return loop;
}

ObjStmtBlock* blockStatement() {
    ObjStmtBlock* stmt = newStmtBlock(parser.previous.line);
    pushWorkingNode((Obj*)stmt);
    block(&stmt->statements);
    popWorkingNode();
    return stmt;
}

ObjStmt* statement() {
    if (match(TOKEN_PRINT)) {
        return (ObjStmt*) printStatement();
    } else if (match(TOKEN_POKE)) {
        return (ObjStmt*) pokeStatement();
    } else if (match(TOKEN_FOR)) {
        return (ObjStmt*) forStatement();
    } else if (match(TOKEN_IF)) {
        return ifStatement();
    } else if (match(TOKEN_YIELD)) {
        return (ObjStmt*) returnOrYieldStatement(OBJ_STMT_YIELD, "Expect ';' after yield value.");
    } else if (match(TOKEN_RETURN)) {
        return (ObjStmt*) returnOrYieldStatement(OBJ_STMT_RETURN, "Expect ';' after return value.");
    } else if (match(TOKEN_WHILE)) {
        return (ObjStmt*) whileStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        return (ObjStmt*) blockStatement();
    } else {
        return (ObjStmt*) expressionStatement();
    }
}

static ObjStmtFunDeclaration* funDeclaration(const char* msg) {
    consume(TOKEN_IDENTIFIER, msg);
    ObjStmtFunDeclaration* fun = newStmtFunDeclaration(parser.previous.start, parser.previous.length, parser.previous.line);
    pushWorkingNode((Obj*)fun);

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            appendToDynamicObjArray(&fun->parameters, (Obj*)expression());
            if (fun->parameters.objectCount > 255) {
                errorAt(&parser.previous, "Can't have more than 255 parameters.");
            }
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");

    block(&fun->body);
    
    popWorkingNode();
    return fun;
}

static bool tokenIdentifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static ObjStmtClassDeclaration* classDeclaration() {
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;

    ObjStmtClassDeclaration* decl = newStmtClassDeclaration(className.start, className.length, parser.previous.line);
    pushWorkingNode((Obj*)decl);

    if (match(TOKEN_LESS)) {
        consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        decl->superclass = variable(false);

        if (tokenIdentifiersEqual(&className, &parser.previous)) {
            error("A class can't inherit from itself.");
        }

    }

    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        appendToDynamicObjArray(&decl->methods, (Obj*)funDeclaration("Expect method name."));
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");

    popWorkingNode();
    return decl;
}

static void placeAliasDeclaration(ObjStmtPlaceDeclaration* place) {

    ObjExpr* location = expression();
    pushWorkingNode((Obj*)location);

    consume(TOKEN_IDENTIFIER, "Expect place name.");
    appendPlaceAlias(place, location, parser.previous.start, parser.previous.length);

    popWorkingNode();
    consume(TOKEN_SEMICOLON, "Expect ';' after place declaration.");
}

static ObjStmtPlaceDeclaration* placeDeclaration() {

    ObjStmtPlaceDeclaration* place = newStmtPlaceDeclaration(parser.previous.line);
    pushWorkingNode((Obj*)place);
    place->type = expression();

    if (match(TOKEN_LEFT_BRACE)) {
        while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
            placeAliasDeclaration(place);
        }
        consume(TOKEN_RIGHT_BRACE, "Expect '}' after place declarations.");
    } else {
        placeAliasDeclaration(place);
    }

    popWorkingNode();
    return place;
}

ObjStmt* declaration() {
    ObjStmt* stmt = NULL;

    if (match(TOKEN_CLASS)) {
        stmt = (ObjStmt*) classDeclaration();
    } else if (match(TOKEN_FUN)) {
        stmt = (ObjStmt*) funDeclaration("Expect function name.");
    } else if (match(TOKEN_VAR) || check(TOKEN_CONST) || checkTypeToken()) {
        stmt = varDeclaration();
    } else if (match(TOKEN_PLACE)) {
        stmt = (ObjStmt*) placeDeclaration();
    } else {
        stmt = statement();
    }

    if (parser.panicMode) synchronize();
    return stmt;
}

bool parse(ObjAst* ast_root) {
    initParser(ast_root);
    advance();

    ObjStmt** cursor = &ast_root->statements;
    while (!match(TOKEN_EOF)) {
        *cursor = declaration();
        if (*cursor) {
            cursor = &(*cursor)->nextStmt;
        }
    }

#ifdef DEBUG_AST_PARSE
    printf("Parse Tree max working objects: %d\n", parser.workingNodes.objectCapacity);
#endif

    endParser();
    return parser.hadError;
}
