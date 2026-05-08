#ifndef cyarg_pack_h
#define cyarg_pack_h

#include <stdbool.h>

struct ObjFunction;

int packScript(char const *sourceFileName, struct ObjFunction const *scriptFn, bool includeLines, char const *path);
struct ObjFunction *loadPackage(char const *path);

#endif
