#ifndef cyarg_parser_h
#define cyarg_parser_h

#include <stdbool.h>

typedef struct ObjRoutine ObjRoutine;
typedef struct ObjAst ObjAst;
typedef struct ObjScanner ObjScanner;
typedef struct ObjParser ObjParser;

ObjParser* newParser(ObjRoutine* context, ObjAst* ast, ObjScanner* scanner);
void markParser(ObjParser* parser);
void gc_freeParser(ObjParser* parser);

bool parse(ObjParser*);

#endif
