#include "uart.h"

namespace Platform {
bool UART::read(ConstSpan cmd, Span buffer, AddressSize size) {
    assert(cmd.empty() && "Just read using the buffer");
    return HAL_UART_Receive(handle, buffer.data(), buffer.size(),
                            HAL_MAX_DELAY);
}

bool UART::write(ConstSpan cmd, ConstSpan buffer, AddressSize size) {
    assert(cmd.empty() && "Just write using the buffer");
    return HAL_UART_Transmit(handle, buffer.data(), buffer.size(),
                             HAL_MAX_DELAY);
}

bool UART::readDMA(Span buffer) {
    return HAL_UARTEx_ReceiveToIdle_DMA(handle, buffer.data(), buffer.size());
}
} // namespace Platform
