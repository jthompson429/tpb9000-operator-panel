#pragma once

#include <cstdint>

#include "esp_err.h"

namespace tpb9000::network {
esp_err_t initialize();
bool configured();
bool online();
bool rssi_dbm(int8_t& value);
}  // namespace tpb9000::network
