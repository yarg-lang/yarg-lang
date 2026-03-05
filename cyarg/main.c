#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef CYARG_FEATURE_HOSTED_REPL
#include <sysexits.h>
#endif

#include "common.h"
#include "platform_hal.h"
#include "vm.h"
#include "fs/fs.h"

#ifdef CYARG_FEATURE_HOSTED_REPL
#include "compiler.h"
#include "chunk.h"
#include "debug.h"
#endif

static void repl() {
    char line[4096];
    printf("> ");
    for (;;) {
        if (fgets(line, sizeof(line), stdin) != 0) {
            interpret(line);
            printf("> ");
        }
        else {
            int feofi = feof(stdin); // fgets returns null when pause/continue in lldb
            if (feofi) { // only exit if EOF
                break;
            }
        }
    }
}

#ifdef CYARG_FEATURE_HOSTED_REPL
static void runFile(const char* path) {
    char* source = readFile(path);
    if (source) {
        InterpretResult result = interpret(source);
        free(source);

        if (result == INTERPRET_COMPILE_ERROR) exit(EX_DATAERR);
        if (result == INTERPRET_RUNTIME_ERROR) exit(EX_SOFTWARE);
    } else {
        exit(EX_NOINPUT);
    }
}

static void compileFile(const char* path) {
    char* source = readFile(path);
    if (!source) {
        exit(EX_NOINPUT);
    }

    ObjFunction* result = compile(source);
    if (!result) {
        exit(EX_DATAERR);
    }
}

static void disassembleFile(const char* path) {
    char* source = readFile(path);
    if (!source) {
        exit(EX_NOINPUT);
    }

    ObjFunction* result = compile(source);
    if (!result) {
        exit(EX_DATAERR);
    }

    disassembleChunk(&result->chunk, path);
    for (int i = 0; i < result->chunk.constants.count; i++) {
        if (IS_FUNCTION(result->chunk.constants.values[i])) {
            ObjFunction* fun = AS_FUNCTION(result->chunk.constants.values[i]);
            disassembleChunk(&fun->chunk, fun->name->chars);
        }
    }
}
#endif

#if defined(CYARG_FEATURE_SELF_HOSTED_REPL)
int main() {
    plaform_hal_init();

    initVM();
    char* source = readFile("main.ya");
    if (source) {
        interpret(source);
        free(source);
    }
    repl();

    freeVM();
    return 0;
}
#elif defined(CYARG_FEATURE_HOSTED_REPL)
int main(int argc, const char* argv[]) {
    plaform_hal_init();
    initVM();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else if (argc == 3 && strcmp(argv[1], "--compile") == 0) {
        compileFile(argv[2]);
    } else if (argc == 3 && strcmp(argv[1], "--disassemble") == 0) {
        disassembleFile(argv[2]);
    }
    else {
        FPRINTMSG(stderr, "Usage: cyarg [path]\n");
        exit(EX_USAGE);
    }

    freeVM();
    return 0;
}
#endif
