#ifndef cyarg_pack_h
#define cyarg_pack_h

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct ObjFunction ObjFunction;
typedef struct ObjRoutine ObjRoutine;

int packScript(char const *sourceFileName, ObjFunction const *scriptFn, bool includeLines, char const *path);
ObjFunction *loadPackageFromBuffer(ObjRoutine* context, uint8_t* buffer, size_t bufferSize);

#endif
