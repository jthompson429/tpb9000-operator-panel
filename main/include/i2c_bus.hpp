#pragma once

#include <cstdint>
#include "driver/i2c_master.h"
#include "esp_err.h"

namespace tpb9000::i2c {
esp_err_t initialize();
i2c_master_bus_handle_t handle();
bool device_present(uint8_t address);
void scan_and_log();
esp_err_t write_byte(uint8_t address, uint8_t value);
}  // namespace tpb9000::i2c
