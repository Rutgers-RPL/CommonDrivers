#ifndef TASKS_BASE_TASK_H
#define TASKS_BASE_TASK_H

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>

#include "FreeRTOS.h"

namespace Common {

/// TaskEvent ---
/// Use this to communicate between tasks. You can broadcast an event to
/// all tasks and just have them listen for the ones they want.
namespace TaskEvent {
enum : uint32_t {
    Telemetry = (1 << 0),     /// Save to flash, transmit packet over radio
    Staging = (1 << 1),       /// Perform Staging
    FlashDisabled = (1 << 2), /// Disable flash
    /// Note that we can only have 32 events, which should be plenty
};
}

/// DoubleBuffer ---
/// This is an alternative to using a mutex and is likely more
/// performant. However that requires a couple of assumptions
///   1. dropping packets is fine
///   2. only a single writer
///   3. writer cannot be pre-empted by reader
/// Normally using atomics is not a good idea because of how
/// tricky they are. If this gets difficult to maintain/understand
/// you may swap to a mutex, a queue, or remove the memory::order (still
/// uses atomic, but is easier to reason), or use a critical section
/// (ensure T is not too large!).
///
/// The main rational for this is that mutexes are expensive, and may require a
/// context switch, which we can avoid by double buffering, at the cost of more
/// storage. Why use atomics? Atomics solve two issues here, it prevents
/// compiler and execution re-ordering (due to optimizations), and it does
/// fences/memory barriers. Note that specifying a memory_order is an
/// optimization, the default is `memory_order_seq_cst` which is the strictest,
/// safest, but also least performant (especially on ARM, weak memory).
///
/// The main issue with double buffering is what happens if a writer pre-empters
/// a read mid-read? With double buffering we eliminate courrupted stack, and
/// with atomics we can prevent idx from pointing to the wrong buffer.
///
/// Use relaxed mode for when you want to use the result of some operation that
/// doesn't really affect other variables. Use release (release a mutex analogy)
/// to publish changes to other threads (all writes and reads must be done
/// before this) And use acquire (acquire a mutex analogy) to make sure all
/// reads and occur after this. This is what the C++ standard refers to as the
/// Release-Acquire ordering, this is how mutexes work under the hood as well.
template <typename T> class DoubleBuffer {
private:
    T buffer[2];
    std::atomic<uint8_t> idx = 0;

public:
    DoubleBuffer() {};

    /// @brief returns a pointer which can be used for writing, use commit
    ///        to apply the change
    /// @return pointer to the current buffer
    T& write() {
        uint8_t i = idx.load(std::memory_order_relaxed);
        buffer[i ^ 1] = buffer[i];
        return buffer[i ^ 1];
    }

    /// @brief commits the changes
    void commit() {
        uint8_t i = idx.load(std::memory_order_relaxed);
        idx.store(i ^ 1, std::memory_order_release);
    }

    /// @brief reads the front buffer (read buffer) to a variable
    /// @param out  the variable to read the data into
    void read(T& out) {
        uint8_t i = idx.load(std::memory_order_acquire);
        out = buffer[i];
    }
};

/// Registry ---
/// Mom, can we get EventGroups? We already have EventGroups at home.
/// This is a lightweight EventGroups using direct-to-task notifications,
/// which uses less memory and is generally faster than EventGroups. We
/// are also able to centralize all event dispatching to a central location.
class Registry {
private:
    struct Subscriber {
        TaskHandle_t handle;
        uint32_t mask = 0;
    };
    /// 8 is a good-ish limit, I don't think we will ever hit an amount
    /// greater than than this (we want to limit tasks in general)
    /// NOTE: this can be converted to a vec or a runtime
    ///       instantiated singleton if this becomes a problem.
    std::array<Subscriber, 8> subscribers;
    uint8_t count = 0;

public:
    Registry() = default;

    /// @brief subscribes the task to the registry
    /// @param handle  the task to subscribe for
    void subscribe(TaskHandle_t handle, uint32_t mask) {
        assert(count < subscribers.size());
        subscribers[count] = {handle, mask};
        ++count;
    }

    /// @brief publishes events to all subscribers (non ISR)
    /// @param event  the event bits to publish
    void publish(uint32_t event) {
        for (auto& subscriber : subscribers) {
            if (subscriber.handle != nullptr && subscriber.mask & event) {
                xTaskNotify(subscriber.handle, event, eSetBits);
            }
        }
    }

    /// @brief publishes events to all subscribers (ISR)
    /// @param event            the event bits to publish
    /// @param higher_priority  whether to perform a context switch immediately
    ///                         may be pdFALSE or pdTRUE
    void publish_isr(uint32_t event, BaseType_t* higher_priority) {
        for (auto& subscriber : subscribers) {
            if (subscriber.handle != nullptr && subscriber.mask & event) {
                xTaskNotifyFromISR(subscriber.handle, event, eSetBits,
                                   higher_priority);
            }
        }
    }
};

/// Task ---
/// To implement a task, all you have to do is implement the `loop` method. In
/// each task, you may use any member variables you want. You may use the static
/// registry for inter-task communication. You may use `subscribe` of registry
/// in order to be notified of certain events
class Task {
protected:
    TaskHandle_t handle;

private:
    /// Allows C++ methods to be used in `xTaskCreate`
    /// Do not touch this unless you know what you are doing.
    static void entry(void* args) {
        auto* self = static_cast<Task*>(args);
        self->loop();
    }

public:
    virtual ~Task() = default;

    /// @brief runs the task
    /// This should never return! If you want to return delete the task!
    virtual void loop() = 0;

    /// @brief returns the global static registry, you may use this to publish
    /// messages to all tasks
    /// @return the global registry instance
    static Registry& registry() {
        static Registry instance;
        return instance;
    }

    /// @brief creates a FreeRTOS task
    /// @return true on success, flse on failure
    bool init(const char* name, uint32_t stacksize, uint32_t priority) {
        auto res =
            xTaskCreate(&Task::entry, name, stacksize, this, priority, &handle);
        if (res != pdPASS || handle == nullptr)
            return false;
        return true;
    }
};
} // namespace Common

#endif // TASKS_BASE_TASK_H
