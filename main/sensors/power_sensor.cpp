#include "power_sensor.hpp"

#include <array>
#include <cmath>
#include <cstdint>

#include "board_config.hpp"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "i2c_bus.hpp"

namespace tpb9000::power {
namespace {
constexpr char tag[] = "ina219";
constexpr uint8_t address = 0x40;
constexpr int transaction_timeout_ms = 100;

constexpr uint8_t configuration_register = 0x00;
constexpr uint8_t bus_voltage_register = 0x02;
constexpr uint8_t power_register = 0x03;
constexpr uint8_t current_register = 0x04;
constexpr uint8_t calibration_register = 0x05;

// 32 V bus range, +/-320 mV shunt range, 12-bit bus and shunt ADCs,
// continuous shunt-and-bus conversion.
constexpr uint16_t configuration = 0x399F;

// Current_LSB = 100 uA with the fitted R100 (0.1 ohm) shunt:
// CAL = trunc(0.04096 / (0.0001 A * 0.1 ohm)) = 4096.
constexpr uint16_t calibration = 4096;
constexpr double current_lsb_a = 0.0001;
constexpr double power_lsb_w = 20.0 * current_lsb_a;
constexpr double smoothing_alpha = 0.25;
constexpr double maximum_bus_voltage_v = 26.0;
constexpr double maximum_absolute_current_a = 3.2;
constexpr double maximum_power_w =
    maximum_bus_voltage_v * maximum_absolute_current_a;

i2c_master_dev_handle_t device = nullptr;
bool ready = false;
bool has_filtered_reading = false;
Reading filtered = {};

esp_err_t write_register(uint8_t register_address, uint16_t value) {
    const std::array<uint8_t, 3> transaction = {
        register_address,
        static_cast<uint8_t>(value >> 8),
        static_cast<uint8_t>(value & 0xFF),
    };
    return i2c_master_transmit(device, transaction.data(), transaction.size(),
                               transaction_timeout_ms);
}

esp_err_t read_register(uint8_t register_address, uint16_t& value) {
    std::array<uint8_t, 2> data = {};
    const esp_err_t result = i2c_master_transmit_receive(
        device, &register_address, 1, data.data(), data.size(),
        transaction_timeout_ms);
    if (result != ESP_OK) return result;
    value = static_cast<uint16_t>(data[0]) << 8 | data[1];
    return ESP_OK;
}

void reset_sensor() {
    ready = false;
    has_filtered_reading = false;
    filtered = {};
    if (device != nullptr) {
        const esp_err_t result = i2c_master_bus_rm_device(device);
        if (result != ESP_OK) {
            ESP_LOGW(tag, "Could not detach failed INA219: %s",
                     esp_err_to_name(result));
        }
        device = nullptr;
    }
}

esp_err_t fail(esp_err_t result) {
    reset_sensor();
    return result;
}
}  // namespace

esp_err_t initialize() {
    if (ready) return ESP_OK;
    if (i2c::handle() == nullptr) return ESP_ERR_INVALID_STATE;
    if (!i2c::device_present(address)) return ESP_ERR_NOT_FOUND;

    const i2c_device_config_t device_configuration = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = board::i2c_frequency_hz,
        .scl_wait_us = 0,
        .flags = {.disable_ack_check = false},
    };
    esp_err_t result = i2c_master_bus_add_device(
        i2c::handle(), &device_configuration, &device);
    if (result != ESP_OK) return result;

    result = write_register(configuration_register, configuration);
    if (result != ESP_OK) return fail(result);
    result = write_register(calibration_register, calibration);
    if (result != ESP_OK) return fail(result);

    uint16_t calibration_check = 0;
    result = read_register(calibration_register, calibration_check);
    if (result != ESP_OK) return fail(result);
    if (calibration_check != calibration) {
        ESP_LOGE(tag, "Calibration readback mismatch: 0x%04X",
                 calibration_check);
        return fail(ESP_ERR_INVALID_RESPONSE);
    }

    ready = true;
    ESP_LOGI(tag,
             "INA219 ready at 0x40 (R100 shunt, +/-3.2 A measurement range)");
    return ESP_OK;
}

esp_err_t read(Reading& reading) {
    if (!ready) return ESP_ERR_INVALID_STATE;

    uint16_t bus_raw = 0;
    uint16_t current_raw = 0;
    uint16_t power_raw = 0;
    esp_err_t result = read_register(bus_voltage_register, bus_raw);
    if (result != ESP_OK) return fail(result);
    result = read_register(current_register, current_raw);
    if (result != ESP_OK) return fail(result);
    result = read_register(power_register, power_raw);
    if (result != ESP_OK) return fail(result);

    Reading measured = {
        .bus_voltage_v = static_cast<double>(bus_raw >> 3) * 0.004,
        .current_a = static_cast<double>(static_cast<int16_t>(current_raw)) *
                     current_lsb_a,
        .power_w = static_cast<double>(power_raw) * power_lsb_w,
    };
    const bool invalid =
        !std::isfinite(measured.bus_voltage_v) ||
        !std::isfinite(measured.current_a) || !std::isfinite(measured.power_w) ||
        measured.bus_voltage_v < 0.0 ||
        measured.bus_voltage_v > maximum_bus_voltage_v ||
        std::abs(measured.current_a) > maximum_absolute_current_a ||
        measured.power_w < 0.0 || measured.power_w > maximum_power_w;
    if (invalid) {
        ESP_LOGE(tag, "Rejected implausible reading: %.3f V, %.3f A, %.3f W",
                 measured.bus_voltage_v, measured.current_a,
                 measured.power_w);
        return fail(ESP_ERR_INVALID_RESPONSE);
    }

    if (!has_filtered_reading) {
        filtered = measured;
        has_filtered_reading = true;
    } else {
        filtered.bus_voltage_v += smoothing_alpha *
                                  (measured.bus_voltage_v -
                                   filtered.bus_voltage_v);
        filtered.current_a +=
            smoothing_alpha * (measured.current_a - filtered.current_a);
        filtered.power_w +=
            smoothing_alpha * (measured.power_w - filtered.power_w);
    }
    reading = filtered;
    return ESP_OK;
}

}  // namespace tpb9000::power
