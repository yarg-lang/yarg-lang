#ifndef cyarg_fs_h
#define cyarg_fs_h

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

char* readFile(const char* path);
size_t fileSize(const char* path);
void readFileIntoBuffer(const char* path, uint8_t* buffer, size_t bufferSize);
bool fileExists(const char* path);

#endif
