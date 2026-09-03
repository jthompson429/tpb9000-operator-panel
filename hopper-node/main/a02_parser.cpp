#include "a02_frame_parser.hpp"

#include <algorithm>
#include <iterator>

namespace tpb9000::hopper_node::sensor {
namespace {
constexpr uint16_t minimum_distance_mm = 30;
constexpr uint16_t maximum_distance_mm = 4500;
}

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
                       candidate >= minimum_distance_mm &&
                       candidate <= maximum_distance_mm;
    const bool trailing_header = frame_[3] == 0xFF;
    length_ = trailing_header ? 1 : 0;
    if (trailing_header) frame_[0] = 0xFF;
    if (!valid) return false;
    distance_mm = candidate;
    return true;
}

void FrameParser::reset() {
    std::fill(std::begin(frame_), std::end(frame_), 0);
    length_ = 0;
}

}  // namespace tpb9000::hopper_node::sensor
