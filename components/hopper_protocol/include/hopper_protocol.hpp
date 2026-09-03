#pragma once

#include <cstdint>
#include <type_traits>

namespace tpb9000::hopper_protocol {

inline constexpr uint16_t magic = 0x4854;  // "TH" on the wire.
inline constexpr uint8_t version = 1;

enum class SensorStatus : uint8_t {
    ok = 0,
    no_data = 1,
    invalid_frame = 2,
};

// This structure is the wire format. Keep it fixed-width, packed, and shared by
// both applications. battery_mv is reserved for the later battery/solar phase;
// zero means that battery telemetry is not available.
struct __attribute__((packed)) HopperTelemetry {
    uint16_t magic;
    uint8_t protocol_version;
    uint8_t packet_size;
    uint32_t sequence;
    uint16_t distance_mm;
    uint16_t battery_mv;
    SensorStatus sensor_status;
    uint8_t reserved[3];
};

static_assert(sizeof(HopperTelemetry) == 16);
static_assert(std::is_trivially_copyable_v<HopperTelemetry>);

inline HopperTelemetry make_telemetry(uint32_t sequence,
                                      uint16_t distance_mm,
                                      SensorStatus status) {
    return {
        .magic = magic,
        .protocol_version = version,
        .packet_size = sizeof(HopperTelemetry),
        .sequence = sequence,
        .distance_mm = distance_mm,
        .battery_mv = 0,
        .sensor_status = status,
        .reserved = {},
    };
}

}  // namespace tpb9000::hopper_protocol
