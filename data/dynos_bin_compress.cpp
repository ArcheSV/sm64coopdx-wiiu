#include "dynos.cpp.h"
#include <zlib.h>

extern "C" {
#include "pc/mods/mod_fs.h"
#include "pc/platform.h"
}

static const u8 DYNOS_BIN_COMPRESS_MAGIC[] = { 'D', 'Y', 'N', 'O', 'S', 'B', 'I', 'N' };
static FILE  *sFile = NULL;
static u8 *sBufferUncompressed = NULL;
static u8 *sBufferCompressed = NULL;
static u64 sLengthUncompressed = 0;
static u64 sLengthCompressed = 0;

static inline bool DynOS_Bin_Compress_MagicEquals(const u8 *aBuffer) {
    return memcmp(aBuffer, DYNOS_BIN_COMPRESS_MAGIC, sizeof(DYNOS_BIN_COMPRESS_MAGIC)) == 0;
}

static inline u64 DynOS_Bin_Compress_ReadU64LE(const u8 *aBuffer) {
    u64 value = 0;
    for (u32 i = 0; i < sizeof(u64); i++) {
        value |= ((u64) aBuffer[i]) << (i * 8);
    }
    return value;
}

static inline void DynOS_Bin_Compress_WriteU64LE(u8 *aBuffer, u64 aValue) {
    for (u32 i = 0; i < sizeof(u64); i++) {
        aBuffer[i] = (u8) ((aValue >> (i * 8)) & 0xFF);
    }
}

static inline void DynOS_Bin_Compress_Init() {
    sFile = NULL;
    sBufferUncompressed = NULL;
    sBufferCompressed = NULL;
    sLengthUncompressed = 0;
    sLengthCompressed = 0;
}

static inline void DynOS_Bin_Compress_Close() {
    if (sFile) f_close(sFile);
    sFile = NULL;
}

static inline void DynOS_Bin_Compress_Free() {
    if (sBufferCompressed) free(sBufferCompressed);
    if (sBufferUncompressed) free(sBufferUncompressed);
    DynOS_Bin_Compress_Close();
}

static inline bool DynOS_Bin_Compress_Check(bool condition, const char *function, const char *filename, const char *message) {
    if (!condition) {
        PrintError("ERROR: %s: File \"%s\": %s", function, filename, message);
        DynOS_Bin_Compress_Free();
        return false;
    }
    return true;
}

bool DynOS_Bin_IsCompressed(const SysPath &aFilename) {
    DynOS_Bin_Compress_Init();

    // Open input file
    if (!DynOS_Bin_Compress_Check(
        (sFile = fopen(aFilename.c_str(), "rb")) != NULL,
        __FUNCTION__, aFilename.c_str(), "Cannot open file"
    )) return false;

    // Read magic
    u8 _Magic[sizeof(DYNOS_BIN_COMPRESS_MAGIC)] = { 0 };
    if (!DynOS_Bin_Compress_Check(
        fread(_Magic, sizeof(u8), sizeof(_Magic), sFile) == sizeof(_Magic),
        __FUNCTION__, aFilename.c_str(), "Cannot read magic"
    )) return false;

    // Compare with magic constant
    if (!DynOS_Bin_Compress_MagicEquals(_Magic)) {
        DynOS_Bin_Compress_Free();
        return false;
    }

    // It is a compressed file
    DynOS_Bin_Compress_Free();
    return true;
}

