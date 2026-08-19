#pragma once

#include "esp_err.h"

namespace tpb9000::network {
esp_err_t initialize();
bool configured();
bool online();
}  // namespace tpb9000::network
