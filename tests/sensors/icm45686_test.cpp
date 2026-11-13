/******************************************************************************
* @file     icm45686_test.cpp
* @brief    Software-only unit tests for the ICM-45686 driver.
*
* @details  This module checks the ICM-45686 driver without any hardware by
*           connecting it to a simulated sensor through the CommonDrivers MockProtocol.
*           
*           It verifies:
*           - Default and custom configuration handling (low-noise and low-power)
*           - The initialization sequence (identity check, register writes, write order)
*           - Exact bus traffic during initialization (four reads, three writes)
*           - Initialization failures (wrong device, dead bus, ignored register writes)
*           - Configuration validation (out-of-range values and illegal mode/rate
*             pairs are rejected before any bus traffic)
*           - Raw data decoding (little-endian, signed 16-bit) and lastRaw() tracking
*           - Conversion of raw counts to SI units for default and custom ranges
*           - Optional temperature output
*           - Failed reads leaving the shared Packet unchanged
*           - Use through the shared Sensor interface
*
*           How the tests work:
*           1. The driver is given a MockProtocol instead of a real SPI bus.
*           2. Every bus call made by the driver is forwarded to SimulatedICM45686,
*              a small model of the sensor's register map.
*           3. Each test sets up the simulated sensor, runs the driver, and checks
*              both the driver's results and what the driver wrote to the sensor.
*
*           What these tests do NOT prove:
*           - The simulation encodes our reading of the datasheet (read flag in bit 7,
*             little-endian data, register addresses). If that reading is wrong, the
*             tests still pass while real hardware fails.
*           - Timing is not tested. In test builds Delay() does nothing, so the power-up
*             and start-up waits in init() are never actually waited for.
*           - Noise, sensor power state, and clock-domain behavior are not simulated.
*
*           Build and run (from the repository root):
*           cmake -B build -S .
*           cmake --build build
*           ctest --test-dir build --output-on-failure
*
*           To run only these tests, use:
*
*           build/tests/common_drivers_tests --gtest_filter=ICM45686Test.*
*
* @author   Sumedh Camarushi
* @date     November 12, 2026
******************************************************************************/

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------

#include "icm45686.h"       // Driver under test
#include "protocol_mock.h"  // CommonDrivers MockProtocol (include only once)

#include <array>            // std::array for the fixed-size register table
#include <cstddef>          // std::size_t
#include <cstdint>          // Fixed-width integers (uint8_t, int16_t, ...)
#include <utility>          // std::pair for the write log
#include <vector>           // std::vector for the write log

#include <gmock/gmock.h>    // GoogleMock: ON_CALL, EXPECT_CALL, NiceMock
#include <gtest/gtest.h>    // GoogleTest: TEST_F, EXPECT_*, ASSERT_*

// -----------------------------------------------------------------------------
// Using declarations
// -----------------------------------------------------------------------------

using ::testing::_;        ///< Matches any argument in a mock rule
using ::testing::NiceMock; ///< Mock wrapper that ignores calls with no rule

namespace Common
{

    // -----------------------------------------------------------------------------
    // Simulated ICM-45686
    // -----------------------------------------------------------------------------

    /**
    * @brief    Software model of the ICM-45686 register map.
    *
    * @details  Behaves like the sensor as seen from the SPI bus:
    *           - A read transfer starts with one command byte whose bit 7 is set. The
    *             remaining seven bits are the register address.
    *           - A write transfer starts with one command byte whose bit 7 is clear.
    *             The remaining seven bits are the register address.
    *           - Consecutive registers are returned (or written) for multi-byte transfers.
    *
    *           Like the Protocol class, read() and write() return true on ERROR and
    *           false on success.
    *
    *           Faults can be injected for failure tests:
    *           - busError: every transfer fails (dead SPI link).
    *           - stuckRegister: one register silently ignores writes. SPI has no
    *             acknowledge, so the bus still reports success. Only a read-back can
    *             detect this.
    */
    class SimulatedICM45686
    {
    public:

        std::array<uint8_t, 128>            regs{}; ///< Register table, all zero until the constructor sets reset values
        std::vector<std::pair<int, int>>    writes; ///< Every write attempt as (register, value), in order

        int readCalls =     0;      ///< Number of read transfers received
        int writeCalls =    0;      ///< Number of write transfers received
        bool busError =     false;  ///< true makes every transfer fail
        int stuckRegister = -1;     ///< Register that ignores writes (-1 = none)

