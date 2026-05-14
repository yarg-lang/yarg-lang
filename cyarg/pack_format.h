#ifndef cyarg_pack_format_h
#define cyarg_pack_format_h

// internal header to packaging - pack.c and pack_load.c

#include <stdint.h>

#if defined(DEBUG_PACK)
#define DP(DPA) do { DPA; } while (0)
#else
#define DP(DPA)
#endif

#define PACKAGE_MAGIC_LEN 6

typedef struct ObjString ObjString;
typedef struct ObjInt ObjInt;
typedef struct ObjFunction ObjFunction;
typedef struct Chunk Chunk;

extern int8_t const packageMagic[PACKAGE_MAGIC_LEN];
extern int16_t const packageVersion;

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
// 18 A addresses(2) -- max 64k addresses
// 20 B body length(4)
// =-= Body =-=
// x8   double D*8
// x8   addresses A*8
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
    uint16_t numAddresses_;
    uint32_t bodyLength_;
} PackageFileHeader;

typedef struct {
    uint32_t n_;
    uint32_t extent_;
    uint32_t *i_;
} LinesFile;

enum { PACK_CONST_TYPE_S, PACK_CONST_TYPE_I, PACK_CONST_TYPE_D, PACK_CONST_TYPE_F, PACK_CONST_TYPE_A};

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
    uint32_t n_;
    uint32_t extent_;
    int64_t *i_;
} AddressesFile;

typedef struct {
    LinesFile linesFile_; // only included for debugging/error reporting may be excluded for compact binaries
    StringsFile stringsFile_;
    DoublesFile doublesFile_;
    IntsFile intsFile_;
    FunsFile funsFile_;
    AddressesFile addressesFile_;
} FlatFiles;

#endif
