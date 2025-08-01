#ifndef cyarg_chunk_h
#define cyarg_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_BUILTIN,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_GET_SUPER,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_LEFT_SHIFT,
    OP_RIGHT_SHIFT,
    OP_ADD,
    OP_SUBTRACT,
    OP_BITOR,
    OP_BITAND,
    OP_BITXOR,
    OP_MODULO,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_INVOKE,
    OP_SUPER_INVOKE,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_YIELD,
    OP_CLASS,
    OP_INHERIT,
    OP_METHOD,
    OP_ELEMENT,
    OP_SET_ELEMENT,
    OP_IMMEDIATE,
    OP_TYPE_LITERAL,
    OP_ARRAY_TYPE,
    OP_SET_TYPE
} OpCode;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    ValueArray constants;
} Chunk;

typedef enum {
    BUILTIN_IMPORT,
    BUILTIN_MAKE_ARRAY,
    BUILTIN_MAKE_ROUTINE,
    BUILTIN_MAKE_CHANNEL,
    BUILTIN_RESUME,
    BUILTIN_START,
    BUILTIN_RECEIVE,
    BUILTIN_SEND,
    BUILTIN_PEEK,
    BUILTIN_SHARE,
    BUILTIN_RPEEK,
    BUILTIN_RPOKE,
    BUILTIN_LEN,
    BUILTIN_PIN
} BuiltinFn;

typedef enum {
    TYPE_BUILTIN_BOOL,
    TYPE_BUILTIN_INTEGER,
    TYPE_BUILTIN_MACHINE_UINT32,
    TYPE_BUILTIN_MACHINE_FLOAT64,
    TYPE_BUILTIN_STRING
} BuiltinType;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);

#endif