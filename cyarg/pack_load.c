#include "pack.h"

#include "object.h"
#include "memory.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "pack_format.h"

enum { EX_OK = 0, EX_DATAERR = 65, EX_PROTOCOL = 71, EX_SOFTWARE = 70 };

struct ObjFunction *loadPackageFromBuffer(uint8_t* buffer, size_t bufferSize) {
    int r = EX_OK;
    ObjFunction **functions = 0;

    PackageFileHeader* h = (PackageFileHeader*)buffer;
    assert(sizeof *h == 24);

    if (memcmp(h->magic_, magic, PACKAGE_MAGIC_LEN) != 0) {
        r = EX_PROTOCOL;
    }
    if (h->version_ != version) {
        r = EX_DATAERR;
        goto exit;
    }

    assert(h->bodyLength_ == bufferSize - sizeof *h);

    uint8_t* const body = (uint8_t*)buffer + sizeof *h;
    assert((long)body % 8 == 0);

    double const *doubles = (double *)body;
    int64_t const *addresses = (int64_t *)(body + h->numDoubles_ * sizeof (double));
    uint8_t const *next =  (uint8_t *)addresses + h->numAddresses_ * sizeof (int64_t);

    typedef uint8_t Uint24[3];
    typedef struct {
        uint16_t numConsts_;
        uint16_t codeLength_;
        uint16_t arity_;
        uint16_t numUpvalues_;
        struct {
            uint8_t type_;
            Uint24 constOffset_;
        } typedIndexs[];
    } PackedChunk;
    PackedChunk const *chunks[256];

    DP(chunks__ = (uint32_t)(next - body));
    for (int i = 0; i < h->numChunks_; i++) {
        chunks[i] = (PackedChunk const *)next;
        next += 8 + 4 * chunks[i]->numConsts_;
    }
    uint8_t const *intFile = next;

    DP(ints__ = (uint32_t)(next - body));
    for (int i = 0; i < h->numInts_; i++) {
        Int const *thisInt = (Int const *)next;
        next += sizeof (Int) + sizeof (uint16_t) * thisInt->m_;
    }
    uint8_t const *stringFile = next;

    DP(strings__ = (uint32_t)(next - body));
    for (int i = 0; i < h->numStrings_; i++) {
        char const *thisString = (char const *)next;
        next += strlen(thisString) + 1;
        DP(printf("%s, %u, %u\n", thisString, (uint32_t)((next - body) - strings__), (uint32_t)(next - body)));
    }

    ObjFunction *currentFunction;
    functions = realloc(0, h->numChunks_ * sizeof (ObjFunction *));

    for (int i = 0; i < h->numChunks_; i++) {
        functions[i] = newFunction();
    }

    uint8_t const *startOfCode = next;
    DP(code__ = (uint32_t)(next - body));
    for (int i = 0; i < h->numChunks_; i++) {
        currentFunction = functions[i];
        initDynamicValueArray(&currentFunction->chunk.constants);
        currentFunction->chunk.xip = true;
        currentFunction->chunk.code = (uint8_t *)next; // discard the const and hope the vm does the right thing
        currentFunction->chunk.count = chunks[i]->codeLength_;
        currentFunction->arity = chunks[i]->arity_;
        currentFunction->upvalueCount = chunks[i]->numUpvalues_;
        next += chunks[i]->codeLength_;
    }

    DP(lines__ = (uint32_t)(next - body));
    DP(printf("line, offset\n"));
    for (int i = 0; i < h->numLines_; i++) {
        int offset = 0;
        memcpy(&offset, next, 3);
        if (offset != 0xffffff) {
            for (int c = 0; c < h->numChunks_; c++) {
                ObjFunction *cf = functions[c];
                size_t start = cf->chunk.code - startOfCode;
                size_t end = start + cf->chunk.count;
                if (offset >= start && offset < end) {
                    DP(printf("%d, %d, %d\n", i + 1, offset, offset - (int)start));
                    if (cf->chunk.numLines == cf->chunk.lineCapacity) {
                        int capacity = cf->chunk.lineCapacity;
                        int newCapacity = capacity + 16;
                        cf->chunk.lineCapacity = newCapacity;
                        cf->chunk.lines = reallocate(cf->chunk.lines, sizeof (ChunkSource) * capacity, sizeof (ChunkSource) * newCapacity);
                    }
                    cf->chunk.lines[cf->chunk.numLines++] = (ChunkSource){ .address = offset - start, .line = i + 1};
                    break;
                }
            }
        }
        next += 3;
    }

