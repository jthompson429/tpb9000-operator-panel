#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace tpb9000::hopper {

inline constexpr float full_distance_cm = 5.8F;
inline constexpr float empty_distance_cm = 39.2F;

struct Reading {
    float distance_cm;
    float percent;
    uint8_t bars;
};

class Processor {
  public:
    bool push(uint16_t distance_mm, Reading& reading);
    void reset();

  private:
    static constexpr size_t history_size = 5;
    std::array<uint16_t, history_size> history_ = {};
    size_t history_count_ = 0;
    size_t history_next_ = 0;
    uint8_t displayed_bars_ = 0;
};

float calculate_percent(float distance_cm);
uint8_t calculate_bars(float percent);

const char* status_name(const Reading& reading);

}  // namespace tpb9000::hopper
