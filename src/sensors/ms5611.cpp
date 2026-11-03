#include "sensors/ms5611.h"
#include <cmath> // For powf function

bool Common::MS5611::init() {
    uint8_t cmdReset = MS5611_RESET;

    if (!api.write(Common::ConstSpan(&cmdReset, 1), Common::ConstSpan(),
                   Common::AddressSize::None)) {
        return false;
    }

    // Read the PROM values
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t promCmd = MS5611_PROM_READ_BASE + (i + 1) * 2;
        uint8_t buffer[2] = {0}; // Temporary 2-byte array initialized to
                                 // zeros to store raw high and low bytes.

        // Send the PROM read command and read the 2-byte response. The !
        // check if even one of them is false, it will return false
        if (!api.write(Common::ConstSpan(&promCmd, 1), Common::ConstSpan(),
                       Common::AddressSize::None) ||
            !api.read(Common::ConstSpan(&promCmd, 1), Common::ConstSpan(),
                      Common::AddressSize::None) ||
            !api.read(Common::ConstSpan(&promCmd, 1),
                      Common::ConstSpan(buffer, 2),
                      Common::AddressSize::None)) {
            return false;
        }

        // Combine the 2 bytes into a single 16-bit number and store it in
        // the PROM array.
        PROM[i + 1] = ((uint16_t)buffer[0] << 8) | buffer[1];
    }
    return true; // Successfully initialized and calibrated the sensor
}

// Command to start pressure conversion with OSR=4096
int Common::MS5611::read() {
    // Command to start temperature conversion with OSR=4096
    uint8_t convd1Cmd = MS5611_CONVERT_D1_4096; //
    if (!api.write(Common::ConstSpan(&convd1Cmd, 1), Common::ConstSpan(),
                   Common::AddressSize::None)) {
        return false;
    }

    // Command to read the ADC result
    uint8_t adcCmd = MS5611_ADC_READ;
    uint8_t d1buffer[3] = {0}; // Temporary 3-byte array initialized to zeros to
                               // store raw ADC values.

    if (!api.write(Common::ConstSpan(&adcCmd, 1), Common::ConstSpan(),
                   Common::AddressSize::None) ||
        !api.read(Common::ConstSpan(&adcCmd, 1), Common::ConstSpan(d1buffer, 3),
                  Common::AddressSize::None)) {
        return false;
    }

    // Switch 3 bytes into a single 32-bit integer:
    // Byte 0 gets shifted to left by 16 bits
    // Byte 1 gets shifted to left by 8 bits
    // Byte remains in it's original position
    uint32_t d1 = ((uint32_t)d1buffer[0] << 16) | ((uint32_t)d1buffer[1] << 8) |
                  ((d1buffer[2]));

    uint8_t convd2Cmd = MS5611_CONVERT_D2_4096;
    if (!api.write(Common::ConstSpan(&convd2Cmd, 1), Common::ConstSpan(),
                   Common::AddressSize::None) ||
        !api.read(Common::ConstSpan(&adcCmd, 1), Common::ConstSpan(d2buffer, 3),
                  Common::AddressSize::None)) {
        return false;
    }

    adcCmd = MS5611_ADC_READ; // Temporary 3-byte array initialized to zeros to
                              //  store raw ADC values.
    uint8_t d2buffer[3] = {0};

    if (!api.write(Common::ConstSpan(&adcCmd, 1), Common::ConstSpan(),
                   Common::AddressSize::None) ||
        !api.read(Common::ConstSpan(&adcCmd, 1), Common::ConstSpan(d2buffer, 3),
                  Common::AddressSize::None)) {
        return false;
    }

    // Switch 3 bytes into a single 32-bit integer:
    // Byte 0 gets shifted to left by 16 bits
    // Byte 1 gets shifted to left by 8 bits
    // Byte 2 remains in it's original position
    uint32_t d2 = ((uint32_t)d2buffer[0] << 16) | ((uint32_t)d2buffer[1] << 8) |
                  ((uint32_t)d2buffer[2]);
}

// Calculating temeperature difference

// Raw temp reading is 24-bit ADC read and C5 is only a 16-bit number for
// mathcing their scales PROM[5] is bit sifted to left by 8 times
int32_t dt = (int32_t)d2 - ((int32_t)PROM[5] << 8);

// Actual Temperature calculation
// It requires a 64-bit integer container to hold the resulting value because
// the input values for calcualtion are big(to avoid overflow memmory) It is
// scaled down by bit shifting 23 to the reduce the scale of the inflated value
// that comes from * with C6
int32_t temp = 2000 + ((int64_t)dt * PROM[6] >> 23);

// Calculating sensor's baseline pressure offset adjusted for Temp drift(dT)
//
int64_t offset = ((int64_t)PROM[2] << 16) + ((int64_t)PROM[4] * dt >> 7);

// Caculates sensor baseline senstivity adjusted for temp drift (dT)
int64_t sens = ((int64_t)PROM[1] << 15) + ((int64_t)PROM[3] * dt >> 8);

// Final temperature compensated pressure
int32_t pressure = ((d1 * sens) >> 21) - offset >> 15;

// Packet Structure

// Convert and assign temp to celsius in the packet structure
Packet.temperature_c = (float)temp / 100.0f;

// Convert raw pressure(Pascals) to Altitude Above Mean sea Leveel (meters)
//  Using the ISA barometric formula: h = 44330 * (1 - (P / P0)^(1/5.255))
//  p0 = 101325 Pa (Standard atmospheric pressure at sea level)
float p_float = (float)pressure;
Packet.barometer_hMSL_m =
    44330.0f * (1.0f - powf(p_float / 101325.0f, 0.1903f));

return true; // Successfully read and processed the sensor data
} // namespace Common
