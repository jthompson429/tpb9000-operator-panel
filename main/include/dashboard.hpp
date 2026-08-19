#pragma once

#include "environment_sensor.hpp"
#include "esp_err.h"

namespace tpb9000::dashboard {
esp_err_t show_reading(const environment::Reading& reading);
esp_err_t show_sensor_error();
}  // namespace tpb9000::dashboard
