#ifndef DEFS_H
#define DEFS_H

#include <stddef.h>
#include <stdbool.h>
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
#elif USE_STM32_L4XX
#include "stm32l4xx_hal.h"
#else
#include "stm32f4xx_hal.h"
#endif
#endif // end TEST

//// | Handle Abstractions |
//// We provide a thin abstraction over the built-in STM32 handles for two
//// main reasons
////   1. compile time errors, not all projects will enable all modules
////   2. group required data together
//// For example, the spi handle requires CS ports and pins to work, so it is a
//// good idea to group them together. The compile time errors are due to
//// missing definitions, when HAL_I2C_MODULE_ENABLED is not defined, no I2C
//// functions are imported, which results in errors accross this code base.
////
//// To resolve that, we gate code using ifdefs in this centralized location 
//// and replace missing definitions with an empty one, which will throw an 
//// error at compile and run-time when you try to use them.

/// SPI Handle Abstraction
/// Aside the handle typedef, a port and pin representing the CS is required
#ifdef HAL_SPI_MODULE_ENABLED
struct handle_spi {
	SPI_HandleTypeDef *handle;
	GPIO_TypeDef *port;
	uint8_t pin;
};

#define HANDLE_SPI(in_handle, in_port, in_pin) \
   (struct handle) {                           \
     .protocol = SPI,                          \
     .serial = {                               \
       .spi = {                                \
         .pin = (in_pin),                      \
         .port = (in_port),                    \
         .handle = (in_handle),                \
       }                                       \
     }                                         \
   };
#else
struct handle_spi { int placeholder; };
#define HANDLE_SPI(handle, port, pin) assert(0 & "SPI is not enabled!");
#endif

/// I2C Handle Abstraction
/// Aside the handle typedef, the address of the device on the line is required
#ifdef HAL_I2C_MODULE_ENABLED
struct handle_i2c {
	I2C_HandleTypeDef *handle;
	uint32_t address;
};

#define HANDLE_I2C(in_handle, in_address)   \
   (struct handle) {                        \
     .protocol = I2C,                       \
     .serial = {                            \
       .i2c = {                             \
          .address = (in_address),          \
          .handle = (in_handle),            \
       }                                    \
     }                                      \
   };
#else
struct handle_i2c { int placeholder; };
#define HANDLE_I2C(handle, address) assert(0 & "I2C is not enabled!");
#endif

/// UART Handle Abstraction
/// Only the handle typedef is required
#ifdef HAL_UART_MODULE_ENABLED
struct handle_uart { UART_HandleTypeDef *handle; };

