/*
 * bmi088.c
 *
 *  Created on: Oct 26, 2025
 *      Author: Dhruv Shah
 */

#include "hal.h"
#include "protocol.h"
#include "sensor.h"

#include "bmi08.h"
#include "bmi088.h"
#include "bmi08_defs.h"
#include "bmi08x.h"

#include <cassert>
#include <cmath>
#include <cstdint>

#define CONVERT_GYRO_RAW_RANGE(raw, range)                                     \
    ((((float)raw * (float)range) / 32768.0f) * (std::numbers::pi / 180.0f))

namespace {
BMI08_INTF_RET_TYPE bosch_read(uint8_t reg_addr, uint8_t* reg_data,
                               uint32_t length, void* intf_ptr) {
    auto* protocol = static_cast<Platform::Protocol*>(intf_ptr);
    return protocol->read(Platform::ConstSpan(&reg_addr, 1),
                          Platform::Span(reg_data, length),
                          Platform::AddressSize::Byte);
}

BMI08_INTF_RET_TYPE bosch_write(uint8_t reg_addr, const uint8_t* reg_data,
                                uint32_t length, void* intf_ptr) {
    auto* protocol = static_cast<Platform::Protocol*>(intf_ptr);
    return protocol->write(Platform::ConstSpan(&reg_addr, 1),
                           Platform::ConstSpan(reg_data, length),
                           Platform::AddressSize::Byte);
}

void bosch_delay(uint32_t period, void* intf_ptr) {
    HAL_Delay(ceil(static_cast<double>(period) / (1000.0)));
}

float bmi088_convert_accel_axis_data(struct bmi08_dev& dev, int16_t axis_data) {
    return (static_cast<float>(axis_data) / 32768.0f * 1000 *
            pow(2, dev.accel_cfg.range + 1) * 1.5) *
           0.00981;
}

float bmi088_convert_gyro_axis_data(struct bmi08_dev& dev, int16_t axis_data) {
    switch (dev.gyro_cfg.range) {
    case BMI08_GYRO_RANGE_2000_DPS:
        return CONVERT_GYRO_RAW_RANGE(axis_data, 2000);
    case BMI08_GYRO_RANGE_1000_DPS:
        return CONVERT_GYRO_RAW_RANGE(axis_data, 1000);
    case BMI08_GYRO_RANGE_500_DPS:
        return CONVERT_GYRO_RAW_RANGE(axis_data, 500);
    case BMI08_GYRO_RANGE_250_DPS:
        return CONVERT_GYRO_RAW_RANGE(axis_data, 250);
    case BMI08_GYRO_RANGE_125_DPS:
        return CONVERT_GYRO_RAW_RANGE(axis_data, 125);
    default:
        return 0.0f;
    }
}
} // namespace

namespace Platform {
bool BMI088::init() {
    struct bmi08_accel_int_channel_cfg accel_new_data_int_cfg;
    struct bmi08_gyro_int_channel_cfg gyro_new_data_int_cfg;

    dev.intf_ptr_accel = &accel;
    dev.intf_ptr_gyro = &gyro;
    dev.intf = BMI08_SPI_INTF;
    dev.variant = BMI088_VARIANT;
    dev.read_write_len = 8;
    dev.read = bosch_read;
    dev.write = bosch_write;
    dev.delay_us = bosch_delay;

    dev.accel_cfg.power = BMI08_ACCEL_PM_ACTIVE;
    dev.accel_cfg.range = BMI088_ACCEL_RANGE_24G;
    dev.accel_cfg.bw = BMI08_ACCEL_BW_NORMAL;
    dev.accel_cfg.odr = BMI08_ACCEL_ODR_800_HZ;

    dev.gyro_cfg.power = BMI08_GYRO_PM_NORMAL;
    dev.gyro_cfg.range = BMI08_GYRO_RANGE_2000_DPS;
    dev.gyro_cfg.bw = BMI08_GYRO_BW_116_ODR_1000_HZ;
    dev.gyro_cfg.odr = BMI08_GYRO_BW_116_ODR_1000_HZ;

    accel_new_data_int_cfg.int_channel = BMI08_INT_CHANNEL_1;
    accel_new_data_int_cfg.int_type = BMI08_ACCEL_INT_DATA_RDY;
    accel_new_data_int_cfg.int_pin_cfg.output_mode = BMI08_INT_MODE_PUSH_PULL;
    accel_new_data_int_cfg.int_pin_cfg.lvl = BMI08_INT_ACTIVE_HIGH;
    accel_new_data_int_cfg.int_pin_cfg.enable_int_pin = BMI08_ENABLE;

    gyro_new_data_int_cfg.int_channel = BMI08_INT_CHANNEL_3;
    gyro_new_data_int_cfg.int_type = BMI08_GYRO_INT_DATA_RDY;
    gyro_new_data_int_cfg.int_pin_cfg.output_mode = BMI08_INT_MODE_PUSH_PULL;
    gyro_new_data_int_cfg.int_pin_cfg.lvl = BMI08_INT_ACTIVE_HIGH;
    gyro_new_data_int_cfg.int_pin_cfg.enable_int_pin = BMI08_ENABLE;

    int8_t ret = BMI08_OK;

    ret = bmi08a_soft_reset(&dev);
    if (ret != BMI08_OK)
        return false;

    ret = bmi08xa_init(&dev);
    if (ret != BMI08_OK)
        return false;

    ret = bmi08a_load_config_file(&dev);
    if (ret != BMI08_OK)
        return false;

    ret = bmi08a_set_power_mode(&dev);
    if (ret != BMI08_OK)
        return false;

    ret = bmi08xa_set_meas_conf(&dev);
    if (ret != BMI08_OK)
        return false;

    ret = bmi08a_set_int_config(&accel_new_data_int_cfg, &dev);
    if (ret != BMI08_OK)
        return false;

    ret = bmi08g_soft_reset(&dev);
    if (ret != BMI08_OK)
        return false;

    ret = bmi08g_init(&dev);
    if (ret != BMI08_OK)
        return false;

    ret = bmi08g_set_power_mode(&dev);
    if (ret != BMI08_OK)
        return false;

    ret = bmi08g_set_meas_conf(&dev);
    if (ret != BMI08_OK)
        return false;

    ret = bmi08g_set_int_config(&gyro_new_data_int_cfg, &dev);
    if (ret != BMI08_OK)
        return false;

    return true;
}

bool BMI088::read(Packet& packet) {
    struct bmi08_sensor_data gyro_data;
    struct bmi08_sensor_data accel_data;

    bmi08a_get_data(&accel_data, &dev);
    bmi08g_get_data(&gyro_data, &dev);

    packet.acceleration_x_mss =
        bmi088_convert_accel_axis_data(dev, accel_data.x);
    packet.acceleration_y_mss =
        bmi088_convert_accel_axis_data(dev, accel_data.y);
    packet.acceleration_z_mss =
        bmi088_convert_accel_axis_data(dev, accel_data.z);

    packet.angular_velocity_x_rads =
        bmi088_convert_gyro_axis_data(dev, gyro_data.x);
    packet.angular_velocity_y_rads =
        bmi088_convert_gyro_axis_data(dev, gyro_data.y);
    packet.angular_velocity_z_rads =
        bmi088_convert_gyro_axis_data(dev, gyro_data.z);
    return true;
}
} // namespace Platform
