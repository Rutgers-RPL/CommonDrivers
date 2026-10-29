/******************************************************************************
* @file     icm45686.h
* @brief    Header file for the ICM-45686 driver.
*
* @details  This module provides:
*           - Register-level SPI communication (through the CommonDrivers Protocol layer)
*           - Device initialization
*           - Sensor configuration (full-scale ranges, output data rate, power mode)
*           - Connection verification
*           - Raw accelerometer, gyroscope, and temperature data acquisition
*           - Conversion of raw measurements to SI units for the shared Packet
*
*           The driver communicates directly with the ICM-45686
*           without relying on third-party libraries.
*
* @author   Sumedh Camarushi
* @date     October 29, 2026
******************************************************************************/

// -----------------------------------------------------------------------------
// Define to prevent recursive inclusion
// -----------------------------------------------------------------------------

#ifndef ICM45686_H
#define ICM45686_H

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------

#include "protocol.h"
#include "sensor.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

// -----------------------------------------------------------------------------
// ICM-45686 register definitions
// -----------------------------------------------------------------------------

#define ICM45686_REG_ACCEL_DATA_X1  0x00U   ///< First sensor data register (accelerometer X)

#define ICM45686_REG_PWR_MGMT0      0x10U   ///< Sensor power mode register
#define ICM45686_REG_ACCEL_CONFIG0  0x1BU   ///< Accelerometer range and output rate register
#define ICM45686_REG_GYRO_CONFIG0   0x1CU   ///< Gyroscope range and output rate register

#define ICM45686_REG_WHO_AM_I       0x72U   ///< Device identification register
#define ICM45686_WHO_AM_I_VALUE     0xE9U   ///< Expected contents of WHO_AM_I
#define ICM45686_SPI_READ_FLAG      0x80U   ///< Bit 7 of the first SPI byte: 1 = read

// -----------------------------------------------------------------------------
// ICM-45686 register field positions
// -----------------------------------------------------------------------------

#define ICM45686_FS_SHIFT           4U  ///< Range field position in ACCEL_CONFIG0 and GYRO_CONFIG0
#define ICM45686_GYRO_MODE_SHIFT    2U  ///< Gyroscope mode field position in PWR_MGMT0

// -----------------------------------------------------------------------------
// Driver constants
// -----------------------------------------------------------------------------

#define ICM45686_DATA_SIZE          14U             ///< Accel (6) + gyro (6) + temperature (2) bytes
#define ICM45686_POWER_UP_DELAY_MS  2U              ///< Wait before the first register access (1 ms required)
#define ICM45686_STARTUP_DELAY_MS   50U             ///< Wait after enabling the sensors (gyro needs 35 ms)
#define ICM45686_FULL_SCALE_COUNTS  32768.0f        ///< Raw counts at positive full scale
#define ICM45686_GRAVITY_MSS        9.80665f        ///< Standard gravity in m/s^2
#define ICM45686_DEG_TO_RAD         0.017453293f    ///< Degrees to radians

// -----------------------------------------------------------------------------
// Configuration options
// -----------------------------------------------------------------------------

namespace Common
{

/**
* @brief Accelerometer full-scale range. Values are the datasheet register codes.
*/
enum class AccelRange : uint8_t
{
    G32 = 0,
    G16,
    G8,
    G4,
    G2
};

/**
* @brief Gyroscope full-scale range. Values are the datasheet register codes.
*/
enum class GyroRange : uint8_t
{
    Dps4000 = 0,
    Dps2000,
    Dps1000,
    Dps500,
    Dps250,
    Dps125,
    Dps62_5,
    Dps31_25,
    Dps15_625
};

/**
* @brief    Output data rate. Values are the datasheet register codes.
*
* @details  Rates of 800 Hz and above require low-noise mode. Rates of
*           6.25 Hz and below require low-power mode. Below about 25 Hz, the first
*           read after init() may happen before the first sample is ready.
*/
enum class Odr : uint8_t
{
    Hz6400 = 3,
    Hz3200,
    Hz1600,
    Hz800,
    Hz400,
    Hz200,
    Hz100,
    Hz50,
    Hz25,
    Hz12_5,
    Hz6_25,
    Hz3_125,
    Hz1_5625
};

/**
* @brief Power mode for both sensors. Values are the datasheet register codes.
*/
enum class PowerMode : uint8_t
{
    LowPower = 2,
    LowNoise = 3
};

// -----------------------------------------------------------------------------
// Sensor data structures
// -----------------------------------------------------------------------------

/**
* @brief    Driver configuration.
*
* @details  Any field left out takes the value written here.
*/
struct ICM45686Config
{
    AccelRange accel_range =    AccelRange::G16;        ///< Accelerometer full-scale range
    GyroRange gyro_range =      GyroRange::Dps2000;     ///< Gyroscope full-scale range
    Odr odr =                   Odr::Hz800;             ///< Output data rate
    PowerMode mode =            PowerMode::LowNoise;    ///< Power mode for both sensors
    bool write_temperature =    false;                  ///< true to write temperature_c to the Packet
};

/**
* @brief Default configuration, used when a board passes none.
*/
inline constexpr ICM45686Config ICM45686DefaultConfig{};

/**
* @brief    One raw sensor sample.
*
* @details  Stores the values exactly as reported by the ICM-45686
*           registers before any unit conversion.
*/
struct ICM45686RawSample
{
    int16_t accel_x =       0;  ///< Raw X-axis acceleration
    int16_t accel_y =       0;  ///< Raw Y-axis acceleration
    int16_t accel_z =       0;  ///< Raw Z-axis acceleration