#define HANDLE_QSPI(handle)           \
   (struct handle) {                  \
     .protocol = UART,                \
     .serial = {                      \
      .uart = { .handle = handle }    \
   };
#else
struct handle_uart { int placeholder; };
#define HANDLE_UART(handle) assert(0 & "UART is not enabled!");
#endif

/// QSPI Handle Abstraction
/// Only the handle typedef is required
#ifdef HAL_QSPI_MODULE_ENABLED
struct handle_qspi { QSPI_HandleTypeDef *handle; };

#define HANDLE_QSPI(handle)           \
   (struct handle) {                  \
     .protocol = QSPI,                \
     .serial = {                      \
      .qspi = { .handle = handle }    \
   };
#else
struct handle_qspi { int placeholder; };
#define HANDLE_QSPI(handle) assert(0 & "QSPI is not enabled!");
#endif

//// | Unified Handle Abstractions |
//// This handle uses a tagged union to allow a sensor to switch between
//// different serial protocols. You may pass in a handle with the I2C protocol
//// or SPI protocol and it will use the correct read and write functions.
////
//// To create a handle, you MUST use the following methods
////   struct handle spi = HANDLE_SPI(&hspi1, port, pin);
////   struct handle i2c = HANDLE_I2C(&hi2c1, address);
////   struct handle uart = HANDLE_UART(&huart1);
////   struct handle qspi = HANDLE_QSPI(&hqspi);
//// Then you may pass the handle to the initialization function of a device. 

enum protocol { SPI, UART, I2C, QSPI };
struct handle {
	enum protocol protocol;
	union {
		struct handle_spi spi;
		struct handle_i2c i2c;
		struct handle_uart uart;
		struct handle_qspi qspi;
	} serial;
};

static inline struct handle_spi* handle_as_spi(struct handle *handle)
{
	return handle->protocol == SPI ? &(handle->serial.spi) : NULL;
}

static inline struct handle_i2c* handle_as_i2c(struct handle *handle)
{
	return handle->protocol == I2C ? &(handle->serial.i2c) : NULL;
}

static inline struct handle_uart* handle_as_uart(struct handle *handle)
{
	return handle->protocol == UART ? &(handle->serial.uart) : NULL;
}

static inline struct handle_qspi* handle_as_qspi(struct handle *handle)
{
	return handle->protocol == QSPI ? &(handle->serial.qspi) : NULL;
}

//// | Serial API Abstractions |
//// All serial protocols implement an API with the same write and read
//// functions, with differing implmentations of course. The serial api contains
//// the handle and is requested by a device in its initialization function.
////
//// See the currently provided drivers for an example. But the gist is 
////   int8_t sensor_init(struct sensor_ctx *ctx, struct sensor *sensor, struct handle *handle)
////   {
////      assert(handle->protocol == SPI);
////      sensor->ctx = ctx;
////      sensor->read = sensor_read;
////      serial_api_spi(ctx->api, handle);
////   }
//// Of course you may decide to support multiple protocols if you wish, which
//// this abstraction makes really simple. You would pass `api` around and call
//// `api->write(...)` and `api->read(...)` for writing and reading respectively.

struct op_params {
	// For sensors, cmd is where you pass either the address of the register
	// to access or an actual opcode to send to the device
	const void *cmd;
	const size_t cmd_size;

	// For reads, this is the buffer to read into
	// For writes, this is the buffer to transmit 
	// Set buffer_size to zero to transmit or read nothing
	void *buffer;
	const size_t buffer_size;

	// Used only for qspi, for spi and other methods you may embed dummy
	// cycles directly in the cmd buffer like so
	//   char *cmd = [MY_OPCODE, 0x000000, 0x000000];
	// This value is ignored for non-qspi methods
	const size_t dummy_cycles; 
};
/// Each protocol implements an instance of the serial api, which is then passed
/// to a sensor to allow it to use the given protocol.
struct serial_api {
	struct handle *handle;
	bool (*read)(struct handle*, struct op_params*);
	bool (*write)(struct handle*, struct op_params*);
};
void serial_api_spi(struct serial_api *api, struct handle *handle);
void serial_api_i2c(struct serial_api *api, struct handle *handle);
void serial_api_qspi(struct serial_api *api, struct handle *handle);

//// | Helper methods |
//// These are used for when both `cmd` and `buffer` are static arrays.
//// In that case you may use STATIC_CMD as a shorthand. STATIC_EXEC is  
//// is a shorthand for `buffer_size = 0`, useful for when you want to
//// to send a command which doesn't return anything back. 

#define STATIC_CMD(cmd_arr, buf_arr)                 \
    (struct op_params){                              \
        .cmd          = (cmd_arr),                   \
        .cmd_size     = sizeof(cmd_arr),             \
        .buffer       = (buf_arr),                   \
        .buffer_size  = sizeof(buf_arr),             \
        .dummy_cycles = 0                            \
    }

#define STATIC_EXEC(cmd_arr)                         \
    (struct op_params){                              \
        .cmd          = (cmd_arr),                   \
        .cmd_size     = sizeof(cmd_arr),             \
        .buffer       = NULL,                        \
        .buffer_size  = 0,                           \
        .dummy_cycles = 0                            \
    }

#endif // end DEFS_H
