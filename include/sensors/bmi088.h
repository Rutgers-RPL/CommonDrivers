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
	struct serial_api accel;
	struct serial_api gyro;
};
int8_t bmi088_init(struct bmi088_ctx *ctx, struct sensor *sensor,
		   struct handle* accel, struct handle* gyro);

#endif /* INC_BMI088_H_ */
