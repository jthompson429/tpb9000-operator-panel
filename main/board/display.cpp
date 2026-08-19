#include "display.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include "board_config.hpp"
#include "esp_check.h"
#include "esp_idf_version.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "i2c_bus.hpp"

namespace tpb9000::display {
namespace {
constexpr char tag[] = "display";
esp_lcd_panel_handle_t panel = nullptr;
uint16_t* framebuffer = nullptr;
uint16_t* alternate_framebuffer = nullptr;

esp_err_t enable_panel_and_backlight() {
    ESP_RETURN_ON_ERROR(i2c::write_byte(board::ch422g_mode_address, 0x01), tag,
                        "Could not configure CH422G");
    ESP_RETURN_ON_ERROR(i2c::write_byte(board::ch422g_output_address, 0x1E), tag,
                        "Could not enable LCD/backlight");
    return ESP_OK;
}

std::array<uint8_t, 7> glyph(char character) {
    const char c = static_cast<char>(std::toupper(
        static_cast<unsigned char>(character)));
    switch (c) {
        case '0': return {14, 17, 19, 21, 25, 17, 14};
        case '1': return {4, 12, 4, 4, 4, 4, 14};
        case '2': return {14, 17, 1, 2, 4, 8, 31};
        case '3': return {30, 1, 1, 14, 1, 1, 30};
        case '4': return {2, 6, 10, 18, 31, 2, 2};
        case '5': return {31, 16, 16, 30, 1, 1, 30};
        case '6': return {6, 8, 16, 30, 17, 17, 14};
        case '7': return {31, 1, 2, 4, 8, 8, 8};
        case '8': return {14, 17, 17, 14, 17, 17, 14};
        case '9': return {14, 17, 17, 15, 1, 2, 12};
        case 'A': return {14, 17, 17, 31, 17, 17, 17};
        case 'B': return {30, 17, 17, 30, 17, 17, 30};
        case 'C': return {14, 17, 16, 16, 16, 17, 14};
        case 'D': return {28, 18, 17, 17, 17, 18, 28};
        case 'E': return {31, 16, 16, 30, 16, 16, 31};
        case 'F': return {31, 16, 16, 30, 16, 16, 16};
        case 'G': return {14, 17, 16, 23, 17, 17, 15};
        case 'H': return {17, 17, 17, 31, 17, 17, 17};
        case 'I': return {14, 4, 4, 4, 4, 4, 14};
        case 'J': return {7, 2, 2, 2, 2, 18, 12};
        case 'K': return {17, 18, 20, 24, 20, 18, 17};
        case 'L': return {16, 16, 16, 16, 16, 16, 31};
        case 'M': return {17, 27, 21, 21, 17, 17, 17};
        case 'N': return {17, 25, 21, 19, 17, 17, 17};
        case 'O': return {14, 17, 17, 17, 17, 17, 14};
        case 'P': return {30, 17, 17, 30, 16, 16, 16};
        case 'Q': return {14, 17, 17, 17, 21, 18, 13};
        case 'R': return {30, 17, 17, 30, 20, 18, 17};
        case 'S': return {15, 16, 16, 14, 1, 1, 30};
        case 'T': return {31, 4, 4, 4, 4, 4, 4};
        case 'U': return {17, 17, 17, 17, 17, 17, 14};
        case 'V': return {17, 17, 17, 17, 17, 10, 4};
        case 'W': return {17, 17, 17, 21, 21, 21, 10};
        case 'X': return {17, 17, 10, 4, 10, 17, 17};
        case 'Y': return {17, 17, 10, 4, 4, 4, 4};
        case 'Z': return {31, 1, 2, 4, 8, 16, 31};
        case '.': return {0, 0, 0, 0, 0, 6, 6};
        case ':': return {0, 6, 6, 0, 6, 6, 0};
        case '-': return {0, 0, 0, 31, 0, 0, 0};
        case '%': return {17, 2, 4, 8, 17, 0, 0};
        default: return {};
    }
}
}  // namespace

esp_err_t initialize() {
    if (panel != nullptr) return ESP_OK;
    ESP_RETURN_ON_ERROR(enable_panel_and_backlight(), tag,
                        "Display power control failed");
    const esp_lcd_rgb_panel_config_t config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = board::display_pixel_clock_hz,
            .h_res = board::display_width,
            .v_res = board::display_height,
            .hsync_pulse_width = 4, .hsync_back_porch = 8, .hsync_front_porch = 8,
            .vsync_pulse_width = 4, .vsync_back_porch = 8, .vsync_front_porch = 8,
            .flags = {.hsync_idle_low = false, .vsync_idle_low = false,
                      .de_idle_high = false, .pclk_active_neg = true,
                      .pclk_idle_high = false},
        },
        .data_width = 16,
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
        .bits_per_pixel = 16,
#else
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
#endif
        .num_fbs = 2,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        .user_fbs = {},
#endif
        // Internal-RAM line buffers absorb PSRAM bandwidth spikes and keep
        // RGB frame timing locked. Rendering uses PSRAM-safe volatile writes.
        .bounce_buffer_size_px = board::display_width * 10,
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
        .sram_trans_align = 4,
        .psram_trans_align = 64,
#else
        .dma_burst_size = 64,
#endif
        .hsync_gpio_num = board::display_hsync,
        .vsync_gpio_num = board::display_vsync,
        .de_gpio_num = board::display_data_enable,
        .pclk_gpio_num = board::display_pixel_clock,
        .disp_gpio_num = GPIO_NUM_NC,
        .data_gpio_nums = {
            board::display_data_pins[0], board::display_data_pins[1],
            board::display_data_pins[2], board::display_data_pins[3],
            board::display_data_pins[4], board::display_data_pins[5],
            board::display_data_pins[6], board::display_data_pins[7],
            board::display_data_pins[8], board::display_data_pins[9],
            board::display_data_pins[10], board::display_data_pins[11],
            board::display_data_pins[12], board::display_data_pins[13],
            board::display_data_pins[14], board::display_data_pins[15]},
        .flags = {.disp_active_low = false, .refresh_on_demand = false,
                  .fb_in_psram = true, .double_fb = true, .no_fb = false,
                  .bb_invalidate_cache = false},
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&config, &panel), tag,
                        "Could not allocate RGB panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), tag, "Panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), tag, "Panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(
                            panel, 2, reinterpret_cast<void**>(&framebuffer),
                            reinterpret_cast<void**>(&alternate_framebuffer)),
                        tag, "Could not obtain framebuffer");
    // The RGB engine begins streaming buffer 0 immediately after panel init.
    // Render the first frame into buffer 1, which is not being scanned out.
    uint16_t* buffer_zero = framebuffer;
    framebuffer = alternate_framebuffer;
    alternate_framebuffer = buffer_zero;
    ESP_LOGI(tag, "800x480 RGB565 display ready; double framebuffer in PSRAM");
    return ESP_OK;
}

