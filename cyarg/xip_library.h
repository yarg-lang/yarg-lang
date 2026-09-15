#ifndef cyarg_xip_library_h
#define cyarg_xip_library_h

#include <stdint.h>
#include <stddef.h>
#include "value.h"

/*
 * xipLibrary
 *
 * For reading a collection of files that are stored in a linearized format in memory.
 * Intended for XIP Yarg bytecode.
 */

bool xipLibraryReadFilename(const char* filename, const uint8_t** data, size_t* size);
bool xipLibraryReadNode(uint16_t node, const uint8_t** data, size_t* size);

#endif


