/*
 * bmi088.h
 *
 *  Created on: Oct 26, 2025
 *      Author: Dhruv Shah
 */

#ifndef INC_BMI088_H_
#define INC_BMI088_H_

#include "bmi08x.h"
#include "stm32h7xx_hal.h"
#include "stm32h743xx.h"
#include "bmi08_defs.h"

#include <stdint.h>
#include <math.h>

#define CONVERT_GYRO_RAW_RANGE(raw, range) ((((float)raw * (float)range) / 32768.0f) * (M_PI / 180.0f))

#define BMI088_GYRO_INT_PIN GPIO_PIN_0
#define BMI088_GYRO_CS_PIN GPIO_PIN_1
#define BMI088_ACCEL_INT_PIN GPIO_PIN_2
#define BMI088_ACCEL_CS_PIN GPIO_PIN_3
#define BMI088_GPIO_PORT GPIOA

struct bmi088_sensor_intf {
	uint8_t gpio_pin;
	SPI_HandleTypeDef* spi_handle;
};

struct BMI088 {
	struct bmi08_dev dev;
	struct bmi088_sensor_intf accel_intf;
	struct bmi088_sensor_intf gyro_intf;
};

BMI08_INTF_RET_TYPE bmi088_read_spi(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
BMI08_INTF_RET_TYPE bmi088_write_spi(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);
void bmi088_delay(uint32_t period, void *intf_ptr);
int8_t bmi088_init(struct BMI088* bmi, SPI_HandleTypeDef* spi_handle);
int8_t bmi088_update_accel_data(struct BMI088* bmi, struct bmi08_sensor_data* accel_data);
int8_t bmi088_update_gyro_data(struct BMI088* bmi, struct bmi08_sensor_data* gyro_data);
float bmi088_convert_accel_axis_data(struct BMI088* bmi, int16_t axis_data);
float bmi088_convert_gyro_axis_data(struct BMI088* bmi, int16_t axis_data);

#endif /* INC_BMI088_H_ */
