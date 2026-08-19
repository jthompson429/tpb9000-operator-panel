#include "environment_sensor.hpp"

#include <array>
#include <cmath>
#include <cstdint>

#include "bme280.h"
#include "board_config.hpp"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "i2c_bus.hpp"

namespace tpb9000::environment {
namespace {
constexpr char tag[] = "bme280";
constexpr uint8_t primary_address = 0x76;
constexpr uint8_t alternate_address = 0x77;
constexpr int transaction_timeout_ms = 100;
constexpr double minimum_temperature_c = -40.0;
constexpr double maximum_temperature_c = 70.0;
constexpr double minimum_pressure_pa = 80000.0;
constexpr double maximum_pressure_pa = 110000.0;
constexpr double maximum_temperature_step_c = 8.5;
constexpr double maximum_humidity_step_percent = 20.0;
constexpr double maximum_pressure_step_pa = 1000.0;

bme280_dev sensor = {};
i2c_master_dev_handle_t i2c_device = nullptr;
bool ready = false;
bme280_data previous_data = {};
bool has_previous_data = false;

int8_t read_registers(uint8_t register_address, uint8_t* data, uint32_t length,
                      void* interface_pointer) {
    auto device = static_cast<i2c_master_dev_handle_t>(interface_pointer);
    const esp_err_t result = i2c_master_transmit_receive(
        device, &register_address, 1, data, length, transaction_timeout_ms);
    return result == ESP_OK ? BME280_INTF_RET_SUCCESS : BME280_E_COMM_FAIL;
}

int8_t write_registers(uint8_t register_address, const uint8_t* data,
                       uint32_t length, void* interface_pointer) {
    if (length > BME280_MAX_LEN) return BME280_E_INVALID_LEN;
    std::array<uint8_t, BME280_MAX_LEN + 1> transaction = {};
    transaction[0] = register_address;
    for (uint32_t index = 0; index < length; ++index) {
        transaction[index + 1] = data[index];
    }
    auto device = static_cast<i2c_master_dev_handle_t>(interface_pointer);
    const esp_err_t result = i2c_master_transmit(
        device, transaction.data(), length + 1, transaction_timeout_ms);
    return result == ESP_OK ? BME280_INTF_RET_SUCCESS : BME280_E_COMM_FAIL;
}

void delay_microseconds(uint32_t period, void*) {
    esp_rom_delay_us(period);
}

esp_err_t add_i2c_device(uint8_t address) {
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = board::i2c_frequency_hz,
        .scl_wait_us = 0,
        .flags = {.disable_ack_check = false},
    };
    return i2c_master_bus_add_device(i2c::handle(), &config, &i2c_device);
}

void reset_sensor() {
    ready = false;
    has_previous_data = false;
    previous_data = {};
    sensor = {};
    if (i2c_device != nullptr) {
        const esp_err_t result = i2c_master_bus_rm_device(i2c_device);
        if (result != ESP_OK) {
            ESP_LOGW(tag, "Could not detach failed BME280 device: %s",
                     esp_err_to_name(result));
        }
        i2c_device = nullptr;
    }
}
}  // namespace

esp_err_t initialize() {
    if (ready) return ESP_OK;
    if (i2c::handle() == nullptr) return ESP_ERR_INVALID_STATE;

    const uint8_t address = i2c::device_present(primary_address)
                                ? primary_address
                                : alternate_address;
    if (!i2c::device_present(address)) {
        ESP_LOGE(tag, "No sensor found at 0x76 or 0x77");
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t result = add_i2c_device(address);
    if (result != ESP_OK) return result;

    sensor.intf = BME280_I2C_INTF;
    sensor.intf_ptr = i2c_device;
    sensor.read = read_registers;
    sensor.write = write_registers;
    sensor.delay_us = delay_microseconds;

    int8_t sensor_result = bme280_init(&sensor);
    if (sensor_result != BME280_OK) {
        ESP_LOGE(tag, "Bosch driver initialization failed: %d", sensor_result);
        reset_sensor();
        return ESP_FAIL;
    }

    bme280_settings settings = {};
    sensor_result = bme280_get_sensor_settings(&settings, &sensor);
    if (sensor_result != BME280_OK) {
        reset_sensor();
        return ESP_FAIL;
    }
    settings.osr_t = BME280_OVERSAMPLING_2X;
    settings.osr_p = BME280_OVERSAMPLING_16X;
    settings.osr_h = BME280_OVERSAMPLING_1X;
    settings.filter = BME280_FILTER_COEFF_16;
    settings.standby_time = BME280_STANDBY_TIME_1000_MS;
    sensor_result = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS,
                                                &settings, &sensor);
    if (sensor_result != BME280_OK) {
        reset_sensor();
        return ESP_FAIL;
    }
    sensor_result = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &sensor);
    if (sensor_result != BME280_OK) {
        reset_sensor();
        return ESP_FAIL;
    }

    // Do not expose the power-on register contents as the first live reading.
    uint32_t measurement_delay_us = 0;
    sensor_result = bme280_cal_meas_delay(&measurement_delay_us, &settings);
    if (sensor_result != BME280_OK) {
        reset_sensor();
        return ESP_FAIL;
    }
    sensor.delay_us(measurement_delay_us, sensor.intf_ptr);

    ready = true;
    ESP_LOGI(tag, "BME280 ready at 0x%02X (chip ID 0x%02X)", address,
             sensor.chip_id);
    return ESP_OK;
}

esp_err_t read(Reading& reading) {
    if (!ready) return ESP_ERR_INVALID_STATE;
    bme280_data data = {};
    const int8_t result = bme280_get_sensor_data(BME280_ALL, &data, &sensor);
    if (result != BME280_OK) {
        ESP_LOGE(tag, "Sensor read failed: %d", result);
        reset_sensor();
        return ESP_FAIL;
    }
    const bool outside_operating_range =
        !std::isfinite(data.temperature) || !std::isfinite(data.humidity) ||
        !std::isfinite(data.pressure) ||
        data.temperature < minimum_temperature_c ||
        data.temperature > maximum_temperature_c || data.humidity < 0.0 ||
        data.humidity > 100.0 || data.pressure < minimum_pressure_pa ||
        data.pressure >= maximum_pressure_pa;
    const bool implausible_step =
        has_previous_data &&
        (std::abs(data.temperature - previous_data.temperature) >
             maximum_temperature_step_c ||
         std::abs(data.humidity - previous_data.humidity) >
             maximum_humidity_step_percent ||
         std::abs(data.pressure - previous_data.pressure) >
             maximum_pressure_step_pa);
    if (outside_operating_range || implausible_step) {
        ESP_LOGE(tag,
                 "Rejected implausible reading: %.2f C, %.2f %%RH, %.2f hPa",
                 data.temperature, data.humidity, data.pressure / 100.0);
        reset_sensor();
        return ESP_ERR_INVALID_RESPONSE;
    }
    previous_data = data;
    has_previous_data = true;
    reading.temperature_c = data.temperature;
    reading.temperature_f = data.temperature * 9.0 / 5.0 + 32.0;
    reading.humidity_percent = data.humidity;
    reading.pressure_pa = data.pressure;
    reading.pressure_hpa = data.pressure / 100.0;
    return ESP_OK;
}

}  // namespace tpb9000::environment
