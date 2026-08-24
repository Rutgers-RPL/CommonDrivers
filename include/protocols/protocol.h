#ifndef PROTOCOLS_PROTOCOL_H
#define PROTOCOLS_PROTOCOL_H

#include <cstdint>
#include <span>

/// This is our thin abstraction over the HAL. We implement a base `Protocol`
/// class which protocols like I2C, QSPI, SPI, and UART inherit from. Then
/// throughout the sensor code, we will use the `protocol.read()` and
/// `protocol.write()` methods instead. See the documentation below
/// for more information on how to implement them. And see the documentation
/// in `i2c.h`, `qspi.h`, `spi.h`, and `uart.h` for further information
/// on their implementations .
///
/// The Protocol class was designed following the assumptions that it is going
/// to only be used for sensor and flash devices. The behavior of these devices
/// can be roughly categorized into three types
///
///  * Register based: these sensors require you first send a register address,
///                    then writes will write to that register while reads will
///                    read from that register
///  * Protocol based: these sensors require you to just send some form of
///                    standardized or in-house messaging format
///  * Command based:  usually more complex devices like flash, they are like
///                    register based, but instead requires you to send some
///                    form of op code, followed by addresses or arguments.
///
/// One can argue that command-based is a special case of register based, but
/// instead taking arguments in addition to the initial address. The bosch
/// sensors are examples of register based while the gps (cd-pa1616s) is an
/// example of protocol based. Most flash devices will be command based.
///
/// Command based devices tend to also allow QSPI, which if you look at its
/// implementation actually has commands baked into its core. The complex
/// devices tend to require dummy cycles. You should specify dummy cycles using
/// empty bytes `0x00`, look at the spreadsheets to see how many you need.
namespace Common {

/// Span ---
/// A quick primer on span, it just a regular C buffer but it also
/// includes the size, really convenient, we will be using this a lot
using Span = std::span<uint8_t>;
using ConstSpan = std::span<const uint8_t>;

/// The type of the protocol, used by `Protocol` when specific protocols have
/// different code paths. This is common for QSPI where extra configuration
/// is often neccessary.
enum class ProtocolType { SPI, UART, I2C, QSPI };

/// The size/width of the address. Some protocols like I2C and QSPI do require
/// this information so it is good to supply it. `Byte1` corresponds to an
/// adress with 8 bits, `Byte2` an address with 16 bits and so on. You may use
/// `None` if no address is sent.
enum class AddressSize : std::size_t {
    None = 0,
    Byte = 1,
    Byte2 = 2,
    Byte3 = 3,
    Byte4 = 4
};

/// Some protocols require configuration, use this with the `configure` method
/// as a super lightweight method to configure them. In each subclass, make sure
/// you handle the config accordingly. The config can be changed at run time.
enum class Config {
    QSPI_Data1,    // QSPI receive data across one line
    QSPI_Data4,    // QSPI receive data across four lines
    QSPI_Address1, // QSPI send address across one line
    QSPI_Address4  // QSPI send address across four lines
};

/// Protocol ---
/// Every peripheral (SPI, I2C, UART, etc...) inherits from this base
/// instance. They MUST implment two methods, `read` and `write`
/// A couple of notes on implmentation (look in respective files for more info)
///   cmd: sends the address or register to perform the action on, or a command
///   code buffer: the buffer to read into or write into dummy_cycles: only used
///   by QSPI
/// For writes, you may set size of the buffer to zero to not transmit
/// anything. This is more relevant for flash which might just need to
/// send a single command and not receive anything afterwards.
class Protocol {
protected:
    ProtocolType ptype;

public:
    /// @brief Initializes the Protocol
    ///
    /// Subclasses must use this constructor to define their type
    ///
    /// @param type  must be one of SPI, UART, I2C, QSPI
    explicit Protocol(ProtocolType type) : ptype{type} {}
    virtual ~Protocol() = default;

    /// @brief Reads from the protocol after sending cmd
    ///
    /// The cmd buffer may include an command as its first element
    /// or an address, handle accordingly based on the protocol. You
    /// must handle dummy bytes and clocks accordingly as well.
    ///
    /// @param cmd             span holding the address/cmd to send
    /// @param buffer          buffer to read into
    /// @param address_size    size of the address
    /// @return true on error, false on success
    virtual bool read(ConstSpan cmd, Span buffer, AddressSize size) = 0;

    /// @brief Writes using the protocol after sending cmd
    ///
    /// The cmd buffer may include an command as its first element
    /// or an address, handle accordingly based on the protocol. You
    /// must handle dummy bytes and clocks accordingly as well.
    ///
    /// @param cmd             span holding the address/cmd to send
    /// @param buffer          buffer to write from, set to `{}` to write
    ///                        nothing
    /// @param address_size    size of the address
    /// @return true on error, false on success
    virtual bool write(ConstSpan cmd, ConstSpan buffer, AddressSize size) = 0;

    /// @brief Configures the given protocol using the given config
    ///
    /// All subclasses must ignore the config if they don't
    /// recognize it, otherwise they must handle the config
    /// accordingly.
    ///
    /// @param config  config object
    /// @return true on error, false on success
    virtual bool configure(Config config) { return false; };

    /// @brief non-blocking read using DMA
    ///
    /// Not currently implemented for all protocols, you should read the
    /// relevant protocol's documentation for how to use this as it differs
    /// between them.
    virtual bool readDMA(Span buffer) { return true; };

    /// @brief returns the type of the protocol
    /// @return protocol type
    ProtocolType type() const { return ptype; }
};
} // namespace Common

#endif // PROTOCOLS_PROTOCOL_H
