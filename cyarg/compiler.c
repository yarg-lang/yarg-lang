#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "parser.h"
#include "ast.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"

static void generateExpr(ObjExpr* expr);

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT
} FunctionType;

typedef struct {
    ObjString* name;
    int depth;
    bool isCaptured;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    ObjAst* ast;
    FunctionType type;
    ObjStmt* recent;
    bool hadError;
    bool panicMode;

    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    bool hasSuperclass;
} ClassCompiler;

bool hadCompilerError = false;
static struct Compiler* current = NULL;
static ClassCompiler* currentClass = NULL;

static void initCompiler(Compiler* compiler, FunctionType type, ObjString* name) {

    compiler->enclosing = current;
    compiler->ast = NULL;
    compiler->function = NULL;
    compiler->type = type;
    compiler->recent = NULL;
    compiler->hadError = false;
    compiler->panicMode = false;

    compiler->localCount = 0;
    compiler->scopeDepth = 0;

    compiler->ast = newObjAst();
    current = compiler;

    compiler->function = newFunction();

    if (type != TYPE_SCRIPT) {
        current->function->name = name;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = NULL;
    local->depth = 0;
    local->isCaptured = false;
    if (type != TYPE_FUNCTION) {
        local->name = copyString("this", 4);
    } else {
        local->name = copyString("", 0);
    }
}

static void errorAt(const char* location, const char* message) {
    if (current->panicMode) return;

    current->panicMode = true;

    int line = current->recent ? current->recent->line : 1;
    fprintf(stderr, "[line %d] Error", line);

    if (location) {
        fprintf(stderr, " at '%s'", location);
    }

    fprintf(stderr, ": %s\n", message);
    current->hadError = true;
}

static void errorAtValue(Value location, const char* message) {
    if (current->panicMode) return;

    current->panicMode = true;

    int line = current->recent ? current->recent->line : 1;
    fprintf(stderr, "[line %d] Error", line);

    fprintf(stderr, " at '");
    printSourceValue(stderr, location);
    fprintf(stderr, "'");

    fprintf(stderr, ": %s\n", message);
    current->hadError = true;
}


static void error(const char* message) {
    errorAt(NULL, message);
}

static Chunk* currentChunk() {
    return &current->function->chunk;
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, current->recent ? current->recent->line : 1);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static bool identifiersEqual(ObjString* a, ObjString* b) {
    if (a->length != b->length) return false;
    return memcmp(a->chars, b->chars, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, ObjString* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, local->name)) {
            if (local->depth == -1) {
                errorAt(name->chars, "Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static void addLocal(ObjString* name) {
    if (current->localCount == UINT8_COUNT) {
        errorAt(name->chars, "Too many local variables in function.");
        return;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal, ObjString* name) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        errorAt(name->chars, "Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, ObjString* name) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true, name);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false, name);
    }

    return -1;
}

static uint8_t makeConstant(Value value) {

    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        errorAtValue(value, "Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static int emitConstant(Value value) {

    int32_t i = AS_I32(value);
    if (IS_I32(value) && i <= INT8_MAX && i >= INT8_MIN) {
        emitBytes(OP_IMMEDIATE, (uint8_t)i);
    } else {
        emitBytes(OP_CONSTANT, makeConstant(value));
    }
    return 2;
}

static void emitReturn() {
    if (current->type == TYPE_INITIALIZER) {
        emitBytes(OP_GET_LOCAL, 0);
    } else {
        emitByte(OP_NIL);
    }

    emitByte(OP_RETURN);
}

static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static void patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static uint8_t identifierConstant(ObjString* name) {
    return makeConstant(OBJ_VAL(name));
}

static void declareVariable(ObjString* name) {
    if (current->scopeDepth == 0) return;

    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, local->name)) {
            errorAt(name->chars, "Already a variable with this name in this scope.");
            return;
        }
    }

    addLocal(name);
}

static uint8_t parseVariable(ObjString* name) {

    declareVariable(name);
    if (current->scopeDepth > 0) return 0;

    return identifierConstant(name);
}

static void generateStmt(ObjStmt* stmt);

static void generateNumber(ObjExprNumber* num) {
    switch(num->type) {
        case NUMBER_DOUBLE: emitConstant(DOUBLE_VAL(num->val.dbl)); break;
        case NUMBER_INTEGER32: emitConstant(I32_VAL(num->val.integer32)); break;
        case NUMBER_UINTEGER32: emitConstant(UI32_VAL(num->val.uinteger32)); break;
        case NUMBER_UINTEGER64: emitConstant(UI64_VAL(num->val.ui64)); break;
        case NUMBER_ADDRESS: emitConstant(ADDRESS_VAL(num->val.address)); break;
        default:
            return; //  unreachable
    }
}

static void generateExprString(ObjExprString* str) {
    emitConstant(OBJ_VAL(str->string));
}

static void generateExprLogicalAnd(ObjExprOperation* bin) {
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    generateExpr(bin->rhs);

    patchJump(endJump);
}

static void generateExprLogicalOr(ObjExprOperation* bin) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    generateExpr(bin->rhs);
    patchJump(endJump);
}