        /**
        * @brief    Puts the simulated sensor into its power-on state.
        *
        * @details  Only the registers the driver touches are set:
        *           - 0x72 WHO_AM_I holds the device ID 0xE9.
        *           - 0x1B ACCEL_CONFIG0 and 0x1C GYRO_CONFIG0 hold their reset value 0x06.
        *           - 0x10 PWR_MGMT0 stays 0x00, meaning both sensors are off.
        */
        SimulatedICM45686()
        {
            // Identification register. The driver compares this
            // against the expected ID 0xE9.

            regs[0x72] = 0xE9;

            // Configuration registers start at their reset
            // values, not at the values the driver will write.

            regs[0x1B] = 0x06;
            regs[0x1C] = 0x06;
        }

        /**
        * @brief        Returns one register as an int.
        *
        * @param reg:   Register address.
        *
        * @retval       Register contents. An int is returned so that failed checks
        *               print numbers instead of characters.
        */
        int get(std::size_t reg) const
        {
            return regs[reg];
        }

        /**
        * @brief        Stores a signed 16-bit measurement in two consecutive registers.
        *
        * @details      The sensor is little-endian by default, so the low byte is
        *               stored first, at the lower address.
        *
        * @param reg:   Address of the low byte.
        * @param value: Measurement to store.
        */
        void setInt16(std::size_t reg, int16_t value)
        {
            // Reinterpret the (possibly negative) value as its raw 16 bits.

            const uint16_t raw = static_cast<uint16_t>(value);

            // Low byte first, then high byte.

            regs[reg] =     static_cast<uint8_t>(raw & 0xFF);
            regs[reg + 1] = static_cast<uint8_t>(raw >> 8);
        }

        /**
        * @brief        Handles a read transfer from the driver.
        *
        * @param cmd:   Command bytes sent by the driver (one byte: address | 0x80).
        * @param out:   Buffer that receives the register contents.
        *
        * @retval       true on error, false on success (Protocol convention).
        */
        bool read(ConstSpan cmd, Span out)
        {
            // Count every transfer, including failed ones.

            ++readCalls;

            // Injected fault: the bus is dead.

            if (busError)
            {
                return true;
            }

            // A valid read has exactly one command byte with bit 7 set.
            // If the driver forgets the read flag, report an error so
            // that the test fails.

            if ((cmd.size() != 1) || ((cmd[0] & 0x80) == 0))
            {
                return true;
            }

            // Remove the read flag to get the register address.

            const std::size_t address = cmd[0] & 0x7F;

            // Refuse transfers that would run past the last register.

            if ((address + out.size()) > regs.size())
            {
                return true;
            }

            // Copy consecutive registers into the output buffer.

            for (std::size_t i = 0; i < out.size(); ++i)
            {
                out[i] = regs[address + i];
            }

            return false;
        }

        /**
        * @brief        Handles a write transfer from the driver.
        *
        * @param cmd:   Command bytes sent by the driver (one byte: the address).
        * @param data:  Bytes to store, starting at that address.
        *
        * @retval       true on error, false on success (Protocol convention).
        */
        bool write(ConstSpan cmd, ConstSpan data)
        {
            // Count every transfer, including failed ones.

            ++writeCalls;

            // Injected fault: the bus is dead.

            if (busError)
            {
                return true;
            }

            // A valid write has exactly one command byte with bit 7 clear.
            // If the driver sets the read flag on a write, report an error.

            if ((cmd.size() != 1) || ((cmd[0] & 0x80) != 0))
            {
                return true;
            }

            const std::size_t address = cmd[0];

            // Refuse transfers that would run past the last register.

            if ((address + data.size()) > regs.size())
            {
                return true;
            }

            for (std::size_t i = 0; i < data.size(); ++i)
            {
                // Log the attempt first so that tests can check the
                // order of writes, even for a stuck register.

                writes.emplace_back(static_cast<int>(address + i), static_cast<int>(data[i]));

                // A stuck register keeps its old value but still
                // reports success, like a real SPI write would.

                if (static_cast<int>(address + i) != stuckRegister)
                {
                    regs[address + i] = data[i];
                }
            }

            return false;
        }
    };
    
    // -----------------------------------------------------------------------------
    // Test fixture
    // -----------------------------------------------------------------------------

