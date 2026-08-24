#include "status_api.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "board_config.hpp"
#include "esp_app_desc.h"
#include "esp_http_server.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "network.hpp"
#include "system_status.hpp"

namespace tpb9000::api {
namespace {
constexpr char tag[] = "status_api";
constexpr char schema_version[] = "1.0";
httpd_handle_t server = nullptr;

esp_err_t send_json(httpd_req_t* request, const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    va_list length_arguments;
    va_copy(length_arguments, arguments);
    const int length = std::vsnprintf(nullptr, 0, format, length_arguments);
    va_end(length_arguments);
    if (length < 0) {
        va_end(arguments);
        return ESP_FAIL;
    }
    char* body = static_cast<char*>(std::malloc(static_cast<size_t>(length) + 1));
    if (body == nullptr) {
        va_end(arguments);
        return ESP_ERR_NO_MEM;
    }
    std::vsnprintf(body, static_cast<size_t>(length) + 1, format, arguments);
    va_end(arguments);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "Access-Control-Allow-Origin", "*");
    const esp_err_t result = httpd_resp_send(request, body, length);
    std::free(body);
    return result;
}

esp_err_t info_handler(httpd_req_t* request) {
    const esp_app_desc_t* application = esp_app_get_description();
    return send_json(
        request,
        "{\"schema_version\":\"%s\",\"device\":\"TPB9000 Operator Panel\","
        "\"board\":\"%s\",\"firmware_version\":\"%s\","
        "\"project_name\":\"%s\",\"build_date\":\"%s\","
        "\"build_time\":\"%s\",\"esp_idf_version\":\"%s\","
        "\"capabilities\":{\"environment\":true,\"power\":true,"
        "\"ambient_light\":false,\"operator_presence\":false,"
        "\"hopper\":false,\"buzzer\":false}}",
        schema_version, board::model, application->version,
        application->project_name, application->date, application->time,
        esp_get_idf_version());
}

esp_err_t status_handler(httpd_req_t* request) {
    const status::Snapshot live = status::snapshot();
    const bool is_online = network::online();
    int8_t rssi = 0;
    const bool has_rssi = is_online && network::rssi_dbm(rssi);

    char rssi_json[16] = "null";
    char temperature_json[32] = "null";
    char humidity_json[32] = "null";
    char pressure_json[32] = "null";
    char environment_updated_json[32] = "null";
    char bus_voltage_json[32] = "null";
    char current_json[32] = "null";
    char power_json[32] = "null";
    char power_updated_json[32] = "null";
    if (has_rssi) std::snprintf(rssi_json, sizeof(rssi_json), "%d", rssi);
    if (live.environment_available) {
        std::snprintf(temperature_json, sizeof(temperature_json), "%.2f",
                      live.environment.temperature_f);
        std::snprintf(humidity_json, sizeof(humidity_json), "%.2f",
                      live.environment.humidity_percent);
        std::snprintf(pressure_json, sizeof(pressure_json), "%.2f",
                      live.environment.pressure_hpa);
        std::snprintf(environment_updated_json,
                      sizeof(environment_updated_json), "%llu",
                      static_cast<unsigned long long>(
                          live.environment_updated_ms));
    }
    if (live.power_available) {
        std::snprintf(bus_voltage_json, sizeof(bus_voltage_json), "%.3f",
                      live.power.bus_voltage_v);
        std::snprintf(current_json, sizeof(current_json), "%.4f",
                      live.power.current_a);
        std::snprintf(power_json, sizeof(power_json), "%.3f",
                      live.power.power_w);
        std::snprintf(power_updated_json, sizeof(power_updated_json), "%llu",
                      static_cast<unsigned long long>(live.power_updated_ms));
    }
    return send_json(
        request,
        "{\"schema_version\":\"%s\",\"machine\":{\"online\":%s,"
        "\"uptime_seconds\":%.3f,\"wifi_rssi_dbm\":%s},"
        "\"environment\":{\"available\":%s,\"temperature_f\":%s,"
        "\"humidity_percent\":%s,\"pressure_hpa\":%s,"
        "\"ambient_lux\":null,\"updated_ms\":%s},"
        "\"power\":{\"available\":%s,\"bus_voltage_v\":%s,"
        "\"current_a\":%s,\"power_w\":%s,\"updated_ms\":%s},"
        "\"hopper\":{\"available\":false,\"percent\":null,"
        "\"distance_cm\":null,\"bars\":null,\"status\":\"Unavailable\"},"
        "\"display\":{\"brightness_percent\":100,"
        "\"automatic_brightness\":false,\"operator_present\":null,"
        "\"last_refresh_ms\":%llu}}",
        schema_version, is_online ? "true" : "false",
        esp_timer_get_time() / 1'000'000.0, rssi_json,
        live.environment_available ? "true" : "false", temperature_json,
        humidity_json, pressure_json, environment_updated_json,
        live.power_available ? "true" : "false", bus_voltage_json,
        current_json, power_json, power_updated_json,
        static_cast<unsigned long long>(live.display_refreshed_ms));
}
}  // namespace

esp_err_t initialize() {
    if (server != nullptr) return ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 4;
    esp_err_t result = httpd_start(&server, &config);
    if (result != ESP_OK) return result;

    const httpd_uri_t info = {
        .uri = "/api/info",
        .method = HTTP_GET,
        .handler = info_handler,
        .user_ctx = nullptr,
    };
    const httpd_uri_t status = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = status_handler,
        .user_ctx = nullptr,
    };
    if ((result = httpd_register_uri_handler(server, &info)) != ESP_OK ||
        (result = httpd_register_uri_handler(server, &status)) != ESP_OK) {
        httpd_stop(server);
        server = nullptr;
        return result;
    }
    ESP_LOGI(tag, "Status API ready: GET /api/info and GET /api/status");
    return ESP_OK;
}

}  // namespace tpb9000::api