bool DynOS_Bin_Compress(const SysPath &aFilename) {
    DynOS_Bin_Compress_Init();
    PrintNoNewLine("Compressing file \"%s\"...", aFilename.c_str());

    // Open input file
    if (!DynOS_Bin_Compress_Check(
        (sFile = fopen(aFilename.c_str(), "rb")) != NULL,
        __FUNCTION__, aFilename.c_str(), "Cannot open file"
    )) return false;

    // Retrieve file length
    if (!DynOS_Bin_Compress_Check(
        fseek(sFile, 0, SEEK_END) == 0,
        __FUNCTION__, aFilename.c_str(), "Cannot retrieve file length"
    )) return false;

    // Check file length
    if (!DynOS_Bin_Compress_Check(
        (sLengthUncompressed = (u64) ftell(sFile)) != 0,
        __FUNCTION__, aFilename.c_str(), "Empty file"
    )) return false;

    // Allocate memory for uncompressed buffer
    if (!DynOS_Bin_Compress_Check(
        (sBufferUncompressed = (u8 *) calloc(sLengthUncompressed, sizeof(u8))) != NULL,
        __FUNCTION__, aFilename.c_str(), "Cannot allocate memory for compression"
    )) return false; else rewind(sFile);

    // Read input data
    if (!DynOS_Bin_Compress_Check(
        fread(sBufferUncompressed, sizeof(u8), sLengthUncompressed, sFile) == sLengthUncompressed,
        __FUNCTION__, aFilename.c_str(), "Cannot read uncompressed data"
    )) return false; else DynOS_Bin_Compress_Close();

    // Compute maximum output file size
    if (!DynOS_Bin_Compress_Check(
        (sLengthCompressed = compressBound(sLengthUncompressed)) != 0,
        __FUNCTION__, aFilename.c_str(), "Cannot compute compressed size"
    )) return false;

    // Allocate memory for compressed buffer
    if (!DynOS_Bin_Compress_Check(
        (sBufferCompressed = (u8 *) calloc(sLengthCompressed, sizeof(u8))) != NULL,
        __FUNCTION__, aFilename.c_str(), "Cannot allocate memory for compression"
    )) return false;

    // Compress data
    uLongf _LengthCompressed = (uLongf)sLengthCompressed;
    if (!DynOS_Bin_Compress_Check(
        compress2(sBufferCompressed, &_LengthCompressed, sBufferUncompressed, sLengthUncompressed, Z_BEST_COMPRESSION) == Z_OK,
        __FUNCTION__, aFilename.c_str(), "Cannot compress data"
    )) return false;
    sLengthCompressed = _LengthCompressed;

    // Check output length
    // If the compression generates a bigger file, skip the process, but don't return a failure
    if (!DynOS_Bin_Compress_Check(
        sLengthCompressed < sLengthUncompressed,
        __FUNCTION__, aFilename.c_str(), "Compressed data is bigger than uncompressed; Skipping compression"
    )) return true;

    // Open output file
    if (!DynOS_Bin_Compress_Check(
        (sFile = fopen(aFilename.c_str(), "wb")) != NULL,
        __FUNCTION__, aFilename.c_str(), "Cannot open file"
    )) return false;

    // Write magic
    if (!DynOS_Bin_Compress_Check(
        fwrite(DYNOS_BIN_COMPRESS_MAGIC, sizeof(u8), sizeof(DYNOS_BIN_COMPRESS_MAGIC), sFile) == sizeof(DYNOS_BIN_COMPRESS_MAGIC),
        __FUNCTION__, aFilename.c_str(), "Cannot write magic"
    )) return false;

    // Write uncompressed file size
    u8 _LengthUncompressedBytes[sizeof(u64)];
    DynOS_Bin_Compress_WriteU64LE(_LengthUncompressedBytes, sLengthUncompressed);
    if (!DynOS_Bin_Compress_Check(
        fwrite(_LengthUncompressedBytes, sizeof(u8), sizeof(_LengthUncompressedBytes), sFile) == sizeof(_LengthUncompressedBytes),
        __FUNCTION__, aFilename.c_str(), "Cannot write uncompressed file size"
    )) return false;

    // Write compressed data
    if (!DynOS_Bin_Compress_Check(
        fwrite(sBufferCompressed, sizeof(u8), sLengthCompressed, sFile) == sLengthCompressed,
        __FUNCTION__, aFilename.c_str(), "Cannot write compressed data"
    )) return false;

    // Done, free buffers and files
    DynOS_Bin_Compress_Free();
    Print(" Done.");
    return true;
}

