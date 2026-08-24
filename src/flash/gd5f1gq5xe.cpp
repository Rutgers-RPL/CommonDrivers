#include "gd5f1gq5xe.h"

#include "hal.h"
#include "log.h"
#include "protocol.h"

// Flash Commands
constexpr uint8_t GD5F_SET_FEATURE = 0x1F;
constexpr uint8_t GD5F_WRITE_ENABLE = 0x06;
constexpr uint8_t GD5F_WRITE_DISABLE = 0x04;
constexpr uint8_t GD5F_READ_TO_CACHE = 0x13;
constexpr uint8_t GD5F_READ_FROM_CACHE = 0x03;
constexpr uint8_t GD5F_PROGRAM_LOAD = 0x02;
constexpr uint8_t GD5F_PROGRAM_EXECUTE = 0x10;
constexpr uint8_t GD5F_ERASE = 0xD8;
constexpr uint8_t GD5F_READ_ID = 0x9F;

// QSPI Flash commands
constexpr uint8_t GD5F_READ_FROM_CACHE_QUAD_IO = 0xEB;
constexpr uint8_t GD5F_PROGRAM_LOAD_QUAD = 0x32;

// Flash Sizes
constexpr uint32_t GD5F_BLOCK_COUNT = 1024;
constexpr uint32_t GD5F_PAGES_PER_BLOCK = 64;
constexpr uint32_t GD5F_PAGE_SIZE = 2048;
constexpr uint32_t GD5F_BLOCK_SIZE = GD5F_PAGES_PER_BLOCK * GD5F_PAGE_SIZE;

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
// For writing, we do the inverse. We populate the cache with bytes using the
// index. After it is filled, we can then write the page to the flash using the
// 3-byte addressing
//
// Note that this flash has a page size of 2048, so we actually only need 11
// bytes for the column address.
namespace {
using namespace Common;

/// Generally needed when you want to write to the flash using execute
int write_enable(Protocol* protocol) {
    uint8_t cmd[] = {GD5F_WRITE_ENABLE};
    return protocol->write(ConstSpan(cmd), {}, AddressSize::None);
}

/// Read a page from the flash into the `buffer`.
///
///  `block` must be less than the block-count
///  `offset` must be less than than the block-size
///  `size` can be larger than the page-size, but this function will only
///   read up to the page-size boundary
///
/// Returns the number of bytes read.
uint32_t read_page(Protocol* protocol, const uint32_t block,
                   const uint32_t offset, void* buffer, uint32_t size) {
    assert(block < GD5F_BLOCK_COUNT);
    assert(offset < GD5F_BLOCK_SIZE);

    // This combines the block and offset to create our 3-byte address
    // Notice that `block * GD5F_PAGES_PER_BLOCK` is equivalent to
    // `block << 6`, which is why this code here works.
    uint32_t addr = block * GD5F_PAGES_PER_BLOCK + (offset / GD5F_PAGE_SIZE);
    uint8_t cmd1[] = {
        GD5F_READ_TO_CACHE,        // our op code
        (addr & 0xFF0000) >> 16,   // first byte of the address
        (addr & 0x00FF00) >> 8,    // second byte
        (addr & 0x0000FF)          // third byte
    };
    protocol->write(ConstSpan(cmd1), {}, AddressSize::Byte3);
    Delay(2);  // maximum delay for reading to cache

    // We can then use the column address to read offsets into the page
    uint16_t col = offset % GD5F_PAGE_SIZE;
    uint32_t read_size = std::min(size, GD5F_PAGE_SIZE - col);
    auto cmdlen = protocol->type() == ProtocolType::QSPI ? 5 : 4;
    uint8_t cmd2[] = {
        protocol->type() == ProtocolType::QSPI ? GD5F_READ_FROM_CACHE_QUAD_IO
                                               : GD5F_READ_FROM_CACHE,
        (col & 0x0F00) >> 8, // first 4 bytes are not needed,
        (col & 0x00FF),      // remember that we only need 12 bytes
        0x00,                // we need a dummy byte (from datasheet)
        0x00 // for qspi we need another dummy byte (for 4 clock cycles)
    };
    // We use the QUAD IO feature of our flash for even more read speed This
    // requires us to use four address lines though
    protocol->configure(Config::QSPI_Address4);
    auto* buf = static_cast<uint8_t*>(buffer);
    protocol->read(ConstSpan(cmd2, cmdlen), Span(buf, read_size),
                   AddressSize::Byte2);
    // Unset it now that we are done
    protocol->configure(Config::QSPI_Address1);

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
uint32_t write_page(Protocol* protocol, const uint32_t block,
                    const uint32_t offset, const void* buffer,
                    const uint32_t size) {
    assert(block < GD5F_BLOCK_COUNT);
    assert(offset < GD5F_BLOCK_SIZE);

    // Note that there isn't a QUAD IO load command so we don't have to
    // configure four address lines like we did before.
    uint16_t col = offset % GD5F_PAGE_SIZE;
    uint32_t write_size = std::min(size, GD5F_PAGE_SIZE - col);
    uint8_t cmd1[] = {
        protocol->type() == ProtocolType::QSPI ? GD5F_PROGRAM_LOAD_QUAD
                                               : GD5F_PROGRAM_LOAD,
        (col & 0x0F00) >> 8, // similar to before, the first 4 bytes
        (col & 0x00FF)       // are not needed, hence the 0x0F00
    };
    auto* buf = static_cast<const uint8_t*>(buffer);
    protocol->write(ConstSpan(cmd1), ConstSpan(buf, write_size),
                    AddressSize::Byte2);

    if (write_enable(protocol) != 0)
        return 0;
    uint32_t addr = block * GD5F_PAGES_PER_BLOCK + (offset / GD5F_PAGE_SIZE);
    uint8_t cmd2[] = {
        GD5F_PROGRAM_EXECUTE,
        (addr & 0xFF0000) >> 16, // execute essentially writes the page
        (addr & 0x00FF00) >> 8,  // to the flash, so you use load
        (addr & 0x0000FF)        // to fill a page, and execute to write it
    };
    protocol->write(ConstSpan(cmd2), {}, AddressSize::Byte3);
    Delay(1); // maximum delay required for executing

    return write_size;
}

/// LFS can handle a size which is larger than a page, so we need to handle
/// that in both our writes and reads. This is why we need these next  
/// two functions even though we already have page read and writes
bool read(Protocol* protocol, uint32_t block, uint32_t offset, void* buffer,
          uint32_t size) {
    uint8_t* buf = (uint8_t*)buffer;
    while (size > 0) {
        uint32_t s = read_page(protocol, block, offset, buf, size);
        if (s == 0)
            return false;
        size -= s;
        offset += s;
        buf += s;
    }
    return true;
}

bool write(Protocol* protocol, uint32_t block, uint32_t offset,
           const void* buffer, uint32_t size) {
    uint8_t* buf = (uint8_t*)buffer;
    while (size > 0) {
        uint32_t s = write_page(protocol, block, offset, buf, size);
        if (s == 0)
            return false;
        size -= s;
        offset += s;
        buf += s;
    }
    return true;
}

bool erase(Protocol* protocol, uint32_t block) {
    // Erase acts on blocks and not pages, so we should only have the block
    // section of the address set and not the page section.
    uint32_t addr = block * GD5F_PAGES_PER_BLOCK;
    if (write_enable(protocol) != 0)
        return false;
    uint8_t cmd[] = {
        GD5F_ERASE,
        (addr & 0xFF0000) >> 16,  // Erase only takes the address
        (addr & 0x00FF00) >> 8,   // remember that this is because 
        (addr & 0x0000FF)         // it operates on whiole blocks
    };
    protocol->write(ConstSpan(cmd), {}, AddressSize::Byte3);
    Delay(12); // maximum delay required for erases
    return true;
}

int lfs_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t offset,
             void* data, lfs_size_t size) {
    auto* protocol = static_cast<Protocol*>(c->context);
    if (!read(protocol, block, offset, data, size))
        return LFS_ERR_IO;
    return LFS_ERR_OK;
}

int lfs_prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t offset,
             const void* data, lfs_size_t size) {
    auto* protocol = static_cast<Protocol*>(c->context);
    if (!write(protocol, block, offset, data, size))
        return LFS_ERR_IO;
    return LFS_ERR_OK;
}

