#ifndef PROTOCOLS_SPI_H
#define PROTOCOLS_SPI_H

#include "hal.h"
#include "protocol.h"

/// SPI generally doesn't care too much about your underlying device. It can
/// handle both register and command based devices well. For Register based, you
/// generally must transmit the register address, and then you may read from or
/// write to it. For command based devices, you may group the command with
/// any arguments you want and then send the command buffer. You can then choose
/// to read or write or do nothing based on the command. Note that you must do a
/// chip select before you are able to communicate anything. This is handled for
/// you already by this implementation so you don't have to worry about it.
///
/// SPI doesn't really care about the address size, it just sends whatever it
/// wants. Kind of demure isn't it? Hence in our implementations you may just
/// ignore the `AddressSize`, but it is good idea to still specify it since the
/// other protocols do the same. To send dummy bytes you should do this
///
///   auto cmd_buffer = { my_command_code, some_parameters, 0x00 };
///
/// The 0x00 is eight bytes of dummy. SPI will just send it as is and everything
/// should work well.
namespace Platform {
class SPI final : public Protocol {
private:
    SPI_HandleTypeDef* handle;
    GPIO_TypeDef* port;
    uint8_t pin;

public:
    SPI(SPI_HandleTypeDef* handle_, GPIO_TypeDef* port_, uint8_t pin_)
        : Protocol{ProtocolType::SPI}, handle{handle_}, port{port_}, pin{pin_} {
    }
    bool read(ConstSpan cmd, Span buffer, AddressSize size) override;
    bool write(ConstSpan cmd, ConstSpan buffer, AddressSize size) override;
};
} // namespace Platform

#endif
