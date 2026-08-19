idf_component_register(
    SRCS "app_main.cpp" "board/display.cpp" "board/i2c_bus.cpp"
         "sensors/environment_sensor.cpp" "vendor/bme280/bme280.c"
         "ui/dashboard.cpp"
         "network/network.cpp"
    INCLUDE_DIRS "include" "vendor/bme280"
    REQUIRES esp_driver_i2c esp_driver_gpio esp_lcd esp_wifi esp_netif
             esp_event nvs_flash esp_app_format
             esp_timer
    EMBED_FILES "assets/background_normal.rgb565"
                "assets/background_offline.rgb565"
)
