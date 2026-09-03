#pragma once
#include <cstddef>
#include <cstdint>
#include "esp_err.h"

namespace tpb9000::display {
using Color = uint16_t;

constexpr Color rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<Color>(((red & 0xF8U) << 8U) |
                              ((green & 0xFCU) << 3U) | (blue >> 3U));
}

esp_err_t initialize();
void clear(Color color);
esp_err_t draw_background(const Color* pixels, size_t pixel_count);
void fill_rectangle(int x, int y, int width, int height, Color color);
void draw_text(int x, int y, const char* text, int scale, Color color);
esp_err_t present();
}  // namespace tpb9000::display