    DP(names__ = (uint32_t)(next - body));
    if (h->numLines_ > 0) {
        for (int i = 0; i < h->numChunks_; i++) {
            functions[i]->fName = copyString((char *)next, (int)strlen((char *)next)); // todo - mark as xip
            next += 16;
        }
    }

    DP(end__ = (uint32_t)(next - body));
    assert(h->numChunks_ >= 1);
    for (int i = 0; i < h->numChunks_; i++) {
        currentFunction = functions[i];
        DP(printf("Chunk:%d code@%p numConsts:%d\n", i, currentFunction->chunk.code, (int)chunks[i]->numConsts_));
        for (int c = 0; c < chunks[i]->numConsts_; c++) {
            uint32_t index = 0;
            memcpy(&index, chunks[i]->typedIndexs[c].constOffset_, 3);
            uint8_t type = chunks[i]->typedIndexs[c].type_;
#if defined (DEBUG_PACK)
            static const char *typesName[] = {"string", "int", "float", "fun", "addr"};
            printf("%d %s@%u", (int)c, typesName[type], index);
#endif
            switch (type) {
            case PACK_CONST_TYPE_S: {
                char *thisString = (char *)&stringFile[index];
                ObjString *obj = copyString(thisString, (int)strlen(thisString)); // mark as xip
                Value value = OBJ_VAL(obj);
                appendToDynamicValueArray(&currentFunction->chunk.constants, value);
                DP(printf(":\"%s\"", thisString));
                break;
            }
            case PACK_CONST_TYPE_I: {
                Int const *thisInt = (Int const *)&intFile[index];
                ObjInt *obj = (ObjInt *)allocateObject(sizeof (ObjInt) + thisInt->m_ * sizeof (uint16_t), OBJ_INT);
                memcpy(&obj->bigInt, thisInt, sizeof (Int) + thisInt->m_ * sizeof (uint16_t)); // should be able to shallow copy
                Value value = OBJ_VAL(obj);
                appendToDynamicValueArray(&currentFunction->chunk.constants, value);
                DP(printf(":");
                int_print(thisInt));
                break;
            }
            case PACK_CONST_TYPE_D: {
                double thisFloat = doubles[index];
                Value value = DOUBLE_VAL(thisFloat);
                appendToDynamicValueArray(&currentFunction->chunk.constants, value); // should be able to shallow copy
                DP(printf(":%f", thisFloat));
                break;
            }
            case PACK_CONST_TYPE_F: {
                ObjFunction *thisFun = functions[index];
                Value value = OBJ_VAL(thisFun);
                appendToDynamicValueArray(&currentFunction->chunk.constants, value);
                if (thisFun->fName != 0) {
                    char name[21];
                    int len = thisFun->fName->length;
                    len = len > 20 ? 20 : len;
                    memcpy(name, thisFun->fName->chars, len);
                    name[len] = '\0';
                    DP(printf(":%s", name));
                } else {
                    DP(printf(":<no name>"));
                }
                break;
            }
            case PACK_CONST_TYPE_A: {
                int64_t thisAddress = addresses[index];
                Value value = ADDRESS_VAL((uintptr_t)thisAddress);
                appendToDynamicValueArray(&currentFunction->chunk.constants, value); // should be able to shallow copy
                DP(printf(":%llx", thisAddress));
                break;
            }
            }
            DP(printf("\n"));
        }
    }

exit:
    if (functions != 0) {
        currentFunction = functions[0];
        free(functions);
    } else {
        currentFunction = 0;
    }

// todo leak for the moment until we can have the body owned by currentFunction or vm
//    free(body);

    DP(printf("chunks__:%u\nints__:%u\nstrings__:%u\ncode__:%u\nlines__:%u\nnames__:%u\nend__:%u\n", chunks__, ints__, strings__, code__, lines__, names__, end__));

    if (r == EX_OK)
        return currentFunction;
    else
        return 0;
}
