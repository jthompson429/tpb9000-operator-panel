#include <cassert>
#include <cstdint>

#include "a02_frame_parser.hpp"

int main() {
    tpb9000::hopper_node::sensor::FrameParser parser;
    uint16_t distance_mm = 0;
    const uint8_t valid[] = {0xFF, 0x00, 0x9A, 0x99};
    for (size_t index = 0; index < 3; ++index) {
        assert(!parser.push(valid[index], distance_mm));
    }
    assert(parser.push(valid[3], distance_mm));
    assert(distance_mm == 154);

    const uint8_t corrupt[] = {0xFF, 0x01, 0x00, 0x01};
    for (uint8_t byte : corrupt) assert(!parser.push(byte, distance_mm));
}
