#ifndef GD5F1GQ5XE_H
#define GD5F1GQ5XE_H

#include <stddef.h>
#include <stdint.h>

// Flash Sizes 
#define GD5F_BLOCK_COUNT      1024
#define GD5F_PAGES_PER_BLOCK  64
#define GD5F_PAGE_SIZE        2048
#define GD5F_BLOCK_SIZE       GD5F_PAGES_PER_BLOCK * GD5F_PAGE_SIZE

bool gd5f1gq5xe_read(uint32_t block, uint32_t offset, void *buffer, uint32_t size);
bool gd5f1gq5xe_write(uint32_t block, uint32_t offset, void *buffer, uint32_t size);
bool gd5f1gq5xe_erase(uint32_t block);
bool gd5f1gq5xe_unlock();

#endif
