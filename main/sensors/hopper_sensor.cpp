#include "hopper_sensor.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "board_config.hpp"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

namespace tpb9000::hopper {
namespace {
constexpr char tag[] = "hopper";
constexpr uart_port_t uart_port = UART_NUM_1;
constexpr uint32_t baud_rate = 9600;
constexpr uint64_t sample_interval_us = 1'000'000;
constexpr uint64_t receive_timeout_us = 5'000'000;
constexpr size_t history_size = 5;
constexpr float bar_hysteresis_percent = 3.0F;

FrameParser parser;
std::array<uint16_t, history_size> history = {};
size_t history_count = 0;
size_t history_next = 0;
uint64_t last_accepted_us = 0;
uint64_t last_valid_frame_us = 0;
uint8_t displayed_bars = 0;
bool initialized = false;

float median_distance_cm() {
    std::array<uint16_t, history_size> sorted = history;
    std::sort(sorted.begin(), sorted.begin() + history_count);
    if ((history_count & 1U) != 0U) {
        return sorted[history_count / 2] / 10.0F;
    }
    const size_t upper = history_count / 2;
    return (sorted[upper - 1] + sorted[upper]) / 20.0F;
}

uint8_t bars_with_hysteresis(float percent) {
    const uint8_t ideal = calculate_bars(percent);
    if (history_count == 1) {
        displayed_bars = ideal;
        return displayed_bars;
    }
    if (ideal > displayed_bars) {
        const float threshold = displayed_bars * (100.0F / 6.0F) +
                                bar_hysteresis_percent;
        if (percent >= threshold) displayed_bars = ideal;
    } else if (ideal < displayed_bars) {
        const float threshold = (displayed_bars - 1) * (100.0F / 6.0F) -
                                bar_hysteresis_percent;
        if (percent <= threshold) displayed_bars = ideal;
    }
    return displayed_bars;
}

void accept_sample(uint16_t distance_mm) {
    history[history_next] = distance_mm;
    history_next = (history_next + 1) % history_size;
    if (history_count < history_size) ++history_count;
}
}  // namespace

esp_err_t initialize() {
    if (initialized) return ESP_OK;
    const uart_config_t config = {
        .baud_rate = static_cast<int>(baud_rate),
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .rx_glitch_filt_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {},
    };
    ESP_RETURN_ON_ERROR(uart_param_config(uart_port, &config), tag,
                        "UART configuration failed");
    ESP_RETURN_ON_ERROR(
        uart_set_pin(uart_port, board::rs485_tx, board::rs485_rx,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
        tag, "UART pin assignment failed");
    ESP_RETURN_ON_ERROR(uart_driver_install(uart_port, 512, 0, 0, nullptr, 0),
                        tag, "UART driver installation failed");
    parser.reset();
    initialized = true;
    ESP_LOGI(tag, "A02 receiver ready on RS485 UART at 9600 baud");
    return ESP_OK;
}

esp_err_t read(Reading& reading) {
    if (!initialized) return ESP_ERR_INVALID_STATE;

    const uint64_t started_us = esp_timer_get_time();
    uint8_t bytes[64] = {};
    while (esp_timer_get_time() - started_us < receive_timeout_us) {
        const int count = uart_read_bytes(uart_port, bytes, sizeof(bytes),
                                          pdMS_TO_TICKS(250));
        if (count < 0) return ESP_FAIL;
        for (int index = 0; index < count; ++index) {
            uint16_t distance_mm = 0;
            if (!parser.push(bytes[index], distance_mm)) continue;
            const uint64_t now_us = esp_timer_get_time();
            last_valid_frame_us = now_us;
            if (last_accepted_us != 0 &&
                now_us - last_accepted_us < sample_interval_us) {
                continue;
            }
            last_accepted_us = now_us;
            accept_sample(distance_mm);
            reading.distance_cm = median_distance_cm();
            reading.percent = calculate_percent(reading.distance_cm);
            reading.bars = bars_with_hysteresis(reading.percent);
            return ESP_OK;
        }
    }

    if (last_valid_frame_us == 0 ||
        esp_timer_get_time() - last_valid_frame_us >= receive_timeout_us) {
        history.fill(0);
        history_count = 0;
        history_next = 0;
        displayed_bars = 0;
    }
    return ESP_ERR_TIMEOUT;
}

const char* status_name(const Reading& reading) {
    if (reading.bars == 0) return "Empty";
    if (reading.bars == 1) return "Critical";
    if (reading.bars <= 3) return "Low";
    return "Normal";
}

}  // namespace tpb9000::hopper
