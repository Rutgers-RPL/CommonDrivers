#ifndef FLASH_H
#define FLASH_H

#include "lfs.h"
#include "defs.h"

#include <stdint.h>

struct flash_ctx {
	GPIO_TypeDef *port;
	uint16_t pin;
	SPI_HandleTypeDef *spi;
};

struct flash {
	char *name;
	struct lfs_config config;
	lfs_t lfs;
};

int flash_mount(struct flash *flash);
int flash_unmount(struct flash *flash);
uint32_t flash_boot_count(struct flash *flash, bool update);
uint32_t flash_open(struct flash *flash, lfs_file_t *file, const char *filename);
bool flash_append(struct flash *flash, lfs_file_t *file, const uint8_t *bytes, size_t size);
int flash_close(struct flash *flash, lfs_file_t *file);

#endif