static void generateExprAssignable(ObjExprOperation* op) {
    generateExpr(op->rhs);
    if (  op->operation == EXPR_OP_DEREF_PTR
        && op->assignment == NULL) {
        emitByte(OP_DEREF_PTR);
    }
    else if (op->operation == EXPR_OP_DEREF_PTR) {
        generateExpr(op->assignment);
        emitByte(OP_SET_PTR_TARGET);
    }
}

static void generateArithOperation(ObjExprOperation* op) {    
    generateExpr(op->rhs);

    switch (op->operation) {
        case EXPR_OP_EQUAL: emitByte(OP_EQUAL); return;
        case EXPR_OP_GREATER: emitByte(OP_GREATER); return;
        case EXPR_OP_RIGHT_SHIFT: emitByte(OP_RIGHT_SHIFT); return;
        case EXPR_OP_LESS: emitByte(OP_LESS); return;
        case EXPR_OP_LEFT_SHIFT: emitByte(OP_LEFT_SHIFT); return;
        case EXPR_OP_ADD: emitByte(OP_ADD); return;
        case EXPR_OP_SUBTRACT: emitByte(OP_SUBTRACT); return;
        case EXPR_OP_MULTIPLY: emitByte(OP_MULTIPLY); return;
        case EXPR_OP_DIVIDE: emitByte(OP_DIVIDE); return;
        case EXPR_OP_BIT_OR: emitByte(OP_BITOR); return;
        case EXPR_OP_BIT_AND: emitByte(OP_BITAND); return;
        case EXPR_OP_BIT_XOR: emitByte(OP_BITXOR); return;
        case EXPR_OP_MODULO: emitByte(OP_MODULO); return;
        case EXPR_OP_NOT_EQUAL: emitBytes(OP_EQUAL, OP_NOT); return;
        case EXPR_OP_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); return;
        case EXPR_OP_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); return;
        case EXPR_OP_NOT: emitByte(OP_NOT); return;
        case EXPR_OP_NEGATE: emitByte(OP_NEGATE); return;
        default: return; // unreachable
    }
}

static void generateExprOperation(ObjExprOperation* op) {

    switch (op->operation) {
        case EXPR_OP_LOGICAL_AND: generateExprLogicalAnd(op); return;
        case EXPR_OP_LOGICAL_OR: generateExprLogicalOr(op); return;
        case EXPR_OP_DEREF_PTR: generateExprAssignable(op); return;
        default: generateArithOperation(op); return;
    }
}

static void generateExprGrouping(ObjExprGrouping* grp) {
    generateExpr(grp->expression);
}

static void generateGetNamedVariable(ObjString* name) {
    uint8_t getOp;
    int arg = resolveLocal(current, name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
    } else if ((arg = resolveUpvalue(current, name)) != -1) {
        getOp = OP_GET_UPVALUE;
    } else {
        arg = identifierConstant(name);
        getOp = OP_GET_GLOBAL;
    }
    
    emitBytes(getOp, (uint8_t)arg);
}

