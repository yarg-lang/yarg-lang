#ifndef cyarg_interpret_context_h
#define cyarg_interpret_context_h

#include "table.h"

typedef struct InterpretContext {
    Table constants;
} InterpretContext;

void initContext(InterpretContext* ctx);
void freeContext(InterpretContext* ctx);

#endif