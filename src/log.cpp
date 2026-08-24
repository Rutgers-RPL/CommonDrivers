#include "log.h"

#include "hal.h"

#ifndef USE_SWO_DEBUG
#include "usbd_cdc_if.h"
#endif

#include <cstdarg>
#include <cstdint>
#include <cstdio>

namespace Platform {
// TODO: compile this out for release builds
void log(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len <= 0)
        return;

#ifdef USE_SWO_DEBUG
    for (int i = 0; i < len; ++i)
        ITM_SendChar(buf[i]);
#else
    while (CDC_Transmit_FS(reinterpret_cast<uint8_t*>(buf), len) == USBD_BUSY)
        Delay(1);
#endif
}
} // namespace Platform
