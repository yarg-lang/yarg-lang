#ifndef cyarg_vm_result_h
#define cyarg_vm_result_h

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

#endif