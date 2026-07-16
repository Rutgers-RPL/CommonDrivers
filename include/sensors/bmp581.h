#ifndef BMP581_H
#define BMP581_H

#include "defs.h"
#include "sensor.h"
#include "bmp5_defs.h"

#include <stdint.h>

struct bmp581_ctx {
	struct bmp5_dev dev;
	struct bmp5_osr_odr_press_config odr_config;
	struct bmp5_int_source_select int_config;
	struct handle handle;
};

int8_t bmp581_init(struct bmp581_ctx *ctx, struct sensor *sensor);
int8_t bmp581_get_power_mode(struct bmp581_ctx *ctx, enum bmp5_powermode *powermode);

#endif
