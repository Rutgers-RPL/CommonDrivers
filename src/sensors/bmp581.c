#include "bmp581.h"

#include "sensor.h"
#include "defs.h"

#include "bmp5.h"
#include "bmp5_defs.h"

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

static inline int8_t bmp581_get_power_mode(struct bmp581_ctx *ctx, enum bmp5_powermode *powermode) // GCOVR_EXCL_FUNCTION
{
    return bmp5_get_power_mode(powermode, &(ctx->dev));
}

static BMP5_INTF_RET_TYPE read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr) // GCOVR_EXCL_FUNCTION
{
    struct serial_api *api = (struct serial_api*) intf_ptr;
    struct op_params params = { .cmd = &reg_addr, .cmd_size = 1,
                                .buffer = reg_data, .buffer_size = length };
    return api->read(api->handle, &params);
}

static BMP5_INTF_RET_TYPE write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr) // GCOVR_EXCL_FUNCTION
{
    struct serial_api *api = (struct serial_api*) intf_ptr;
    struct op_params params = { .cmd = &reg_addr, .cmd_size = 1,
                                .buffer = reg_data, .buffer_size = length };
    return api->write(api->handle, &params);
}

static void delay(uint32_t period, void *intf_ptr) // GCOVR_EXCL_FUNCTION
{
    HAL_Delay(ceil((double)(period)/(1000.0)));
}

STATIC bool bmp581_read(void *context, struct packet *packet)
{
    struct bmp581_ctx *ctx = (struct bmp581_ctx*) context;
    struct bmp5_sensor_data data;

    bmp5_get_sensor_data(&data, &(ctx->odr_config), &(ctx->dev));

    packet->barometer_hMSL_m = bmp581_estimate_altitude_msl(&data);
    packet->temperature_c = data.temperature;
    packet->kf_position_m = data.pressure;
    return true;
}

int8_t bmp581_init(struct bmp581_ctx *ctx, struct sensor *sensor, struct handle *handle) // GCOVR_EXCL_FUNCTION
{
    int8_t result = BMP5_OK;

    sensor->ctx = ctx;
    sensor->read = bmp581_read;

    switch (handle->protocol) {
    case SPI:
        ctx->dev.intf = BMP5_SPI_INTF;
        serial_api_spi(&ctx->api, handle);
        break;
    case I2C:
        ctx->dev.intf = BMP5_I2C_INTF;
        serial_api_i2c(&ctx->api, handle);
        break;
    default:
        assert("Invalid interface for bmp581, must choose either SPI or I2C");
    };

    ctx->dev.intf_ptr = &ctx->api;
    ctx->dev.read = read;
    ctx->dev.write = write;
    ctx->dev.delay_us = delay;
    ctx->odr_config.odr = BMP5_ODR_240_HZ;
    ctx->odr_config.press_en = BMP5_ENABLE;
    ctx->int_config.drdy_en = BMP5_ENABLE;
    ctx->int_config.fifo_full_en = BMP5_DISABLE;
    ctx->int_config.fifo_thres_en = BMP5_DISABLE;
    ctx->int_config.oor_press_en = BMP5_DISABLE;

    bmp5_soft_reset(&(ctx->dev));

    // Initialize the device
    result = bmp5_init(&(ctx->dev));
    if (result != BMP5_OK) {
	return result;
    }

    // Set odr frequency
    result = bmp5_set_osr_odr_press_config(&(ctx->odr_config), &(ctx->dev));
    if (result != BMP5_OK) {
	return result;
    }

    result = bmp5_int_source_select(&(ctx->int_config), &(ctx->dev));
    if (result != BMP5_OK) {
	return result;
    }
    // Enable interrupt handler
    result = bmp5_configure_interrupt(BMP5_PULSED, BMP5_ACTIVE_HIGH, BMP5_INTR_PUSH_PULL, BMP5_INTR_ENABLE, &(ctx->dev));
    if (result != BMP5_OK) {
	return result;
    }

    result = bmp5_set_power_mode(BMP5_POWERMODE_NORMAL, &(ctx->dev));
    return result;
}
