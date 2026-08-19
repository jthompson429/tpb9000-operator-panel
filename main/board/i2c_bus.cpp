#include "i2c_bus.hpp"

#include "board_config.hpp"
#include "esp_check.h"
#include "esp_log.h"

namespace tpb9000::i2c {
namespace {
constexpr char tag[] = "i2c";
constexpr int probe_timeout_ms = 25;
i2c_master_bus_handle_t bus = nullptr;

const char* known_device_name(uint8_t address) {
    switch (address) {
        case 0x14:
        case 0x5D: return "GT911 touch controller";
        case 0x24:
        case 0x38: return "CH422G I/O expander";
        case 0x51: return "PCF85063A real-time clock";
        case 0x76:
        case 0x77: return "BME280 candidate";
        default: return "unknown device";
    }
}
}  // namespace

esp_err_t initialize() {
    if (bus != nullptr) return ESP_OK;
    const i2c_master_bus_config_t config = {
        .i2c_port = board::i2c_port,
        .sda_io_num = board::i2c_sda,
        .scl_io_num = board::i2c_scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {.enable_internal_pullup = true, .allow_pd = false},
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&config, &bus), tag,
                        "Unable to initialize I2C");
    ESP_LOGI(tag, "Shared I2C ready: SDA GPIO %d, SCL GPIO %d",
             board::i2c_sda, board::i2c_scl);
    return ESP_OK;
}

i2c_master_bus_handle_t handle() { return bus; }

bool device_present(uint8_t address) {
    return bus != nullptr &&
           i2c_master_probe(bus, address, probe_timeout_ms) == ESP_OK;
}

void scan_and_log() {
    if (bus == nullptr) {
        ESP_LOGE(tag, "Cannot scan an uninitialized bus");
        return;
    }
    unsigned count = 0;
    ESP_LOGI(tag, "Scanning I2C addresses...");
    for (uint8_t address = 0x08; address <= 0x77; ++address) {
        if (device_present(address)) {
            ++count;
            ESP_LOGI(tag, "  0x%02X  %s", address, known_device_name(address));
        }
    }
    ESP_LOGI(tag, "I2C scan complete: %u address(es) responded", count);
    if (!device_present(0x76) && !device_present(0x77)) {
        ESP_LOGW(tag, "No BME280 candidate found at 0x76 or 0x77");
    }
}

esp_err_t write_byte(uint8_t address, uint8_t value) {
    if (bus == nullptr) return ESP_ERR_INVALID_STATE;
    i2c_master_dev_handle_t device = nullptr;
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = board::i2c_frequency_hz,
        .scl_wait_us = 0,
        .flags = {.disable_ack_check = false},
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &config, &device), tag,
                        "Could not attach device 0x%02X", address);
    const esp_err_t result = i2c_master_transmit(device, &value, 1, 100);
    const esp_err_t remove_result = i2c_master_bus_rm_device(device);
    return result != ESP_OK ? result : remove_result;
}
}  // namespace tpb9000::i2c
