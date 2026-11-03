#include "flash.h"

#include "log.h"
#include "lfs.h"

#include <algorithm>
#include <cstdint>

namespace {
/// See:
/// https://github.com/littlefs-project/littlefs/issues/564#issuecomment-2363032827
/// This gives how many bytes are needed before we need to do a fsync. Since we
/// want to sync at the end of a block, this just gives offset from the end.
///
/// See also Ardupilot for the formula and logic for syncing:
/// https://github.com/ArduPilot/ardupilot/blob/13519aaabb8da31ccc54c7c66e7c68ee69c96f2d/libraries/AP_Filesystem/AP_Filesystem_FlashMemory_LittleFS.cpp#L432
///
/// We don't want to sync too often, especially for NAND flashes which have
/// larger block sizes because littlefs does a whole scan and write of the
/// partial block, which is very time-consuming. The reason for this is that
/// LFS must transfer the partial block data to a new block if we sync in the
/// middle of a block.
/// @return number of bytes left for a sync, or zero for any amount
inline uint32_t bytes_until_sync(uint32_t filesize, uint32_t blocksize) {
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

bool Flash::append(lfs_file_t* file, const uint8_t* bytes, uint32_t size) {
    uint8_t* buf = (uint8_t*)bytes;
    uint32_t filesize = lfs_file_size(&lfs, file);

    while (size > 0) {
        uint32_t bytes_left = bytes_until_sync(filesize, lfs.cfg->block_size);
        uint32_t write_size = bytes_left > 0 ? std::min(size, bytes_left) : size;
        
        lfs_ssize_t res = lfs_file_write(&lfs, file, buf, write_size);
        if (res < 0) {
            LOG("Error: flash write with error code %d", res);
            return false;
        }
        if (write_size == bytes_left) {
            lfs_file_sync(&lfs, file);
            LOG("Flash synced", size);
        }

        size -= res;
        filesize += res;
        buf += res;
    }
    return true;
}
} // namespace Common