    /**
    * @brief    Shared starting point for every ICM-45686 test.
    *
    * @details  GoogleTest creates a fresh fixture object for each test, so one
    *           test cannot affect another. Each fixture owns:
    *           - chip: the simulated sensor.
    *           - bus: a MockProtocol that forwards every call to chip.
    */
    class ICM45686Test : public testing::Test
    {
    protected:

        SimulatedICM45686       chip;   ///< Simulated sensor behind the bus
        NiceMock<MockProtocol>  bus;    ///< Fake SPI bus handed to the driver

        /**
        * @brief    Creates the fake bus as an SPI protocol.
        *
        * @details  The driver asserts that it receives an SPI protocol, so the
        *           mock must be created with ProtocolType::SPI.
        */
        ICM45686Test() : bus(ProtocolType::SPI) {}

        /**
        * @brief    Connects the fake bus to the simulated sensor.
        *           
        * @details  ON_CALL sets the default behavior of a mock function:
        *           "when read/write is called with any arguments, run this code". The
        *           code forwards the call to the simulated sensor and returns its result.
        *           The address size argument is ignored by the simulation.
        */
        void SetUp() override
        {
            // Forward every bus read to the simulated sensor.

            ON_CALL(bus, read(_, _, _))
                .WillByDefault([this](ConstSpan cmd, Span out, AddressSize)
                            { return chip.read(cmd, out); });

            ON_CALL(bus, write(_, _, _))
                .WillByDefault([this](ConstSpan cmd, ConstSpan data, AddressSize)
                            { return chip.write(cmd, data); });
        }
    };

    // -----------------------------------------------------------------------------
    // Configuration tests
    // -----------------------------------------------------------------------------

    /**
    * @brief    A driver created without a configuration uses ICM45686DefaultConfig.
    *
    * @details  Checks every field of the default configuration through getConfig().
    *           Enum class values cannot be printed directly, so they are compared with ==.
    */
    TEST_F(ICM45686Test, DefaultConfigIsApplied)
    {
        // Create the driver with only the bus. The constructor's
        // default argument supplies the default configuration.

        ICM45686 sensor(bus);

        // Defaults: +/-16 g, +/-2000 dps, 800 Hz, low-noise mode,
        // temperature output disabled.

        EXPECT_TRUE(sensor.getConfig().accel_range == AccelRange::G16);
        EXPECT_TRUE(sensor.getConfig().gyro_range == GyroRange::Dps2000);
        EXPECT_TRUE(sensor.getConfig().odr == Odr::Hz800);
        EXPECT_TRUE(sensor.getConfig().mode == PowerMode::LowNoise);
        EXPECT_FALSE(sensor.getConfig().write_temperature);
    }

    // -----------------------------------------------------------------------------
    // Initialization tests
    // -----------------------------------------------------------------------------

    /**
    * @brief    init() writes the expected values, in the expected order.
    *
    * @details  With the default configuration the sensor must end up with:
    *           - 0x1B ACCEL_CONFIG0 = 0x16 (+/-16 g in bits 6:4, 800 Hz code 6 in bits 3:0)
    *           - 0x1C GYRO_CONFIG0  = 0x16 (+/-2000 dps in bits 6:4, 800 Hz code 6 in bits 3:0)
    *           - 0x10 PWR_MGMT0     = 0x0F (both sensors in low-noise mode)
    *
    *           The power register must be written last, so the sensors are only enabled
    *           after both configurations were written and confirmed.
    */
    TEST_F(ICM45686Test, InitWritesExpectedRegistersInOrder)
    {
        ICM45686 sensor(bus);

        // ASSERT stops the test on failure. Everything below
        // depends on init() having succeeded.

        ASSERT_TRUE(sensor.init());
        EXPECT_TRUE(sensor.isReady());

        // Check the final contents of the three registers.

        EXPECT_EQ(chip.get(0x1B), 0x16);
        EXPECT_EQ(chip.get(0x1C), 0x16);
        EXPECT_EQ(chip.get(0x10), 0x0F);

        // Check the order of the writes. Reads are not logged,
        // so exactly three writes are expected.

        ASSERT_EQ(chip.writes.size(), 3U);
        EXPECT_EQ(chip.writes[0].first, 0x1B);
        EXPECT_EQ(chip.writes[1].first, 0x1C);
        EXPECT_EQ(chip.writes[2].first, 0x10);
    }

