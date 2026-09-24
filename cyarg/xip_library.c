#include "xip_library.h"
#include <assert.h>
#include <stdalign.h>

// the library is linearised a set of 'nodes', all concatenated in memory.
// the tool creating the library will pad nodes as needed for alignment.
// the first node (0) contains an index offset and length of all nodes, including itself.

struct XIPLibHeader {
    alignas(1) uint8_t  magic[4];
    alignas(2) uint16_t byteOrder;
    alignas(2) uint16_t version;
    alignas(4) uint32_t length;
    alignas(2) uint16_t directoryNode;
    alignas(1) uint8_t  nodeZeroOffset;
};

// with these serialised in a little endian byte order, the first 5 bytes will spell 'yargX' (with 0xa for a)
const uint8_t expectedMagic[4] = { 'y', 0x0a, 'r', 'g' };
const uint16_t expectedByteOrder = 0xff58;

const uint16_t expectedVersion = 0x2602;

const struct XIPLibHeader *const xipLibHeader = (const struct XIPLibHeader*)&cyarg_ylib[0];
const uint8_t* const xipLibraryBytes = &cyarg_ylib[0];

struct nodeIndex {
    uint32_t offset;
    uint32_t length;
};

const struct nodeIndex* nodeIndex(uint16_t node) {
    const uint8_t* const nodeZero = &xipLibraryBytes[xipLibHeader->nodeZeroOffset];
    const struct nodeIndex* index = (const struct nodeIndex*)nodeZero;
    assert(index[0].offset == xipLibHeader->nodeZeroOffset);
    return &index[node];
}

size_t nodeSize(uint16_t node) {
    const struct nodeIndex* index = nodeIndex(node);
    return index->length;
}

uint16_t nodeCount() {
    size_t length = nodeSize(0);
    return (uint16_t)(length / sizeof(struct nodeIndex));
}

const uint8_t* nodeData(uint16_t node) {
    const struct nodeIndex* index = nodeIndex(node);
    return &xipLibraryBytes[index->offset];
}

const uint16_t bootstrap_node = 1;
const uint16_t root_directory_node = 3;

struct directoryEntry {
    uint16_t fileNode;
    uint16_t nameNode;
};

const struct directoryEntry* directoryEntryRoot() {
    const uint8_t* indexNode = nodeData(root_directory_node);
    return (const struct directoryEntry*)indexNode;
}

size_t directoryEntryCount() {
    const struct nodeIndex* index = nodeIndex(root_directory_node);
    return index->length / sizeof(struct directoryEntry);
}

const struct directoryEntry* directoryEntryForFile(const char* filename) {
    size_t entries = directoryEntryCount();
    for (size_t i = 0; i < entries; i++) {
        const struct directoryEntry* entry = &directoryEntryRoot()[i];
        const uint8_t* nameNode = nodeData(entry->nameNode);
        const char* name = (const char*)nameNode;
        if (strcmp(name, filename) == 0) {
            return entry;
        }
    }

    return NULL;
}

void xipLibraryInvariant() {
    assert(xipLibHeader->version == expectedVersion);
    assert(xipLibHeader->length == cyarg_ylib_len);
    assert(xipLibHeader->byteOrder == expectedByteOrder);

    for (int i = 0; i < sizeof(xipLibHeader->magic) / sizeof(xipLibHeader->magic[0]); i++) {
        assert(xipLibHeader->magic[i] == expectedMagic[i]);
    }
}

bool xipLibraryReadFilename(const char* filename, const uint8_t** data, size_t* size) {
    xipLibraryInvariant();

    const struct directoryEntry* entry = directoryEntryForFile(filename);
    if (!entry) {
        return false;
    }
    *data = nodeData(entry->fileNode);
    *size = nodeSize(entry->fileNode);
    return true;
}

bool xipLibraryReadNode(uint16_t node, const uint8_t** data, size_t* size) {
    xipLibraryInvariant();
    if (node >= nodeCount()) {
        return false;
    } else {
        *data = nodeData(node);
        *size = nodeSize(node);
        return true;
    }
}

const char* xipLibraryStringNode(uint16_t node) {
    const uint8_t* data = nodeData(node);
    if (!data) {
        return NULL;
    }
    return (const char*)data;
}