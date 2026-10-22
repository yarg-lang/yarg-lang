#ifndef cyarg_parser_h
#define cyarg_parser_h

#include <stdbool.h>

typedef struct ObjAst ObjAst;
typedef struct ObjScanner ObjScanner;

bool parse(ObjAst* ast, ObjScanner* scanner);
void markParserRoots();


#endif
