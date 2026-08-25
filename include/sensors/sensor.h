#ifndef SENSORS_SENSOR_H
#define SENSORS_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

namespace Common {
struct __attribute__((packed)) Packet {
    int16_t magic = 0xBEEF;      // 2 bytes
    uint32_t status = 0;         // 4 bytes
    uint32_t time_us = 0;        // 4 bytes
    float main_voltage_v = 0.0f; // 4 bytes
    float pyro_voltage_v = 0.0f; // 4 bytes
    uint8_t numSatellites = 0;   // 1 byte
    uint8_t gpsFixType = 0; // 1 byte    0 = no fix, 1 = fix, 2 = DGPS fix, etc.
    float latitude_degrees = 0.0f;   // 4 bytes
    float longitude_degrees = 0.0f;  // 4 bytes
    float gps_hMSL_m = 0.0f;         // 4 bytes   altitude above mean sea level
    float barometer_hMSL_m = 0.0f;   // 4 bytes
    float temperature_c = 0.0f;      // 4 bytes
    float acceleration_x_mss = 0.0f; // 4 bytes
    float acceleration_y_mss = 0.0f; // 4 bytes
    float acceleration_z_mss = 0.0f; // 4 bytes
    float angular_velocity_x_rads = 0.0f; // 4 bytes
    float angular_velocity_y_rads = 0.0f; // 4 bytes
    float angular_velocity_z_rads = 0.0f; // 4 bytes
    float gauss_x = 0.0f;                 // 4 bytes
    float gauss_y = 0.0f;                 // 4 bytes
    float gauss_z = 0.0f;                 // 4 bytes
    float kf_acceleration_mss = 0.0f;     // 4 bytes
    float kf_velocity_ms = 0.0f;          // 4 bytes
    float kf_position_m = 0.0f;           // 4 bytes
    float w = 0.0f;                       // 4 bytes
    float x = 0.0f;                       // 4 bytes
    float y = 0.0f;                       // 4 bytes
    float z = 0.0f;                       // 4 bytes
    uint32_t checksum = 0;                // 4 bytes
    Packet() = default;
};

/// Sensor ---
/// To implement a sensor, you must implement a `read` method which updates
/// the packet in place after it reads from the hardware.
/// You must also implement an init method, which allows for retrying.
///
/// Both methods must return `true` on success.
class Sensor {
protected:
public:
    virtual ~Sensor() = default;
    virtual bool read(Packet& packet) = 0;
    virtual bool init() = 0;
};
} // namespace Common

#endif // SENSORS_SENSOR_H
