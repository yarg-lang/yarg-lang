#ifndef cyarg_parser_h
#define cyarg_parser_h

#include <stdbool.h>

typedef struct ObjAst ObjAst;
typedef struct InterpretContext InterpretContext;

bool parse(ObjAst* ast, InterpretContext* ctx);
void markParserRoots();


#endif