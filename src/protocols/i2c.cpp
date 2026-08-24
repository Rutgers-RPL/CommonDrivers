#include "i2c.h"

#include "hal.h"
#include "protocol.h"

#include <cassert>
#include <cstdint>

namespace {
uint32_t get_address_size(Common::AddressSize size) {
    assert((size == Common::AddressSize::Byte ||
            size == Common::AddressSize::Byte2) &&
           "I2C addressses must one or two bytes");
    switch (size) {
    case Common::AddressSize::Byte:
        return I2C_MEMADD_SIZE_8BIT;
    case Common::AddressSize::Byte2:
        return I2C_MEMADD_SIZE_16BIT;
    default:
        return 0; // shouldn't be reached
    }
}

uint16_t get_address(Common::ConstSpan cmd, Common::AddressSize size) {
    assert(!cmd.empty() && "Address cannot be empty");
    if (size == Common::AddressSize::Byte)
        return cmd[0];
    assert((cmd.size() == 2) && "Address length must be be two bytes bytes");
    return static_cast<uint16_t>((cmd[0] << 8) | cmd[1]);
}
} // namespace

namespace Common {
bool I2C::read(ConstSpan cmd, Span buffer, AddressSize size) {
    auto asize = get_address_size(size);
    return HAL_I2C_Mem_Read(handle, address << 1, get_address(cmd, size), asize,
                            buffer.data(), buffer.size(), HAL_MAX_DELAY);
}

bool I2C::write(ConstSpan cmd, ConstSpan buffer, AddressSize size) {
    auto asize = get_address_size(size);
    return HAL_I2C_Mem_Write(handle, address << 1, get_address(cmd, size),
                             asize, buffer.data(), buffer.size(),
                             HAL_MAX_DELAY);
}
} // namespace Common
