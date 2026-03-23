#ifndef cyarg_host_h
#define cyarg_host_h

#include "value.h"

typedef struct {
    int argc;
    const char** argv;
} Host;

extern Host vmHost;

int runFile(const char* libraryPath, const char* path);
int compileFile(const char* path, Value* yargResult);
int disassembleFile(const char* path);

#endif