    /**
    * @brief    init() performs exactly four reads and three writes.
    *
    * @details  The four reads are one WHO_AM_I read plus one read-back for each of
    *           the three written registers. EXPECT_CALL makes the mock itself fail the
    *           test if the call counts differ. With no action given, the mock falls back
    *           to the ON_CALL default from SetUp(), so the simulation still answers.
    *           AddressSize::Byte also confirms that every transfer declares a one-byte
    *           address.
    */
    TEST_F(ICM45686Test, InitUsesExactlyFourReadsAndThreeWrites)
    {
        // Expectations must be set before the driver makes any calls.

        EXPECT_CALL(bus, read(_, _, AddressSize::Byte)).Times(4);
        EXPECT_CALL(bus, write(_, _, AddressSize::Byte)).Times(3);

        ICM45686 sensor(bus);

        EXPECT_TRUE(sensor.init());
    }

    // -----------------------------------------------------------------------------
    // Initialization failure tests
    // -----------------------------------------------------------------------------

    /**
    * @brief    init() fails and writes nothing when the device ID is wrong.
    *
    * @details  Protects against configuring some other device that happens to
    *           answer on the bus.
    */
    TEST_F(ICM45686Test, InitFailsOnWrongDeviceId)
    {
        // Make the sensor report an ID other than 0xE9.

        chip.regs[0x72] = 0x00;

        ICM45686 sensor(bus);

        // Initialization must fail, the driver must not report itself
        // as ready, and no register may have been written.

        EXPECT_FALSE(sensor.init());
        EXPECT_FALSE(sensor.isReady());
        EXPECT_TRUE(chip.writes.empty());
    }

    /**
    * @brief init() fails and counts the failure when the bus is dead.
    */
    TEST_F(ICM45686Test, InitFailsOnBusError)
    {
        // Every transfer will report an error.

        chip.busError = true;

        ICM45686 sensor(bus);

        // Initialization must fail and the failed transfer
        // must be counted for diagnostics.

        EXPECT_FALSE(sensor.init());
        EXPECT_FALSE(sensor.isReady());
        EXPECT_GT(sensor.getBusFailureCount(), 0U);
    }

    /**
    * @brief    init() fails if a register does not keep the value written to it.
    *
    * @details  ACCEL_CONFIG0 ignores writes, so the driver writes 0x16, reads back
    *           the reset value 0x06, and must fail. The sensors must never be enabled
    *           in that state, so PWR_MGMT0 (0x10) must not appear in the write log.
    */
    TEST_F(ICM45686Test, InitFailsWhenARegisterIgnoresWrites)
    {
        // The accelerometer configuration register silently
        // ignores writes.

        chip.stuckRegister = 0x1B;

        ICM45686 sensor(bus);

        EXPECT_FALSE(sensor.init());
        EXPECT_FALSE(sensor.isReady());

        // Enabling the sensors must never have been attempted.

        for (const auto &w : chip.writes)
        {
            EXPECT_NE(w.first, 0x10);
        }
    }

    /**
    * @brief    init() rejects an invalid configuration before using the bus.
    *
    * @details  The default output rate is 800 Hz, which is only allowed in
    *           low-noise mode. Selecting low-power mode makes the combination illegal.
    */
    TEST_F(ICM45686Test, InitRejectsInvalidConfigWithoutTouchingTheBus)
    {
        // Combine low-power mode with the default 800 Hz rate.

        ICM45686Config bad;
        bad.mode = PowerMode::LowPower;

        ICM45686 sensor(bus, bad);

        // Initialization must fail without a single bus transfer.

        EXPECT_FALSE(sensor.init());
        EXPECT_EQ(chip.readCalls, 0);
        EXPECT_EQ(chip.writeCalls, 0);
    }

