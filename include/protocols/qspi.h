#ifndef PROTOCOLS_QSPI_H
#define PROTOCOLS_QSPI_H

#include "hal.h"
#include "protocol.h"

/// By far the most complicated protocol. This one enforces a command based
/// communication system. You need to fill out a `QSPI_CommandTypeDef` and
/// send it before you are able to do anything. With this you can really
/// see the command and address separation. There are several important partsjko
///
///   1. Command:  this is the op code to send
///   2. Address:  address (argument), make sure to set the appropriate
///   AddressSize,
///                you can also configure how many lines to send the address
///                over
///   3. Data:     how to receive the data, you also need to set the data buffer
///                and size of course, and how many line to get/write data over
///
/// You can also configure the dummy cycles as well as alternate byte mode.
/// The dummy cycle is more important; I haven't seen any usage of the alternate
/// byte mode yet. Sometimes you can only recieve data over a single line, so
/// a method is provided in order to configure the dataMode, usually at startup
/// only. You can do so using `configure` method with the appropriate `Config`.
namespace Platform {
class QSPI final : public Protocol {
private:
    QSPI_HandleTypeDef* handle;
    uint32_t data_mode = QSPI_DATA_4_LINES;
    uint32_t address_mode = QSPI_ADDRESS_1_LINE;

public:
    QSPI(QSPI_HandleTypeDef* handle_)
        : Protocol{ProtocolType::QSPI} handle{handle_} {}
    bool read(ConstSpan cmd, Span buffer, AddressSize size) override;
    bool write(ConstSpan cmd, ConstSpan buffer, AddressSize size) override;
    bool configure(Config config) override {
        switch (config) {
        case Config::QSPI_Data1:
            data_mode = QSPI_DATA_1_LINE;
            return false;
        case Config::QSPI_Data4:
            data_mode = QSPI_DATA_4_LINES;
            return false;
        case Config::QSPI_Address1:
            address_mode = QSPI_ADDRESS_1_LINE;
            return false;
        case Config::QSPI_Address4:
            address_mode = QSPI_ADDRESS_4_LINES;
            return false;
        }
        return true;
    };
};
} // namespace Platform

#endif
