#ifndef HAL_H
#define HAL_H

#ifndef TEST
// Stringifies the macro, since #include requires a string
// NOTE: this trick was once used in the linux kernel via __gcc_header, go check
// it out here. Note that it is no longer in use now.
// https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/linux/compiler-gcc.h?id=f6d133f877c8bb0a0934dc8c521c758ee771e901#n125
#define _hal_header(x) #x
#define __hal_header(x) _hal_header(stm32 ## x.h)
#define hal_header(x) __hal_header(x)
// This will include the correct hal header, see the root CMakeLists.txt
// for more information on how we get `PLATFORM_STM32_SERIES`
#include hal_header(PLATFORM_STM32_SERIES)
#else
// Dummy stub used for testing
#define HAL_Delay(...) (0)
#endif // end TEST

// This might be used in non-cpp files so no cpp features will be used
// Of course that also means no namespace usage
// TODO: decide if we do want to stuff this in a namespace, or if we should add
// a prefix so there is no name collision
#ifdef PLATFORM_USE_RTOS
#include "FreeRTOS.h"
#include "task.h"
#define Delay(x) vTaskDelay(pdMS_TO_TICKS(x))
#else
#define Delay(x) HAL_Delay(x)
#endif

#endif // end HAL_H
