#include "hopper_receiver.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#if __has_include("private_config.h")
#include "private_config.h"
#endif

#ifndef TPB9000_HOPPER_NODE_MAC
#define TPB9000_HOPPER_NODE_MAC 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#endif

namespace tpb9000::hopper_radio {
namespace {
constexpr char tag[] = "hopper_radio";
constexpr uint16_t minimum_distance_mm = 30;
constexpr uint16_t maximum_distance_mm = 4500;
constexpr std::array<uint8_t, 6> expected_node_mac = {
    TPB9000_HOPPER_NODE_MAC};

enum class EventType : uint8_t {
    telemetry,
    unexpected_sender,
    wrong_size,
    wrong_magic,
    wrong_version,
};

struct ReceiveEvent {
    EventType type;
    std::array<uint8_t, 6> sender;
    hopper_protocol::HopperTelemetry telemetry;
    int received_size;
};

QueueHandle_t receive_queue = nullptr;
bool initialized = false;
bool sender_logged = false;
bool have_sequence = false;
uint32_t last_sequence = 0;

bool is_zero_mac(const std::array<uint8_t, 6>& address) {
    return std::all_of(address.begin(), address.end(),
                       [](uint8_t byte) { return byte == 0; });
}

void receive_callback(const esp_now_recv_info_t* information,
                      const uint8_t* data, int data_length) {
    if (receive_queue == nullptr || information == nullptr ||
        information->src_addr == nullptr || data == nullptr) {
        return;
    }

    ReceiveEvent event = {};
    std::memcpy(event.sender.data(), information->src_addr,
                event.sender.size());
    event.received_size = data_length;
    if (!is_zero_mac(expected_node_mac) &&
        event.sender != expected_node_mac) {
        event.type = EventType::unexpected_sender;
    } else if (data_length != sizeof(hopper_protocol::HopperTelemetry)) {
        event.type = EventType::wrong_size;
    } else {
        std::memcpy(&event.telemetry, data, sizeof(event.telemetry));
        if (event.telemetry.magic != hopper_protocol::magic) {
            event.type = EventType::wrong_magic;
        } else if (event.telemetry.protocol_version !=
                       hopper_protocol::version ||
                   event.telemetry.packet_size != sizeof(event.telemetry)) {
            event.type = EventType::wrong_version;
        } else {
            event.type = EventType::telemetry;
        }
    }
    xQueueSend(receive_queue, &event, 0);
}

void log_mac(const char* prefix, const std::array<uint8_t, 6>& address) {
    ESP_LOGI(tag, "%s %02X:%02X:%02X:%02X:%02X:%02X", prefix,
             address[0], address[1], address[2], address[3], address[4],
             address[5]);
}
}  // namespace

esp_err_t initialize() {
    if (initialized) return ESP_OK;
    receive_queue = xQueueCreate(8, sizeof(ReceiveEvent));
    if (receive_queue == nullptr) return ESP_ERR_NO_MEM;

    esp_err_t result = esp_now_init();
    if (result != ESP_OK) {
        vQueueDelete(receive_queue);
        receive_queue = nullptr;
        return result;
    }
    result = esp_now_register_recv_cb(receive_callback);
    if (result != ESP_OK) {
        esp_now_deinit();
        vQueueDelete(receive_queue);
        receive_queue = nullptr;
        return result;
    }

    uint8_t own_mac_bytes[6] = {};
    ESP_RETURN_ON_ERROR(esp_wifi_get_mac(WIFI_IF_STA, own_mac_bytes), tag,
                        "Could not read panel station MAC");
    std::array<uint8_t, 6> own_mac = {};
    std::copy(std::begin(own_mac_bytes), std::end(own_mac_bytes),
              own_mac.begin());
    log_mac("Panel station MAC:", own_mac);
    if (is_zero_mac(expected_node_mac)) {
        ESP_LOGW(tag,
                 "Hopper-node MAC not configured; commissioning mode accepts "
                 "valid TPB9000 telemetry from any sender");
    } else {
        log_mac("Expected hopper-node peer:", expected_node_mac);
    }
    initialized = true;
    ESP_LOGI(tag, "ESP-NOW receiver READY");
    return ESP_OK;
}

esp_err_t receive(Sample& sample, uint32_t timeout_ms) {
    if (!initialized) return ESP_ERR_INVALID_STATE;
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    ReceiveEvent event = {};
    while (static_cast<int32_t>(deadline - xTaskGetTickCount()) > 0) {
        const TickType_t remaining = deadline - xTaskGetTickCount();
        if (xQueueReceive(receive_queue, &event, remaining) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
        switch (event.type) {
            case EventType::unexpected_sender:
                ESP_LOGW(tag,
                         "Ignored telemetry from unexpected sender "
                         "%02X:%02X:%02X:%02X:%02X:%02X",
                         event.sender[0], event.sender[1], event.sender[2],
                         event.sender[3], event.sender[4], event.sender[5]);
                continue;
            case EventType::wrong_size:
                ESP_LOGW(tag, "Ignored packet with size %d (expected %u)",
                         event.received_size,
                         static_cast<unsigned>(
                             sizeof(hopper_protocol::HopperTelemetry)));
                continue;
            case EventType::wrong_magic:
                ESP_LOGW(tag, "Ignored non-TPB9000 ESP-NOW packet");
                continue;
            case EventType::wrong_version:
                ESP_LOGW(tag,
                         "Ignored incompatible hopper protocol version %u",
                         event.telemetry.protocol_version);
                continue;
            case EventType::telemetry:
                break;
        }

        if (!sender_logged) {
            log_mac("Receiving hopper telemetry from", event.sender);
            sender_logged = true;
        }
        const uint32_t sequence = event.telemetry.sequence;
        if (have_sequence && sequence != last_sequence + 1U) {
            const uint32_t forward_gap = sequence - (last_sequence + 1U);
            if (forward_gap < 100'000U) {
                ESP_LOGW(tag, "Packet gap: expected #%lu, received #%lu",
                         static_cast<unsigned long>(last_sequence + 1U),
                         static_cast<unsigned long>(sequence));
            } else {
                ESP_LOGI(tag,
                         "Sequence restarted/out of order: #%lu after #%lu",
                         static_cast<unsigned long>(sequence),
                         static_cast<unsigned long>(last_sequence));
            }
        }
        have_sequence = true;
        last_sequence = sequence;
        sample = {
            .sequence = sequence,
            .distance_mm = event.telemetry.distance_mm,
            .sensor_status = event.telemetry.sensor_status,
        };
        ESP_LOGI(tag, "RX #%lu | Distance: %u mm | Sensor: %s",
                 static_cast<unsigned long>(sequence), sample.distance_mm,
                 sample.sensor_status == hopper_protocol::SensorStatus::ok
                     ? "OK"
                     : "NO DATA");
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

}  // namespace tpb9000::hopper_radio
