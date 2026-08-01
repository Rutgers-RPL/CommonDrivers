#ifndef FAKE_STM32F4XX_HAL_H
#define FAKE_STM32F4XX_HAL_H

#include <stdint.h>

typedef int HAL_StatusTypeDef;
#define HAL_OK 0
#define HAL_MAX_DELAY 0

typedef struct { uint32_t dummy; } I2C_HandleTypeDef;
typedef struct { uint32_t dummy; } SPI_HandleTypeDef;
typedef struct { uint32_t dummy; } GPIO_TypeDef;
typedef struct { uint32_t dummy; } UART_HandleTypeDef;

#define HAL_I2C_Mem_Read(...)    (0)
#define HAL_I2C_Mem_Write(...)   (0)
#define HAL_I2C_GetError(...)    (0)
#define HAL_SPI_Transmit(...)    (0)
#define HAL_SPI_Receive(...)     (0)
#define HAL_GPIO_WritePin(...)   (0)
#define HAL_GPIO_ReadPin(...)    (0)
#define HAL_UART_Transmit(...)   (0)
#define HAL_Delay(...)           (0)

#endif