static void generateExprNamedVariable(ObjExprNamedVariable* var) {

    ObjString* this_ = copyString("this", 4);
    if (identifiersEqual(this_, var->name) && currentClass == NULL) {
        errorAt("this", "Can't use 'this' outside of a class.");
        return;
    }
    this_ = NULL;

    uint8_t getOp, setOp;
    int arg = resolveLocal(current, var->name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, var->name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(var->name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }
    
    if (var->assignment) {
        generateExpr(var->assignment);
        emitBytes(setOp, (uint8_t)arg);
    } else {
        emitBytes(getOp, (uint8_t)arg);
    }
}

static void generateExprNamedConstant(ObjExprNamedConstant* const_) {

        generateExpr(const_->value);
}

static void generateExprLiteral(ObjExprLiteral* lit) {
    switch (lit->literal) {
        case EXPR_LITERAL_FALSE: emitByte(OP_FALSE); break;
        case EXPR_LITERAL_TRUE: emitByte(OP_TRUE); break;
        case EXPR_LITERAL_NIL: emitByte(OP_NIL); break;
    }
}

static void generateExprSet(DynamicObjArray* args) {
    for (int i = 0; i < args->objectCount; i++) {
        generateExpr((ObjExpr*)args->objects[i]);
    } 
}

static void generateExprCall(ObjExprCall* call) {
    generateExprSet(&call->arguments);

    emitBytes(OP_CALL, call->arguments.objectCount);
}

static void generateExprArrayInit(ObjExprArrayInit* array) {
 
    emitBytes(OP_GET_BUILTIN, BUILTIN_NEW);
    emitByte(OP_NIL);
    emitConstant(I32_VAL(array->initializers.objectCount));
    emitByte(OP_TYPE_ARRAY);
    emitBytes(OP_CALL, 1);
 
    for (int i = 0; i < array->initializers.objectCount; i++) {
        emitConstant(I32_VAL(i));
        generateExpr((ObjExpr*)array->initializers.objects[i]);
        emitByte(OP_SET_ELEMENT);
    }
}

static void generateExprArrayElement(ObjExprArrayElement* array) {

    generateExpr(array->element);

    if (array->assignment) {
        generateExpr(array->assignment);
        emitByte(OP_SET_ELEMENT);
    } else {
        emitByte(OP_ELEMENT);
    }
}

static void generateExprBuiltin(ObjExprBuiltin* fn) {
    switch(fn->builtin) {
        case EXPR_BUILTIN_IMPORT: emitBytes(OP_GET_BUILTIN, BUILTIN_IMPORT); break;
        case EXPR_BUILTIN_MAKE_ROUTINE: emitBytes(OP_GET_BUILTIN, BUILTIN_MAKE_ROUTINE); break;
        case EXPR_BUILTIN_MAKE_CHANNEL: emitBytes(OP_GET_BUILTIN, BUILTIN_MAKE_CHANNEL); break;
        case EXPR_BUILTIN_RESUME: emitBytes(OP_GET_BUILTIN, BUILTIN_RESUME); break;
        case EXPR_BUILTIN_START: emitBytes(OP_GET_BUILTIN, BUILTIN_START); break;
        case EXPR_BUILTIN_RECEIVE: emitBytes(OP_GET_BUILTIN, BUILTIN_RECEIVE); break;
        case EXPR_BUILTIN_SEND: emitBytes(OP_GET_BUILTIN, BUILTIN_SEND); break;
        case EXPR_BUILTIN_CPEEK: emitBytes(OP_GET_BUILTIN, BUILTIN_CPEEK); break;
        case EXPR_BUILTIN_SHARE: emitBytes(OP_GET_BUILTIN, BUILTIN_SHARE); break;
        case EXPR_BUILTIN_PEEK: emitBytes(OP_GET_BUILTIN, BUILTIN_PEEK); break;
        case EXPR_BUILTIN_LEN: emitBytes(OP_GET_BUILTIN, BUILTIN_LEN); break;
        case EXPR_BUILTIN_PIN: emitBytes(OP_GET_BUILTIN, BUILTIN_PIN); break;
        case EXPR_BUILTIN_NEW: emitBytes(OP_GET_BUILTIN, BUILTIN_NEW); break;
        case EXPR_BUILTIN_INT8: emitBytes(OP_GET_BUILTIN, BUILTIN_INT8); break;
        case EXPR_BUILTIN_UINT8: emitBytes(OP_GET_BUILTIN, BUILTIN_UINT8); break;
        case EXPR_BUILTIN_INT32: emitBytes(OP_GET_BUILTIN, BUILTIN_INT32); break;
        case EXPR_BUILTIN_UINT32: emitBytes(OP_GET_BUILTIN, BUILTIN_UINT32); break;
        case EXPR_BUILTIN_INT64: emitBytes(OP_GET_BUILTIN, BUILTIN_INT64); break;
        case EXPR_BUILTIN_UINT64: emitBytes(OP_GET_BUILTIN, BUILTIN_UINT64); break;
    }    
}

static void generateExprDot(ObjExprDot* dot) {
    uint8_t name = identifierConstant(dot->name);

    if (dot->offset) {
        generateExpr(dot->offset);
        emitByte(OP_ADD);
    } else if (dot->assignment) {
        generateExpr(dot->assignment);
        emitBytes(OP_SET_PROPERTY, name);
    } else if (dot->call) {
        generateExprSet(&dot->call->arguments);
        emitBytes(OP_INVOKE, name);
        emitByte(dot->call->arguments.objectCount);
    } else {
        emitBytes(OP_GET_PROPERTY, name);
    }
}

static void generateExprSuper(ObjExprSuper* super) {
    if (currentClass == NULL) {
        errorAt("super", "Can't use 'super' outside of a class.");
    } else if (!currentClass->hasSuperclass) {
        errorAt("super", "Can't use 'super' in a class with no superclass.");
    }
    uint8_t name = identifierConstant(super->name);

    ObjString* this_ = copyString("this", 4);
    tempRootPush(OBJ_VAL(this_));
    ObjString* super_ = copyString("super", 5);
    tempRootPush(OBJ_VAL(super_));

    generateGetNamedVariable(this_);
    if (super->call) {
        generateExprSet(&super->call->arguments);
        generateGetNamedVariable(super_);
        emitBytes(OP_SUPER_INVOKE, name);
        emitByte(super->call->arguments.objectCount);
    } else {
        generateGetNamedVariable(super_);
        emitBytes(OP_GET_SUPER, name);
    }

    tempRootPop();
    tempRootPop();
}

static void generateExprType(ObjExprTypeLiteral* type) {
    switch (type->type) {
        case EXPR_TYPE_LITERAL_BOOL: emitBytes(OP_TYPE_LITERAL, TYPE_LITERAL_BOOL); return;
        case EXPR_TYPE_LITERAL_INT8: emitBytes(OP_TYPE_LITERAL, TYPE_LITERAL_INT8); return;
        case EXPR_TYPE_LITERAL_UINT8: emitBytes(OP_TYPE_LITERAL, TYPE_LITERAL_UINT8); return;
        case EXPR_TYPE_LITERAL_INTEGER: emitBytes(OP_TYPE_LITERAL, TYPE_LITERAL_INTEGER); return;
        case EXPR_TYPE_LITERAL_MFLOAT64: emitBytes(OP_TYPE_LITERAL, TYPE_LITERAL_MACHINE_FLOAT64); return;
        case EXPR_TYPE_LITERAL_UINT32: emitBytes(OP_TYPE_LITERAL, TYPE_LITERAL_UINT32); return;
        case EXPR_TYPE_LITERAL_INT64: emitBytes(OP_TYPE_LITERAL, TYPE_LITERAL_INT64); return;
        case EXPR_TYPE_LITERAL_UINT64: emitBytes(OP_TYPE_LITERAL, TYPE_LITERAL_UINT64); return;
        case EXPR_TYPE_LITERAL_STRING: emitBytes(OP_TYPE_LITERAL, TYPE_LITERAL_STRING); return;
        case EXPR_TYPE_MODIFIER_CONST: emitBytes(OP_TYPE_MODIFIER, TYPE_MODIFIER_CONST); return;
        default: return; // unreachable.
    }
}

static void generateExprTypeStruct(ObjExprTypeStruct* struct_) {
    for (int i = struct_->fieldsByIndex.count - 1; i >= 0; i--) {
        Value field = struct_->fieldsByIndex.values[i];
        generateStmt((ObjStmt*)AS_OBJ(field));
    }
    size_t fieldCount = struct_->fieldsByIndex.count;
    if (fieldCount > UINT8_MAX) {
        error("Can't have more than 256 fields.");
    }
    emitBytes(OP_TYPE_STRUCT, (uint8_t)fieldCount);
}

static void generateExprTypeArray(ObjExprTypeArray* array) {
    if (array->cardinality) {
        generateExpr(array->cardinality);
    } else {
        emitByte(OP_NIL);
    }
    emitByte(OP_TYPE_ARRAY);
}

static void generateExprElt(ObjExpr* expr) {
    
    switch (expr->obj.type) {
        case OBJ_EXPR_NUMBER: {
            ObjExprNumber* num = (ObjExprNumber*)expr;
            generateNumber(num);
            break;
        }
        case OBJ_EXPR_OPERATION: {
            ObjExprOperation* op = (ObjExprOperation*)expr;
            generateExprOperation(op);
            break;
        }
        case OBJ_EXPR_GROUPING: {
            ObjExprGrouping* grp = (ObjExprGrouping*)expr;
            generateExprGrouping(grp);
            break;
        }
        case OBJ_EXPR_NAMEDVARIABLE: {
            ObjExprNamedVariable* var = (ObjExprNamedVariable*)expr;
            generateExprNamedVariable(var);
            break;
        }
        case OBJ_EXPR_NAMEDCONSTANT: {
            ObjExprNamedConstant* const_ = (ObjExprNamedConstant*)expr;
            generateExprNamedConstant(const_);
            break;
        }
        case OBJ_EXPR_LITERAL: {
            ObjExprLiteral* lit = (ObjExprLiteral*)expr;
            generateExprLiteral(lit);
            break;
        }
        case OBJ_EXPR_STRING: {
            ObjExprString* str = (ObjExprString*)expr;
            generateExprString(str);
            break;
        }
        case OBJ_EXPR_CALL: {
            ObjExprCall* call = (ObjExprCall*)expr;
            generateExprCall(call);
            break;
        }
        case OBJ_EXPR_ARRAYINIT: {
            ObjExprArrayInit* array = (ObjExprArrayInit*)expr;
            generateExprArrayInit(array);
            break;
        }
        case OBJ_EXPR_ARRAYELEMENT: {
            ObjExprArrayElement* array = (ObjExprArrayElement*)expr;
            generateExprArrayElement(array);
            break;
        }
        case OBJ_EXPR_BUILTIN: {
            ObjExprBuiltin* fn = (ObjExprBuiltin*)expr;
            generateExprBuiltin(fn);
            break;
        }
        case OBJ_EXPR_DOT: {
            ObjExprDot* dot = (ObjExprDot*)expr;
            generateExprDot(dot);
            break;
        }
        case OBJ_EXPR_SUPER: {
            ObjExprSuper* super = (ObjExprSuper*)expr;
            generateExprSuper(super);
            break;
        }
        case OBJ_EXPR_TYPE: {
            ObjExprTypeLiteral* t = (ObjExprTypeLiteral*)expr;
            generateExprType(t);
            break;
        }
        case OBJ_EXPR_TYPE_STRUCT: {
            ObjExprTypeStruct* t = (ObjExprTypeStruct*)expr;
            generateExprTypeStruct(t);
            break;
        }
        case OBJ_EXPR_TYPE_ARRAY: {
            ObjExprTypeArray* a = (ObjExprTypeArray*)expr;
            generateExprTypeArray(a);
            break;
        }
        default:
            return; // unexpected
    }
}

static void generateExpr(ObjExpr* expr) {

    while (expr != NULL) {
        generateExprElt(expr);
        expr = expr->nextExpr;
    }
}

static void markInitialized() {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }

    emitBytes(OP_DEFINE_GLOBAL, global);
}

