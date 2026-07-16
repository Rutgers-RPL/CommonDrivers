/*
 * bmi088.c
 *
 *  Created on: Oct 26, 2025
 *      Author: Dhruv Shah
 */

#include "sensor.h"
#include "defs.h"

#include "bmi08_defs.h"
#include "bmi088.h"
#include "bmi08x.h"
#include "bmi08.h"

#include <stdint.h>
#include <assert.h>
#include <math.h>

#define CONVERT_GYRO_RAW_RANGE(raw, range) ((((float)raw * (float)range) / 32768.0f) * (M_PI / 180.0f))

static BMI08_INTF_RET_TYPE bmi088_read_spi(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) // GCOVR_EXCL_FUNCTION
{
	struct handle_spi* spi = (struct handle_spi*) intf_ptr;

	HAL_GPIO_WritePin(spi->port, spi->pin, GPIO_PIN_RESET);

	HAL_StatusTypeDef ret = HAL_OK;

	ret = HAL_SPI_Transmit(spi->handle, &reg_addr, 1, HAL_MAX_DELAY);

	if (ret != HAL_OK) {
	    HAL_GPIO_WritePin(spi->port, spi->pin, GPIO_PIN_SET);
		return ret;
	}

	ret = HAL_SPI_Receive(spi->handle, reg_data, len, HAL_MAX_DELAY);

	if (ret != HAL_OK) {
	    HAL_GPIO_WritePin(spi->port, spi->pin, GPIO_PIN_SET);
		return ret;
	}

	HAL_GPIO_WritePin(spi->port, spi->pin, GPIO_PIN_SET);

	return 0;
}

static BMI08_INTF_RET_TYPE bmi088_write_spi(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) // GCOVR_EXCL_FUNCTION
{
	struct handle_spi* spi = (struct handle_spi*) intf_ptr;

	HAL_GPIO_WritePin(spi->port, spi->pin, GPIO_PIN_RESET);

	HAL_StatusTypeDef ret = HAL_OK;

	ret = HAL_SPI_Transmit(spi->handle, &reg_addr, 1, HAL_MAX_DELAY);

	if (ret != HAL_OK) {
		HAL_GPIO_WritePin(spi->port, spi->pin, GPIO_PIN_SET);
		return ret;
	}

	ret = HAL_SPI_Transmit(spi->handle, reg_data, len, HAL_MAX_DELAY);

	if (ret != HAL_OK) {
		HAL_GPIO_WritePin(spi->port, spi->pin, GPIO_PIN_SET);
		return ret;
	}

	HAL_GPIO_WritePin(spi->port, spi->pin, GPIO_PIN_SET);

	return 0;
}

static void bmi088_delay_us(uint32_t period, void *intf_ptr) // GCOVR_EXCL_FUNCTION
{
	HAL_Delay(ceil((double)(period)/(1000.0)));
}

static float bmi088_convert_accel_axis_data(struct bmi088_ctx *ctx, int16_t axis_data)
{
	return ((float)axis_data / 32768.0f * 1000 * pow(2, ctx->dev.accel_cfg.range + 1) * 1.5) * 0.00981;
}

