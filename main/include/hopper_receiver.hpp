#pragma once

#include <cstdint>

#include "esp_err.h"
#include "hopper_protocol.hpp"

namespace tpb9000::hopper_radio {

struct Sample {
    uint32_t sequence;
    uint16_t distance_mm;
    hopper_protocol::SensorStatus sensor_status;
};

esp_err_t initialize();

// Waits for the next validated telemetry packet. Invalid packets are logged and
// ignored. ESP_ERR_TIMEOUT means the hopper node has gone stale/offline.
esp_err_t receive(Sample& sample, uint32_t timeout_ms);

}  // namespace tpb9000::hopper_radio
