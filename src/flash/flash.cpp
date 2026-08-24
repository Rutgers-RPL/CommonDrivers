#include "flash.h"

#include "log.h"
#include "lfs.h"

#include <stdint.h>

namespace {
// See:
// https://github.com/littlefs-project/littlefs/issues/564#issuecomment-2363032827
// This gives how many bytes are needed before we need to do a fsync. Since we
// want to sync at the end of a block, this just gives offset from the end.
//
// We don't want to sync too often, especially for NAND flashes which have
// larger block sizes because littlefs does a whole scan and write of the
// partial block, which is very time-consuming.
inline uint32_t offset(uint32_t filesize, uint32_t blocksize) {
    // Edge case for the first block
    if (filesize < blocksize) {
        return blocksize - filesize;
    }

    const uint32_t w = sizeof(uint32_t);
    uint32_t pop = __builtin_popcount((filesize / (blocksize - (2 * w))) - 1);
    uint32_t n = (filesize - (w * (pop + 2))) / (blocksize - 2 * w);
    uint32_t offset =
        filesize - (blocksize - 2 * w) * n - w * __builtin_popcount(n);
    return blocksize - offset;
}
} // namespace

namespace Common {
uint32_t Flash::mount() {
    int err = lfs_mount(&lfs, &config);
    if (err) {
        lfs_format(&lfs, &config);
        int res = lfs_mount(&lfs, &config);
        if (res < 0)
            return -1;
    }
    bootcount(true);
    return lfs_fs_size(&lfs);
}

uint32_t Flash::unmount() { return lfs_unmount(&lfs); }

uint32_t Flash::bootcount(bool update) {
    lfs_file_t file;
    uint32_t current_count = 0;

    lfs_file_open(&lfs, &file, "boot_count", LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_read(&lfs, &file, &current_count, sizeof(current_count));

    if (update) {
        ++current_count;
        lfs_file_rewind(&lfs, &file);
        lfs_file_write(&lfs, &file, &current_count, sizeof(current_count));
    }

    lfs_file_close(&lfs, &file);
    return current_count;
}

uint32_t Flash::open(lfs_file_t* file, const char* filename) {
    lfs_file_open(&lfs, file, filename,
                  LFS_O_RDWR | LFS_O_CREAT | LFS_O_APPEND);
    return lfs_file_size(&lfs, file);
}

uint32_t Flash::close(lfs_file_t* file) { return lfs_file_close(&lfs, file); }

bool Flash::append(lfs_file_t* file, const uint8_t* bytes, size_t size) {
    uint8_t* buf = (uint8_t*)bytes;
    uint32_t filesize = lfs_file_size(&lfs, file);
    uint32_t off = offset(filesize, lfs.cfg->block_size);

    while (size > 0) {
        uint32_t write_size = size < off ? size : off;
        if (off == 0) {
            // Time for a new block, sync and just write whatever
            lfs_file_sync(&lfs, file);
            // Due to constraints, the size is guaranteed to be less
            // than a block size, so we can just safely do this
            write_size = size;
            LOG("Flash synced", size);
        }

        uint32_t res = lfs_file_write(&lfs, file, buf, write_size);
        if (res != write_size)
            return false;

        Common::LOG("Flash wrote %u\r\n", res);

        size -= res;
        filesize += res;
        buf += res;
        off = offset(filesize, lfs.cfg->block_size);
    }
    return true;
}
} // namespace Common
