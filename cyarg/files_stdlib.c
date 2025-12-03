#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "files.h"
#include "print.h"

char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        FPRINTMSG(stderr, "Could not open file \"%s\".\n", path);
        char cwdpath[1024];
        if (getcwd(cwdpath, sizeof(cwdpath)) != NULL) {
            FPRINTMSG(stderr, "Current working directory: \"%s\"\n", cwdpath);
        }
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        FPRINTMSG(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        FPRINTMSG(stderr, "could not read file \"%s\".\n", path);
        exit(74);

    }
    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}