static void generateStmtPoke(ObjStmtPoke* stmt) {
    generateExpr(stmt->assignment);
    generateExpr(stmt->location);
    emitByte(OP_POKE);
}

static void generateVarDeclaration(ObjStmtVarDeclaration* decl) {
    uint8_t global = parseVariable(decl->name);

    if (decl->type) {
        generateExpr(decl->type);
    }
    else {
        emitByte(OP_NIL);
    }
    emitByte(OP_SET_CELL_TYPE);

    if (decl->initialiser) {
        generateExpr(decl->initialiser);
        emitByte(OP_INITIALISE);
    }

    defineVariable(global);
}

static void generatePlaceDeclaration(ObjStmtPlaceDeclaration* decl) {

    for (int i = 0; i < decl->aliases.objectCount; i++) {
        ObjPlaceAlias* alias = (ObjPlaceAlias*) decl->aliases.objects[i];
        generateExpr(decl->type);

        generateExpr(alias->location);
        emitByte(OP_PLACE);
        
        uint8_t global = parseVariable(alias->name);
        defineVariable(global);

    }
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;

    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth > current->scopeDepth) {
        if (current->locals[current->localCount - 1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }
        current->localCount--;
    }
}

static void generate(ObjStmt* stmt);
static ObjFunction* endCompiler();

