#include "../../external/littlefs/lfs.h"
#include "pico_lfs_hal.h"
#include "pico_flash_fs.h"
#include "../print.h"

static struct lfs_config cfg = {
    .read = pico_read_flash_block,
    .prog  = pico_prog_flash_block,
    .erase = pico_erase_flash_block,
    .sync  = pico_sync_flash_block,
    
    // block device configuration

    // device is memory mapped for reading, so reading can be per byte
    .read_size = 1,
    
    .prog_size = PICO_PROG_PAGE_SIZE,
    .block_size = PICO_ERASE_PAGE_SIZE,

    // the number of blocks we use for a flash fs.
    .block_count = FLASHFS_BLOCK_COUNT,

    // cache needs to be a multiple of the programming page size.
    .cache_size = PICO_PROG_PAGE_SIZE * 1,

    .lookahead_size = 16,
    .block_cycles = 500,
};

static void lsDir(const char* path) {
    lfs_t lfs;
    lfs_dir_t dir;
    memset(&lfs, 0, sizeof(lfs));
    memset(&dir, 0, sizeof(dir));
    
    // mount the filesystem
    int err = lfs_mount(&lfs, &cfg);
    if (err < 0) {
        FPRINTMSG(stderr, "Could not mount filesystem (%d).\n", err);
        exit(74);
    }

    err = lfs_dir_open(&lfs, &dir, path);
    if (err < 0) {
        FPRINTMSG(stderr, "Could not open dir \"%s\" (%d).\n", path, err);
        exit(74);
    }
    struct lfs_info info;
    memset(&info, 0, sizeof(info));

    while ((err = lfs_dir_read(&lfs, &dir, &info)) > 0) {

        if (info.type == LFS_TYPE_DIR) {
            printf("'%s' (dir)\n", info.name);
        }
        else if (info.type == LFS_TYPE_REG) {
            printf("'%s' (%d)\n", info.name, info.size);
        }
        memset(&info, 0, sizeof(info));
    }
    if (err < 0) {
        FPRINTMSG(stderr, "Could not read dir \"%s\" (%d).\n", path, err);
        exit(74);
    }

    lfs_dir_close(&lfs, &dir);
    lfs_unmount(&lfs);

}

char* readFile(const char* path) {

    lfs_t lfs;
    lfs_file_t file;
    memset(&lfs, 0, sizeof(lfs));
    memset(&file, 0, sizeof(file));
    
    // mount the filesystem
    int err = lfs_mount(&lfs, &cfg);
    if (err < 0) {
        FPRINTMSG(stderr, "Could not mount filesystem (%d).\n", err);
        return NULL;
    }
    struct lfs_info info;
    memset(&info, 0, sizeof(info));

    err = lfs_stat(&lfs, path, &info);
    if (err < 0) {
        return NULL;
    }
    int fileSize = info.size;

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        FPRINTMSG(stderr, "Not enough memory to read \"%s\".\n", path);
        return NULL;
    }

    err = lfs_file_open(&lfs, &file, path, LFS_O_RDONLY);
    if (err < 0) {
        FPRINTMSG(stderr, "Could not open file \"%s\".\n", path);
        return NULL;
    }

    int bytesRead = lfs_file_read(&lfs, &file, buffer, fileSize);
    if (bytesRead < fileSize) {
        FPRINTMSG(stderr, "could not read file \"%s\".\n", path);
        return NULL;
    }
    buffer[bytesRead] = '\0';
    
    lfs_file_close(&lfs, &file);    
    lfs_unmount(&lfs);

    return buffer;
}

size_t fileSize(const char* path) {
    lfs_t lfs;
    memset(&lfs, 0, sizeof(lfs));
    
    // mount the filesystem
    int err = lfs_mount(&lfs, &cfg);
    if (err < 0) {
        FPRINTMSG(stderr, "Could not mount filesystem (%d).\n", err);
        return 0;
    }
    struct lfs_info info;
    memset(&info, 0, sizeof(info));

    err = lfs_stat(&lfs, path, &info);
    if (err < 0) {
        return 0;
    }
    int fileSize = info.size;

    lfs_unmount(&lfs);

    return fileSize;
}

void readFileIntoBuffer(const char* path, uint8_t* buffer, size_t bufferSize) {
    lfs_t lfs;
    lfs_file_t file;
    memset(&lfs, 0, sizeof(lfs));
    memset(&file, 0, sizeof(file));
    
    // mount the filesystem
    int err = lfs_mount(&lfs, &cfg);
    if (err < 0) {
        FPRINTMSG(stderr, "Could not mount filesystem (%d).\n", err);
        return;
    }
    struct lfs_info info;
    memset(&info, 0, sizeof(info));

    err = lfs_stat(&lfs, path, &info);
    if (err < 0) {
        return;
    }
    int fileSize = info.size;

    if (fileSize > bufferSize) {
        FPRINTMSG(stderr, "Buffer too small to read file \"%s\".\n", path);
        return;
    }

    err = lfs_file_open(&lfs, &file, path, LFS_O_RDONLY);
    if (err < 0) {
        FPRINTMSG(stderr, "Could not open file \"%s\".\n", path);
        return;
    }

    int bytesRead = lfs_file_read(&lfs, &file, buffer, fileSize);
    if (bytesRead < fileSize) {
        FPRINTMSG(stderr, "could not read file \"%s\".\n", path);
        return;
    }
    
    lfs_file_close(&lfs, &file);    
    lfs_unmount(&lfs);
}

bool fileExists(const char* path) {
    lfs_t lfs;
    memset(&lfs, 0, sizeof(lfs));
    
    // mount the filesystem
    int err = lfs_mount(&lfs, &cfg);
    if (err < 0) {
        FPRINTMSG(stderr, "Could not mount filesystem (%d).\n", err);
        return false;
    }
    struct lfs_info info;
    memset(&info, 0, sizeof(info));

    err = lfs_stat(&lfs, path, &info);

    lfs_unmount(&lfs);

    if (err < 0) {
        return false;
    }

    return info.type == LFS_TYPE_REG;
}