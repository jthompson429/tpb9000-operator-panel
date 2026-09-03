idf_component_register(
    SRCS "app_main.cpp" "a02_sensor.cpp" "a02_parser.cpp"
         "espnow_sender.cpp"
    INCLUDE_DIRS "include"
    REQUIRES hopper_protocol esp_driver_gpio esp_driver_uart esp_wifi esp_netif esp_event
             nvs_flash esp_timer
)
