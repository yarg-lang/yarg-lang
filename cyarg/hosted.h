#ifndef cyarg_host_h
#define cyarg_host_h

#include "value.h"

typedef struct {
    int argc;
    const char** argv;
    int exitCode;
} Host;

extern Host vmHost;

int bootHosted();
int bootstrapHostedFile(const char* path);
int compileFile(const char* path, const char* outputPath);
int disassembleFile(const char* path);

#endif
