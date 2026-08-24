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
#include "power_sensor.hpp"
#include "status_api.hpp"
#include "system_status.hpp"

namespace {
constexpr char tag[] = "tpb9000";
constexpr unsigned sensor_retry_interval_cycles = 5;

void power_monitor_task(void*) {
    bool sensor_ready = false;
    unsigned retry_seconds = 0;
    unsigned log_cycles = 0;
    while (true) {
        if (!sensor_ready && retry_seconds == 0) {
            const esp_err_t result = tpb9000::power::initialize();
            sensor_ready = result == ESP_OK;
            if (!sensor_ready) {
                tpb9000::status::set_power_unavailable();
                ESP_LOGW(tag,
                         "INA219 unavailable (%s); retrying in ten seconds",
                         esp_err_to_name(result));
                retry_seconds = 10;
            }
        }

        if (sensor_ready) {
            tpb9000::power::Reading reading = {};
            const esp_err_t result = tpb9000::power::read(reading);
            if (result == ESP_OK) {
                tpb9000::status::set_power(reading);
                if (++log_cycles >= 10) {
                    ESP_LOGI(tag, "Power: %.3f V, %.3f A, %.3f W",
                             reading.bus_voltage_v, reading.current_a,
                             reading.power_w);
                    log_cycles = 0;
                }
            } else {
                sensor_ready = false;
                retry_seconds = 10;
                log_cycles = 0;
                tpb9000::status::set_power_unavailable();
                ESP_LOGE(tag,
                         "INA219 read failed (%s); retrying in ten seconds",
                         esp_err_to_name(result));
            }
        } else if (retry_seconds > 0) {
            --retry_seconds;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

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
    result = tpb9000::network::initialize();
    if (result != ESP_OK) {
        ESP_LOGE(tag, "Network initialization failed: %s; continuing offline",
                 esp_err_to_name(result));
    }
    result = tpb9000::api::initialize();
    if (result != ESP_OK) {
        ESP_LOGE(tag, "Status API initialization failed: %s; continuing",
                 esp_err_to_name(result));
    }
    if (xTaskCreate(power_monitor_task, "power_monitor", 4096, nullptr, 5,
                    nullptr) != pdPASS) {
        ESP_LOGE(tag, "Could not start INA219 monitor; continuing without it");
        tpb9000::status::set_power_unavailable();
    }
    bool sensor_ready = false;
    unsigned sensor_retry_cycles = 0;
    ESP_LOGI(tag, "Bring-up complete; updating every two seconds");
    while (true) {
        if (!sensor_ready && sensor_retry_cycles == 0) {
            result = tpb9000::environment::initialize();
            sensor_ready = result == ESP_OK;
            if (!sensor_ready) {
                ESP_LOGE(tag,
                         "BME280 unavailable (%s); retrying in ten seconds",
                         esp_err_to_name(result));
                sensor_retry_cycles = sensor_retry_interval_cycles;
            }
        }

        tpb9000::environment::Reading reading = {};
        result = sensor_ready ? tpb9000::environment::read(reading)
                              : ESP_ERR_INVALID_STATE;
        if (sensor_ready && result == ESP_OK) {
            tpb9000::status::set_environment(reading);
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
            if (display_result == ESP_OK) {
                tpb9000::status::note_display_refresh();
            }
        } else {
            tpb9000::status::set_environment_unavailable();
            if (sensor_ready) {
                sensor_ready = false;
                sensor_retry_cycles = sensor_retry_interval_cycles;
            }
            ESP_LOGE(tag, "Environment: SENSOR ERROR (%s)",
                     esp_err_to_name(result));
            const esp_err_t display_result =
                tpb9000::dashboard::show_sensor_error(
                    tpb9000::network::online());
            if (display_result == ESP_OK) {
                tpb9000::status::note_display_refresh();
            }
        }
        if (!sensor_ready && sensor_retry_cycles > 0) --sensor_retry_cycles;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
