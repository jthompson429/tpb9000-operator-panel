#pragma once

#include "esp_err.h"
#include "hopper_protocol.hpp"

namespace tpb9000::hopper_node::radio {

esp_err_t initialize();
bool peer_configured();
esp_err_t send(const hopper_protocol::HopperTelemetry& telemetry);

}  // namespace tpb9000::hopper_node::radio