static void generateStmtBlock(ObjStmtBlock* block) {
    beginScope();
    generate(block->statements);
    endScope();
}

static void generateStmtIf(ObjStmtIf* ctrl) {
    generateExpr(ctrl->test);
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    generate(ctrl->ifStmt);

    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);
    if (ctrl->elseStmt) {
        generate(ctrl->elseStmt);
    }
    patchJump(elseJump);
}

static void generateFunction(FunctionType type, ObjStmtFunDeclaration* decl) {
    Compiler compiler;
    initCompiler(&compiler, type, decl->name);
    beginScope();

    for (int i = 0; i < decl->parameters.objectCount; i++) {
        uint8_t constant = parseVariable(((ObjExprNamedVariable*)decl->parameters.objects[i])->name);
        defineVariable(constant);
    }

    generate(decl->body);

    ObjFunction* function = endCompiler();
    function->arity = decl->parameters.objectCount;
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

static void generateStmtFunDeclaration(ObjStmtFunDeclaration* decl) {

    uint8_t global = parseVariable(decl->name);
    markInitialized();

    generateFunction(TYPE_FUNCTION, decl);

    defineVariable(global);
}

static void generateStmtWhile(ObjStmtWhile* loop) {
    int loopStart = currentChunk()->count;
    generateExpr(loop->test);

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    generate(loop->loop);
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

static void generateStmtYield(ObjStmtExpression* stmt) {
    if (current->type == TYPE_SCRIPT) {
        errorAt("yield", "Can't yield from top-level code.");
    }

    if (!stmt->expression) {
        emitBytes(OP_NIL, OP_YIELD);
    } else {
        if (current->type == TYPE_INITIALIZER) {
            errorAt("yield", "Can't yield a value from an initializer.");
        }

        generateExpr(stmt->expression);
        emitByte(OP_YIELD);
    }
}

static void generateStmtReturn(ObjStmtExpression* stmt) {
    if (current->type == TYPE_SCRIPT) {
        errorAt("return", "Can't return from top-level code.");
    }
    
    if (!stmt->expression) {
        emitReturn();
    } else {
        if (current->type == TYPE_INITIALIZER) {
            errorAt("return", "Can't return a value from an initializer.");
        }

        generateExpr(stmt->expression);
        emitByte(OP_RETURN);
    }
}

static void generateStmtFor(ObjStmtFor* loop) {
    beginScope();
    if (loop->initializer) {
        generate(loop->initializer);
    }

    int loopStart = currentChunk()->count;
    int exitJump = -1;
    if (loop->condition) {
        generateExpr(loop->condition);

        // Jump out of the loop if the condition is false.
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Condition.
    }

    if (loop->loopExpression) {

        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->count;
        generateExpr(loop->loopExpression);
        emitByte(OP_POP);

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    generate(loop->body);
    
    emitLoop(loopStart);

    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP);
    }

    endScope();    
}

static void generateStmtMethodDeclaration(ObjStmtFunDeclaration* method) {
    uint8_t constant = identifierConstant(method->name);

    FunctionType type = TYPE_METHOD;
    ObjString* init = copyString("init", 4);
    tempRootPush(OBJ_VAL(init));
    if (identifiersEqual(method->name, init)) {
        type = TYPE_INITIALIZER;
    }
    
    generateFunction(type, method);

    emitBytes(OP_METHOD, constant);
    tempRootPop(); // initi
}

static void generateStmtClassDeclaration(ObjStmtClassDeclaration* decl) {
    uint8_t nameConstant = identifierConstant(decl->name);
    declareVariable(decl->name);

    emitBytes(OP_CLASS, nameConstant);
    defineVariable(nameConstant);

    ClassCompiler classCompiler;
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    if (decl->superclass) {
        generateGetNamedVariable(((ObjExprNamedVariable*)decl->superclass)->name);

        beginScope();
        ObjString* super = copyString("super", 5);
        tempRootPush(OBJ_VAL(super));
        addLocal(super);
        defineVariable(0);

        generateGetNamedVariable(decl->name);
        emitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
        tempRootPop(); //super
    }

    // Make the class name conveniently available on the stack for method definition.
    generateGetNamedVariable(decl->name);
    for (int i = 0; i < decl->methods.objectCount; i++) {
        generateStmtMethodDeclaration((ObjStmtFunDeclaration*)decl->methods.objects[i]);
    }
    emitByte(OP_POP);

    if (classCompiler.hasSuperclass) {
        endScope();
    }

    currentClass = currentClass->enclosing;    
}

static void generateStmtFieldDeclaration(ObjStmtFieldDeclaration* stmt) {
    generateExpr(stmt->type);
    generateExpr(stmt->offset);
    emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(stmt->name)));
}

