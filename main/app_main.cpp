#include "board_config.hpp"
#include "dashboard.hpp"
#include "display.hpp"
#include "environment_sensor.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.hpp"
#include "network.hpp"

namespace {
constexpr char tag[] = "tpb9000";
void fatal(const char* operation, esp_err_t error) {
    ESP_LOGE(tag, "%s: %s", operation, esp_err_to_name(error));
    ESP_LOGE(tag, "Restarting in five seconds");
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
}
}  // namespace

extern "C" void app_main() {
    ESP_LOGI(tag, "TPB9000 Operator Panel");
    ESP_LOGI(tag, "Board: %s", tpb9000::board::model);
    ESP_LOGI(tag, "Milestone: live BME280 measurements");
    esp_err_t result = tpb9000::i2c::initialize();
    if (result != ESP_OK) fatal("I2C initialization failed", result);
    tpb9000::i2c::scan_and_log();
    result = tpb9000::display::initialize();
    if (result != ESP_OK) fatal("Display initialization failed", result);
    result = tpb9000::environment::initialize();
    if (result != ESP_OK) fatal("BME280 initialization failed", result);
    result = tpb9000::network::initialize();
    if (result != ESP_OK) {
        ESP_LOGE(tag, "Network initialization failed: %s; continuing offline",
                 esp_err_to_name(result));
    }
    ESP_LOGI(tag, "Bring-up complete; logging live readings every two seconds");
    while (true) {
        tpb9000::environment::Reading reading = {};
        result = tpb9000::environment::read(reading);
        if (result == ESP_OK) {
            ESP_LOGI(tag, "Environment: %.1f F, %.1f %%RH, %.1f hPa",
                     reading.temperature_f, reading.humidity_percent,
                     reading.pressure_hpa);
            const esp_err_t display_result =
                tpb9000::dashboard::show_reading(
                    reading, tpb9000::network::online());
            if (display_result != ESP_OK) {
                ESP_LOGE(tag, "Dashboard update failed: %s",
                         esp_err_to_name(display_result));
            }
        } else {
            ESP_LOGE(tag, "Environment: SENSOR ERROR (%s)",
                     esp_err_to_name(result));
            tpb9000::dashboard::show_sensor_error(tpb9000::network::online());
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
