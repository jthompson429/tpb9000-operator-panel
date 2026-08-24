#pragma once

#include <cstdint>

#include "environment_sensor.hpp"
#include "power_sensor.hpp"

namespace tpb9000::status {

struct Snapshot {
    bool environment_available;
    environment::Reading environment;
    uint64_t environment_updated_ms;
    bool power_available;
    power::Reading power;
    uint64_t power_updated_ms;
    uint64_t display_refreshed_ms;
};

void set_environment(const environment::Reading& reading);
void set_environment_unavailable();
void set_power(const power::Reading& reading);
void set_power_unavailable();
void note_display_refresh();
Snapshot snapshot();

}  // namespace tpb9000::status