static void generateStmt(ObjStmt* stmt) {
    current->panicMode = false;
    current->recent = stmt;
    switch (stmt->obj.type) {
        case OBJ_STMT_EXPRESSION:
            generateExpr(((ObjStmtExpression*)stmt)->expression);
            emitByte(OP_POP);
            break;
        case OBJ_STMT_PRINT:
            generateExpr(((ObjStmtExpression*)stmt)->expression);
            emitByte(OP_PRINT);
            break;
        case OBJ_STMT_POKE:
            generateStmtPoke((ObjStmtPoke*)stmt);
            break;
        case OBJ_STMT_VARDECLARATION:
            generateVarDeclaration((ObjStmtVarDeclaration*)stmt);
            break;
        case OBJ_STMT_PLACEDECLARATION:
            generatePlaceDeclaration((ObjStmtPlaceDeclaration*)stmt);
            break;
        case OBJ_STMT_BLOCK:
            generateStmtBlock((ObjStmtBlock*)stmt);
            break;
        case OBJ_STMT_IF:
            generateStmtIf((ObjStmtIf*)stmt);
            break;
        case OBJ_STMT_FUNDECLARATION:
            generateStmtFunDeclaration((ObjStmtFunDeclaration*)stmt);
            break;
        case OBJ_STMT_WHILE:
            generateStmtWhile((ObjStmtWhile*)stmt);
            break;
        case OBJ_STMT_YIELD:
            generateStmtYield((ObjStmtExpression*)stmt);
            break;
        case OBJ_STMT_RETURN:
            generateStmtReturn((ObjStmtExpression*)stmt);
            break;
        case OBJ_STMT_FOR:
            generateStmtFor((ObjStmtFor*)stmt);
            break;
        case OBJ_STMT_CLASSDECLARATION:
            generateStmtClassDeclaration((ObjStmtClassDeclaration*)stmt);
            break;
        case OBJ_STMT_FIELDDECLARATION:
            generateStmtFieldDeclaration((ObjStmtFieldDeclaration*)stmt);
            break;
        default:
            return; // Unexpected
    }
}

