#include <stdlib.h>
#include <sysexits.h>
#include <assert.h>

#include "common.h"
#include "hosted.h"
#include "object.h"
#include "memory.h"
#include "debug.h"
#include "pack.h"
#include "vm.h"

Host vmHost;

static int packageBinary(const char *path, Value const *yargResult);

static char* libraryNameFor(const char* importname, const char* libraryPath) {
    size_t namelen = strlen(importname);
    size_t pathlen = 0;
    if (libraryPath) {
        pathlen = strlen(libraryPath);
    }
    char* filename = malloc(pathlen + 1 + namelen + 1);
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
    ObjString* replPathString = copyString(replPath, (int) strlen(replPath));
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

int compileFile(const char* path, const char* outputPath) {

    ObjString* pathString = copyString(path, (int) strlen(path));
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
        if (outputPath) {
            exitCode = packageBinary(outputPath, &compilerResult);
        }
    }

    tempRootPop();
    tempRootPop();
    return exitCode;
}

int loadPackageFile(const char *path) {

    ObjString* pathString = copyString(path, (int) strlen(path));
    tempRootPush(OBJ_VAL(pathString));

    InterpretResult result = bootBinary(pathString);

    tempRootPop();
    if (result == INTERPRET_FILE_ERROR) {
        return EX_DATAERR;
    } else if (result == INTERPRET_RUNTIME_ERROR) {
        return EX_SOFTWARE;
    } else {
        return EX_OK;
    }
}

int disassembleFile(const char* path) {

    ObjString* pathString = copyString(path, (int)strlen(path));
    tempRootPush(OBJ_VAL(pathString));

    Value compilerResult;
    InterpretResult result = compileScript(pathString, &compilerResult);
    tempRootPush(compilerResult);

    char const *file = strrchr(path, '/');
    if (file == 0) {
        file = path;
    } else {
        file++;
    }

    int returnCode = EX_OK;

    if (result == INTERPRET_RUNTIME_ERROR) {
        returnCode = EX_SOFTWARE;
    } else if (result == INTERPRET_OK && IS_NIL(compilerResult)) {
        returnCode = EX_DATAERR;
    } else {
        returnCode = EX_OK;
        ObjFunction* function = AS_CLOSURE(compilerResult)->function;

        disassembleChunk(&function->chunk, file);
    }

    tempRootPop();
    tempRootPop();
    return returnCode;
}

int packageBinary(const char *path, Value const *script) {

    int r = EX_SOFTWARE;
    if (IS_CLOSURE(*script)) {
        char const *scriptFileName = strrchr(path, '/');
        if (scriptFileName == 0) {
            scriptFileName = path;
        } else {
            scriptFileName++;
        }

        size_t len = strlen(path);
        char *packagePath = malloc(len + 1);
        strcpy(packagePath, path);
        assert(packagePath[len - 1] == 'a');
        packagePath[len - 1] = 'b';

        r = packScript(scriptFileName, AS_CLOSURE(*script)->function, true, packagePath);

     }
    return r;
}
