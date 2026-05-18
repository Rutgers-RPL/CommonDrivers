#ifndef BMP581_H
#define BMP581_H

#include "bmp5_defs.h"
#include "stm32h7xx_hal.h"

#include <stdint.h>

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

struct BMP581 {
	struct bmp5_dev device;
	struct bmp5_osr_odr_press_config odr_config;
	struct bmp5_int_source_select int_config;
	I2C_HandleTypeDef *hi2c;
};

BMP5_INTF_RET_TYPE read_i2c(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);
BMP5_INTF_RET_TYPE write_i2c(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);
int8_t bmp581_init(struct BMP581 *bmp581, I2C_HandleTypeDef *handle);
int8_t bmp581_update_data(struct BMP581 *bmp581, struct bmp5_sensor_data *data);
int8_t bmp581_get_power_mode(struct BMP581 *bmp581, enum bmp5_powermode *powermode);
float bmp581_estimate_altitude_msl(struct BMP581 *bmp581, struct bmp5_sensor_data *data);

#endif
