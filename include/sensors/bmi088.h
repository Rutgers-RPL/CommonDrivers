/*
 * bmi088.h
 *
 *  Created on: Oct 26, 2025
 *      Author: Dhruv Shah
 */

#ifndef INC_BMI088_H_
#define INC_BMI088_H_

#include "sensor.h"
#include "defs.h"

#include "bmi08_defs.h"

#include <stdint.h>

struct bmi088_sensor_intf {
	GPIO_TypeDef *gpio_port;
	uint8_t gpio_pin;
	SPI_HandleTypeDef* spi_handle;
};

struct bmi088_ctx {
	struct bmi08_dev dev;
	struct bmi088_sensor_intf accel_intf;
	struct bmi088_sensor_intf gyro_intf;
};
int8_t bmi088_init(struct bmi088_ctx *ctx, struct sensor *sensor);

#endif /* INC_BMI088_H_ */
