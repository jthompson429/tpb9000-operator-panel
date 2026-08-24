#pragma once

#include "environment_sensor.hpp"
#include "esp_err.h"
#include "power_sensor.hpp"

namespace tpb9000::dashboard {
esp_err_t show_reading(const environment::Reading& reading,
                       bool network_online,
                       bool power_available,
                       const power::Reading& power_reading);
esp_err_t show_sensor_error(bool network_online,
                            bool power_available,
                            const power::Reading& power_reading);
}  // namespace tpb9000::dashboard