static BinFile *DynOS_Bin_Decompress_ModFs(const SysPath &aFilename) {
    DynOS_Bin_Compress_Init();
    sys_trace("DynOS_Bin_Decompress_ModFs: begin %s", aFilename.c_str());

    // Read file data
    void *_Buffer = NULL;
    u32 _Size = 0;
    if (!mod_fs_read_file_from_uri(aFilename.c_str(), &_Buffer, &_Size)) {
        DynOS_Bin_Compress_Free();
        return NULL;
    }
    sBufferCompressed = (u8 *) _Buffer;
    sLengthCompressed = _Size;

    // Check file length
    u64 _LengthHeader = (u64) (sizeof(DYNOS_BIN_COMPRESS_MAGIC) + sizeof(u64));
    if (!DynOS_Bin_Compress_Check(
        sLengthCompressed >= _LengthHeader,
        __FUNCTION__, aFilename.c_str(), "Empty file"
    )) return NULL;

    // Compare with magic constant
    // If not equal, it's not a compressed file
    if (!DynOS_Bin_Compress_MagicEquals(sBufferCompressed)) {
        BinFile *_BinFile = BinFile::OpenB(sBufferCompressed, sLengthCompressed);
        sys_trace("DynOS_Bin_Decompress_ModFs: raw end %s size=%llu bin=%p",
                  aFilename.c_str(), (unsigned long long)sLengthCompressed, _BinFile);
        DynOS_Bin_Compress_Free();
        return _BinFile;
    }
    PrintNoNewLine("Decompressing file \"%s\"...", aFilename.c_str());

    // Read expected uncompressed file size
    sLengthUncompressed = DynOS_Bin_Compress_ReadU64LE(sBufferCompressed + sizeof(DYNOS_BIN_COMPRESS_MAGIC));
    sLengthCompressed -= _LengthHeader;
    u8 *_BufferCompressed = sBufferCompressed + _LengthHeader;

    // Allocate memory for uncompressed buffer
    if (!DynOS_Bin_Compress_Check(
        (sBufferUncompressed = (u8 *) calloc(sLengthUncompressed, sizeof(u8))) != NULL,
        __FUNCTION__, aFilename.c_str(), "Cannot allocate memory for decompression"
    )) return NULL;

    // Uncompress data
    uLongf _LengthUncompressed = (uLongf)sLengthUncompressed;
    int uncompressRc = uncompress(sBufferUncompressed, &_LengthUncompressed, _BufferCompressed, sLengthCompressed);
    sLengthUncompressed = _LengthUncompressed;
    if (!DynOS_Bin_Compress_Check(
        uncompressRc == Z_OK,
        __FUNCTION__, aFilename.c_str(), "Cannot uncompress data"
    )) {
        PrintError("ERROR: uncompress rc: %d, length uncompressed: %lu, length compressed: %lu, length header: %lu", uncompressRc, sLengthUncompressed, sLengthCompressed, _LengthHeader);
        return NULL;
    }
    Print("uncompress rc: %d, length uncompressed: %lu, length compressed: %lu, length header: %lu", uncompressRc, sLengthUncompressed, sLengthCompressed, _LengthHeader);

    // Return uncompressed data as a BinFile
    BinFile *_BinFile = BinFile::OpenB(sBufferUncompressed, sLengthUncompressed);
    sys_trace("DynOS_Bin_Decompress_ModFs: end %s compressed=%llu uncompressed=%llu bin=%p",
              aFilename.c_str(), (unsigned long long)sLengthCompressed,
              (unsigned long long)sLengthUncompressed, _BinFile);
    DynOS_Bin_Compress_Free();
    Print(" Done.");
    return _BinFile;
}

