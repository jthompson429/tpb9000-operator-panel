#include "espnow_sender.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#if __has_include("hopper_node_config.hpp")
#include "hopper_node_config.hpp"
#else
#include "hopper_node_config.example.hpp"
#endif

namespace tpb9000::hopper_node::radio {
namespace {
constexpr char tag[] = "hopper_radio";
constexpr std::array<uint8_t, 6> panel_mac = {TPB9000_PANEL_MAC};
bool ready = false;

bool is_zero_mac(const std::array<uint8_t, 6>& address) {
    return std::all_of(address.begin(), address.end(),
                       [](uint8_t byte) { return byte == 0; });
}

void send_complete(const esp_now_send_info_t*, esp_now_send_status_t status) {
    ESP_LOGI(tag, "Send: %s",
             status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAILED");
}
}  // namespace

bool peer_configured() { return !is_zero_mac(panel_mac); }

esp_err_t initialize() {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), tag, "NVS erase failed");
        result = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(result, tag, "NVS initialization failed");
    ESP_RETURN_ON_ERROR(esp_netif_init(), tag, "esp-netif initialization failed");
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;

    wifi_init_config_t wifi_configuration = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_configuration), tag,
                        "Wi-Fi initialization failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), tag,
                        "Wi-Fi storage selection failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), tag,
                        "Wi-Fi mode selection failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), tag, "Wi-Fi start failed");
    ESP_RETURN_ON_ERROR(
        esp_wifi_set_channel(TPB9000_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE),
        tag, "Wi-Fi channel selection failed");

    uint8_t own_mac[6] = {};
    ESP_RETURN_ON_ERROR(esp_wifi_get_mac(WIFI_IF_STA, own_mac), tag,
                        "Could not read station MAC");
    ESP_LOGI(tag, "Hopper station MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             own_mac[0], own_mac[1], own_mac[2], own_mac[3], own_mac[4],
             own_mac[5]);
    ESP_LOGI(tag, "ESP-NOW channel: %d", TPB9000_ESPNOW_CHANNEL);

    ESP_RETURN_ON_ERROR(esp_now_init(), tag, "ESP-NOW initialization failed");
    ESP_RETURN_ON_ERROR(esp_now_register_send_cb(send_complete), tag,
                        "ESP-NOW send callback registration failed");
    if (!peer_configured()) {
        ESP_LOGW(tag,
                 "Panel MAC is not configured; copy the logged panel MAC into "
                 "hopper_node_config.hpp before bench transmission");
        return ESP_OK;
    }

    esp_now_peer_info_t peer = {};
    std::memcpy(peer.peer_addr, panel_mac.data(), panel_mac.size());
    peer.channel = 0;  // Use the radio's configured/current channel.
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_RETURN_ON_ERROR(esp_now_add_peer(&peer), tag,
                        "Could not register operator-panel peer");
    ready = true;
    ESP_LOGI(tag, "ESP-NOW READY; panel peer %02X:%02X:%02X:%02X:%02X:%02X",
             panel_mac[0], panel_mac[1], panel_mac[2], panel_mac[3],
             panel_mac[4], panel_mac[5]);
    return ESP_OK;
}

esp_err_t send(const hopper_protocol::HopperTelemetry& telemetry) {
    if (!ready) return ESP_ERR_INVALID_STATE;
    return esp_now_send(panel_mac.data(),
                        reinterpret_cast<const uint8_t*>(&telemetry),
                        sizeof(telemetry));
}

}  // namespace tpb9000::hopper_node::radio
