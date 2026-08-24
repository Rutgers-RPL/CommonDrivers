/*
 * CD-PA1616S.h
 *
 *  Created on: Feb 21, 2025
 *      Author: Mahir Shah
 */

#ifndef SENSORS_CD_PA1616S_H
#define SENSORS_CD_PA1616S_H

#include "protocol.h"
#include "sensor.h"

#include <cassert>
#include <stdint.h>

namespace Common {
class GPS final : public Sensor {
private:
    // used for DMA reception buffer
    uint8_t buffer[128];
    Protocol& protocol;

public:
    GPS(Protocol& protocol_) : protocol(protocol_) {
        assert(protocol.type() == ProtocolType::UART);
    };
    bool init() override;
    bool read(Packet& packet) override;
};
} // namespace Common

#endif /* INC_CD_PA1616S_H_ */
