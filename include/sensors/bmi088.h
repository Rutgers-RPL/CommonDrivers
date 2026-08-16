/*
 * bmi088.h
 *
 *  Created on: Oct 26, 2025
 *      Author: Dhruv Shah
 */

#ifndef SENSORS_BMI088_H
#define SENSORS_BMI088_H

#include "bmi08_defs.h"
#include "sensor.h"

#include <cassert>

namespace Platform {
class BMI088 final : public Sensor {
private:
    struct bmi08_dev dev;
    Protocol& accel;
    Protocol& gyro;

public:
    BMI088(Protocol& accel_, Protocol& gyro_) : accel{accel_}, gyro{gyro_} {
        assert(accel.type() == ProtocolType::SPI);
        assert(gyro.type() == ProtocolType::SPI);
    }
    bool init() override;
    bool read(Packet& packet) override;
};
} // namespace Platform

#endif
