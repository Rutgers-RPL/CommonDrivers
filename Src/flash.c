#include "flash.h"

#include "w25n01kv.h"
#include "gd5f1gq5xe.h"
#include "lfs.h"

#include "STRUCTS.h"

#include <stdint.h>

// Wrappers for w25n01kv
static int w25n01kv_lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t offset,
		      void *location, lfs_size_t size)
{
	uint32_t page = block * W25N_PAGES_PER_BLOCK + (offset / W25N_PAGE_SIZE);
	uint16_t col = offset % W25N_PAGE_SIZE;
	return w25n01kv_read(page, col, location, size);
}

static int w25n01kv_lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t offset,
		      const void *data, lfs_size_t size)
{
	uint32_t page = block * W25N_PAGES_PER_BLOCK + (offset / W25N_PAGE_SIZE);
	uint16_t col = offset % W25N_PAGE_SIZE;
	return w25n01kv_write(page, col, data, size);
}

static int w25n01kv_lfs_erase(const struct lfs_config *c, lfs_block_t block) {
	uint32_t page = block * W25N_PAGES_PER_BLOCK;
	return w25n01kv_erase(page);
}
// End Wrappers for w25n01kv

// Wrappers for gd5f1gq5xe
static int gd5f1gq5xe_lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t offset,
		      void *location, lfs_size_t size)
{
	uint32_t page = block * GD5F_PAGES_PER_BLOCK + (offset / GD5F_PAGE_SIZE);
	uint16_t col = offset % GD5F_PAGE_SIZE;
	return gd5f1gq5xe_read(page, col, location, size);
}

static int gd5f1gq5xe_lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t offset,
		      const void *data, lfs_size_t size)
{
	uint32_t page = block * GD5F_PAGES_PER_BLOCK + (offset / GD5F_PAGE_SIZE);
	uint16_t col = offset % GD5F_PAGE_SIZE;
	return gd5f1gq5xe_write(page, col, data, size);
}

static int gd5f1gq5xe_lfs_erase(const struct lfs_config *c, lfs_block_t block) {
	uint32_t page = block * GD5F_PAGES_PER_BLOCK;
	return gd5f1gq5xe_erase(page);
}
// End wrappers for gd5f1gq5xe

void flash_init(struct flash_dev *dev, enum flash_name name)
{
	dev->name = name;
}

int flash_mount(struct flash_dev *flash, const struct lfs_config *config)
{
	gd5f1gq5xe_unlock();
	int err = lfs_mount(&(flash->lfs), config);
	if (err) {
		lfs_format(&(flash->lfs), config);
		lfs_mount(&(flash->lfs), config);
		return flash_boot_count(flash, true);
	}
	flash_boot_count(flash, true);
	return err;
}

int flash_unmount(struct flash_dev *flash)
{
	return lfs_unmount(&(flash->lfs));
}

uint32_t flash_boot_count(struct flash_dev *flash, bool update)
{
	lfs_file_t file;
	uint32_t boot_count = 0;

	lfs_file_open(&(flash->lfs), &file, "boot_count", LFS_O_RDWR | LFS_O_CREAT);
	lfs_file_read(&(flash->lfs), &file, &boot_count, sizeof(boot_count));

	if (update) {
		++boot_count;
		lfs_file_rewind(&(flash->lfs), &file);
		lfs_file_write(&(flash->lfs), &file, &boot_count, sizeof(boot_count));
	}

	lfs_file_close(&(flash->lfs), &file);
	return boot_count;
}

bool flash_open(struct flash_dev *flash, lfs_file_t *file, const char *filename)
{
	ganesha_II_packet packet = {0};
	lfs_file_open(&(flash->lfs), file, filename, LFS_O_RDWR | LFS_O_CREAT | LFS_O_APPEND);
	// Do a test read to make sure the file exists and is properly readable
	lfs_file_seek(&(flash->lfs), file, 0, LFS_SEEK_SET);
	lfs_ssize_t size = lfs_file_read(&(flash->lfs), file, &packet, sizeof(packet));
	return size == sizeof(packet);
}

bool flash_append(struct flash_dev *flash, lfs_file_t *file, const uint8_t *bytes, const size_t size)
{
	lfs_ssize_t s = lfs_file_write(&(flash->lfs), file, bytes, size);
	lfs_file_sync(&(flash->lfs), file);
	return s == size;
}

int flash_close(struct flash_dev *flash, lfs_file_t *file)
{
	return lfs_file_close(&(flash->lfs), file);
}
