#ifndef FLASH_H
#define FLASH_H

#include "lfs.h"
#include "defs.h"

#include <stdint.h>

struct flash {
	struct lfs_config config;
	struct serial_api api;
	lfs_t lfs;
};

uint32_t flash_mount(struct flash *flash);
int flash_unmount(struct flash *flash);
uint32_t flash_boot_count(struct flash *flash, bool update);
uint32_t flash_open(struct flash *flash, lfs_file_t *file, const char *filename);
bool flash_append(struct flash *flash, lfs_file_t *file, const uint8_t *bytes, size_t size);
int flash_close(struct flash *flash, lfs_file_t *file);

#endif
