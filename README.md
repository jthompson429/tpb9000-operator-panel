# TPB9000 Operator Panel

Local environmental-monitoring and status display for the TPB9000 outdoor cat
feeding system. The operator panel is deliberately non-critical: TPB9000 and
its separate FFC controller must continue operating if this display is offline.

## Version 1 scope

- Read temperature, relative humidity, and pressure from an external BME280.
- Display temperature in degrees Fahrenheit.
- Show network availability without making readings depend on the network.
- Show an explicit error instead of stale values when the sensor fails.
- Run unattended for long periods.

## Hardware

- Waveshare ESP32-S3-Touch-LCD-4.3B-BOX (ESP32-S3-N16R8)
- External BME280 on the exposed I2C terminal

The terminal shares GPIO 8 (SDA) and GPIO 9 (SCL) with onboard devices.

## Current milestone: live environmental dashboard

The current firmware initializes and scans I2C, verifies the BME280 identity,
and displays live compensated temperature, humidity, and pressure readings on
the 800 x 480 RGB565 display. Sensor communication and range failures replace
the readings with an explicit error state. No Wi-Fi credentials are present
yet.

Expected addresses include `0x14` or `0x5D` (GT911 touch), `0x51` (RTC), and
`0x76` or `0x77` (BME280). The CH422G expander uses command addresses `0x24`
and `0x38`; not every onboard device must respond to a zero-data probe.

## BME280 wiring

Disconnect power before changing wiring.

| BME280 breakout | Panel I2C terminal |
| --- | --- |
| `VIN` / `VCC` | `VOUT` (factory-default I2C supply is 3.3 V) |
| `GND` | `GND` |
| `SDA` | `SDA` |
| `SCL` | `SCL` |

Never connect the sensor to the panel's 9-36 V input. Waveshare's schematic
shows that the terminal-block `VOUT` pin is the selectable `I2C_VCC` rail and
is factory-configured for 3.3 V. Confirm the breakout's pin labels before
applying power.

## Build, flash, and monitor

This is a native ESP-IDF project:

```sh
source ~/esp/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

Exit the monitor with `Ctrl+]`. If port detection fails, use
`idf.py -p /dev/cu.usbmodemXXXX flash monitor` with the actual device port.

## Structure

```text
main/
  app_main.cpp
  board/display.cpp
  board/i2c_bus.cpp
  sensors/environment_sensor.cpp
  ui/dashboard.cpp
  include/board_config.hpp
  include/display.hpp
  include/i2c_bus.hpp
  include/environment_sensor.hpp
  include/dashboard.hpp
  vendor/bme280/ (Bosch BME280 SensorAPI)
```

Sensor, Wi-Fi, and UI modules remain separate from `app_main.cpp`.
