#include "dashboard.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "display.hpp"
#include "esp_app_desc.h"

namespace tpb9000::dashboard {
namespace {
constexpr auto text = display::rgb565(238, 243, 244);
constexpr auto muted = display::rgb565(176, 176, 176);
constexpr auto online = display::rgb565(34, 197, 94);
constexpr auto error = display::rgb565(220, 65, 65);
constexpr auto power_accent = display::rgb565(0, 209, 255);
constexpr auto gauge_outline = display::rgb565(67, 100, 122);
constexpr auto gauge_empty = display::rgb565(12, 20, 28);
constexpr auto gauge_green = display::rgb565(34, 197, 94);
constexpr auto gauge_amber = display::rgb565(245, 158, 11);

extern const uint8_t background_normal_start[]
    asm("_binary_background_normal_rgb565_start");
extern const uint8_t background_normal_end[]
    asm("_binary_background_normal_rgb565_end");
extern const uint8_t background_offline_start[]
    asm("_binary_background_offline_rgb565_start");
extern const uint8_t background_offline_end[]
    asm("_binary_background_offline_rgb565_end");

esp_err_t draw_shell(bool network_online) {
    const uint8_t* start = network_online ? background_normal_start
                                          : background_offline_start;
    const uint8_t* end = network_online ? background_normal_end
                                        : background_offline_end;
    return display::draw_background(
        reinterpret_cast<const display::Color*>(start),
        static_cast<size_t>(end - start) / sizeof(display::Color));
}

int text_width(const char* value, int scale) {
    const size_t length = std::strlen(value);
    return length == 0 ? 0 : static_cast<int>((length * 6 - 1) * scale);
}

void draw_centered_value(int center_x, int y, const char* value, int scale) {
    display::draw_text(center_x - text_width(value, scale) / 2, y, value,
                       scale, text);
}

void draw_hopper_gauge(bool available, const hopper::Reading& reading) {
    constexpr int gauge_x = 288;
    constexpr int gauge_width = 17;
    constexpr int segment_height = 20;
    constexpr int segment_gap = 5;
    constexpr int gauge_bottom = 322;

    display::Color fill = gauge_green;
    if (reading.bars == 1) {
        fill = error;
    } else if (reading.bars <= 3) {
        fill = gauge_amber;
    }

    char percent[8] = "--";
    if (available) {
        std::snprintf(percent, sizeof(percent), "%.0f%%", reading.percent);
    }
    display::draw_text(296 - text_width(percent, 2) / 2, 128, percent, 2,
                       available ? fill : muted);
    display::draw_text(296 - text_width("KIBBLE", 1) / 2, 151, "KIBBLE", 1,
                       power_accent);

    for (uint8_t segment = 0; segment < 6; ++segment) {
        const int y = gauge_bottom - segment_height -
                      segment * (segment_height + segment_gap);
        display::fill_rectangle(gauge_x, y, gauge_width, segment_height,
                                gauge_outline);
        display::fill_rectangle(gauge_x + 2, y + 2, gauge_width - 4,
                                segment_height - 4,
                                available && segment < reading.bars
                                    ? fill
                                    : gauge_empty);
    }

}

void draw_footer(bool network_online,
                 bool power_available,
                 const power::Reading& power_reading) {
    constexpr int footer_y = 413;
    const char* network_text = network_online ? "NETWORK ONLINE"
                                              : "NETWORK OFFLINE";
    display::draw_text(64, footer_y, network_text, 2,
                       network_online ? online : error);

    char power_text[40] = {};
    if (power_available) {
        const double displayed_current =
            std::abs(power_reading.current_a) < 0.005
                ? 0.0
                : power_reading.current_a;
        std::snprintf(power_text, sizeof(power_text),
                      "PWR %.2fV %.2fA %.2fW",
                      power_reading.bus_voltage_v, displayed_current,
                      power_reading.power_w);
    } else {
        std::snprintf(power_text, sizeof(power_text), "PWR UNAVAILABLE");
    }
    display::draw_text(400 - text_width(power_text, 2) / 2, footer_y,
                       power_text, 2,
                       power_available ? power_accent : error);

    char firmware_text[24] = {};
    std::snprintf(firmware_text, sizeof(firmware_text), "FW %.13s",
                  esp_app_get_description()->version);
    display::draw_text(736 - text_width(firmware_text, 2), footer_y,
                       firmware_text, 2, muted);
}
}  // namespace

esp_err_t show_reading(const environment::Reading& reading,
                       bool network_online,
                       bool power_available,
                       const power::Reading& power_reading,
                       bool hopper_available,
                       const hopper::Reading& hopper_reading) {
    char temperature[16] = {};
    char humidity[16] = {};
    char pressure[16] = {};
    std::snprintf(temperature, sizeof(temperature), "%.1f", reading.temperature_f);
    std::snprintf(humidity, sizeof(humidity), "%.1f", reading.humidity_percent);
    std::snprintf(pressure, sizeof(pressure), "%.1f", reading.pressure_hpa);

    const esp_err_t shell_result = draw_shell(network_online);
    if (shell_result != ESP_OK) return shell_result;
    draw_centered_value(388, 226, temperature, 5);
    draw_centered_value(542, 226, humidity, 5);
    draw_centered_value(691, 230, pressure, 4);
    draw_hopper_gauge(hopper_available, hopper_reading);
    draw_footer(network_online, power_available, power_reading);
    return display::present();
}

esp_err_t show_sensor_error(bool network_online,
                            bool power_available,
                            const power::Reading& power_reading,
                            bool hopper_available,
                            const hopper::Reading& hopper_reading) {
    const esp_err_t shell_result = draw_shell(network_online);
    if (shell_result != ESP_OK) return shell_result;
    display::fill_rectangle(315, 94, 449, 316, display::rgb565(8, 12, 18));
    display::fill_rectangle(315, 94, 449, 5, error);
    display::draw_text(373, 205, "SENSOR ERROR", 6, error);
    display::draw_text(371, 282, "CHECK BME280", 3, text);
    draw_hopper_gauge(hopper_available, hopper_reading);
    draw_footer(network_online, power_available, power_reading);
    return display::present();
}

}  // namespace tpb9000::dashboard
