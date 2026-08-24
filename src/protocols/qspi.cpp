#include "qspi.h"

#include <cstdint>

namespace {
uint32_t get_address_size(Common::AddressSize size) {
    switch (size) {
    case Common::AddressSize::Byte:
        return QSPI_ADDRESS_8_BITS;
    case Common::AddressSize::Byte2:
        return QSPI_ADDRESS_16_BITS;
    case Common::AddressSize::Byte3:
        return QSPI_ADDRESS_24_BITS;
    case Common::AddressSize::Byte4:
        return QSPI_ADDRESS_32_BITS;
    case Common::AddressSize::None:
        return QSPI_ADDRESS_NONE;
    }
}

uint32_t dummycycle_per_byte(uint32_t datalines) {
    switch (datalines) {
    case QSPI_ADDRESS_1_LINE:
        return 4;
    case QSPI_ADDRESS_4_LINES:
        return 2;
    default:
        assert(0 && "Invalid datalines setting");
    }
}

uint32_t get_address(Common::ConstSpan cmd, Common::AddressSize size) {
    auto numbytes = static_cast<std::size_t>(size);
    uint32_t address = 0;
    assert(cmd.size() > numbytes && "You misconfigured your address size");
    for (int i = 0; i < numbytes; ++i)
        address |= static_cast<uint32_t>(cmd[numbytes - i]) << (8 * i);
    return address;
}

void configure_cmd(Common::ConstSpan cmd, Common::AddressSize size,
                   QSPI_CommandTypeDef& qcmd) {
    // Configure instruction and address
    qcmd.Instruction = cmd[0];
    qcmd.AddressMode = address_mode;
    qcmd.AddressSize = get_address_size(size);
    if (qcmd.AddressSize != QSPI_ADDRESS_NONE)
        qcmd.Address = get_address(cmd, size);
    else
        qcmd.AddressMode = QSPI_ADDRESS_NONE;

    // Calculate dummy cycles from the number of empty bytes empty bytes are the
    // bytes that are not part of the address and not part of the command, so we
    // do `- 1` for the command and another subtraction based on the address width.
    auto mult = dummycycle_per_byte(data_mode);
    qcmd.DummyCycles = (cmd.size() - 1 - static_cast<std::size_t>(size)) * mult;
    // Keep this here for now, should never trigger
    assert(qcmd.DummyCycles < 12);

    // Configure data mode
    qcmd.DataMode = data_mode;
    if (!buffer.empty())
        qcmd.NbData = buffer.size();
    else
        qcmd.DataMode = QSPI_DATA_NONE;

    // These are generally the most common configuration, you can
    // probably deviate from it but then you get some weird confusing
    // stuff that we probably shouldn't deal with
    qcmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    qcmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
}
} // namespace

namespace Common {
bool QSPI::read(ConstSpan cmd, Span buffer, AddressSize size) {
    QSPI_CommandTypeDef qcmd;
    configure_cmd(cmd, size, qcmd);

    auto res = HAL_QSPI_Command(handle, &qcmd, HAL_MAX_DELAY);
    if (res != HAL_OK)
        return res;
    return HAL_QSPI_Receive(handle, buffer.data(), HAL_MAX_DELAY);
}

bool QSPI::write(ConstSpan cmd, ConstSpan buffer, AddressSize size) {
    QSPI_CommandTypeDef qcmd;
    configure_cmd(cmd, size, qcmd);

    auto res = HAL_QSPI_Command(handle, &qcmd, HAL_MAX_DELAY);
    if (res != HAL_OK)
        return res;

    if (!buffer.empty())
        return HAL_QSPI_Transmit(handle, buffer.data(), HAL_MAX_DELAY);
    return res;
}
} // namespace Common
