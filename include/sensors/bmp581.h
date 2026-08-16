#ifndef SENSORS_BMP581_H
#define SENSORS_BMP581_H

#include "bmp5_defs.h"
#include "protocol.h"
#include "sensor.h"

#include <cassert>

namespace Platform {
class BMP581 final : public Sensor {
private:
    struct bmp5_dev dev;
    struct bmp5_osr_odr_press_config odr_config;
    struct bmp5_int_source_select int_config;
    struct Protocol& api;

public:
    BMP581(Protocol& api_) : api(api_) {
        assert(api.type() == ProtocolType::SPI);
    };
    bool init() override;
    bool read(Packet& packet) override;
};
} // namespace Platform

#endif
