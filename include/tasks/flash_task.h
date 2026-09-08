#ifndef TASKS_FLASH_TASK_H
#define TASKS_FLASH_TASK_H

#include "base_task.h"
#include "flash.h"
#include "sensor.h"

namespace Common {
class FlashTask final : public Task {
private:
    Flash& flash;
    lfs_file_t& packet_file;
    DoubleBuffer<Packet>& packet;

public:
    FlashTask(Flash& flash_, lfs_file_t& packet_file_,
              DoubleBuffer<Packet>& packet_)
        : flash(flash_), packet_file(packet_file_), packet(packet_) {};

    void loop() override {
        registry().subscribe(handle,
                             TaskEvent::FlashDisabled | TaskEvent::Telemetry);
        for (;;) {
            // Wait until notified
            uint32_t event;
            xTaskNotifyWait(0, 0xFFFFFFFF, &event, portMAX_DELAY);

            if (event & TaskEvent::Telemetry) {
                // Save to flash
                Packet copy;
                packet.read(copy);
                flash.append(&packet_file, (uint8_t*)&copy, sizeof(copy));
            }

            // Someone notified us to stop task collection, shut the poor flash
            // down :(
            if (event & TaskEvent::FlashDisabled) {
                flash.close(&packet_file);
                flash.unmount();
                vTaskDelete(NULL);
                return;
            }
        }
    }
};
} // namespace Common

#endif // TASKS_FLASH_TASK_H
