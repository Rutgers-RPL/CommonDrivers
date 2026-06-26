#include "flash.h"

#include "gd5f1gq5xe.h"
#include "STRUCTS.h"
#include "lfs.h"

#include <stdint.h>

// See: https://github.com/littlefs-project/littlefs/issues/564#issuecomment-2363032827
// This gives how many bytes are needed before we need to do a fsync. Since we
// want to sync at the end of a block, this just gives offset from the end.
//
// We don't want to sync too often, especially for NAND flashes which have
// larger block sizes because littlefs does a whole scan and write of the 
// partial block, which is very time-consuming.
static inline uint32_t offset(uint32_t filesize, uint32_t blocksize)
{
	// Edge case for the first block 
	if (filesize < blocksize) {
		return blocksize - filesize;
	}
	
	const uint32_t w = sizeof(uint32_t);
	uint32_t pop = __builtin_popcount((filesize / (blocksize - (2*w))) - 1);
	uint32_t n = (filesize - (w * (pop + 2))) / (blocksize - 2*w);
	uint32_t offset = filesize - (blocksize - 2*w)*n - w*__builtin_popcount(n);
	return blocksize - offset;
}

void flash_init(struct flash_dev *dev, enum flash_name name)
{
	dev->name = name;
}

lfs_ssize_t flash_mount(struct flash_dev *flash, const struct lfs_config *config)
{
	if (!gd5f1gq5xe_unlock()) return -1;
	int err = lfs_mount(&(flash->lfs), config);
	if (err) {
		lfs_format(&(flash->lfs), config);
		int res = lfs_mount(&(flash->lfs), config);
		if (res < 0) return -1;
	}
	flash_boot_count(flash, true);
	return lfs_fs_size(&(flash->lfs));
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

uint32_t flash_open(struct flash_dev *flash, lfs_file_t *file, const char *filename)
{
	lfs_file_open(&(flash->lfs), file, filename, LFS_O_RDWR | LFS_O_CREAT | LFS_O_APPEND);
	return lfs_file_size(&(flash->lfs), file);
}

bool flash_append(struct flash_dev *flash, lfs_file_t *file, const uint8_t *bytes, uint32_t size)
{
	uint8_t *buf = (uint8_t *) bytes;
	uint32_t filesize = lfs_file_size(&(flash->lfs), file);
	uint32_t off = offset(filesize, flash->lfs.cfg->block_size);

	while (size > 0) {
		uint32_t write_size = size < off ? size : off;
		if (off == 0) {
			// Time for a new block, sync and just write whatever 
			lfs_file_sync(&(flash->lfs), file);
			// Due to constraints, the size is guaranteed to be less
			// than a block size, so we can just safely do this
			write_size = size;
		}

		uint32_t res = lfs_file_write(&(flash->lfs), file, buf, write_size);
		if (res != write_size) return false;

		size -= res;
		filesize += res;
		buf += res;
		off = offset(filesize, flash->lfs.cfg->block_size);
	}
	return true;
}

int flash_close(struct flash_dev *flash, lfs_file_t *file)
{
	return lfs_file_close(&(flash->lfs), file);
}
