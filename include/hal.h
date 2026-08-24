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
// for more information on how we get `HAL_STM32_SERIES`
#include hal_header(HAL_STM32_SERIES)
#else
// Dummy stub used for testing
#define HAL_Delay(...) (0)
#endif

#endif
