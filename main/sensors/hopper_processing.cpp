#include "hopper_sensor.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace tpb9000::hopper {
namespace {
constexpr uint16_t sensor_minimum_mm = 30;
constexpr uint16_t sensor_maximum_mm = 4500;
constexpr float bar_hysteresis_percent = 3.0F;
}  // namespace

float calculate_percent(float distance_cm) {
    const float span = empty_distance_cm - full_distance_cm;
    const float percent =
        (empty_distance_cm - distance_cm) * 100.0F / span;
    return std::clamp(percent, 0.0F, 100.0F);
}

uint8_t calculate_bars(float percent) {
    if (percent <= 0.0F) return 0;
    return static_cast<uint8_t>(
        std::clamp(static_cast<int>(std::ceil(percent * 6.0F / 100.0F)),
                   1, 6));
}

bool Processor::push(uint16_t distance_mm, Reading& reading) {
    if (distance_mm < sensor_minimum_mm || distance_mm > sensor_maximum_mm) {
        return false;
    }
    history_[history_next_] = distance_mm;
    history_next_ = (history_next_ + 1) % history_size;
    if (history_count_ < history_size) ++history_count_;

    std::array<uint16_t, history_size> sorted = history_;
    std::sort(sorted.begin(), sorted.begin() + history_count_);
    float filtered_mm = 0;
    if ((history_count_ & 1U) != 0U) {
        filtered_mm = sorted[history_count_ / 2];
    } else {
        const size_t upper = history_count_ / 2;
        filtered_mm =
            (sorted[upper - 1] + sorted[upper]) / 2.0F;
    }
    reading.distance_cm = filtered_mm / 10.0F;
    reading.percent = calculate_percent(reading.distance_cm);

    const uint8_t ideal = calculate_bars(reading.percent);
    if (history_count_ == 1) {
        displayed_bars_ = ideal;
    } else if (ideal > displayed_bars_) {
        const float threshold = displayed_bars_ * (100.0F / 6.0F) +
                                bar_hysteresis_percent;
        if (reading.percent >= threshold) displayed_bars_ = ideal;
    } else if (ideal < displayed_bars_) {
        const float threshold = (displayed_bars_ - 1) * (100.0F / 6.0F) -
                                bar_hysteresis_percent;
        if (reading.percent <= threshold) displayed_bars_ = ideal;
    }
    reading.bars = displayed_bars_;
    return true;
}

void Processor::reset() {
    history_.fill(0);
    history_count_ = 0;
    history_next_ = 0;
    displayed_bars_ = 0;
}

const char* status_name(const Reading& reading) {
    if (reading.bars == 0) return "Empty";
    if (reading.bars == 1) return "Critical";
    if (reading.bars <= 3) return "Low";
    return "Normal";
}

}  // namespace tpb9000::hopper
