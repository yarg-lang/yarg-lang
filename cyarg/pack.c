#include "pack.h"

#include "object.h"
#include "memory.h"

#include <stdlib.h>
#include <stdio.h>
#include <sysexits.h>
#include <assert.h>

#if defined(DEBUG_PACK)
#define DP(DPA) do { DPA; } while (0)
#else
#define DP(DPA)
#endif

#define PACKAGE_MAGIC_LEN 6

static int8_t const magic[PACKAGE_MAGIC_LEN] = {0x79, 0x0a, 0x72, 0x67, 0xff, 0x42};
static int16_t const version = 0x2601;

//
// xN is alignment from start of body, number is offset, int16(2)/24(3) types are lsb first
//
// =-= Header =-=
// 0    magic(6) - 79 0a 72 67 00 42
// 6    version(2) - 26, 01 - year, issue (update for any changes to byte-code, number format, file format)
// 8  M chunks(2) -- 0..65535 (0: just global)
// 10 N strings(2) -- max 64k strings
// 12 D doubles(2) -- max 64k floats
// 14 Q ints(2) -- max 64k ints
// 16 L lines(2) -- min zero, max 64k lines - debug/error reporting
// 18   packing(2)
// 20 B body length(4)
// =-= Body =-=
// x8   double D*8
// per chunk -- m == M-1
// x8 K0    num consts in chunk0 2
// x1       code length for chunk0 2
// x1       0 2
// x1       0 2
// x8       consts -- K0*4 - type(1):index/offset(3)
// x8 K1    num consts in chunk0 2
// x1       code length for chunk0 2
// x1       arity 2
// x1       num upvalues 2
// x4       consts -- K1*4
// …
// x4 Km    num consts in chunk0 2
// x1       code length for chunk0 2
// x1       arity 2
// x1       num upvalues 2
// x4       consts -- Km*4
// x4   ints *1 -- these could be shrunk by two or four bytes each, but would not then be xip
// x1   strings *1
// x1   function arities(M-1)*1
// x1   code *1
// x1   code offsets for lines L*3
// x1   function names (M)x16 -- included if (L > 0) debug/error reporting, truncated if over 15 chars

#if defined(DEBUG_PACK)
uint32_t chunks__ = 0;
uint32_t ints__ = 0;
uint32_t strings__ = 0;
uint32_t code__ = 0;
uint32_t lines__ = 0;
uint32_t names__ = 0;
uint32_t end__ = 0;
#endif

typedef struct {
    uint8_t magic_[PACKAGE_MAGIC_LEN];  // 79 0a 72 67 ff 42 "y\x0arg\xffB"
    uint16_t version_;                  // update for any changes to byte-code, number format or package format)
    uint16_t numChunks_;
    uint16_t numStrings_;
    uint16_t numDoubles_;
    uint16_t numInts_;
    uint16_t numLines_;
    uint16_t packing_;                  // reserved for future use and to ensure body is 8-byte aligned in xip
    uint32_t bodyLength_;
} PackageFileHeader;

typedef struct {
    uint32_t n_;
    uint32_t extent_;
    uint32_t *i_;
} LinesFile;

enum { PACK_CONST_TYPE_S, PACK_CONST_TYPE_I, PACK_CONST_TYPE_D, PACK_CONST_TYPE_F} type_;

typedef struct {
    uint8_t type_;
    uint32_t index_; // index for d and f
    uint32_t offset_; // offset for s and i - calculated after duplicates removed
} ConstItem;

// if const were sorted by type ConstItem::type_ and FlatChunk::numConsts_ could be replaced by
//  FlatChunk::numS_, FlatChunk::numI_, FlatChunk::numD_
typedef struct {
    uint8_t *code_;
    int codeLength_;
    int codeOffset_;
    struct ConstTypesAndOffsets {
        uint32_t numConsts_;
        uint32_t extent_;
        ConstItem *i_;
    } constTypesAndOffsets_;
} FlatChunk;

typedef struct {
    uint32_t n_;
    uint32_t extent_;
        ObjString **i_;
    int totalStringLength_;
} StringsFile;

typedef struct {
    uint32_t n_;
    uint32_t extent_;
    double *i_;
} DoublesFile;

typedef struct {
    uint32_t n_;
    uint32_t extent_;
    ObjInt **i_;
    int totalIntsLength_;
} IntsFile;

