#ifndef cyarg_compiler_h
#define cyarg_compiler_h

#include "object.h"
#include "vm.h"

typedef struct ObjFunction ObjFunction;

ObjFunction* compile(const char* source);
void markCompilerRoots();

#endif
