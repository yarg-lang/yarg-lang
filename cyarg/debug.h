#ifndef cyarg_debug_h
#define cyarg_debug_h

#include "chunk.h"
#include "value.h"
#include "value_cell.h"
#include <stdio.h>

void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);

void printValue(Value value);
void fprintValue(FILE* op, Value value);

void printObj(const Obj* object);
void fprintObj(FILE* op, const Obj* object);

void fprintValueCell(FILE* op, ValueCell* cell);
#endif
