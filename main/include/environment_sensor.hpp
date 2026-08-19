#pragma once

#include "esp_err.h"

namespace tpb9000::environment {

struct Reading {
    double temperature_c;
    double temperature_f;
    double humidity_percent;
    double pressure_pa;
    double pressure_hpa;
};

esp_err_t initialize();
esp_err_t read(Reading& reading);

}  // namespace tpb9000::environment
