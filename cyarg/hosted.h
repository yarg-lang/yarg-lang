#ifndef cyarg_host_h
#define cyarg_host_h

#include "value.h"

typedef struct {
    int argc;
    const char** argv;
    int exitCode;
} Host;

extern Host vmHost;

int runHostedFile(const char* libraryPath, const char* path);
int compileFile(const char* path);
int disassembleFile(const char* path);

#endif