typedef struct {
    uint32_t n_;
    uint32_t extent_;
    struct Fun {
        ObjFunction const *f_;
        FlatChunk chunk_;
    } *i_;
    int totalCodeLength_;
} FunsFile;

typedef struct {
    LinesFile linesFile_; // only included for debugging/error reporting may be excluded for compact binaries
    StringsFile stringsFile_;
    DoublesFile doublesFile_;
    IntsFile intsFile_;
    FunsFile funsFile_;
} FlatFiles;

static void fileSet(void *, int, size_t i);
static void fileExtend(void *, int, size_t);
static void flattenConstants(int funIndex, Chunk const *, FlatFiles *);
static void removecapturedNames(FlatFiles *);
static void calcStringAndIntOffsets(FlatFiles *);
static void flattenLines(FlatFiles *);
static int pack(char const *, FlatFiles *, FILE *);

int packScript(char const *sourceFileName, struct ObjFunction const *scriptFn, bool includeLines, char const *path) {
    FILE *file = fopen(path, "wb");
    if (file == 0) return EX_DATAERR;

    int r = EX_OK;

    FlatFiles f = {0};
    if (includeLines) {
        fileSet(&f.linesFile_, 64, sizeof *f.linesFile_.i_);
    }
    fileSet(&f.stringsFile_, 16, sizeof *f.stringsFile_.i_);
    fileSet(&f.doublesFile_, 4, sizeof *f.doublesFile_.i_);
    fileSet(&f.intsFile_, 4, sizeof *f.intsFile_.i_);
    fileSet(&f.funsFile_, 4, sizeof *f.funsFile_.i_);

    f.funsFile_.n_ = 1;
    f.funsFile_.i_[0].f_ = scriptFn;
    f.funsFile_.totalCodeLength_ = 0;
    flattenConstants(0, &scriptFn->chunk, &f);

#if 0
    // todo remove non escapee i.e. non global, non returned method/property
    removecapturedNames(&f);
#endif

    calcStringAndIntOffsets(&f);

    if (includeLines) {
        flattenLines(&f);
    }

    r = pack(sourceFileName, &f, file);
    if (r != EX_OK) goto exit;

exit:
    fclose(file);

    for (int i = 0; i < f.funsFile_.n_; i++) {
        free(f.funsFile_.i_[i].chunk_.constTypesAndOffsets_.i_);
    }
    free(f.funsFile_.i_);
    free(f.intsFile_.i_);
    free(f.doublesFile_.i_);
    free(f.stringsFile_.i_);
    free(f.linesFile_.i_);

    if (r != EX_OK) {
        remove(path);
    }

    return r;
}

struct ObjFunction *loadPackage(char const *path) {
    int r = EX_OK;
    ObjFunction **functions = 0;

    FILE *file = fopen(path, "rb");
    if (file == 0) return 0;

    PackageFileHeader h = {0};
    assert(sizeof h == 24);

    size_t n = fread(&h, sizeof h, 1, file);
    assert(n == 1);

    if (memcmp(h.magic_, magic, PACKAGE_MAGIC_LEN) != 0) {
        r = EX_PROTOCOL;
    }
    if (h.version_ != version) {
        r = EX_DATAERR;
        goto exit;
    }

    uint8_t* const body = realloc(0, h.bodyLength_);
    assert((long)body % 8 == 0);

    size_t bodyLen = fread(body, 1, h.bodyLength_, file);
    assert(bodyLen == h.bodyLength_);

    double *doubles = (double *)body;

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

    uint8_t const *next = body + h.numDoubles_ * sizeof (double);

    DP(chunks__ = (uint32_t)(next - body));
    for (int i = 0; i < h.numChunks_; i++) {
        chunks[i] = (PackedChunk const *)next;
        next += 8 + 4 * chunks[i]->numConsts_;
    }
    uint8_t const *intFile = next;

    DP(ints__ = (uint32_t)(next - body));
    for (int i = 0; i < h.numInts_; i++) {
        Int const *thisInt = (Int const *)next;
        next += sizeof (Int) + sizeof (uint16_t) * thisInt->m_;
    }
    uint8_t const *stringFile = next;

