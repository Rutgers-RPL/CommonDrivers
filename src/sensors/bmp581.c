#include "bmp581.h"

#include "sensor.h"

#include "bmp5.h"
#include "bmp5_defs.h"
#include "stm32xxxx_hal.h"

#include <assert.h>
#include <stdint.h>
#include <math.h>

#define GRAVITY_ACCEL 9.80665f // m/s^2
#define AIR_MOLAR_MASS 0.0289644f // kg/mol
#define GAS_CONSTANT 8.31446f // J/(mol*K)

#define TROPOPAUSE_PRESSURE 22630.0f // Pa
#define STRATOSPHERE_MIDDLE_PRESSURE 5475.0f // Pa
#define STANDARD_SEA_LEVEL_PRESSURE 101325.0f // Pa

#define STANDARD_SEA_LEVEL_TEMP 288.15f // K
#define STRATOSPHERE_BASE_TEMP 216.65f // K

#define TROPOPAUSE_BASE_ALTITUDE 11000.0f // m
#define STRATOSPHERE_MIDDLE_BASE_ALTITUDE 20000.0f // m

#define TROPOSPHERE_LAPSE_RATE -0.0065f // K/m
#define UPPER_STRATOSPHERE_LAPSE_RATE 0.001f // K/m

static inline float calc_altitude_troposphere_msl(float pressure)
{
    float exponent = (-GAS_CONSTANT * TROPOSPHERE_LAPSE_RATE) / (GRAVITY_ACCEL * AIR_MOLAR_MASS);
    float pressure_ratio = pressure / STANDARD_SEA_LEVEL_PRESSURE;
    float power_term = pow(pressure_ratio, exponent) - 1;

    return (STANDARD_SEA_LEVEL_TEMP / TROPOSPHERE_LAPSE_RATE) * power_term;
}

static inline float calc_altitude_lower_stratosphere_msl(float pressure)
{
    float log_ratio = log(pressure / TROPOPAUSE_PRESSURE);
    float scale_factor = (GAS_CONSTANT * STRATOSPHERE_BASE_TEMP) / (GRAVITY_ACCEL * AIR_MOLAR_MASS);

    return TROPOPAUSE_BASE_ALTITUDE - (scale_factor * log_ratio);
}

static inline float calc_altitude_upper_stratosphere_msl(float pressure)
{
    float exponent = (-GAS_CONSTANT * UPPER_STRATOSPHERE_LAPSE_RATE) / (GRAVITY_ACCEL * AIR_MOLAR_MASS);
    float pressure_ratio = pressure / STRATOSPHERE_MIDDLE_PRESSURE;
    float power_term = pow(pressure_ratio, exponent) - 1;

    return STRATOSPHERE_MIDDLE_BASE_ALTITUDE + (STRATOSPHERE_BASE_TEMP / UPPER_STRATOSPHERE_LAPSE_RATE) * power_term;
}

static inline float bmp581_estimate_altitude_msl(struct bmp5_sensor_data *data)
{
    if (data->pressure > TROPOPAUSE_PRESSURE) {
        return calc_altitude_troposphere_msl(data->pressure);
    } else if (data->pressure > STRATOSPHERE_MIDDLE_PRESSURE) {
        return calc_altitude_lower_stratosphere_msl(data->pressure);
    } else {
        return calc_altitude_upper_stratosphere_msl(data->pressure);
    }
}

static BMP5_INTF_RET_TYPE read_i2c(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    I2C_HandleTypeDef *handle = (I2C_HandleTypeDef*) intf_ptr;
    HAL_StatusTypeDef read_ret = HAL_I2C_Mem_Read(handle, BMP5_I2C_ADDR_PRIM << 1, reg_addr, I2C_MEMADD_SIZE_8BIT, reg_data, length, HAL_MAX_DELAY);
    uint32_t error = HAL_I2C_GetError(handle);
    return read_ret;
}

static BMP5_INTF_RET_TYPE write_i2c(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    I2C_HandleTypeDef *handle = (I2C_HandleTypeDef*) intf_ptr;
    HAL_StatusTypeDef write_ret = HAL_I2C_Mem_Write(handle, BMP5_I2C_ADDR_PRIM << 1, reg_addr, I2C_MEMADD_SIZE_8BIT, reg_data, length, HAL_MAX_DELAY);
    return write_ret;
}

static void delay(uint32_t period, void *intf_ptr) {
    HAL_Delay(ceil((double)(period)/(1000.0)));
}

static bool bmp581_read(sensor_context *context, packet *packet)
{
    struct bmp581_context context = (bmp581_context*) context;
    struct bmp5_sensor_data data;

    bmp5_get_sensor_data(data, &(context->odr_config), &(context->device));

    packet->barometer_hMSL_m = bmp581_estimate_altitude_msl(&data);
    packet->temperature_c = data.temperature;
    packet.kf_position_m = data.pressure;
    return true;
}

int8_t bmp581_init(struct sensor *sensor, struct bmp581_context *context);
{
    assert(context->i2c != NULL);
    int8_t result = BMP5_OK;

    context->device.intf = BMP5_I2C_INTF;
    context->device.read = read_i2c;
    context->device.write = write_i2c;
    context->device.intf_ptr = handle;
    context->device.delay_us = delay;
    context->odr_config.odr = BMP5_ODR_240_HZ;
    context->odr_config.press_en = BMP5_ENABLE;
    context->int_config.drdy_en = BMP5_ENABLE;
    context->int_config.fifo_full_en = BMP5_DISABLE;
    context->int_config.fifo_thres_en = BMP5_DISABLE;
    context->int_config.oor_press_en = BMP5_DISABLE;

    sensor->context = context;
    sensor->read = bmp581_read;

    bmp5_soft_reset(&(context->device));

    // Initialize the device
    result = bmp5_init(&(context->device));
    if (result != BMP5_OK) {
	return result;
    }

    // Set odr frequency
    result = bmp5_set_osr_odr_press_config(&(context->odr_config), &(context->device));
    if (result != BMP5_OK) {
	return result;
    }

    result = bmp5_int_source_select(&(context->int_config), &(context->device));
    if (result != BMP5_OK) {
	return result;
    }
    // Enable interrupt handler
    result = bmp5_configure_interrupt(BMP5_PULSED, BMP5_ACTIVE_HIGH, BMP5_INTR_PUSH_PULL, BMP5_INTR_ENABLE, &(context->device));
    if (result != BMP5_OK) {
	return result;
    }

    result = bmp5_set_power_mode(BMP5_POWERMODE_NORMAL, &(context->device));
    return result;
}

int8_t bmp581_get_power_mode(struct bmp581_context *context, enum bmp5_powermode *powermode)
{
    return bmp5_get_power_mode(powermode, &(context->device));
}
