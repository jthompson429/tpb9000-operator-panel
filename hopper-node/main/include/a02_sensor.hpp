#pragma once

#include <cstdint>

#include "a02_frame_parser.hpp"
#include "esp_err.h"

namespace tpb9000::hopper_node::sensor {

esp_err_t initialize();
esp_err_t read_frame(uint16_t& distance_mm, uint32_t timeout_ms);

}  // namespace tpb9000::hopper_node::sensor