    /**
    * @brief    Out-of-range values and illegal mode/rate pairs are rejected.
    *
    * @details  Each configuration below must make init() fail without a
    *           single bus transfer:
    *           - accelerometer range code 5 (valid codes are 0 to 4)
    *           - gyroscope range code 9 (valid codes are 0 to 8)
    *           - rate codes 2 and 16 (valid codes are 3 to 15)
    *           - power mode code 0 (valid codes are 2 and 3)
    *           - low-noise mode (the default) with 1.5625 Hz, which is low-power only
    */
    TEST_F(ICM45686Test, InitRejectsOutOfRangeValuesAndIllegalModeRatePairs)
    {
        ICM45686Config badAccel;
        badAccel.accel_range = static_cast<AccelRange>(5);

        ICM45686Config badGyro;
        badGyro.gyro_range = static_cast<GyroRange>(9);

        ICM45686Config badOdrLow;
        badOdrLow.odr = static_cast<Odr>(2);

        ICM45686Config badOdrHigh;
        badOdrHigh.odr = static_cast<Odr>(16);

        ICM45686Config badMode;
        badMode.mode = static_cast<PowerMode>(0);

        ICM45686Config slowRateLowNoise;
        slowRateLowNoise.odr = Odr::Hz1_5625;

        const ICM45686Config cases[] = {badAccel, badGyro, badOdrLow,
                                        badOdrHigh, badMode, slowRateLowNoise};

        for (const auto &c : cases)
        {
            ICM45686 sensor(bus, c);

            EXPECT_FALSE(sensor.init());
        }

        EXPECT_EQ(chip.readCalls, 0);
        EXPECT_EQ(chip.writeCalls, 0);
    }

    // -----------------------------------------------------------------------------
    // Configuration encoding tests
    // -----------------------------------------------------------------------------

    /**
    * @brief    A custom low-noise configuration is encoded into the registers.
    *
    * @details  Expected values are worked out by hand from the datasheet codes
    *           rather than with the driver's own formula:
    *           - +/-4 g = code 3, +/-500 dps = code 3, 100 Hz = code 9
    *           - config register = (range code << 4) | rate code = (3 << 4) | 9 = 0x39
    *           - power register  = (3 << 2) | 3 = 0x0F (low-noise mode for both sensors)
    */
    TEST_F(ICM45686Test, CustomLNConfigIsWrittenToRegisters)
    {
        // Change only the fields under test. The remaining
        // fields keep their default values.

        ICM45686Config config;
        config.accel_range = AccelRange::G4;
        config.gyro_range = GyroRange::Dps500;
        config.odr = Odr::Hz100;

        ICM45686 sensor(bus, config);

        ASSERT_TRUE(sensor.init());

        EXPECT_EQ(chip.get(0x1B), 0x39);
        EXPECT_EQ(chip.get(0x1C), 0x39);
        EXPECT_EQ(chip.get(0x10), 0x0F);
    }

    /**
    * @brief    A custom low-power configuration is encoded into the registers.
    *
    * @details  Expected values are worked out by hand from the datasheet codes:
    *           - +/-2 g = code 4, +/-250 dps = code 4, 50 Hz = code 10
    *           - config register = (4 << 4) | 10 = 0x4A
    *           - power register  = (2 << 2) | 2 = 0x0A (low-power mode for both sensors)
    */
    TEST_F(ICM45686Test, CustomLPConfigIsWrittenToRegisters)
    {
        ICM45686Config config;
        config.accel_range = AccelRange::G2;
        config.gyro_range = GyroRange::Dps250;
        config.odr = Odr::Hz50;
        config.mode = PowerMode::LowPower;

        ICM45686 sensor(bus, config);

        ASSERT_TRUE(sensor.init());

        EXPECT_EQ(chip.get(0x1B), 0x4A);
        EXPECT_EQ(chip.get(0x1C), 0x4A);
        EXPECT_EQ(chip.get(0x10), 0x0A);
    }

    // -----------------------------------------------------------------------------
    // Raw data acquisition tests
    // -----------------------------------------------------------------------------

    /**
    * @brief    readRaw() decodes signed, little-endian 16-bit values correctly.
    *
    * @details  The fourteen data registers start at address 0x00:
    *
    *           0-1 accel X, 2-3 accel Y, 4-5 accel Z,
    *           6-7 gyro X, 8-9 gyro Y, 10-11 gyro Z, 12-13 temperature
    *
    *           The test values are the awkward cases: a normal positive, a negative,
    *           +1, -1 (all bits set), and both limits of a signed 16-bit number.
    */
    TEST_F(ICM45686Test, ReadRawDecodesSignedLittleEndianValues)
    {
        ICM45686 sensor(bus);

        ASSERT_TRUE(sensor.init());

        // Place known measurements in the data registers.

        chip.setInt16(0, 2048);
        chip.setInt16(2, -2048);
        chip.setInt16(4, 1);

        chip.setInt16(6, -1);
        chip.setInt16(8, 32767);
        chip.setInt16(10, -32768);

        chip.setInt16(12, 1280);

        // Pass the address of the sample so that
        // readRaw() can fill it in.

        ICM45686RawSample sample;

        ASSERT_TRUE(sensor.readRaw(&sample));

        EXPECT_EQ(sample.accel_x, 2048);
        EXPECT_EQ(sample.accel_y, -2048);
        EXPECT_EQ(sample.accel_z, 1);

        EXPECT_EQ(sample.gyro_x, -1);
        EXPECT_EQ(sample.gyro_y, 32767);
        EXPECT_EQ(sample.gyro_z, -32768);

        EXPECT_EQ(sample.temperature, 1280);
    }

