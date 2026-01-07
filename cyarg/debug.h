#ifndef cyarg_debug_h
#define cyarg_debug_h

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);
void printValueStack(ObjRoutine* routine, const char* message);

#endif
