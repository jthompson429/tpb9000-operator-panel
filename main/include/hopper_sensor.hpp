#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

namespace tpb9000::hopper {

inline constexpr float full_distance_cm = 5.8F;
inline constexpr float empty_distance_cm = 39.2F;

struct Reading {
    float distance_cm;
    float percent;
    uint8_t bars;
};

class FrameParser {
  public:
    bool push(uint8_t byte, uint16_t& distance_mm);
    void reset();

  private:
    uint8_t frame_[4] = {};
    size_t length_ = 0;
};

float calculate_percent(float distance_cm);
uint8_t calculate_bars(float percent);

// Initializes UART1 on the panel's protected RS485 receive path. The A02
// sensor requires an external TTL-to-RS485 transmitter; raw TTL must never be
// connected directly to the panel's A/B terminals.
esp_err_t initialize();

// Waits for validated sensor data and returns one filtered, calibrated sample.
// ESP_ERR_TIMEOUT means no current reading is available and callers must not
// continue displaying an older value as live.
esp_err_t read(Reading& reading);

const char* status_name(const Reading& reading);

}  // namespace tpb9000::hopper
