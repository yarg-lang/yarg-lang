#include "pack.h"

#include "object.h"
#include "memory.h"

#include <stdlib.h>
#include <stdio.h>
#include <sysexits.h>
#include <assert.h>

#include "pack_format.h"

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
        written = fwrite__(s->chars, 1, s->length, file);
        if (written != s->length) return EX_SOFTWARE;
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
