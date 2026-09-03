#pragma once

#include "esp_err.h"

namespace tpb9000::power {

struct Reading {
    double bus_voltage_v;
    double current_a;
    double power_w;
};

esp_err_t initialize();
esp_err_t read(Reading& reading);

}  // namespace tpb9000::power