    /**
    * @brief    readRaw() refuses to run before init() and never touches its output.
    *
    * @details  Covers two safety checks: the driver is not ready, and a null
    *           output pointer is passed. Neither case may use the bus.
    */
    TEST_F(ICM45686Test, ReadRawFailsBeforeInitAndLeavesOutputUntouched)
    {
        ICM45686 sensor(bus);

        // Fill the sample with a recognizable value so that any
        // accidental write to it would be noticed.

        ICM45686RawSample sample;
        sample.accel_x = 1234;

        EXPECT_FALSE(sensor.readRaw(&sample));
        EXPECT_EQ(sample.accel_x, 1234);

        // A null pointer must be rejected safely.

        EXPECT_FALSE(sensor.readRaw(nullptr));

        // Neither call may have reached the bus.

        EXPECT_EQ(chip.readCalls, 0);
    }

    /**
    * @brief    lastRaw() always holds the most recent SUCCESSFUL raw sample.
    *
    * @details  Before the first read it is zero. A failed read must not
    *           overwrite it with zeros or partial data.
    */
    TEST_F(ICM45686Test, LastRawTracksTheMostRecentSuccessfulRead)
    {
        ICM45686 sensor(bus);

        ASSERT_TRUE(sensor.init());

        // Nothing has been read yet.

        EXPECT_EQ(sensor.lastRaw().accel_z, 0);

        // First successful read.

        chip.setInt16(4, 2048);

        ICM45686RawSample sample;

        ASSERT_TRUE(sensor.readRaw(&sample));
        EXPECT_EQ(sensor.lastRaw().accel_z, 2048);

        // Second successful read replaces the first.

        chip.setInt16(4, -512);

        ASSERT_TRUE(sensor.readRaw(&sample));
        EXPECT_EQ(sensor.lastRaw().accel_z, -512);

        // A failed read leaves the stored sample alone.

        chip.busError = true;

        EXPECT_FALSE(sensor.readRaw(&sample));
        EXPECT_EQ(sensor.lastRaw().accel_z, -512);
    }

    // -----------------------------------------------------------------------------
    // SI conversion tests
    // -----------------------------------------------------------------------------

    /**
    * @brief    read() converts raw counts to SI units using the default ranges.
    *
    * @details  Hand calculations (one count = full scale / 32768):
    *           - accel at +/-16 g: 2048 counts = 1 g = 9.80665 m/s^2, so -1024 counts = -0.5 g = -4.903325 m/s^2
    *           - gyro at +/-2000 dps: 1000 counts = 61.035 dps = 1.06526 rad/s
    *
    *           EXPECT_NEAR allows a small rounding difference for floating-point values.
    *           Packet is a packed structure, so its fields are only compared with
    *           EXPECT_NEAR and EXPECT_FLOAT_EQ, which take their numbers by value.
    */
    TEST_F(ICM45686Test, ReadConvertsUsingTheDefaultRanges)
    {
        ICM45686 sensor(bus);

        ASSERT_TRUE(sensor.init());

        // Accelerometer X and Z, gyroscope X and Y. Unset
        // registers remain zero.

        chip.setInt16(0, -1024);
        chip.setInt16(4, 2048);

        chip.setInt16(6, 1000);
        chip.setInt16(8, -1000);

        Packet packet;

        ASSERT_TRUE(sensor.read(packet));

        EXPECT_NEAR(packet.acceleration_x_mss, -4.903325f, 1e-3f);
        EXPECT_NEAR(packet.acceleration_z_mss, 9.80665f, 1e-3f);

        EXPECT_NEAR(packet.angular_velocity_x_rads, 1.06526f, 1e-4f);
        EXPECT_NEAR(packet.angular_velocity_y_rads, -1.06526f, 1e-4f);
    }