    DP(strings__ = (uint32_t)(next - body));
    for (int i = 0; i < h.numStrings_; i++) {
        char const *thisString = (char const *)next;
        next += strlen(thisString) + 1;
        DP(printf("%s, %u, %u\n", thisString, (uint32_t)((next - body) - strings__), (uint32_t)(next - body)));
    }

    ObjFunction *currentFunction;
    functions = realloc(0, h.numChunks_ * sizeof (ObjFunction *));

    for (int i = 0; i < h.numChunks_; i++) {
        functions[i] = newFunction();
    }

    uint8_t const *startOfCode = next;
    DP(code__ = (uint32_t)(next - body));
    for (int i = 0; i < h.numChunks_; i++) {
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
    for (int i = 0; i < h.numLines_; i++) {
        int offset = 0;
        memcpy(&offset, next, 3);
        if (offset != 0xffffff) {
            for (int c = 0; c < h.numChunks_; c++) {
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
    if (h.numLines_ > 0) {
        for (int i = 0; i < h.numChunks_; i++) {
            functions[i]->fName = copyString((char *)next, (int)strlen((char *)next)); // todo - mark as xip
            next += 16;
        }
    }

    DP(end__ = (uint32_t)(next - body));
    assert(h.numChunks_ >= 1);
    for (int i = 0; i < h.numChunks_; i++) {
        currentFunction = functions[i];
        DP(printf("Chunk:%d code@%p numConsts:%d\n", i, currentFunction->chunk.code, (int)chunks[i]->numConsts_));
        for (int c = 0; c < chunks[i]->numConsts_; c++) {
            uint32_t index = 0;
            memcpy(&index, chunks[i]->typedIndexs[c].constOffset_, 3);
            uint8_t type = chunks[i]->typedIndexs[c].type_;
#if defined (DEBUG_PACK)
            static const char *typesName[] = {"string", "int", "float", "fun"};
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
            }
            DP(printf("\n"));
        }
    }

exit:
    fclose(file);
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

void flattenConstants(int chunkIndex, Chunk const *chunk, FlatFiles *f) {
    int startSubs = f->funsFile_.n_;
    int constCount = chunk->constants.count;

    ObjFunction const *cf = f->funsFile_.i_[chunkIndex].f_;
    FlatChunk *fc = &f->funsFile_.i_[chunkIndex].chunk_;

    fc->code_ = cf->chunk.code;
    fc->codeLength_ = cf->chunk.count;
    fc->codeOffset_ = f->funsFile_.totalCodeLength_;
    f->funsFile_.totalCodeLength_ += fc->codeLength_;

    fc->constTypesAndOffsets_.numConsts_ = 0;
    fc->constTypesAndOffsets_.i_ = 0;
    fileSet(&fc->constTypesAndOffsets_, constCount, sizeof *fc->constTypesAndOffsets_.i_);
    fc->constTypesAndOffsets_.numConsts_ = constCount;

    for (int i = 0; i < constCount; i++) {
        Value *v = &chunk->constants.values[i];
        if (IS_DOUBLE(*v)) {
            fc->constTypesAndOffsets_.i_[i] = (ConstItem){ .type_ = PACK_CONST_TYPE_D, .index_ = f->doublesFile_.n_ };
            fileExtend(&f->doublesFile_, 4, sizeof *f->doublesFile_.i_);
            f->doublesFile_.i_[f->doublesFile_.n_++] = AS_DOUBLE(*v);
        } else if (IS_OBJ(*v)) {
            switch (AS_OBJ(*v)->type) {
            case OBJ_FUNCTION: {
                fc->constTypesAndOffsets_.i_[i] = (ConstItem){ .type_ = PACK_CONST_TYPE_F, .index_ = f->funsFile_.n_ };
                fileExtend(&f->funsFile_, 4, sizeof *f->funsFile_.i_);
                fc = &f->funsFile_.i_[chunkIndex].chunk_; // fc invalidated by fileExtend() above
                f->funsFile_.i_[f->funsFile_.n_++].f_ = AS_FUNCTION(*v);
                break;
            }
            case OBJ_INT: {
                fc->constTypesAndOffsets_.i_[i] = (ConstItem){ .type_ = PACK_CONST_TYPE_I, .index_ = f->intsFile_.n_ };
                fileExtend(&f->intsFile_, 16, sizeof *f->intsFile_.i_);
                f->intsFile_.i_[f->intsFile_.n_++] = AS_INTOBJ(*v);
               break;
            }
            case OBJ_STRING: {
                ObjString *from = AS_STRING(*v);
                int sI = 0;
                for (; sI < f->stringsFile_.n_;sI++) {
                    if (f->stringsFile_.i_[sI] == from) {
                        fc->constTypesAndOffsets_.i_[i] = (ConstItem){ .type_ = PACK_CONST_TYPE_S, .index_ = sI };
                        break;
                    }
                }
                if (sI == f->stringsFile_.n_) {
                    fc->constTypesAndOffsets_.i_[i] = (ConstItem){ .type_ = PACK_CONST_TYPE_S, .index_ = f->stringsFile_.n_ };
                    fileExtend(&f->stringsFile_, 16, sizeof *f->stringsFile_.i_);
                    f->stringsFile_.i_[f->stringsFile_.n_++] = from;
                }
               break;
            }
            default:
                assert(!"Unexpected constant obj");
                break;
            }
        } else {
            assert(!"Unexpected constant value");
        }
    }

    int endSubs = f->funsFile_.n_;
    for (int i = startSubs; i < endSubs; i++) {
        DP(char nn[21];
           int len = f->funsFile_.i_[i].f_->fName->length;
           len = len > 20 ? 20 : len;
           memcpy(nn, f->funsFile_.i_[i].f_->fName->chars, len);
           nn[len] = 0;
           printf("%s:%d\n", nn, i));
        flattenConstants(i, &f->funsFile_.i_[i].f_->chunk, f);
    }
}

#if 0
void removecapturedNames(FlatFiles *f) {
    for (int funI = 0; funI < f->funsFile_.n_; funI++) {
        struct Fun *fun = &f->funsFile_.i_[funI];
        struct ConstTypesAndOffsets *ctao = &fun->chunk_.constTypesAndOffsets_;
        for (int constI = 0; constI < ctao->numConsts_; constI++) {
            ConstItem *ci = &ctao->i_[constI];
        }
    }
}
#endif

void calcStringAndIntOffsets(FlatFiles *f) {
    int sOffset = 0;
    for (int sI = 0; sI < f->stringsFile_.n_; sI++) {
        for (int fI = 0; fI < f->funsFile_.n_; fI++) {
            struct ConstTypesAndOffsets *ctao = &f->funsFile_.i_[fI].chunk_.constTypesAndOffsets_;
            for (int constI = 0; constI < ctao->numConsts_; constI++) {
                ConstItem *ci = &ctao->i_[constI];
                if (ci->type_ == PACK_CONST_TYPE_S && ci->index_ == sI) {
                    ci->offset_ = sOffset; // don’t break as there may be duplicate strings
                }
            }
        }
        sOffset += f->stringsFile_.i_[sI]->length + 1;
    }
    f->stringsFile_.totalStringLength_ = sOffset;

    int iOffset = 0;
    for (int iI = 0; iI < f->intsFile_.n_; iI++) {
        for (int fI = 0; fI < f->funsFile_.n_; fI++) {
            struct ConstTypesAndOffsets *ctao = &f->funsFile_.i_[fI].chunk_.constTypesAndOffsets_;
            for (int constI = 0; constI < ctao->numConsts_; constI++) {
                ConstItem *ci = &ctao->i_[constI];
                if (ci->type_ == PACK_CONST_TYPE_I && ci->index_ == iI) {
                    ci->offset_ = iOffset; // don’t break as there may be duplicate ints
                }
            }
        }
        Int *from = &f->intsFile_.i_[iI]->bigInt;
        int len = (int)((char *) from->w_ - (char *) from);
        assert(len == 4);
        len += sizeof (uint16_t) * (from->d_ + from->d_ % 2);
        iOffset += len;
    }
    f->intsFile_.totalIntsLength_ = iOffset;

    DP(printf("int:%d string:%d\n", f->intsFile_.totalIntsLength_, f->stringsFile_.totalStringLength_));
}

void flattenLines(FlatFiles *f) {
    for (int c = 0; c < f->funsFile_.n_; c++) {
        FlatChunk *cf = &f->funsFile_.i_[c].chunk_;
        Chunk const *chunk = &f->funsFile_.i_[c].f_->chunk;

        if (chunk->numLines > 0) {
            int lastLine = chunk->lines[chunk->numLines - 1].line;
            int flattenedLines = f->linesFile_.n_;
            
            if (lastLine > flattenedLines) {
                fileSet(&f->linesFile_, lastLine, sizeof *f->linesFile_.i_);
                f->linesFile_.n_ = lastLine;
                memset(&f->linesFile_.i_[flattenedLines], 0xff, sizeof (uint32_t) * (lastLine - flattenedLines));
            }
            
            for (int i = 0; i < chunk->numLines; i++) {
                int a = chunk->lines[i].address;
                int l = chunk->lines[i].line;
                assert(l <= f->linesFile_.extent_);
                if (l > f->linesFile_.n_) f->linesFile_.n_ = l;
                f->linesFile_.i_[l - 1] = a + cf->codeOffset_;
            }
        }
    }
}

#if defined(DEBUG_PACK)
uint32_t offset__ = 0;
size_t fwrite__(const void *__ptr, size_t __size, size_t __nitems, FILE *__stream) {
    size_t w = fwrite(__ptr, __size, __nitems, __stream);
    offset__ += (uint32_t)(__size * w);
    return w;
}
#else
#define fwrite__(P__, S__, N__, F__) fwrite(P__, S__, N__, F__)
#endif // !DEBUGGING_PACK

int pack(char const *sourceFileName, FlatFiles *f, FILE *file) {

    PackageFileHeader h;

    memcpy(&h.magic_, magic, PACKAGE_MAGIC_LEN);
    h.version_ = version;
    h.numChunks_ = f->funsFile_.n_;
    h.numStrings_ = f->stringsFile_.n_;
    h.numInts_ = f->intsFile_.n_;
    h.numDoubles_ = f->doublesFile_.n_;
    h.numLines_ = f->linesFile_.n_;
    h.packing_ = 0xffff;
    h.bodyLength_ = 0;

    h.bodyLength_ += h.numDoubles_ * sizeof (double);

    h.bodyLength_ += 8 * f->funsFile_.n_;
    for (int i = 0; i < f->funsFile_.n_; i++) {
        h.bodyLength_ += 4 * f->funsFile_.i_[i].chunk_.constTypesAndOffsets_.numConsts_;
    }
    h.bodyLength_ += f->intsFile_.totalIntsLength_;
    assert(h.bodyLength_ % 4 == 0);
    h.bodyLength_ += f->stringsFile_.totalStringLength_;
    h.bodyLength_ += f->funsFile_.totalCodeLength_;
    h.bodyLength_ += 3 * h.numLines_;
    if (h.numLines_ > 0) {
        h.bodyLength_ += 16 * h.numChunks_;
    }

    size_t written = fwrite__(&h, sizeof h, 1, file);
    if (written != 1) return EX_SOFTWARE;

    DP(offset__ = 0);
    for (int i = 0; i < f->doublesFile_.n_; i++) {
        double *d = &f->doublesFile_.i_[i];
        written = fwrite__(d, sizeof (double), 1, file);
        if (written != 1) return EX_SOFTWARE;
    }

    DP(chunks__ = offset__);
    for (int i = 0; i < f->funsFile_.n_; i++) {
        FlatChunk *fc = &f->funsFile_.i_[i].chunk_;
        uint16_t arity, numUpvalues;
        if (i == 0) {
            arity = numUpvalues = 0;
        } else {
            ObjFunction const *fn = f->funsFile_.i_[i].f_;
            arity = fn->arity;
            numUpvalues = fn->upvalueCount;
        }

        written = fwrite__(&fc->constTypesAndOffsets_.numConsts_, sizeof (uint16_t), 1, file);
        if (written != 1) return EX_SOFTWARE;
        written = fwrite__(&fc->codeLength_, sizeof (uint16_t), 1, file);
        if (written != 1) return EX_SOFTWARE;
        written = fwrite__(&arity, sizeof (uint16_t), 1, file);
        if (written != 1) return EX_SOFTWARE;
        written = fwrite__(&numUpvalues, sizeof (uint16_t), 1, file);
        if (written != 1) return EX_SOFTWARE;
        for (int k = 0; k < fc->constTypesAndOffsets_.numConsts_; k++) {
            ConstItem *ci = &fc->constTypesAndOffsets_.i_[k];
            assert(ci->type_ >= 0 && ci->type_ <= 3);
            written = fwrite__(&ci->type_, sizeof (char), 1, file);
            if (written != 1) return EX_SOFTWARE;
            uint32_t indexOrOffset = ci->type_ == PACK_CONST_TYPE_S || ci->type_ == PACK_CONST_TYPE_I ? ci->offset_ : ci->index_;
            written = fwrite__(&indexOrOffset, sizeof (char), 3, file);
            if (written != 3) return EX_SOFTWARE;
            DP(if (ci->type_ == PACK_CONST_TYPE_S) {
                printf("%s, %d\n", f->stringsFile_.i_[ci->index_]->chars, ci->offset_);
            });
        }
    }

    DP(ints__ = offset__);
    for (int iI = 0; iI < f->intsFile_.n_; iI++) {
        Int *bigInt = &f->intsFile_.i_[iI]->bigInt;
        IntConcrete254 t;
        int_set_t(bigInt, int_init_concrete254(&t));
        t.m_ = t.d_ + t.d_ % 2;
        written = fwrite__(&t, sizeof (Int) + sizeof (uint16_t) * t.m_, 1, file);
        if (written != 1) return EX_SOFTWARE;
    }
    DP(assert(offset__ - ints__ == f->intsFile_.totalIntsLength_));

    DP(strings__ = offset__);
    for (int sI = 0; sI < f->stringsFile_.n_; sI++) {
        ObjString *s = f->stringsFile_.i_[sI];
        written = fwrite__(s->chars, s->length, 1, file);
        if (written != 1) return EX_SOFTWARE;
        char nullChar = 0;
        written = fwrite__(&nullChar, 1, 1, file);
        if (written != 1) return EX_SOFTWARE;
        DP(char sb[100];
           memcpy(sb, s->chars, s->length); sb[s->length] = '\0';
           printf("%s, %u, %u\n", sb, offset__ - strings__ - s->length - 1, offset__ - s->length - 1));
    }
    DP(assert(offset__ - strings__ == f->stringsFile_.totalStringLength_));

    DP(code__ = offset__);
    for (int i = 0; i < f->funsFile_.n_; i++) {
        FlatChunk *fc = &f->funsFile_.i_[i].chunk_;
        written = fwrite__(fc->code_, fc->codeLength_, 1, file);
        if (written != 1) return EX_SOFTWARE;
    }

    DP(lines__ = offset__);
    for (int i = 0; i < f->linesFile_.n_; i++) {
        uint32_t offset = f->linesFile_.i_[i];
        written = fwrite__(&offset, sizeof (char), 3, file);
        if (written != 3) return EX_SOFTWARE;
    }

    DP(names__ = offset__);
    if (f->linesFile_.n_ > 0) {
        for (int i = 0; i < f->funsFile_.n_; i++) {
            char const *name;
            int len;
            if (i == 0) {
                name = sourceFileName;
                len = (int) strlen(sourceFileName);
            } else {
                name = f->funsFile_.i_[i].f_->fName->chars;
                len = f->funsFile_.i_[i].f_->fName->length;
            }
            if (len > 15) len = 15;
            written = fwrite__(name, sizeof (char), len, file);
            if (written != len) return EX_SOFTWARE;
            for (; len < 16; len++) {
                char nullChar = 0;
                written = fwrite__(&nullChar, sizeof (char), 1, file);
                if (written != 1) return EX_SOFTWARE;
            }
            DP(printf("%s\n", name));
        }
    }
    DP(end__ = offset__);
    DP(printf("chunks__:%u\nints__:%u\nstrings__:%u\ncode__:%u\nlines__:%u\nnames__:%u\nend__:%u\n", chunks__, ints__, strings__, code__, lines__, names__, end__));

    return EX_OK;
}

void fileSet(void *f, int n, size_t i) {

    LinesFile *file = (LinesFile *) f;

    file->extent_ = n;
    file->i_ = realloc(file->i_, file->extent_ * i);
}

void fileExtend(void *f, int n, size_t i) {

    LinesFile *file = (LinesFile *) f;

    if (n > file->extent_ - file->n_) {
        fileSet(f, file->extent_ + n * 2, i);
    }
}
