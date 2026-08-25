#ifndef PROTOCOLS_I2C_H
#define PROTOCOLS_I2C_H

#include "protocol.h"

/// I2C tends to be register and address heavy. This really isn't really used
/// for command based sensors so the cmd buffer tends to just be the address
/// of the register to read from instead.
///
/// The AddressSize gets directly converted to I2C's equivalent. Note that
/// unlike qspi, I2C does not support addresses greater than two bytes.
/// We use the `mem` equivalents of I2C because "most" sensors seem to
/// to prefer the pattern of specifying an address to target, followed
/// by a write or read. The `mem` I2C functions do this in one step instead
/// of two. This contrasts with SPI where we are forced to do it in two.
namespace Common {
class I2C final : public Protocol {
private:
    I2C_HandleTypeDef* handle;
    const uint32_t address;

public:
    I2C(I2C_HandleTypeDef* handle_, uin32_t address_)
        : Protocol{ProtocolType::I2C}, handle{handle_}, address{address_} {}
    bool read(ConstSpan cmd, Span buffer, AddressSize size) override;
    bool write(ConstSpan cmd, ConstSpan buffer, AddressSize size) override;
};
} // namespace Common

#endif
