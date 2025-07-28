#ifndef cyarg_parser_h
#define cyarg_parser_h

#include <stdbool.h>

typedef struct ObjAst ObjAst;

bool parse(ObjAst* ast);
void markParserRoots();


#endif