void clear(Color color) {
    if (framebuffer == nullptr) return;
    volatile uint16_t* pixels = framebuffer;
    const size_t count =
        static_cast<size_t>(board::display_width) * board::display_height;
    for (size_t index = 0; index < count; ++index) pixels[index] = color;
}

esp_err_t draw_background(const Color* source, size_t pixel_count) {
    const size_t expected_count =
        static_cast<size_t>(board::display_width) * board::display_height;
    if (framebuffer == nullptr || source == nullptr) return ESP_ERR_INVALID_STATE;
    if (pixel_count != expected_count) return ESP_ERR_INVALID_SIZE;
    volatile uint16_t* destination = framebuffer;
    for (size_t index = 0; index < expected_count; ++index) {
        destination[index] = source[index];
    }
    return ESP_OK;
}

void fill_rectangle(int x, int y, int width, int height, Color color) {
    if (framebuffer == nullptr || width <= 0 || height <= 0) return;
    const int left = x < 0 ? 0 : x;
    const int top = y < 0 ? 0 : y;
    const int right = x + width > board::display_width
                          ? board::display_width : x + width;
    const int bottom = y + height > board::display_height
                           ? board::display_height : y + height;
    volatile uint16_t* pixels = framebuffer;
    for (int row = top; row < bottom; ++row) {
        const size_t row_offset = static_cast<size_t>(row) * board::display_width;
        for (int column = left; column < right; ++column) {
            pixels[row_offset + column] = color;
        }
    }
}

void draw_text(int x, int y, const char* text, int scale, Color color) {
    if (framebuffer == nullptr || text == nullptr || scale <= 0) return;
    int cursor = x;
    for (const char* character = text; *character != '\0'; ++character) {
        const auto rows = glyph(*character);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((rows[row] & (1U << (4 - column))) != 0) {
                    fill_rectangle(cursor + column * scale, y + row * scale,
                                   scale, scale, color);
                }
            }
        }
        cursor += 6 * scale;
    }
}

esp_err_t present() {
    if (panel == nullptr || framebuffer == nullptr || alternate_framebuffer == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(
                            panel, 0, 0, board::display_width,
                            board::display_height, framebuffer),
                        tag, "Could not present framebuffer");
    uint16_t* completed = framebuffer;
    framebuffer = alternate_framebuffer;
    alternate_framebuffer = completed;
    return ESP_OK;
}
}  // namespace tpb9000::display
