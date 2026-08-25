#ifndef GD5F1GQ5XE_H
#define GD5F1GQ5XE_H

#include "flash.h"
#include "hal.h"
#include "protocol.h"

#include <stdbool.h>
#include <stddef.h>

namespace Common {
class GD5F1GQ5XE final : public Flash {
private:
    Protocol& protocol;

public:
    GD5F1GQ5XE(Protocol& protocol_);
    bool init() override;
};
} // namespace Common

#endif
