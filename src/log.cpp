#include "log.h"

#include "hal.h"

#ifdef USE_CDC_DEBUG
#include "usbd_cdc_if.h"
#endif

#include <cstdarg>
#include <cstdint>
#include <cstdio>

namespace Platform {
#if defined(TEST) || defined(RELEASE)
// TODO: I think the static strings are not compiled out, need to investigate
void LOG(const char* fmt, ...) {}
#else
void LOG(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len <= 0)
        return;

#ifdef USE_CDC_DEBUG
    while (CDC_Transmit_FS(reinterpret_cast<uint8_t*>(buf), len) == USBD_BUSY)
        Delay(1);
#else
    for (int i = 0; i < len; ++i)
        ITM_SendChar(buf[i]);
#endif // end USE_CDC_DEBUG
}
#endif // end defined(TEST) || defined(RELEASE)
} // namespace Platform
