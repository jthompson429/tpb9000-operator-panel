#include "a02_sensor.hpp"

#include <algorithm>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"

#if __has_include("hopper_node_config.hpp")
#include "hopper_node_config.hpp"
#else
#include "hopper_node_config.example.hpp"
#endif

namespace tpb9000::hopper_node::sensor {
namespace {
constexpr char tag[] = "a02_sensor";
constexpr uart_port_t uart_port = UART_NUM_0;
FrameParser parser;
bool initialized = false;
bool initial_stream_logged = false;
}  // namespace

esp_err_t initialize() {
    if (initialized) return ESP_OK;
    const uart_config_t configuration = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .rx_glitch_filt_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {},
    };
    ESP_RETURN_ON_ERROR(uart_param_config(uart_port, &configuration), tag,
                        "UART configuration failed");
    ESP_RETURN_ON_ERROR(
        uart_set_pin(uart_port, TPB9000_A02_TX_GPIO, TPB9000_A02_RX_GPIO,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
        tag, "UART pin assignment failed");
    ESP_RETURN_ON_ERROR(uart_driver_install(uart_port, 512, 0, 0, nullptr, 0),
                        tag, "UART driver installation failed");

    constexpr unsigned sample_count = 25000;
    unsigned transitions = 0;
    unsigned low_samples = 0;
    const auto rx_gpio = static_cast<gpio_num_t>(TPB9000_A02_RX_GPIO);
    int previous_level = gpio_get_level(rx_gpio);
    for (unsigned sample = 0; sample < sample_count; ++sample) {
        const int level = gpio_get_level(rx_gpio);
        if (level == 0) ++low_samples;
        if (level != previous_level) {
            ++transitions;
            previous_level = level;
        }
        esp_rom_delay_us(20);
    }
    ESP_LOGI(tag,
             "RX activity over 500 ms: %u transitions, %u/%u low samples",
             transitions, low_samples, sample_count);

    parser.reset();
    initialized = true;
    ESP_LOGI(tag, "Sensor ONLINE: UART0 TX GPIO %d / RX GPIO %d, 9600 8N1",
             TPB9000_A02_TX_GPIO, TPB9000_A02_RX_GPIO);
    return ESP_OK;
}

esp_err_t read_frame(uint16_t& distance_mm, uint32_t timeout_ms) {
    if (!initialized) return ESP_ERR_INVALID_STATE;
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    uint8_t bytes[32] = {};
    while (static_cast<int32_t>(deadline - xTaskGetTickCount()) > 0) {
        const TickType_t remaining = deadline - xTaskGetTickCount();
        const int count = uart_read_bytes(
            uart_port, bytes, sizeof(bytes),
            std::min<TickType_t>(remaining, pdMS_TO_TICKS(100)));
        if (count < 0) return ESP_FAIL;
        if (count > 0 && !initial_stream_logged) {
            const size_t diagnostic_length =
                std::min<size_t>(static_cast<size_t>(count), 16);
            ESP_LOGI(tag, "First UART bytes (%u of %d):",
                     static_cast<unsigned>(diagnostic_length), count);
            ESP_LOG_BUFFER_HEX_LEVEL(tag, bytes, diagnostic_length,
                                     ESP_LOG_INFO);
            initial_stream_logged = true;
        }
        for (int index = 0; index < count; ++index) {
            if (parser.push(bytes[index], distance_mm)) return ESP_OK;
        }
    }
    return ESP_ERR_TIMEOUT;
}

}  // namespace tpb9000::hopper_node::sensor
