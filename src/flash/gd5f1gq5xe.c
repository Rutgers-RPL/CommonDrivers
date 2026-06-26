#include "gd5f1gq5xe.h"

#include "stm32xxxx_hal.h"

extern SPI_HandleTypeDef hspi4;

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

// TODO: this should be abstracted away into littlefs's context object
extern SPI_HandleTypeDef hspi4;

static void chip_select()
{
	HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);
}

static void chip_deselect()
{
	HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
}

static bool spi_transmit(void *buffer, const size_t size)
{
	return HAL_SPI_Transmit(&hspi4, (uint8_t*) buffer, size, HAL_MAX_DELAY);
}

static bool spi_receive(void *buffer, const size_t size)
{
	return HAL_SPI_Receive(&hspi4, (uint8_t*) buffer, size, HAL_MAX_DELAY);
}

static int write_enable()
{
	uint8_t tx = GD5F_WRITE_ENABLE;
	chip_select();
	if (spi_transmit(&tx, sizeof(tx)) != 0) {
		chip_deselect();
		return 1;
	}
	chip_deselect();
	return 0;
}

static bool check_id()
{
	chip_select();
	uint8_t cmd[] = { GD5F_READ_ID, 0x00 };
	uint8_t data[] = { 0, 0 };
	if (spi_transmit(cmd, sizeof(cmd)) != 0) {
		chip_deselect();
	}
	spi_receive(data, sizeof(data));
	chip_deselect();
	// Sometimes the check is not consistent
	// Investigate for now
	return data[0] == 0xC8 && data[1] == 0x31;
	/* assert(data[0] == 0xC8); */
	/* assert(data[1] == 0x31); */
}

/// Read a page from the flash into the `buffer`.
///
///  `block` must be less than the block-count
///  `offset` must be less than than the block-size 
///  `size` can be larger than the page-size, but this function will only
///   read up to the page-size boundary
///
/// Returns the number of bytes read.
static uint32_t gd5f1gq5xe_read_page(const uint32_t block, const uint32_t offset,
				     void *buffer, uint32_t size)
{
	assert(block < GD5F_BLOCK_COUNT);
	assert(offset < GD5F_BLOCK_SIZE);

	// This combines the block and offset to create our 3-byte address
	// Notice that `block * GD5F_PAGES_PER_BLOCK` is equivalent to
	// `block << 6`, which is why this code here works.
	uint32_t addr = block * GD5F_PAGES_PER_BLOCK + (offset / GD5F_PAGE_SIZE);
	// We can then use the column addreess to read offsets into the page
	uint16_t col = offset % GD5F_PAGE_SIZE;

	uint8_t tx1[] = {
		GD5F_READ_TO_CACHE,
		(addr & 0xFF0000) >> 16,
		(addr & 0x00FF00) >> 8,
		(addr & 0x0000FF)
	};
	chip_select();
	if (spi_transmit(tx1, sizeof(tx1)) != 0) {
		chip_deselect();
		return 0;
	}
	chip_deselect();
	HAL_Delay(2);  // Required delay for cache read

	uint8_t tx2[] = {
		GD5F_READ_FROM_CACHE,
		(col & 0x0F00) >> 8, // first 4 bytes are not needed,
		(col & 0x00FF),      // remember that we only need 12 bytes
		0x00                 // we need a dummy byte (from datasheet)
	};
	chip_select();
	if (spi_transmit(tx2, sizeof(tx2)) != 0) {
		chip_deselect();
		return 0;
	};
	uint32_t read_size = size <= GD5F_PAGE_SIZE - col ? size : GD5F_PAGE_SIZE - col;
	if (spi_receive(buffer, read_size) != 0) {
		chip_deselect();
		return 0;
	}
	chip_deselect();
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
static uint32_t gd5f1gq5xe_write_page(const uint32_t block, const uint32_t offset,
				      const void *buffer, const uint32_t size)
{
	assert(block < GD5F_BLOCK_COUNT);
	assert(offset < GD5F_BLOCK_SIZE);

	uint32_t addr = block * GD5F_PAGES_PER_BLOCK + (offset / GD5F_PAGE_SIZE);
	uint16_t col = offset % GD5F_PAGE_SIZE;

	uint8_t tx1[] = {
		GD5F_PROGRAM_LOAD,
		(col & 0x0F00) >> 8,  // similar to before, the first 4 bytes
		(col & 0x00FF)        // are not needed, hence the 0x0F00
	};
	chip_select();
	if (spi_transmit(tx1, sizeof(tx1)) != 0) {
		chip_deselect();
		return 0;
	}
	uint32_t write_size = size <= GD5F_PAGE_SIZE - col ? size : GD5F_PAGE_SIZE - col;
	if (spi_transmit(buffer, write_size) != 0) {
		chip_deselect();
		return 0;
	};
	chip_deselect();

	if (write_enable() != 0) {
		chip_deselect();
		return 0;
	}

	uint8_t tx2[] = {
		GD5F_PROGRAM_EXECUTE,
		(addr & 0xFF0000) >> 16,
		(addr & 0x00FF00) >> 8,
		(addr & 0x0000FF)
	};
	chip_select();
	if (spi_transmit(tx2, sizeof(tx2)) != 0) {
		chip_deselect();
		return 0;
	}
	chip_deselect();
	
	HAL_Delay(1);
	return write_size;
}

bool gd5f1gq5xe_read(uint32_t block, uint32_t offset, void *buffer, uint32_t size)
{
	uint8_t *buf = (uint8_t *) buffer;
	while (size > 0) {
		uint32_t s = gd5f1gq5xe_read_page(block, offset, buf, size);
		if (s == 0) return false;
		size -= s;
		offset += s;
		buf += s;
	}
	return true;
}

bool gd5f1gq5xe_write(uint32_t block, uint32_t offset, void *buffer, uint32_t size)
{
	uint8_t *buf = (uint8_t *) buffer;
	while (size > 0) {
		uint32_t s = gd5f1gq5xe_write_page(block, offset, buf, size);
		if (s == 0) return false;
		size -= s;
		offset += s;
		buf += s;
	}
	return true;
}

bool gd5f1gq5xe_erase(uint32_t block)
{
	// Erase acts on blocks and not pages, so we should only have the block
	// section of the address set and not the page section.
	uint32_t addr = block * GD5F_PAGES_PER_BLOCK;

	if (write_enable() != 0) {
		chip_deselect();
		return false;
	}

	uint8_t tx[] = {
		GD5F_ERASE,
		(addr & 0xFF0000) >> 16,
		(addr & 0x00FF00) >> 8,
		(addr & 0x0000FF)
	};
	chip_select();
	if (spi_transmit(tx, sizeof(tx)) != 0) {
		chip_deselect();
		return false;
	}
	chip_deselect();

	HAL_Delay(12);
	return true;
}

bool gd5f1gq5xe_unlock()
{
	// Needed for some reason, I don't know why
	HAL_Delay(5000);
	if (!check_id()) return false;
	if (write_enable() != 0) {
		chip_deselect();
		return false;
	}

	uint8_t tx[] = {
		GD5F_SET_FEATURE,
		0xA0,
		0x00,
	};
	chip_select();
	if (spi_transmit(tx, sizeof(tx)) != 0) {
		chip_deselect();
		return false;
	}
	chip_deselect();

	HAL_Delay(5000);
	return true;
}