    int16_t gyro_x =        0;  ///< Raw X-axis angular velocity
    int16_t gyro_y =        0;  ///< Raw Y-axis angular velocity
    int16_t gyro_z =        0;  ///< Raw Z-axis angular velocity
    
    int16_t temperature =   0;  ///< Raw temperature
};

// -----------------------------------------------------------------------------
// Driver class
// -----------------------------------------------------------------------------

/**
* @brief Driver for the ICM-45686 IMU over SPI.
*/
class ICM45686 final : public Sensor
{
private:
    Protocol&       protocol;           ///< Bus used to reach the sensor
    ICM45686Config  config;             ///< Active configuration

    float accelScale =  0.0f;           ///< m/s^2 per raw accelerometer count
    float gyroScale =   0.0f;           ///< rad/s per raw gyroscope count
    
    bool ready = false;                 ///< true after init() succeeded
    
    uint32_t busFailureCount = 0U;      ///< Failed bus transactions
    
    ICM45686RawSample lastRawSample;    ///< Most recent successful raw sample

    // -------------------------------------------------------------------------
    // Low-level register interface
    // -------------------------------------------------------------------------

    /**
    * @brief        Writes a value to an ICM-45686 register.
    *
    * @param reg:   Register address.
    * @param value: Value to write.
    *
    * @retval       true if the write succeeded, false otherwise.
    */
    bool writeReg(uint8_t reg, uint8_t value);

    /**
    * @brief        Writes a register and reads it back to confirm the value.
    *
    * @param reg:   Register address.
    * @param value: Value to write and expect back.
    *
    * @retval       true if the write succeeded and the read-back matched.
    */
    bool writeRegVerified(uint8_t reg, uint8_t value);

    /**
    * @brief        Reads a single register.
    *
    * @param reg:   Register address.
    * @param value: Pointer to store the read value.
    *
    * @retval       true if the read succeeded. The value is stored in the provided pointer.
    */
    bool readReg(uint8_t reg, uint8_t *value);

    /**
    * @brief            Reads consecutive registers in one burst.
    *
    * @param reg:       Address of the first register.
    * @param buffer:    Pointer to store the bytes.
    * @param length:    Number of bytes to read.
    *
    * @retval           true if the read succeeded.
    */
    bool burstReadData(uint8_t reg, uint8_t *buffer, size_t length);

    // -------------------------------------------------------------------------
    // Device initialization helpers
    // -------------------------------------------------------------------------

    /**
    * @brief    Checks that the configuration is internally consistent.
    *
    * @retval   true if valid, false otherwise.
    */
    bool validateConfig() const;

    /**
    * @brief    Verifies communication with the ICM-45686.
    *
    * @details  Reads the WHO_AM_I register and compares the returned
    *           value against the expected device ID.
    *
    * @retval   true if device detected, false if device not detected.
    */
    bool verifyConnection();

    /**
    * @brief    Configures the accelerometer range and output rate.
    *
    * @retval   true if the register was written and confirmed.
    */
    bool setAccelConfig();

    /**
    * @brief    Configures the gyroscope range and output rate.
    *
    * @retval   true if the register was written and confirmed.
    */
    bool setGyroConfig();

    /**
    * @brief    Turns on the accelerometer and gyroscope.
    *
    * @details  The sensors are off after reset. This writes the
    *           power mode register for both sensors.
    *
    * @retval   true if the register was written and confirmed.
    */
    bool enableSensors();

    /**
    * @brief    Computes the raw-count to SI conversion factors.
    *
    * @details  The factors depend on the configured full-scale ranges.
    */
    void computeScaleFactors();

public:
    /**
    * @brief            Creates the driver.
    *
    * @param protocol_: Bus object (must be an SPI protocol).
    * @param config_:   Configuration. Uses ICM45686DefaultConfig when omitted.
    */
    explicit ICM45686(Protocol &protocol_,
                      const ICM45686Config &config_ = ICM45686DefaultConfig);
    
    /**
    * @brief    Initializes the ICM-45686.
    *
    * @details  Performs:
    *           - Configuration validation
    *           - Device verification
    *           - Accelerometer configuration
    *           - Gyroscope configuration
    *           - Sensor enable and start-up wait
    *
    * @retval   true if initialization successful, false if initialization failed.
    */
    bool init() override;

    /**
    * @brief            Reads one sample and writes it to the shared Packet in SI units.
    *
    * @param packet:    Packet to update. Left unchanged on failure.
    *
    * @retval           true if the read succeeded, false otherwise.
    */
    bool read(Packet &packet) override;

    /**
    * @brief            Reads one raw sample.
    *
    * @param sample:    Pointer to store the raw sample. Left unchanged on failure.
    *
    * @retval           true if the read succeeded, false otherwise.
    */
    bool readRaw(ICM45686RawSample *sample);

    // -------------------------------------------------------------------------
    // Status accessors
    // -------------------------------------------------------------------------

    bool                    isReady()               const;
    uint32_t                getBusFailureCount()    const;
    const ICM45686RawSample &lastRaw()              const;
    const ICM45686Config    &getConfig()            const;
};

} // namespace Common

#endif /* ICM45686_H */
