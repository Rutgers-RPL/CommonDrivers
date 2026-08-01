#include "gd5f1gq5xe.h"

#include "defs.h"

// Flash Commands
#define GD5F_SET_FEATURE      0x1F
#define GD5F_WRITE_ENABLE     0x06
#define GD5F_WRITE_DISABLE    0x04
#define GD5F_READ_TO_CACHE    0x13
#define GD5F_READ_FROM_CACHE  0x03
#define GD5F_PROGRAM_LOAD     0x02
#define GD5F_PROGRAM_EXECUTE  0x10
#define GD5F_ERASE            0xD8
#define GD5F_READ_ID          0x9F

// Flash Sizes 
#define GD5F_BLOCK_COUNT      1024
#define GD5F_PAGES_PER_BLOCK  64
#define GD5F_PAGE_SIZE        2048
#define GD5F_BLOCK_SIZE       GD5F_PAGES_PER_BLOCK * GD5F_PAGE_SIZE

// This is a NAND flash which uses 3-byte addressing, this means we have
//
//  | block address | page address |
//  | bytes <15-6>  | bytes <5-0>  |
//
// NAND flashes employ a two step read and progamming process. For reading we
// first fetch the page using the 3-byte addressing to the cache, and then use a
// second command to read from the cache, here we can index the bytes using a 
// column address 12 bytes.
//
// For writing, we do the inverse. We populate the cache with bytes using the index.
// After it is filled, we can then write the page to the flash using the 3-byte addressing
//
// Note that this flash has a page size of 2048, so we actually only need 11 bytes for
// the column address.

static int write_enable(struct serial_api *api)
{
	uint8_t cmd[] = { GD5F_WRITE_ENABLE };
	bool res = api->write(api->handle, &STATIC_EXEC(cmd));
	if (res != 0 && api->handle->protocol == SPI) {
		struct handle_spi *spi = handle_as_spi(api->handle);
		HAL_GPIO_WritePin(spi->port, spi->pin, GPIO_PIN_SET);
	}
	return res;
}

static bool check_id(struct serial_api *api)
{
	uint8_t cmd[] = { GD5F_READ_ID, 0x00 };
	uint8_t data[] = { 0, 0 };
	api->read(api->handle, &STATIC_CMD(cmd, data));
	return data[0] == 0xC8 && data[1] == 0x31;
}

/// Read a page from the flash into the `buffer`.
///
///  `block` must be less than the block-count
///  `offset` must be less than than the block-size 
///  `size` can be larger than the page-size, but this function will only
///   read up to the page-size boundary
///
/// Returns the number of bytes read.
static uint32_t read_page(struct serial_api *api, const uint32_t block,
			  const uint32_t offset, void *buffer, uint32_t size)
{
	assert(block < GD5F_BLOCK_COUNT);
	assert(offset < GD5F_BLOCK_SIZE);

	// This combines the block and offset to create our 3-byte address
	// Notice that `block * GD5F_PAGES_PER_BLOCK` is equivalent to
	// `block << 6`, which is why this code here works.
	uint32_t addr = block * GD5F_PAGES_PER_BLOCK + (offset / GD5F_PAGE_SIZE);
	// We can then use the column addreess to read offsets into the page
	uint16_t col = offset % GD5F_PAGE_SIZE;

	uint8_t cmd1[] = {
		GD5F_READ_TO_CACHE,
		(addr & 0xFF0000) >> 16,
		(addr & 0x00FF00) >> 8,
		(addr & 0x0000FF)
	};
	api->write(api->handle, &STATIC_EXEC(cmd1));
	HAL_Delay(2);  // Required delay for cache read

	uint8_t cmd2[] = {
		GD5F_READ_FROM_CACHE,
		(col & 0x0F00) >> 8, // first 4 bytes are not needed,
		(col & 0x00FF),      // remember that we only need 12 bytes
		0x00                 // we need a dummy byte (from datasheet)
	};
	uint32_t read_size = size <= GD5F_PAGE_SIZE - col ? size : GD5F_PAGE_SIZE - col;
	api->read(api->handle, &(struct op_params) {
		.cmd = cmd2, .cmd_size = sizeof(cmd2),
		.buffer = buffer, .buffer_size = read_size
	});
	return read_size;
}

