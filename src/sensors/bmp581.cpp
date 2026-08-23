#include "bmp581.h"

#include "hal.h"
#include "log.h"
#include "protocol.h"
#include "sensor.h"

#include "bmp5.h"
#include "bmp5_defs.h"

#include <cassert>
#include <cmath>
#include <cstdint>

#include "usbd_cdc_if.h"

constexpr auto GRAVITY_ACCEL = 9.80665f;    // m/s^2
constexpr auto AIR_MOLAR_MASS = 0.0289644f; // kg/mol
constexpr auto GAS_CONSTANT = 8.31446f;     // J/(mol*K)

constexpr auto TROPOPAUSE_PRESSURE = 22630.0f;          // Pa
constexpr auto STRATOSPHERE_MIDDLE_PRESSURE = 5475.0f;  // Pa
constexpr auto STANDARD_SEA_LEVEL_PRESSURE = 101325.0f; // Pa

constexpr auto STANDARD_SEA_LEVEL_TEMP = 288.15f; // K
constexpr auto STRATOSPHERE_BASE_TEMP = 216.65f;  // K

constexpr auto TROPOPAUSE_BASE_ALTITUDE = 11000.0f;          // m
constexpr auto STRATOSPHERE_MIDDLE_BASE_ALTITUDE = 20000.0f; // m

constexpr auto TROPOSPHERE_LAPSE_RATE = -0.0065f;      // K/m
constexpr auto UPPER_STRATOSPHERE_LAPSE_RATE = 0.001f; // K/m

namespace {
inline float calc_altitude_troposphere_msl(float pressure) {
    constexpr auto exponent = (-GAS_CONSTANT * TROPOSPHERE_LAPSE_RATE) /
                              (GRAVITY_ACCEL * AIR_MOLAR_MASS);
    float pressure_ratio = pressure / STANDARD_SEA_LEVEL_PRESSURE;
    float power_term = pow(pressure_ratio, exponent) - 1;

    return (STANDARD_SEA_LEVEL_TEMP / TROPOSPHERE_LAPSE_RATE) * power_term;
}

inline float calc_altitude_lower_stratosphere_msl(float pressure) {
    constexpr auto scale_factor = (GAS_CONSTANT * STRATOSPHERE_BASE_TEMP) /
                                  (GRAVITY_ACCEL * AIR_MOLAR_MASS);
    float log_ratio = log(pressure / TROPOPAUSE_PRESSURE);

    return TROPOPAUSE_BASE_ALTITUDE - (scale_factor * log_ratio);
}

inline float calc_altitude_upper_stratosphere_msl(float pressure) {
    constexpr auto exponent = (-GAS_CONSTANT * UPPER_STRATOSPHERE_LAPSE_RATE) /
                              (GRAVITY_ACCEL * AIR_MOLAR_MASS);
    float pressure_ratio = pressure / STRATOSPHERE_MIDDLE_PRESSURE;
    float power_term = pow(pressure_ratio, exponent) - 1;

    return STRATOSPHERE_MIDDLE_BASE_ALTITUDE +
           (STRATOSPHERE_BASE_TEMP / UPPER_STRATOSPHERE_LAPSE_RATE) *
               power_term;
}

inline float bmp581_estimate_altitude_msl(struct bmp5_sensor_data* data) {
    if (data->pressure > TROPOPAUSE_PRESSURE)
        return calc_altitude_troposphere_msl(data->pressure);
    else if (data->pressure > STRATOSPHERE_MIDDLE_PRESSURE)
        return calc_altitude_lower_stratosphere_msl(data->pressure);
    else
        return calc_altitude_upper_stratosphere_msl(data->pressure);
}

BMP5_INTF_RET_TYPE bosch_read(uint8_t reg_addr, uint8_t* reg_data,
                              uint32_t length, void* intf_ptr) {
    auto* protocol = static_cast<Platform::Protocol*>(intf_ptr);
    return protocol->read(Platform::ConstSpan(&reg_addr, 1),
                          Platform::Span(reg_data, length),
                          Platform::AddressSize::Byte);
}

BMP5_INTF_RET_TYPE bosch_write(uint8_t reg_addr, const uint8_t* reg_data,
                               uint32_t length, void* intf_ptr) {
    auto* protocol = static_cast<Platform::Protocol*>(intf_ptr);
    return protocol->write(Platform::ConstSpan(&reg_addr, 1),
                           Platform::ConstSpan(reg_data, length),
                           Platform::AddressSize::Byte);
}

void bosch_delay(uint32_t period, void* intf_ptr) {
    HAL_Delay(ceil((double)(period) / (1000.0)));
}
} // namespace

namespace Platform {
bool BMP581::init() {
    int8_t result = BMP5_OK;
    dev.intf_ptr = &api;
    dev.intf = BMP5_SPI_INTF;
    dev.read = bosch_read;
    dev.write = bosch_write;
    dev.delay_us = bosch_delay;

    odr_config.odr = BMP5_ODR_240_HZ;
    odr_config.press_en = BMP5_ENABLE;
    int_config.drdy_en = BMP5_ENABLE;
    int_config.fifo_full_en = BMP5_DISABLE;
    int_config.fifo_thres_en = BMP5_DISABLE;
    int_config.oor_press_en = BMP5_DISABLE;

    result = bmp5_soft_reset(&dev);
    log("BMP581 reset returned %d\r\n", result);
    if (result != BMP5_OK)
        return false;

    // Initialize the device
    result = bmp5_init(&dev);
    log("BMP581 init returned %d\r\n", result);
    if (result != BMP5_OK)
        return false;

    // Set odr frequency
    result = bmp5_set_osr_odr_press_config(&odr_config, &dev);
    log("BMP581 odr returned %d\r\n", result);
    if (result != BMP5_OK)
        return false;

    result = bmp5_int_source_select(&int_config, &dev);
    log("BMP581 source returned %d\r\n", result);
    if (result != BMP5_OK)
        return false;

    // Enable interrupt handler
    result =
        bmp5_configure_interrupt(BMP5_PULSED, BMP5_ACTIVE_HIGH,
                                 BMP5_INTR_PUSH_PULL, BMP5_INTR_ENABLE, &dev);
    log("BMP581 interrupt returned %d\r\n", result);
    if (result != BMP5_OK)
        return false;

    result = bmp5_set_power_mode(BMP5_POWERMODE_NORMAL, &dev);
    log("BMP581 power returned %d\r\n", result);
    if (result != BMP5_OK)
        return false;

    return true;
}

bool BMP581::read(Packet& packet) {
    struct bmp5_sensor_data data;

    bmp5_get_sensor_data(&data, &odr_config, &dev);
    packet.barometer_hMSL_m = bmp581_estimate_altitude_msl(&data);
    packet.temperature_c = data.temperature;
    packet.kf_position_m = data.pressure;
    return true;
}
} // namespace Platform