BinFile *DynOS_Bin_Decompress(const SysPath &aFilename) {
    DynOS_Bin_Compress_Init();
    sys_trace("DynOS_Bin_Decompress: begin %s", aFilename.c_str());

    // Check modfs
    if (is_mod_fs_file(aFilename.c_str())) {
        return DynOS_Bin_Decompress_ModFs(aFilename);
    }

    // Open input file
    if (!DynOS_Bin_Compress_Check(
        (sFile = f_open_r(aFilename.c_str())) != NULL,
        __FUNCTION__, aFilename.c_str(), "Cannot open file"
    )) return NULL;

    // Read magic
    u8 _Magic[sizeof(DYNOS_BIN_COMPRESS_MAGIC)] = { 0 };
    if (!DynOS_Bin_Compress_Check(
        f_read(_Magic, sizeof(u8), sizeof(_Magic), sFile) == sizeof(_Magic),
        __FUNCTION__, aFilename.c_str(), "Cannot read magic"
    )) return NULL;

    // Compare with magic constant
    // If not equal, it's not a compressed file
    if (!DynOS_Bin_Compress_MagicEquals(_Magic)) {
        DynOS_Bin_Compress_Free();
        BinFile *_RawFile = BinFile::OpenR(aFilename.c_str());
        sys_trace("DynOS_Bin_Decompress: raw end %s bin=%p", aFilename.c_str(), _RawFile);
        return _RawFile;
    }
    PrintNoNewLine("Decompressing file \"%s\"...", aFilename.c_str());

    // Read expected uncompressed file size
    u8 _LengthUncompressedBytes[sizeof(u64)];
    if (!DynOS_Bin_Compress_Check(
        f_read(_LengthUncompressedBytes, sizeof(u8), sizeof(_LengthUncompressedBytes), sFile) == sizeof(_LengthUncompressedBytes),
        __FUNCTION__, aFilename.c_str(), "Cannot read uncompressed file size"
    )) return NULL;
    sLengthUncompressed = DynOS_Bin_Compress_ReadU64LE(_LengthUncompressedBytes);

    // Retrieve file length
    if (!DynOS_Bin_Compress_Check(
        f_seek(sFile, 0, SEEK_END) == 0,
        __FUNCTION__, aFilename.c_str(), "Cannot retrieve file length"
    )) return NULL;

    // Check file length
    u64 _LengthHeader = (u64) (sizeof(DYNOS_BIN_COMPRESS_MAGIC) + sizeof(u64));
    if (!DynOS_Bin_Compress_Check(
        (sLengthCompressed = (u64) f_tell(sFile)) >= _LengthHeader,
        __FUNCTION__, aFilename.c_str(), "Empty file"
    )) return NULL;
    sLengthCompressed -= _LengthHeader;

    // Allocate memory for compressed buffer
    if (!DynOS_Bin_Compress_Check(
        (sBufferCompressed = (u8 *) calloc(sLengthCompressed, sizeof(u8))) != NULL,
        __FUNCTION__, aFilename.c_str(), "Cannot allocate memory for decompression"
    )) return NULL; else f_seek(sFile, _LengthHeader, SEEK_SET);

    // Read input data
    if (!DynOS_Bin_Compress_Check(
        f_read(sBufferCompressed, sizeof(u8), sLengthCompressed, sFile) == sLengthCompressed,
        __FUNCTION__, aFilename.c_str(), "Cannot read compressed data"
    )) return NULL; else DynOS_Bin_Compress_Close();

    // Allocate memory for uncompressed buffer
    if (!DynOS_Bin_Compress_Check(
        (sBufferUncompressed = (u8 *) calloc(sLengthUncompressed, sizeof(u8))) != NULL,
        __FUNCTION__, aFilename.c_str(), "Cannot allocate memory for decompression"
    )) return NULL;

    // Uncompress data
    uLongf _LengthUncompressed = (uLongf)sLengthUncompressed;
    int uncompressRc = uncompress(sBufferUncompressed, &_LengthUncompressed, sBufferCompressed, sLengthCompressed);
    sLengthUncompressed = _LengthUncompressed;
    if (!DynOS_Bin_Compress_Check(
        uncompressRc == Z_OK,
        __FUNCTION__, aFilename.c_str(), "Cannot uncompress data"
    )) {
        PrintError("ERROR: uncompress rc: %d, length uncompressed: %lu, length compressed: %lu, length header: %lu", uncompressRc, sLengthUncompressed, sLengthCompressed, _LengthHeader);
        return NULL;
    }
    Print("uncompress rc: %d, length uncompressed: %lu, length compressed: %lu, length header: %lu", uncompressRc, sLengthUncompressed, sLengthCompressed, _LengthHeader);

    // Return uncompressed data as a BinFile
    BinFile *_BinFile = BinFile::OpenB(sBufferUncompressed, sLengthUncompressed);
    sys_trace("DynOS_Bin_Decompress: end %s compressed=%llu uncompressed=%llu bin=%p",
              aFilename.c_str(), (unsigned long long)sLengthCompressed,
              (unsigned long long)sLengthUncompressed, _BinFile);
    DynOS_Bin_Compress_Free();
    Print(" Done.");
    return _BinFile;
}
