#include "hosted.h"

#include <stdlib.h>
#include <sysexits.h>
#include <assert.h>
#include <string.h>

#include "common.h"
#include "object.h"
#include "memory.h"
#include "debug.h"
#include "pack.h"
#include "vm.h"
#include "vmobject.h"
#include "packed_containers.h"

Host vmHost;

int bootHosted() {
    vmHost.exitCode = EX_OK;

    InterpretResult result = bootXIP();

    if (result == INTERPRET_RUNTIME_ERROR) {
        return EX_SOFTWARE;
    } else {
        return vmHost.exitCode;
    }
}

int bootstrapHostedFile(const char* path) {

    vmHost.exitCode = EX_OK;

    ObjString* pathString = copyString(path, (int) strlen(path));
    tempRootPush(STRING_VAL(pathString));

    InterpretResult result = bootScript(pathString);

    tempRootPop();
    if (result == INTERPRET_RUNTIME_ERROR) {
        return EX_SOFTWARE;
    } else {
        return vmHost.exitCode;
    }
}

int compileFile(const char* path, const char* outputPath) {

    ObjString* pathString = copyString(path, (int) strlen(path));
    tempRootPush(STRING_VAL(pathString));

    Value compilerResult;
    InterpretResult result = compileScript(pathString, &compilerResult);
    tempRootPush(compilerResult);

    Value compileError = structField(compilerResult, 0);
    Value compilerResultClosure = structField(compilerResult, 1);

    int exitCode = EX_OK;
    if (result == INTERPRET_RUNTIME_ERROR) {
        exitCode = EX_SOFTWARE;
    } else if (result == INTERPRET_OK && valuesEqual(compileError, BOOL_VAL(true))) {
        exitCode = EX_DATAERR;
    } else {
        if (outputPath && IS_CLOSURE(compilerResultClosure)) {
            exitCode = packScript(path, AS_CLOSURE(compilerResultClosure)->function, true, outputPath);
        }
    }

    tempRootPop();
    tempRootPop();
    return exitCode;
}

int disassembleFile(const char* path) {

    ObjString* pathString = copyString(path, (int)strlen(path));
    tempRootPush(STRING_VAL(pathString));

    Value compilerResult;
    InterpretResult result = compileScript(pathString, &compilerResult);
    tempRootPush(compilerResult);

    Value compileError = structField(compilerResult, 0);
    Value compilerResultClosure = structField(compilerResult, 1);

    char const *file = strrchr(path, '/');
    if (file == 0) {
        file = path;
    } else {
        file++;
    }

    int returnCode = EX_OK;

    if (result == INTERPRET_RUNTIME_ERROR) {
        returnCode = EX_SOFTWARE;
    } else if (result == INTERPRET_OK && valuesEqual(compileError, BOOL_VAL(true))) {
        returnCode = EX_DATAERR;
    } else {
        returnCode = EX_OK;
        ObjFunction* function = AS_CLOSURE(compilerResultClosure)->function;

        disassembleChunk(&function->chunk, file);
    }

    tempRootPop();
    tempRootPop();
    return returnCode;
}
