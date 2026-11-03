#ifndef SENSORS_MS5611_H
#define SENSORS_MS5611_H

#include "protocol.h"
#include "sensor.h"

// Command to reset the MS5611 sensor
#define MS5611_RESET 0x1E

// Command to read the factory-calibrated values from the PROM
#define MS5611_PROM_READ_BASE 0xA0

// Command to start pressure conversion with OSR=4096
#define MS5611_CONVERT_D1_4096 0x48

// Command to start temperature conversion with OSR=4096
#define MS5611_CONVERT_D2_4096 0x58

// Command to read the ADC result
#define MS5611_ADC_READ 0x00

namespace Common {

class MS5611 final : public Sensor {
private:
    Protocol& api;    // Refrence variable to communicate with the SPI/I2C
    uint16_t PROM[8]; // Variable-array of size 8 unsigned 16-bit integers for
                      // storing factory-calibrated values(C1-C6)

public:
    // Constructor initializes the driver by taking a reference to the protocol
    // bus
    explicit MS5611(Protocol& api_) : api(api_) {}
    // Initializes the sensor by resetting it and loading the factory-calibrated
    // values.
    bool init() override; // Resets and loads all the factory-calibrated values

    bool read(Packet& packet)
        override; // Reads the 24-bit ADC values of pressure and temperature
};

}; // namespace Common

#endif // SENSORS_MS5611_H //END of include guard
