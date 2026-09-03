#pragma once

#include <cstddef>
#include <cstdint>

namespace tpb9000::hopper_node::sensor {

class FrameParser {
  public:
    bool push(uint8_t byte, uint16_t& distance_mm);
    void reset();

  private:
    uint8_t frame_[4] = {};
    size_t length_ = 0;
};

}  // namespace tpb9000::hopper_node::sensor
