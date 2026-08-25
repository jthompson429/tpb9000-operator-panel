#include <cassert>
#include <cmath>
#include <cstdint>

#include "hopper_sensor.hpp"

int main() {
    using namespace tpb9000::hopper;

    FrameParser parser;
    uint16_t distance_mm = 0;
    const uint8_t valid[] = {0xFF, 0x00, 0x9A, 0x99};
    for (size_t index = 0; index < 3; ++index) {
        assert(!parser.push(valid[index], distance_mm));
    }
    assert(parser.push(valid[3], distance_mm));
    assert(distance_mm == 154);

    const uint8_t corrupt[] = {0xFF, 0x01, 0x00, 0x01};
    for (uint8_t byte : corrupt) assert(!parser.push(byte, distance_mm));

    assert(std::abs(calculate_percent(5.8F) - 100.0F) < 0.01F);
    assert(std::abs(calculate_percent(39.2F)) < 0.01F);
    assert(std::abs(calculate_percent(15.4F) - 71.26F) < 0.02F);
    assert(calculate_percent(2.0F) == 100.0F);
    assert(calculate_percent(45.0F) == 0.0F);
    assert(calculate_bars(0.0F) == 0);
    assert(calculate_bars(1.0F) == 1);
    assert(calculate_bars(50.0F) == 3);
    assert(calculate_bars(100.0F) == 6);
}