/// Write a page from the `buffer` into the flash
///
///  `block` must be less than the block-count
///  `offset` must be less than than the block-size 
///  `size` can be larger than the page-size, but this function will only
///   write up to the page-size boundary
///
/// Returns the number of bytes written.
static uint32_t write_page(struct serial_api *api, const uint32_t block,
			   const uint32_t offset, const void *buffer, const uint32_t size)
{
	assert(block < GD5F_BLOCK_COUNT);
	assert(offset < GD5F_BLOCK_SIZE);

	uint32_t addr = block * GD5F_PAGES_PER_BLOCK + (offset / GD5F_PAGE_SIZE);
	uint16_t col = offset % GD5F_PAGE_SIZE;

	uint8_t cmd1[] = {
		GD5F_PROGRAM_LOAD,
		(col & 0x0F00) >> 8,  // similar to before, the first 4 bytes
		(col & 0x00FF)        // are not needed, hence the 0x0F00
	};
	uint32_t write_size = size <= GD5F_PAGE_SIZE - col ? size : GD5F_PAGE_SIZE - col;
	api->write(api->handle, &(struct op_params) {
		.cmd = cmd1, .cmd_size = sizeof(cmd1),
		.buffer = buffer, .buffer_size = write_size
	});

	if (write_enable(api) != 0) return 0;
	uint8_t cmd2[] = {
		GD5F_PROGRAM_EXECUTE,
		(addr & 0xFF0000) >> 16,
		(addr & 0x00FF00) >> 8,
		(addr & 0x0000FF)
	};
	api->write(api->handle, &STATIC_EXEC(cmd2));
	HAL_Delay(1);
	return write_size;
}

static bool read(struct serial_api *api, uint32_t block, uint32_t offset, void *buffer, uint32_t size)
{
	uint8_t *buf = (uint8_t *) buffer;
	while (size > 0) {
		uint32_t s = read_page(api, block, offset, buf, size);
		if (s == 0) return false;
		size -= s;
		offset += s;
		buf += s;
	}
	return true;
}

static bool write(struct serial_api *api, uint32_t block, uint32_t offset, void *buffer, uint32_t size)
{
	uint8_t *buf = (uint8_t *) buffer;
	while (size > 0) {
		uint32_t s = write_page(api, block, offset, buf, size);
		if (s == 0) return false;
		size -= s;
		offset += s;
		buf += s;
	}
	return true;
}

static bool erase(struct serial_api *api, uint32_t block)
{
	if (write_enable(api) != 0) return false;
	// Erase acts on blocks and not pages, so we should only have the block
	// section of the address set and not the page section.
	uint32_t addr = block * GD5F_PAGES_PER_BLOCK;
	uint8_t cmd[] = {
		GD5F_ERASE,
		(addr & 0xFF0000) >> 16,
		(addr & 0x00FF00) >> 8,
		(addr & 0x0000FF)
	};
	api->write(api->handle, &STATIC_EXEC(cmd));

	HAL_Delay(12);
	return true;
}

static bool unlock(struct serial_api *api)
{
	// Needed for some reason, I don't know why
	HAL_Delay(5000);
	if (!check_id(api)) return false;
	if (write_enable(api) != 0) return false;

	uint8_t cmd[] = { GD5F_SET_FEATURE, 0xA0, 0x00 };
	api->write(api->handle, &STATIC_EXEC(cmd));

	HAL_Delay(5000);
	return true;
}

static int lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t offset,
                    void *data, lfs_size_t size)
{
	struct serial_api *api = (struct serial_api*) c->context;
	if (!read(api, block, offset, data, size)) return LFS_ERR_IO;
	return LFS_ERR_OK;
}

static int lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t offset,
                    const void *data, lfs_size_t size)
{
	struct serial_api *api = (struct serial_api*) c->context;
	if (!write(api, block, offset, data, size)) return LFS_ERR_IO;
	return LFS_ERR_OK;
}

static int lfs_erase(const struct lfs_config *c, lfs_block_t block)
{
	struct serial_api *api = (struct serial_api*) c->context;
	if (!erase(api, block)) return LFS_ERR_IO;
	return LFS_ERR_OK;
}

static int lfs_sync(const struct lfs_config *c)
{
	return LFS_ERR_OK;
}

bool gd5f1gq5xe_init(struct flash *flash, struct handle *handle)
{
	serial_api_spi(&flash->api, handle);
	flash->config = (struct lfs_config ) {
		.context = &flash->api,
		.read  = lfs_read,
		.prog  = lfs_prog,
		.erase = lfs_erase,
		.sync  = lfs_sync,

		.read_size      = GD5F_PAGE_SIZE,
		.prog_size      = GD5F_PAGE_SIZE,
		.block_size     = GD5F_BLOCK_SIZE,
		.block_count    = GD5F_BLOCK_COUNT,
		.cache_size     = GD5F_PAGE_SIZE,
		.lookahead_size = 128,
		.block_cycles   = 512,
	};
	if (!unlock(&flash->api)) return false;
	return true;
}
