#ifndef cyarg_pack_h
#define cyarg_pack_h

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct ObjFunction;

int packScript(char const *sourceFileName, struct ObjFunction const *scriptFn, bool includeLines, char const *path);
struct ObjFunction *loadPackage(char const *path);
struct ObjFunction *loadPackageFromBuffer(uint8_t* buffer, size_t bufferSize);

#endif
