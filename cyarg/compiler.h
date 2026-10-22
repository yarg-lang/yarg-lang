#ifndef cyarg_compiler_h
#define cyarg_compiler_h

#include "value.h"

typedef struct ObjFunction ObjFunction;
typedef struct ObjRoutine ObjRoutine;

Value compile(ObjRoutine* context);
void markCompilerRoots();

#endif
