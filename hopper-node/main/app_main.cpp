#include <cstdint>

#include "a02_sensor.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "espnow_sender.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hopper_protocol.hpp"

namespace {
constexpr char tag[] = "hopper_node";
constexpr uint64_t transmit_interval_us = 1'000'000;
constexpr uint64_t sensor_stale_us = 2'000'000;
}

extern "C" void app_main() {
    ESP_LOGI(tag, "TPB9000 Hopper Node");
    ESP_ERROR_CHECK(tpb9000::hopper_node::sensor::initialize());
    ESP_ERROR_CHECK(tpb9000::hopper_node::radio::initialize());

    uint32_t sequence = 0;
    uint16_t last_distance_mm = 0;
    uint64_t last_sensor_frame_us = 0;
    uint64_t next_transmit_us = esp_timer_get_time();
    while (true) {
        uint16_t distance_mm = 0;
        if (tpb9000::hopper_node::sensor::read_frame(distance_mm, 100) ==
            ESP_OK) {
            last_distance_mm = distance_mm;
            last_sensor_frame_us = esp_timer_get_time();
        }

        const uint64_t now_us = esp_timer_get_time();
        if (now_us < next_transmit_us) continue;
        next_transmit_us = now_us + transmit_interval_us;
        const bool sensor_ok = last_sensor_frame_us != 0 &&
                               now_us - last_sensor_frame_us < sensor_stale_us;
        const auto telemetry = tpb9000::hopper_protocol::make_telemetry(
            ++sequence, sensor_ok ? last_distance_mm : 0,
            sensor_ok
                ? tpb9000::hopper_protocol::SensorStatus::ok
                : tpb9000::hopper_protocol::SensorStatus::no_data);
        ESP_LOGI(tag, "TX #%lu | Distance: %u mm | Sensor: %s",
                 static_cast<unsigned long>(sequence), telemetry.distance_mm,
                 sensor_ok ? "OK" : "NO DATA");
        if (!tpb9000::hopper_node::radio::peer_configured()) continue;
        const esp_err_t result = tpb9000::hopper_node::radio::send(telemetry);
        if (result != ESP_OK) {
            ESP_LOGW(tag, "Transmission attempt failed: %s",
                     esp_err_to_name(result));
        }
    }
}
