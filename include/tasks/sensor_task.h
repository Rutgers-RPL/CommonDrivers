#ifndef TASKS_SENSOR_TASK_H
#define TASKS_SENSOR_TASK_H

#include "hal.h"
#include "sensor.h"
#include "task.h"

#include <span>

namespace Common {
class SensorTask final : public Task {
private:
    std::span<Sensor*> sensors;
    CRC_HandleTypeDef& hcrc;
    DoubleBuffer<Packet>& packet;

public:
    SensorTask(std::span<Sensor*> sensors_, CRC_HandleTypeDef& hcrc_,
               DoubleBuffer<Packet>& packet_)
        : sensors(sensors_), hcrc(hcrc_), packet(packet_) {};

    void loop() override {
        // Simple counter for task notification
        uint32_t counter = 0;
        // Run this task 200 times per second
        TickType_t last_wake_up = xTaskGetTickCount();
        int hertz = 200;

        for (;;) {
            Packet& ptr = packet.write();
            // Read from all sensors
            for (auto* sensor : sensors) {
                if (sensor != nullptr)
                    sensor->read(ptr);
            }
            // TODO: change this to microseconds at a later time
            ptr.time_us = xTaskGetTickCount() * portTICK_PERIOD_MS;
            ptr.checksum = HAL_CRC_Calculate(
                &hcrc, (uint32_t*)((const uint8_t*)&ptr + sizeof(short)),
                sizeof(class Packet) - 6);
            packet.commit();

            // Tell flash to save data every other packet
            if ((++counter) >= 2) {
                counter = 0;
                registry().publish(TaskEvent::Telemetry);
            }
            registry().publish(TaskEvent::Staging);
        }
        // Go to sleep little task...
        vTaskDelayUntil(&last_wake_up, configTICK_RATE_HZ / hertz);
    }
};
} // namespace Common

#endif // TASKS_SENSOR_TASK_H