    /**
    * @brief    The SI scale factors follow the configured ranges.
    *
    * @details  Hand calculations for +/-2 g and +/-250 dps:
    *           - accel: 16384 counts = 1 g = 9.80665 m/s^2
    *           - gyro: 16384 counts = 125 dps = 2.18166 rad/s
    */
    TEST_F(ICM45686Test, ReadUsesTheConfiguredRanges)
    {
        ICM45686Config config;
        config.accel_range = AccelRange::G2;
        config.gyro_range = GyroRange::Dps250;
        config.odr = Odr::Hz100;

        ICM45686 sensor(bus, config);

        ASSERT_TRUE(sensor.init());

        chip.setInt16(4, 16384);
        chip.setInt16(6, 16384);

        Packet packet;

        ASSERT_TRUE(sensor.read(packet));

        EXPECT_NEAR(packet.acceleration_z_mss, 9.80665f, 1e-3f);
        EXPECT_NEAR(packet.angular_velocity_x_rads, 2.18166f, 1e-3f);
    }

    /**
    * @brief    Temperature is only written to the Packet when enabled.
    *
    * @details  The temperature field is shared with other sensors, so the driver
    *           must leave it alone by default. With the option on, 1280 raw counts convert
    *           to 1280 / 128 + 25 = 35 degrees C.
    */
    TEST_F(ICM45686Test, TemperatureIsOnlyWrittenWhenEnabled)
    {
        // First driver: default configuration, temperature option off.

        ICM45686 plain(bus);

        ASSERT_TRUE(plain.init());

        chip.setInt16(12, 1280);

        // Fill the field with a recognizable value so that an
        // unwanted overwrite would be noticed.

        Packet packet;
        packet.temperature_c = 99.0f;

        ASSERT_TRUE(plain.read(packet));
        EXPECT_FLOAT_EQ(packet.temperature_c, 99.0f);

        // Second driver: temperature option on. It shares the same
        // simulated sensor, which is fine because init() rewrites
        // the registers.

        ICM45686Config config;
        config.write_temperature = true;

        ICM45686 withTemperature(bus, config);

        ASSERT_TRUE(withTemperature.init());
        ASSERT_TRUE(withTemperature.read(packet));

        EXPECT_NEAR(packet.temperature_c, 35.0f, 1e-3f);
    }

    // -----------------------------------------------------------------------------
    // Read failure and interface tests
    // -----------------------------------------------------------------------------

    /**
    * @brief    A failed read leaves the Packet unchanged and counts the failure.
    *
    * @details  The Packet is shared with other sensors, so a failed read must
    *           never overwrite it with zeros or partial data.
    */
    TEST_F(ICM45686Test, FailedReadLeavesPacketUntouched)
    {
        ICM45686 sensor(bus);

        ASSERT_TRUE(sensor.init());

        // Fill two fields with recognizable values.

        Packet packet;
        packet.acceleration_x_mss = 7.0f;
        packet.angular_velocity_x_rads = 8.0f;

        // Remember the failure count, then break the bus.

        const uint32_t failuresBefore = sensor.getBusFailureCount();

        chip.busError = true;

        EXPECT_FALSE(sensor.read(packet));

        // The packet must be unchanged and the failure counted.

        EXPECT_FLOAT_EQ(packet.acceleration_x_mss, 7.0f);
        EXPECT_FLOAT_EQ(packet.angular_velocity_x_rads, 8.0f);
        EXPECT_GT(sensor.getBusFailureCount(), failuresBefore);
    }

    /**
    * @brief    The driver works when used through the shared Sensor interface.
    *
    * @details  Firmware tasks see every sensor as a Sensor reference. Calling
    *           init() and read() through that reference checks that the virtual functions
    *           reach the ICM-45686 implementation.
    */
    TEST_F(ICM45686Test, WorksThroughTheSensorInterface)
    {
        ICM45686 driver(bus);

        // A base-class reference to the same object.

        Sensor &sensor = driver;

        ASSERT_TRUE(sensor.init());

        // 2048 counts at the default +/-16 g range is exactly 1 g.

        chip.setInt16(4, 2048);

        Packet packet;

        ASSERT_TRUE(sensor.read(packet));
        EXPECT_NEAR(packet.acceleration_z_mss, 9.80665f, 1e-3f);
    }
} // namespace Common
