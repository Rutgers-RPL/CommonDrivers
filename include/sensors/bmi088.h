/*
 * bmi088.h
 *
 *  Created on: Oct 26, 2025
 *      Author: Dhruv Shah
 */

#ifndef INC_BMI088_H_
#define INC_BMI088_H_

#include "bmi08x.h"
#include "bmi08_defs.h"

#include <stdint.h>
#include <math.h>

// Forward declaration
typedef struct SPI_HandleTypeDef SPI_HandleTypeDef;
typedef struct sensor sensor;

struct bmi088_sensor_intf {
	uint8_t gpio_port;
	uint8_t gpio_pin;
	SPI_HandleTypeDef* spi_handle;
};

struct bmi088_context {
	struct bmi08_dev dev;
	struct bmi088_sensor_intf accel_intf;
	struct bmi088_sensor_intf gyro_intf;
};
int8_t bmi088_init(struct bmi088_context *bmi, struct sensor *sensor);

#endif /* INC_BMI088_H_ */
