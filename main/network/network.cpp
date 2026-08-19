#include "network.hpp"

#include <atomic>
#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"
#include "nvs_flash.h"

#if __has_include("private_config.h")
#include "private_config.h"
#endif

#ifndef TPB9000_WIFI_SSID
#define TPB9000_WIFI_SSID ""
#endif
#ifndef TPB9000_WIFI_PASSWORD
#define TPB9000_WIFI_PASSWORD ""
#endif

namespace tpb9000::network {
namespace {
constexpr char tag[] = "network";
std::atomic<bool> has_ip = false;
esp_timer_handle_t reconnect_timer = nullptr;

void reconnect(void*) {
    ESP_LOGI(tag, "Attempting Wi-Fi reconnection");
    esp_wifi_connect();
}

void schedule_reconnect() {
    if (reconnect_timer == nullptr) return;
    esp_timer_stop(reconnect_timer);
    esp_timer_start_once(reconnect_timer, 3'000'000);
}

esp_err_t configure_static_ipv4(esp_netif_t* interface) {
    esp_err_t result = esp_netif_dhcpc_stop(interface);
    if (result != ESP_OK && result != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        return result;
    }

    esp_netif_ip_info_t address = {};
    IP4_ADDR(&address.ip, 192, 168, 20, 204);
    IP4_ADDR(&address.gw, 192, 168, 20, 1);
    IP4_ADDR(&address.netmask, 255, 255, 255, 0);
    if ((result = esp_netif_set_ip_info(interface, &address)) != ESP_OK) {
        return result;
    }

    esp_netif_dns_info_t dns = {};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    IP4_ADDR(&dns.ip.u_addr.ip4, 192, 168, 20, 1);
    if ((result = esp_netif_set_dns_info(interface, ESP_NETIF_DNS_MAIN,
                                          &dns)) != ESP_OK) {
        return result;
    }
    ESP_LOGI(tag, "Static IPv4 configured: 192.168.20.204/24, gateway 192.168.20.1");
    return ESP_OK;
}

void handle_event(void*, esp_event_base_t event_base, int32_t event_id,
                  void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(tag, "Wi-Fi station started; connecting");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        has_ip.store(false);
        const auto* event = static_cast<wifi_event_sta_disconnected_t*>(event_data);
        ESP_LOGW(tag, "Network offline (disconnect reason %u); reconnecting",
                 event == nullptr ? 0 : event->reason);
        schedule_reconnect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const auto* event = static_cast<ip_event_got_ip_t*>(event_data);
        has_ip.store(true);
        ESP_LOGI(tag, "Network online: " IPSTR,
                 IP2STR(&event->ip_info.ip));
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
        has_ip.store(false);
        ESP_LOGW(tag, "Network offline (IP address lost)");
    }
}
}  // namespace

bool configured() { return TPB9000_WIFI_SSID[0] != '\0'; }

bool online() { return has_ip.load(); }

esp_err_t initialize() {
    if (!configured()) {
        ESP_LOGW(tag, "Wi-Fi credentials are not configured; remaining offline");
        return ESP_OK;
    }

    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    if (result != ESP_OK) return result;
    if ((result = esp_netif_init()) != ESP_OK) return result;
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;
    esp_netif_t* interface = esp_netif_create_default_wifi_sta();
    if (interface == nullptr) return ESP_FAIL;
    if ((result = esp_netif_set_hostname(interface, "tpb9000-panel")) != ESP_OK) {
        return result;
    }
    if ((result = configure_static_ipv4(interface)) != ESP_OK) return result;

    wifi_init_config_t initialization = WIFI_INIT_CONFIG_DEFAULT();
    if ((result = esp_wifi_init(&initialization)) != ESP_OK) return result;
    const esp_timer_create_args_t reconnect_timer_configuration = {
        .callback = reconnect,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "wifi_reconnect",
        .skip_unhandled_events = true,
    };
    if ((result = esp_timer_create(&reconnect_timer_configuration,
                                   &reconnect_timer)) != ESP_OK) {
        return result;
    }
    if ((result = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &handle_event, nullptr)) != ESP_OK) {
        return result;
    }
    if ((result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &handle_event, nullptr)) != ESP_OK) {
        return result;
    }
    if ((result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP,
                                              &handle_event, nullptr)) != ESP_OK) {
        return result;
    }

    wifi_config_t configuration = {};
    std::strncpy(reinterpret_cast<char*>(configuration.sta.ssid),
                 TPB9000_WIFI_SSID, sizeof(configuration.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(configuration.sta.password),
                 TPB9000_WIFI_PASSWORD, sizeof(configuration.sta.password) - 1);
    configuration.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    configuration.sta.pmf_cfg.capable = true;
    configuration.sta.pmf_cfg.required = false;

    if ((result = esp_wifi_set_storage(WIFI_STORAGE_RAM)) != ESP_OK) return result;
    if ((result = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK) return result;
    if ((result = esp_wifi_set_config(WIFI_IF_STA, &configuration)) != ESP_OK) {
        return result;
    }
    if ((result = esp_wifi_set_ps(WIFI_PS_NONE)) != ESP_OK) return result;
    ESP_LOGI(tag, "Connecting to configured Wi-Fi network");
    return esp_wifi_start();
}

}  // namespace tpb9000::network
