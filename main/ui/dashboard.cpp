#include "dashboard.hpp"

#include <cstdio>

#include "display.hpp"

namespace tpb9000::dashboard {
namespace {
constexpr auto background = display::rgb565(10, 18, 28);
constexpr auto header = display::rgb565(15, 32, 46);
constexpr auto card = display::rgb565(22, 43, 57);
constexpr auto accent = display::rgb565(43, 187, 173);
constexpr auto warm = display::rgb565(241, 183, 55);
constexpr auto text = display::rgb565(238, 243, 244);
constexpr auto muted = display::rgb565(143, 165, 174);
constexpr auto error = display::rgb565(220, 65, 65);

void draw_shell(bool network_online) {
    display::clear(background);
    display::fill_rectangle(0, 0, 800, 88, header);
    display::fill_rectangle(0, 84, 800, 4, accent);
    display::draw_text(34, 22, "TPB9000", 6, text);
    display::draw_text(570, 32, "OPERATOR PANEL", 3, muted);
    display::draw_text(34, 108, "ENCLOSURE ENVIRONMENT", 3, muted);
    if (!network_online) {
        display::fill_rectangle(0, 0, 8, 480, error);
        display::fill_rectangle(792, 0, 8, 480, error);
    }
}

void draw_card(int x, const char* label, const char* value, const char* unit,
               display::Color value_color) {
    display::fill_rectangle(x, 154, 232, 256, card);
    display::fill_rectangle(x, 154, 232, 5, accent);
    display::draw_text(x + 22, 180, label, 3, muted);
    display::draw_text(x + 20, 252, value, 6, value_color);
    display::draw_text(x + 22, 344, unit, 3, muted);
}
}  // namespace

esp_err_t show_reading(const environment::Reading& reading,
                       bool network_online) {
    char temperature[16] = {};
    char humidity[16] = {};
    char pressure[16] = {};
    std::snprintf(temperature, sizeof(temperature), "%.1f", reading.temperature_f);
    std::snprintf(humidity, sizeof(humidity), "%.1f", reading.humidity_percent);
    std::snprintf(pressure, sizeof(pressure), "%.1f", reading.pressure_hpa);

    draw_shell(network_online);
    draw_card(34, "TEMPERATURE", temperature, "DEGREES F", warm);
    draw_card(284, "HUMIDITY", humidity, "PERCENT RH", text);
    draw_card(534, "PRESSURE", pressure, "HPA", text);
    display::draw_text(34, 442, network_online ? "NETWORK ONLINE"
                                               : "NETWORK OFFLINE",
                       3, network_online ? accent : error);
    return display::present();
}

esp_err_t show_sensor_error(bool network_online) {
    draw_shell(network_online);
    display::fill_rectangle(34, 154, 732, 256, card);
    display::fill_rectangle(34, 154, 732, 6, error);
    display::draw_text(82, 226, "SENSOR ERROR", 8, error);
    display::draw_text(166, 326, "CHECK BME280 CONNECTION", 3, text);
    display::draw_text(34, 442, network_online ? "NETWORK ONLINE"
                                               : "NETWORK OFFLINE",
                       3, network_online ? accent : error);
    return display::present();
}

}  // namespace tpb9000::dashboard
