#include <cassert>
#include <cmath>
#include <cstdint>

#include "hopper_sensor.hpp"

int main() {
    using namespace tpb9000::hopper;

    assert(std::abs(calculate_percent(5.8F) - 100.0F) < 0.01F);
    assert(std::abs(calculate_percent(39.2F)) < 0.01F);
    assert(std::abs(calculate_percent(15.4F) - 71.26F) < 0.02F);
    assert(calculate_percent(2.0F) == 100.0F);
    assert(calculate_percent(45.0F) == 0.0F);
    assert(calculate_bars(0.0F) == 0);
    assert(calculate_bars(1.0F) == 1);
    assert(calculate_bars(50.0F) == 3);
    assert(calculate_bars(100.0F) == 6);

    Processor processor;
    Reading reading = {};
    assert(processor.push(154, reading));
    assert(std::abs(reading.distance_cm - 15.4F) < 0.01F);
    assert(!processor.push(20, reading));
    assert(!processor.push(5000, reading));
    assert(processor.push(160, reading));
    assert(std::abs(reading.distance_cm - 15.7F) < 0.01F);
    processor.reset();
    assert(processor.push(392, reading));
    assert(reading.bars == 0);
}
