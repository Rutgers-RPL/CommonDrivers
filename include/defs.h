#ifndef DEFS_H
#define DEFS_H

#ifdef TEST
// Mark functions you want to unit test with STATIC. We expose everything
// through `struct sensor` and `struct flash` to avoid leaking implmentation.
//
// Of course we still need to test the read function so we use the STATIC hack
// It is static in production, but non static when running tests.
#define STATIC
// Stub HAL file to satisfy compilation during tests, eventually we might write
// and use a thin abstraction on top of the HAL.
#include "stub_hal.h"
#else
// Make static work as usual
#define STATIC static
// TODO: Abstracts over hal series
#ifdef USE_STM32_H7XX
#include "stm32h7xx_hal.h"
#else
#include "stm32f4xx_hal.h"
#endif
#endif // end TEST

/// Common abstraction over SPI, UART, I2C
/// Use this handle struct when a sensor could be configured to use more than of
/// the protocols or if the sensor uses a protocol that might be disabled, like
/// UART or I2C. This helps isolate ifdefs to only implementation files. 
enum protocol { SPI, UART, I2C };
struct handle {
	enum protocol protocol;
	union {
#ifdef HAL_I2C_MODULE_ENABLED
		struct handle_i2c {
			I2C_HandleTypeDef *handle;
			uint32_t address;
		} i2c;
#endif
		// We always have spi present, so we don't have to gate it
		struct handle_spi {
			SPI_HandleTypeDef *handle;
			GPIO_TypeDef *port;
			uint8_t pin;
		} spi;
#ifdef HAL_UART_MODULE_ENABLED
		struct handle_uart {
			UART_HandleTypeDef *handle;
		} uart;
#endif
	} def;
};


#endif // end DEFS_H
