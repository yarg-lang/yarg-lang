#include <stdio.h>

#include "debug.h"
#include "object.h"
#include "value.h"
#include "routine.h"

void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d:'", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int invokeInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    uint8_t argCount = chunk->code[offset + 2];
    printf("%-16s (%d args) %4d:'", name, argCount, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 3;
}

static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static int builtinInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s ", name);
    switch (slot) {
        case BUILTIN_RPEEK: printf("rpeek"); break;
        case BUILTIN_IMPORT: printf("import"); break;
        case BUILTIN_MAKE_ROUTINE: printf("make_routine"); break;
        case BUILTIN_MAKE_CHANNEL: printf("make_channel"); break;
        case BUILTIN_RESUME: printf("resume"); break;
        case BUILTIN_START: printf("start"); break;
        case BUILTIN_SEND: printf("send"); break;
        case BUILTIN_RECEIVE: printf("receive"); break;
        case BUILTIN_SHARE: printf("share"); break;
        case BUILTIN_PEEK: printf("peek"); break;
        case BUILTIN_LEN: printf("len"); break;
        case BUILTIN_PIN: printf("pin"); break;
        case BUILTIN_NEW: printf("new"); break;
        default: printf("<unknown %4d>", slot); break;
    }
    printf("\n");
    return offset + 2;
}

static int typeLiteralInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t type = chunk->code[offset + 1];
    printf("%-16s ", name);
    switch (type) {
        case TYPE_LITERAL_BOOL: printf("bool"); break;
        case TYPE_LITERAL_INTEGER: printf("integer"); break;
        case TYPE_LITERAL_MACHINE_UINT32: printf("muint32"); break;
        case TYPE_LITERAL_MACHINE_FLOAT64: printf("mfloat64"); break;
        case TYPE_LITERAL_STRING: printf("string"); break;
        case TYPE_MODIFIER_CONST: printf("const"); break;
        default: printf("<unknown %4d>", type); break;
    }
    printf("\n");
    return offset + 2;
}

int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_NIL:
            return simpleInstruction("OP_NIL", offset);
        case OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case OP_POP:
            return simpleInstruction("OP_POP", offset);
        case OP_GET_BUILTIN:
            return builtinInstruction("OP_GET_BUILTIN", chunk, offset);
        case OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byteInstruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_INITIALISE:
            return simpleInstruction("OP_INITIALISE", offset);
        case OP_GET_UPVALUE:
            return byteInstruction("OP_GET_UPVALUE", chunk, offset);
        case OP_SET_UPVALUE:
            return byteInstruction("OP_SET_UPVALUE", chunk, offset);
        case OP_GET_PROPERTY:
            return constantInstruction("OP_GET_PROPERTY", chunk, offset);
        case OP_SET_PROPERTY:
            return constantInstruction("OP_SET_PROPERTY", chunk, offset);
        case OP_GET_SUPER:
            return constantInstruction("OP_GET_SUPER", chunk, offset);
        case OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case OP_LESS:
            return simpleInstruction("OP_LESS", offset);
        case OP_LEFT_SHIFT:
            return simpleInstruction("OP_LEFT_SHIFT", offset);
        case OP_RIGHT_SHIFT:
            return simpleInstruction("OP_RIGHT_SHIFT", offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_BITOR:
            return simpleInstruction("OP_BITOR", offset);
        case OP_BITAND:
            return simpleInstruction("OP_BITAND", offset);
        case OP_BITXOR:
            return simpleInstruction("OP_BITXOR", offset);
        case OP_MODULO:
            return simpleInstruction("OP_MODULO", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OP_PRINT:
            return simpleInstruction("OP_PRINT", offset);
        case OP_POKE:
            return simpleInstruction("OP_POKE", offset);
        case OP_JUMP:
            return jumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return jumpInstruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return byteInstruction("OP_CALL", chunk, offset);
        case OP_INVOKE:
            return invokeInstruction("OP_INVOKE", chunk, offset);
        case OP_SUPER_INVOKE:
            return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint8_t constant = chunk->code[offset++];
            printf("%-16s %4d ", "OP_CLOSURE", constant);
            printValue(chunk->constants.values[constant]);
            printf("\n");

            ObjFunction* function = AS_FUNCTION(
                chunk->constants.values[constant]);
            for (int j = 0; j < function->upvalueCount; j++) {
                int isLocal = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%04d       |                    %s %d\n",
                       offset - 2, isLocal ? "local" : "upvalue", index);
            }

            return offset;
        }
        case OP_CLOSE_UPVALUE:
            return simpleInstruction("OP_CLOSE_UPVALUE", offset);
        case OP_YIELD:
            return simpleInstruction("OP_YIELD", offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case OP_CLASS:
            return constantInstruction("OP_CLASS", chunk, offset);
        case OP_INHERIT:
            return simpleInstruction("OP_INHERIT", offset);
        case OP_METHOD:
            return constantInstruction("OP_METHOD", chunk, offset);
        case OP_ELEMENT:
            return simpleInstruction("OP_ELEMENT", offset);
        case OP_SET_ELEMENT:
            return simpleInstruction("OP_SET_ELEMENT", offset);
        case OP_IMMEDIATE:
            return byteInstruction("OP_IMMEDIATE", chunk, offset);
        case OP_TYPE_LITERAL:
            return typeLiteralInstruction("OP_TYPE_LITERAL", chunk, offset);
        case OP_TYPE_MODIFIER:
            return typeLiteralInstruction("OP_TYPE_MODIFIER", chunk, offset);
        case OP_TYPE_STRUCT:
            return byteInstruction("OP_TYPE_STRUCT", chunk, offset);
        case OP_TYPE_ARRAY:
            return simpleInstruction("OP_TYPE_ARRAY", offset);
        case OP_SET_CELL_TYPE:
            return simpleInstruction("OP_SET_CELL_TYPE", offset);
        case OP_DEREF_PTR:
            return simpleInstruction("OP_DEREF_PTR", offset);
        case OP_SET_PTR_TARGET:
            return simpleInstruction("OP_SET_PTR_TARGET", offset);
        case OP_PLACE:
            return simpleInstruction("OP_PLACE", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

void printValueStack(ObjRoutine* routine, const char* message) {
    size_t stackSize = routine->stackTop - routine->stack;
    printf("[%2zu]", stackSize);
    printf("%6s", message);
    for (int i = stackSize - 1; i >= 0; i--) {
        ValueCell* slot = peekCell(routine, i);
        printf("[ ");
        printValue(slot->value);
        printf(" | ");
        printValue(slot->type);
        printf(" ]");
    }
    printf("\n");
}
