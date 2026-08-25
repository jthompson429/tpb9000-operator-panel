#include "hopper_sensor.hpp"

#include <algorithm>
#include <cmath>

namespace tpb9000::hopper {
namespace {
constexpr uint16_t sensor_minimum_mm = 30;
constexpr uint16_t sensor_maximum_mm = 4500;
}  // namespace

bool FrameParser::push(uint8_t byte, uint16_t& distance_mm) {
    if (length_ == 0) {
        if (byte != 0xFF) return false;
        frame_[length_++] = byte;
        return false;
    }

    frame_[length_++] = byte;
    if (length_ < sizeof(frame_)) return false;

    const uint8_t checksum =
        static_cast<uint8_t>(frame_[0] + frame_[1] + frame_[2]);
    const uint16_t candidate =
        static_cast<uint16_t>((frame_[1] << 8U) | frame_[2]);
    const bool valid = checksum == frame_[3] &&
                       candidate >= sensor_minimum_mm &&
                       candidate <= sensor_maximum_mm;

    const bool trailing_header = frame_[3] == 0xFF;
    length_ = trailing_header ? 1 : 0;
    if (trailing_header) frame_[0] = 0xFF;
    if (!valid) return false;
    distance_mm = candidate;
    return true;
}

void FrameParser::reset() {
    frame_[0] = frame_[1] = frame_[2] = frame_[3] = 0;
    length_ = 0;
}

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

}  // namespace tpb9000::hopper
