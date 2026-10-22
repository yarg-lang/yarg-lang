#ifndef cyarg_parser_h
#define cyarg_parser_h

#include <stdbool.h>

typedef struct ObjAst ObjAst;
typedef struct ObjScanner ObjScanner;
typedef struct ObjParser ObjParser;

ObjParser* newParser();
void markParser(ObjParser* parser);
void gc_freeParser(ObjParser* parser);

bool parse(ObjParser*, ObjAst* ast, ObjScanner* scanner);

#endif
