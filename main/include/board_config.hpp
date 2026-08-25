#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"

namespace tpb9000::board {
inline constexpr char model[] = "Waveshare ESP32-S3-Touch-LCD-4.3B-BOX";
inline constexpr gpio_num_t i2c_sda = GPIO_NUM_8;
inline constexpr gpio_num_t i2c_scl = GPIO_NUM_9;
inline constexpr i2c_port_num_t i2c_port = I2C_NUM_0;
inline constexpr uint32_t i2c_frequency_hz = 100000;
inline constexpr uint16_t display_width = 800;
inline constexpr uint16_t display_height = 480;
inline constexpr uint32_t display_pixel_clock_hz = 16'000'000;
inline constexpr gpio_num_t display_data_pins[16] = {
    GPIO_NUM_14, GPIO_NUM_38, GPIO_NUM_18, GPIO_NUM_17,
    GPIO_NUM_10, GPIO_NUM_39, GPIO_NUM_0,  GPIO_NUM_45,
    GPIO_NUM_48, GPIO_NUM_47, GPIO_NUM_21, GPIO_NUM_1,
    GPIO_NUM_2,  GPIO_NUM_42, GPIO_NUM_41, GPIO_NUM_40,
};
inline constexpr gpio_num_t display_hsync = GPIO_NUM_46;
inline constexpr gpio_num_t display_vsync = GPIO_NUM_3;
inline constexpr gpio_num_t display_data_enable = GPIO_NUM_5;
inline constexpr gpio_num_t display_pixel_clock = GPIO_NUM_7;
inline constexpr uint8_t ch422g_mode_address = 0x24;
inline constexpr uint8_t ch422g_output_address = 0x38;
inline constexpr gpio_num_t rs485_rx = GPIO_NUM_43;
inline constexpr gpio_num_t rs485_tx = GPIO_NUM_44;
}  // namespace tpb9000::board
