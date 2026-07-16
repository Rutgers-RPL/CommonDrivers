/*
 * bmi088.h
 *
 *  Created on: Oct 26, 2025
 *      Author: Dhruv Shah
 */

#ifndef INC_BMI088_H_
#define INC_BMI088_H_

#include "defs.h"
#include "sensor.h"
#include "bmi08_defs.h"

#include <stdint.h>

struct bmi088_ctx {
	struct bmi08_dev dev;
	struct handle_spi accel_spi;
	struct handle_spi gyro_spi;
};
int8_t bmi088_init(struct bmi088_ctx *ctx, struct sensor *sensor);

#endif /* INC_BMI088_H_ */
