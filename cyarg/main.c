#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform_hal.h"

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"
#include "files.h"
#include "compiler.h"

const char* defaultScript = "main.ya";

static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

static void runFile(const char* path) {
    char* source = readFile(path);
    if (source) {
        InterpretResult result = interpret(source);
        free(source);

        if (result == INTERPRET_COMPILE_ERROR) exit(65);
        if (result == INTERPRET_RUNTIME_ERROR) exit(70);
    }
}

static void disassembleFile(const char* path) {
    char* source = readFile(path);
    if (!source) {
        return;
    }

    ObjFunction* result = compile(source);
    if (!result) {
        return;
    }

    disassembleChunk(&result->chunk, path);
    for (int i = 0; i < result->chunk.constants.count; i++) {
        if (IS_FUNCTION(result->chunk.constants.values[i])) {
            ObjFunction* fun = AS_FUNCTION(result->chunk.constants.values[i]);
            disassembleChunk(&fun->chunk, fun->name->chars);
        }
    }
}

#ifdef CYARG_PICO_TARGET
int main() {
    plaform_hal_init();

    initVM();

    runFile(defaultScript);
    repl();

    freeVM();
    return 0;
}
#else
int main(int argc, const char* argv[]) {
    plaform_hal_init();
    initVM();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else if (argc == 3 && strcmp(argv[1], "disassemble") == 0) {
        disassembleFile(argv[2]);
    }
    else {
        FPRINTMSG(stderr, "Usage: cyarg [path]\n");
        exit(64);
    }

    freeVM();
    return 0;
}
#endif
