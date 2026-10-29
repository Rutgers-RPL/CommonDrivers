/******************************************************************************
* @file     icm45686.cpp
* @brief    Implementation of the ICM-45686 driver.
*
* @details  This module performs register-level SPI communication
*           with the ICM-45686 and provides functions for:
*
*           - Register reads and writes
*           - Device verification
*           - Sensor initialization
*           - Accelerometer and gyroscope configuration
*           - Raw sensor data acquisition
*           - Conversion to SI units for the shared Packet
*
* @author   Sumedh Camarushi
* @date     October 29, 2026
******************************************************************************/

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------

#include "icm45686.h"

#include "hal.h"
#include "log.h"

// -----------------------------------------------------------------------------
// Full-scale range lookup tables
// -----------------------------------------------------------------------------

// Indexed by the AccelRange and GyroRange register codes.

static const float accelFullScaleG[] =  {32.0f, 16.0f, 8.0f, 4.0f, 2.0f};

static const float gyroFullScaleDps[] = {4000.0f, 2000.0f, 1000.0f, 500.0f, 250.0f,
                                         125.0f, 62.5f, 31.25f, 15.625f};

namespace Common
{

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

ICM45686::ICM45686(Protocol &protocol_, const ICM45686Config &config_) : protocol{protocol_}, config{config_}
{
    // The ICM-45686 is driven over SPI. Stop early in debug
    // builds if the wrong kind of bus was supplied.

    assert(protocol.type() == ProtocolType::SPI);
}

// -----------------------------------------------------------------------------
// Low-level register interface
// -----------------------------------------------------------------------------

bool ICM45686::writeReg(uint8_t reg, uint8_t value)
{
    // Send a register address followed by the value that should
    // be written to it. The address has bit 7 clear, which marks
    // the transfer as a write.

    if (protocol.write(ConstSpan(&reg, 1), ConstSpan(&value, 1), AddressSize::Byte))
    {
        // Increment the bus failure counter for diagnostics.
        busFailureCount++;

        return false;
    }

    return true;
}

bool ICM45686::burstReadData(uint8_t reg, uint8_t *buf, size_t length)
{
    // Verify that the output buffer is valid
    // before attempting the SPI transaction.

    if ((buf == nullptr) || (length == 0U))
    {
        return false;
    }

    // Set bit 7 of the first byte to mark the transfer as a read,
    // then clock in the requested number of bytes.

    const uint8_t cmd = reg | ICM45686_SPI_READ_FLAG;

    if (protocol.read(ConstSpan(&cmd, 1), Span(buf, length), AddressSize::Byte))
    {
        // Increment the bus failure counter for diagnostics.
        busFailureCount++;

        return false;
    }

    return true;
}

bool ICM45686::readReg(uint8_t reg, uint8_t *value)
{
    // Read a single register through the burst read function.

    return burstReadData(reg, value, 1U);
}

bool ICM45686::writeRegVerified(uint8_t reg, uint8_t value)
{
    uint8_t readBack = 0;

    // SPI has no acknowledge, so a successful write only means the
    // bytes were sent. Read the register back to confirm that the
    // sensor actually holds the value.

    if (!writeReg(reg, value))
    {
        return false;
    }

    if (!readReg(reg, &readBack))
    {
        return false;
    }

    return (readBack == value);
}

// -----------------------------------------------------------------------------
// Device initialization
// -----------------------------------------------------------------------------

bool ICM45686::validateConfig() const
{
    const uint8_t accel =   static_cast<uint8_t>(config.accel_range);
    const uint8_t gyro =    static_cast<uint8_t>(config.gyro_range);
    const uint8_t odr =     static_cast<uint8_t>(config.odr);

    // Reject enum values outside the defined register codes.

    if ((accel > 4U) || (gyro > 8U) || (odr < 3U) || (odr > 15U))
    {
        return false;
    }

    if ((config.mode != PowerMode::LowPower) && (config.mode != PowerMode::LowNoise))
    {
        return false;
    }

    // Output rates of 800 Hz and above (codes 3 to 6) are low-noise only.

    if ((config.mode == PowerMode::LowPower) && (odr <= 6U))
    {
        return false;
    }

    // Output rates of 6.25 Hz and below (codes 13 to 15) are low-power only.

    if ((config.mode == PowerMode::LowNoise) && (odr >= 13U))
    {
        return false;
    }

    return true;
}

bool ICM45686::verifyConnection()
{
    uint8_t who_am_i = 0;

    // The WHO_AM_I register should contain
    // the ICM-45686 device ID.

    if (!readReg(ICM45686_REG_WHO_AM_I, &who_am_i))
    {
        return false;
    }

    return (who_am_i == ICM45686_WHO_AM_I_VALUE);
}

bool ICM45686::init()
{
    // Mark the driver as not ready until every step has succeeded.

    ready = false;

    // Reject an inconsistent configuration before
    // touching the sensor.

    if (!validateConfig())
    {
        LOG("ICM45686: invalid configuration\r\n");

        return false;
    }

    // Registers become usable 1 ms after power-up. Wait a little
    // longer to cover timer granularity.

    Delay(ICM45686_POWER_UP_DELAY_MS);

    // Verify that the sensor is responding before
    // attempting any configuration.

    if (!verifyConnection())
    {
        LOG("ICM45686: device not detected\r\n");

        return false;
    }

    // Configure the device. The sensors stay off until the
    // power mode register is written last.

    if (!setAccelConfig())
    {
        LOG("ICM45686: accelerometer configuration failed\r\n");

        return false;
    }

    if (!setGyroConfig())
    {
        LOG("ICM45686: gyroscope configuration failed\r\n");

        return false;
    }

    computeScaleFactors();

    if (!enableSensors())
    {
        LOG("ICM45686: enabling sensors failed\r\n");

        return false;
    }

    // The gyroscope needs about 35 ms after being enabled before
    // its drive is ready. A longer wait leaves some margin.

    Delay(ICM45686_STARTUP_DELAY_MS);

    ready = true;

    return true;
}

// -----------------------------------------------------------------------------
// Sensor configuration
// -----------------------------------------------------------------------------

bool ICM45686::setAccelConfig()
{
    // Combine the full-scale range and output data rate
    // into the accelerometer configuration register.

    const uint8_t value = static_cast<uint8_t>(
        (static_cast<uint8_t>(config.accel_range) << ICM45686_FS_SHIFT) | static_cast<uint8_t>(config.odr));

    return writeRegVerified(ICM45686_REG_ACCEL_CONFIG0, value);
}

bool ICM45686::setGyroConfig()
{
    // Combine the full-scale range and output data rate
    // into the gyroscope configuration register.

    const uint8_t value = static_cast<uint8_t>(
        (static_cast<uint8_t>(config.gyro_range) << ICM45686_FS_SHIFT) | static_cast<uint8_t>(config.odr));

    return writeRegVerified(ICM45686_REG_GYRO_CONFIG0, value);
}

bool ICM45686::enableSensors()
{
    // The ICM-45686 powers up with both sensors off. Both sensors
    // use the same power mode, so the same two-bit code is placed
    // in the gyroscope and accelerometer fields.

    const uint8_t mode =    static_cast<uint8_t>(config.mode);

    const uint8_t value =   static_cast<uint8_t>((mode << ICM45686_GYRO_MODE_SHIFT) | mode);

    return writeRegVerified(ICM45686_REG_PWR_MGMT0, value);
}

void ICM45686::computeScaleFactors()
{
    // One raw count equals the full-scale range divided by 32768.
    // Multiply by gravity to convert g to m/s^2, and by pi/180
    // to convert degrees/second to radians/second.

    accelScale = (accelFullScaleG[static_cast<uint8_t>(config.accel_range)] /
                  ICM45686_FULL_SCALE_COUNTS) *
                 ICM45686_GRAVITY_MSS;

    gyroScale = (gyroFullScaleDps[static_cast<uint8_t>(config.gyro_range)] /
                 ICM45686_FULL_SCALE_COUNTS) *
                ICM45686_DEG_TO_RAD;
}

// -----------------------------------------------------------------------------
// Sensor acquisition
// -----------------------------------------------------------------------------

bool ICM45686::readRaw(ICM45686RawSample *sample)
{
    uint8_t buf[ICM45686_DATA_SIZE];

    // Verify that the output pointer is valid and that the
    // sensor is initialized before attempting the SPI transaction.

    if ((sample == nullptr) || !ready)
    {
        return false;
    }

    // Read fourteen consecutive bytes beginning at the first
    // accelerometer output register: accelerometer X, Y, Z,
    // gyroscope X, Y, Z, then temperature.

    if (!burstReadData(ICM45686_REG_ACCEL_DATA_X1, buf, ICM45686_DATA_SIZE))
    {
        // The output sample is left unchanged on failure so that
        // a failed read cannot be mistaken for a measurement of zero.

        return false;
    }

    // Combine the low and high bytes into signed 16-bit
    // measurements. The sensor sends the low byte first.

    ICM45686RawSample result;

    result.accel_x =        static_cast<int16_t>((buf[1] << 8) | buf[0]);
    result.accel_y =        static_cast<int16_t>((buf[3] << 8) | buf[2]);
    result.accel_z =        static_cast<int16_t>((buf[5] << 8) | buf[4]);

    result.gyro_x =         static_cast<int16_t>((buf[7] << 8) | buf[6]);
    result.gyro_y =         static_cast<int16_t>((buf[9] << 8) | buf[8]);
    result.gyro_z =         static_cast<int16_t>((buf[11] << 8) | buf[10]);

    result.temperature =    static_cast<int16_t>((buf[13] << 8) | buf[12]);

    lastRawSample = result;
    *sample = result;

    return true;
}

bool ICM45686::read(Packet &packet)
{
    ICM45686RawSample sample;

    // Read the raw measurements first. The packet is only
    // modified after the whole read has succeeded.

    if (!readRaw(&sample))
    {
        return false;
    }

    // Convert the raw counts to SI units.

    packet.acceleration_x_mss =         sample.accel_x * accelScale;
    packet.acceleration_y_mss =         sample.accel_y * accelScale;
    packet.acceleration_z_mss =         sample.accel_z * accelScale;

    packet.angular_velocity_x_rads =    sample.gyro_x * gyroScale;
    packet.angular_velocity_y_rads =    sample.gyro_y * gyroScale;
    packet.angular_velocity_z_rads =    sample.gyro_z * gyroScale;

    // The temperature field is shared with other sensors, so it
    // is only written when the configuration asks for it.

    if (config.write_temperature)
    {
        packet.temperature_c = (static_cast<float>(sample.temperature) / 128.0f) + 25.0f;
    }

    return true;
}

// -----------------------------------------------------------------------------
// Status accessors
// -----------------------------------------------------------------------------

bool ICM45686::isReady() const
{
    return ready;
}

uint32_t ICM45686::getBusFailureCount() const
{
    return busFailureCount;
}

const ICM45686RawSample &ICM45686::lastRaw() const
{
    return lastRawSample;
}

const ICM45686Config &ICM45686::getConfig() const
{
    return config;
}

} // namespace Common