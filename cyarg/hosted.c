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

int runHostedFile(const char* libraryPath, const char* path) {

    char* replPath = libraryNameFor(path, libraryPath);
    ObjString* replPathString = copyString(replPath, strlen(replPath));
    tempRootPush(OBJ_VAL(replPathString));
    free(replPath);

    vmHost.exitCode = EX_OK;

    InterpretResult result = bootScript(replPathString);

    tempRootPop();
    if (result == INTERPRET_RUNTIME_ERROR) {
        return EX_SOFTWARE;
    } else {
        return vmHost.exitCode;
    }
}

int compileFile(const char* path) {

    ObjString* pathString = copyString(path, strlen(path));
    tempRootPush(OBJ_VAL(pathString));

    Value compilerResult;
    InterpretResult result = compileScript(pathString, &compilerResult);
    tempRootPush(compilerResult);

    int exitCode = EX_OK;
    if (result == INTERPRET_RUNTIME_ERROR) {
        exitCode = EX_SOFTWARE;
    } else if (result == INTERPRET_OK && IS_NIL(compilerResult)) {
        exitCode = EX_DATAERR;
    } else {
        exitCode = EX_OK;
    }

    tempRootPop();
    tempRootPop();
    return exitCode;
}

int disassembleFile(const char* path) {

    ObjString* pathString = copyString(path, strlen(path));
    tempRootPush(OBJ_VAL(pathString));

    Value compilerResult;
    InterpretResult result = compileScript(pathString, &compilerResult);
    tempRootPush(compilerResult);

    int returnCode = EX_OK;

    if (result == INTERPRET_RUNTIME_ERROR) {
        returnCode = EX_SOFTWARE;
    } else if (result == INTERPRET_OK && IS_NIL(compilerResult)) {
        returnCode = EX_DATAERR;
    } else {
        returnCode = EX_OK;
        ObjFunction* function = AS_CLOSURE(compilerResult)->function;

        disassembleChunk(&function->chunk, path);
        for (int i = 0; i < function->chunk.constants.count; i++) {
            if (IS_FUNCTION(function->chunk.constants.values[i])) {
                ObjFunction* fun = AS_FUNCTION(function->chunk.constants.values[i]);
                disassembleChunk(&fun->chunk, fun->name->chars);
            }
        }
    }

    tempRootPop();
    tempRootPop();
    return returnCode;
}
