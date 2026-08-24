#ifndef PROTOCOLS_UART_H
#define PROTOCOLS_UART_H

#include "protocol.h"

/// Arguably the simplest protocol. UART just sends stuff, much like SPI, but
/// you don't have to deal with chip selects with UART, so it is even more fire
/// and forget. Since we don't have any usages of a UART sensor transmitting
/// commands, this current implementation just writes and reads directly,
/// with no intermediate step. The `cmd` buffer is ignored, just write
/// using the buffer.
namespace Common {
class UART final : public Protocol {
private:
    UART_HandleTypeDef* handle;

public:
    UART(UART_HandleTypeDef* handle_)
        : Protocol{ProtocolType::UART}, handle{handle_} {}
    bool read(ConstSpan cmd, Span buffer, AddressSize size) override;
    bool write(ConstSpan cmd, ConstSpan buffer, AddressSize size) override;
    bool readDMA(Span buffer) override;
};
} // namespace Common

#endif
