#pragma once

#include <cstdint>

#include "environment_sensor.hpp"

namespace tpb9000::status {

struct Snapshot {
    bool environment_available;
    environment::Reading environment;
    uint64_t environment_updated_ms;
    uint64_t display_refreshed_ms;
};

void set_environment(const environment::Reading& reading);
void set_environment_unavailable();
void note_display_refresh();
Snapshot snapshot();

}  // namespace tpb9000::status
