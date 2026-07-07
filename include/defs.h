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
#include "stm32f4xx_hal.h"
#endif // end TEST

#endif // end DEFS_H
