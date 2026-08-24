#include "system_status.hpp"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

namespace tpb9000::status {
namespace {
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
Snapshot current = {};

uint64_t uptime_ms() {
    return static_cast<uint64_t>(esp_timer_get_time()) / 1000U;
}
}  // namespace

void set_environment(const environment::Reading& reading) {
    const uint64_t timestamp = uptime_ms();
    taskENTER_CRITICAL(&lock);
    current.environment_available = true;
    current.environment = reading;
    current.environment_updated_ms = timestamp;
    taskEXIT_CRITICAL(&lock);
}

void set_environment_unavailable() {
    taskENTER_CRITICAL(&lock);
    current.environment_available = false;
    current.environment = {};
    current.environment_updated_ms = 0;
    taskEXIT_CRITICAL(&lock);
}

void set_power(const power::Reading& reading) {
    const uint64_t timestamp = uptime_ms();
    taskENTER_CRITICAL(&lock);
    current.power_available = true;
    current.power = reading;
    current.power_updated_ms = timestamp;
    taskEXIT_CRITICAL(&lock);
}

void set_power_unavailable() {
    taskENTER_CRITICAL(&lock);
    current.power_available = false;
    current.power = {};
    current.power_updated_ms = 0;
    taskEXIT_CRITICAL(&lock);
}

void note_display_refresh() {
    const uint64_t timestamp = uptime_ms();
    taskENTER_CRITICAL(&lock);
    current.display_refreshed_ms = timestamp;
    taskEXIT_CRITICAL(&lock);
}

Snapshot snapshot() {
    taskENTER_CRITICAL(&lock);
    const Snapshot copy = current;
    taskEXIT_CRITICAL(&lock);
    return copy;
}

}  // namespace tpb9000::status