static float bmi088_convert_gyro_axis_data(struct bmi088_ctx *ctx, int16_t axis_data)
{
	switch (ctx->dev.gyro_cfg.range) {
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

STATIC bool bmi088_read(void *context, struct packet *packet)
{
	struct bmi088_ctx *ctx = (struct bmi088_ctx*) context;
	struct bmi08_sensor_data gyro_data;
	struct bmi08_sensor_data accel_data;

	bmi08a_get_data(&accel_data, &ctx->dev);
	bmi08g_get_data(&gyro_data, &ctx->dev);

	packet->acceleration_x_mss = bmi088_convert_accel_axis_data(ctx, accel_data.x);
	packet->acceleration_y_mss = bmi088_convert_accel_axis_data(ctx, accel_data.y);
	packet->acceleration_z_mss = bmi088_convert_accel_axis_data(ctx, accel_data.z);

	packet->angular_velocity_x_rads = bmi088_convert_gyro_axis_data(ctx, gyro_data.x);
	packet->angular_velocity_y_rads = bmi088_convert_gyro_axis_data(ctx, gyro_data.y);
	packet->angular_velocity_z_rads = bmi088_convert_gyro_axis_data(ctx, gyro_data.z);
	return true;
}

int8_t bmi088_init(struct bmi088_ctx *ctx, struct sensor *sensor) // GCOVR_EXCL_FUNCTION
{
	assert(ctx->accel_spi.handle != NULL);
	assert(ctx->gyro_spi.handle != NULL);

	HAL_GPIO_WritePin(ctx->accel_spi.port, ctx->accel_spi.pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(ctx->gyro_spi.port, ctx->gyro_spi.pin, GPIO_PIN_SET);

	struct bmi08_accel_int_channel_cfg accel_new_data_int_cfg;
	struct bmi08_gyro_int_channel_cfg gyro_new_data_int_cfg;

	ctx->dev.intf_ptr_accel = &ctx->accel_spi;
	ctx->dev.intf_ptr_gyro = &ctx->gyro_spi;
	ctx->dev.intf = BMI08_SPI_INTF;
	ctx->dev.variant = BMI088_VARIANT;
	ctx->dev.read_write_len = 8;
	ctx->dev.read = bmi088_read_spi;
	ctx->dev.write = bmi088_write_spi;
	ctx->dev.delay_us = bmi088_delay_us;

	ctx->dev.accel_cfg.power = BMI08_ACCEL_PM_ACTIVE;
	ctx->dev.accel_cfg.range = BMI088_ACCEL_RANGE_24G;
	ctx->dev.accel_cfg.bw = BMI08_ACCEL_BW_NORMAL;
	ctx->dev.accel_cfg.odr = BMI08_ACCEL_ODR_800_HZ;

	ctx->dev.gyro_cfg.power = BMI08_GYRO_PM_NORMAL;
	ctx->dev.gyro_cfg.range = BMI08_GYRO_RANGE_2000_DPS;
	ctx->dev.gyro_cfg.bw = BMI08_GYRO_BW_116_ODR_1000_HZ;
	ctx->dev.gyro_cfg.odr = BMI08_GYRO_BW_116_ODR_1000_HZ;

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

	ret = bmi08a_soft_reset(&ctx->dev);

	if (ret != BMI08_OK) {
		return ret;
	}

	ret = bmi08xa_init(&ctx->dev);

	if (ret != BMI08_OK) {
		return ret;
	}

	ret = bmi08a_load_config_file(&ctx->dev);

	if (ret != BMI08_OK) {
		return ret;
	}

	ret = bmi08a_set_power_mode(&ctx->dev);

	if (ret != BMI08_OK) {
		return ret;
	}

	ret = bmi08xa_set_meas_conf(&ctx->dev);

	if (ret != BMI08_OK) {
		return ret;
	}

	ret = bmi08a_set_int_config(&accel_new_data_int_cfg, &ctx->dev);

	if (ret != BMI08_OK) {
		return ret;
	}

	ret = bmi08g_soft_reset(&ctx->dev);

	if (ret != BMI08_OK) {
		return ret;
	}

	ret = bmi08g_init(&ctx->dev);

	if (ret != BMI08_OK) {
		return ret;
	}

	ret = bmi08g_set_power_mode(&ctx->dev);

	if (ret != BMI08_OK) {
		return ret;
	}

	ret = bmi08g_set_meas_conf(&ctx->dev);

	if (ret != BMI08_OK) {
		return ret;
	}

	ret = bmi08g_set_int_config(&gyro_new_data_int_cfg, &ctx->dev);

	if (ret != BMI08_OK) {
		return ret;
	}
	sensor->ctx = ctx;
	sensor->read = bmi088_read;

	return BMI08_OK;
}
