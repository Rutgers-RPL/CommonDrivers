#include "spi.h"

#include <cassert>

namespace {
/// This handy guard makes sure we call chip deselect after we return
/// from the function, either due to an error or due to a success.
struct Guard {
    GPIO_TypeDef* port;
    uint16_t pin;

    Guard(GPIO_TypeDef* port, uint16_t pin) : port(port), pin(pin) {
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
    }

    ~Guard() { HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET); }
};
} // namespace

namespace Platform {
bool SPI::read(ConstSpan cmd, Span buffer, AddressSize size) {
    assert(!cmd.empty() && "Command buffer should not be empty");
    Guard cs = Guard(port, pin);
    auto res = HAL_SPI_Transmit(handle, cmd.data(), cmd.size(), HAL_MAX_DELAY);
    if (res != HAL_OK)
        return res;
    return HAL_SPI_Receive(handle, buffer.data(), buffer.size(), HAL_MAX_DELAY);
}

bool SPI::write(ConstSpan cmd, ConstSpan buffer, AddressSize size) {
    assert(!cmd.empty() && "Command buffer should not be empty");
    Guard cs = Guard(port, pin);
    auto res = HAL_SPI_Transmit(handle, cmd.data(), cmd.size(), HAL_MAX_DELAY);
    if (res != HAL_OK)
        return res;
    if (!buffer.empty())
        return HAL_SPI_Transmit(handle, buffer.data(), buffer.size(),
                                HAL_MAX_DELAY);
    return res;
}
} // namespace Platform
