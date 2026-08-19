idf_component_register(
    SRCS "app_main.cpp" "board/display.cpp" "board/i2c_bus.cpp"
         "sensors/environment_sensor.cpp" "vendor/bme280/bme280.c"
         "ui/dashboard.cpp"
    INCLUDE_DIRS "include" "vendor/bme280"
    REQUIRES esp_driver_i2c esp_driver_gpio esp_lcd
)
