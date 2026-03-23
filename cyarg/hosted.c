#include <stdlib.h>
#include <sysexits.h>

#include "common.h"
#include "hosted.h"
#include "object.h"
#include "memory.h"
#include "debug.h"
#include "vm.h"

Host vmHost;

static char* libraryNameFor(const char* importname, const char* libraryPath) {
    size_t namelen = strlen(importname);
    size_t pathlen = 0;
    if (libraryPath) {
        pathlen = strlen(libraryPath);
    }
    char* filename = malloc(pathlen + 1 +namelen + 1);
    if (filename) {
        if (libraryPath) {
            strcpy(filename, libraryPath);
            if (libraryPath[pathlen - 1] != '/') {
                strcat(filename, "/");
            }
        } else {
            strcpy(filename, "");
        }
        strcat(filename, importname);
    }
    return filename;
}

int runFile(const char* libraryPath, const char* path) {

    char* replPath = libraryNameFor(path, libraryPath);
    ObjString* replPathString = copyString(replPath, strlen(replPath));
    tempRootPush(OBJ_VAL(replPathString));
    free(replPath);

    // Yarg scripts have no mechanism to return a result, so we discard it (it will always be nil).
    Value discardedResult;
    InterpretResult result = bootstrapVM(exec_bootstrap, &discardedResult, replPathString);

    tempRootPop();
    if (result == INTERPRET_COMPILE_ERROR) {
        return EX_DATAERR;
    } else if (result == INTERPRET_RUNTIME_ERROR) {
        return EX_SOFTWARE;
    } else {
        return EX_OK;
    }
}

int compileFile(const char* path, Value* compileResult) {

    ObjString* pathString = copyString(path, strlen(path));
    tempRootPush(OBJ_VAL(pathString));

    InterpretResult result = bootstrapVM(compile_bootstrap, compileResult, pathString);

    tempRootPop();
    if (result == INTERPRET_COMPILE_ERROR) {
        return EX_DATAERR;
    } else if (result == INTERPRET_RUNTIME_ERROR) {
        return EX_SOFTWARE;
    } else {
        return EX_OK;
    }
}

int disassembleFile(const char* path) {

    Value result;
    int returnCode = compileFile(path, &result);
    if (returnCode != EX_OK) {
        return returnCode;
    }
    tempRootPush(result);

    ObjClosure* closure = AS_CLOSURE(result);
    ObjFunction* function = closure->function;

    disassembleChunk(&function->chunk, path);
    for (int i = 0; i < function->chunk.constants.count; i++) {
        if (IS_FUNCTION(function->chunk.constants.values[i])) {
            ObjFunction* fun = AS_FUNCTION(function->chunk.constants.values[i]);
            disassembleChunk(&fun->chunk, fun->name->chars);
        }
    }

    tempRootPop();
    return returnCode;
}
