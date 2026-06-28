#ifndef BMP581_H
#define BMP581_H

#include "bmp5_defs.h"

#include <stdint.h>

// Forward declarations
typedef struct I2C_HandleTypeDef I2C_HandleTypeDef;
typedef struct sensor sensor;

struct bmp581_context {
	struct bmp5_dev device;
	struct bmp5_osr_odr_press_config odr_config;
	struct bmp5_int_source_select int_config;
	I2C_HandleTypeDef *i2c;
};
int8_t bmp581_init(struct bmp581_context *context, struct sensor *sensor);
int8_t bmp581_get_power_mode(struct bmp581_context *context, enum bmp5_powermode *powermode);

#endif