int lfs_erase(const struct lfs_config* c, lfs_block_t block) {
    auto* protocol = static_cast<Protocol*>(c->context);
    if (!erase(protocol, block))
        return LFS_ERR_IO;
    return LFS_ERR_OK;
}

/// No-op, not required for flash
int lfs_sync(const struct lfs_config* c) { return LFS_ERR_OK; }
} // namespace

namespace Common {
GD5F1GQ5XE::GD5F1GQ5XE(Protocol& protocol_) : protocol(protocol_) {
    assert(protocol.type() == ProtocolType::SPI ||
           protocol.type() == ProtocolType::QSPI);
    config.context = &protocol;
    config.read = lfs_read;
    config.prog = lfs_prog;
    config.erase = lfs_erase;
    config.sync = lfs_sync;

    config.read_size = GD5F_PAGE_SIZE;
    config.prog_size = GD5F_PAGE_SIZE;
    config.block_size = GD5F_BLOCK_SIZE;
    config.block_count = GD5F_BLOCK_COUNT;
    config.cache_size = GD5F_PAGE_SIZE;
    config.lookahead_size = 128;
    config.block_cycles = 512;
}

bool GD5F1GQ5XE::init() {
    // If using QSPI, we have to set this for some commands
    protocol.configure(Config::QSPI_Data1);
    // Check if the id matches
    uint8_t cmd1[] = {GD5F_READ_ID, 0x00};
    uint8_t data[] = {0, 0};
    protocol.read(ConstSpan(cmd1), Span(data), AddressSize::None);
    if (data[0] != 0xC8 && data[1] != 0x31)
        return false;

    // Enable writes to the flash as a whole, by setting Protection
    // to 0x00, so nothing is protected.
    if (write_enable(&protocol) != 0)
        return false;
    uint8_t cmd2[] = {GD5F_SET_FEATURE, 0xA0, 0x00};
    protocol.write(ConstSpan(cmd2), {}, AddressSize::Byte2);
    Delay(5000);

    if (protocol.type() == ProtocolType::QSPI) {
        // Enable QE feature so we can quad reads and writes
        uint8_t cmd3[] = {GD5F_SET_FEATURE, 0xB0, 0x01};
        protocol.write(ConstSpan(cmd3), {}, AddressSize::Byte2);
        // All done, set it back to quad
        protocol.configure(Config::QSPI_Data4);
    }

    LOG("Flash init finished!");
    return true;
}
} // namespace Common