static void generate(ObjStmt* stmt) {

    while (stmt != NULL) {
        generateStmt(stmt);
        stmt = stmt->nextStmt;
    }
}

static ObjFunction* endCompiler() {
    emitReturn();

    current->ast = NULL;
    current->recent = NULL;
    hadCompilerError |= current->hadError;
    ObjFunction* function = current->function;

    current = current->enclosing;
    return function;
}

ObjFunction* compile(const char* source) {
    hadCompilerError = false;

    initScanner(source);
    struct Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT, NULL);

#ifdef DEBUG_AST_PARSE
    collectGarbage();
    size_t bytesAllocated = vm.bytesAllocated;
#endif

    bool parseError = parse(current->ast);

#ifdef DEBUG_AST_PARSE
    collectGarbage();
    printf("Parse Tree (%zu net bytes)\n", vm.bytesAllocated - bytesAllocated);
    printStmts(current->ast->statements);
#endif

    if (!parseError) {

        generate(current->ast->statements);
    }

    ObjFunction* function = endCompiler();

    bool compileError = parseError || hadCompilerError;

    return compileError ? NULL : function;
}

void markCompilerRoots() {
    Compiler* compiler = current;
    while (compiler != NULL) {
        markObject((Obj*)compiler->function);
        markObject((Obj*)compiler->ast);
        markObject((Obj*)compiler->recent);
        markParserRoots();

        for (int i = 0; i < compiler->localCount; i++) {
            markObject((Obj*)compiler->locals[i].name);
        }

        compiler = compiler->enclosing;
    }
}