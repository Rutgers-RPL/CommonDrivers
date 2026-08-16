#ifndef HAL_H
#define HAL_H

#ifndef TEST
#ifdef USE_STM32_H7XX
#include "stm32h7xx_hal.h"
#elif USE_STM32_L4XX
#include "stm32l4xx_hal.h"
#else
#include "stm32f4xx_hal.h"
#endif
#else
#define HAL_Delay(...) (0)
#endif

#endif
