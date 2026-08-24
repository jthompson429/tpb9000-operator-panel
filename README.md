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

## Version 1 status: complete

The current firmware initializes and scans I2C, verifies the BME280 identity,
and displays live compensated temperature, humidity, and pressure readings on
the 800 x 480 RGB565 display. Sensor communication and range failures replace
the readings with an explicit error state. Wi-Fi runs independently of sensor
sampling and displays an obvious online/offline state. If the BME280 is absent
at startup or fails later, the panel remains running, shows `SENSOR ERROR`, and
retries the sensor every ten seconds without interrupting network operation.
Out-of-range values and physically implausible two-second changes are rejected
instead of being displayed as current measurements.

The production dashboard uses embedded 800 x 480 RGB565 artwork with separate
normal and offline backgrounds. Dynamic readings, network state, sensor errors,
and the firmware version are rendered over the artwork. Source PNGs and design
iterations are retained under `design/`; `tools/png_to_rgb565.py` converts the
production PNGs under `main/assets/` into the raw assets embedded by ESP-IDF.

Support for an INA219 voltage/current/power monitor is a possible post-v1
enhancement and is intentionally outside the completed Version 1 scope.

## Post-v1 development

The first v0.2 foundation adds a read-only local JSON API without changing the
verified v1 display behavior. See [API v1](docs/api-v1.md) and the
[v0.2 integration plan](docs/v0.2-integration-plan.md). Hardware-specific
drivers remain disabled until their exact modules, electrical limits, and pin
paths are confirmed.

Confirmed v0.2 hardware includes a VEML7700 light sensor, an `R100` INA219
breakout intended for a 12 VDC rail, a Waveshare HMMD mmWave presence sensor,
a 3-24 V active piezo buzzer, and an A02-family automatic-UART ultrasonic
hopper sensor. The future dashboard will run separately on the mini PC at
`tpb9000-server.local`; the operator panel does not depend on that host.

## Wi-Fi configuration

Copy `main/include/private_config.example.h` to
`main/include/private_config.h`, then replace the example SSID and password.
The private file is ignored by Git and must never be committed. If it is absent,
the application remains operational and displays `NETWORK OFFLINE`.

The Version 1 panel uses static IPv4 address `192.168.20.204/24`, gateway
`192.168.20.1`, and DNS server `192.168.20.1`.

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

## INA219 wiring

The verified `R100` INA219 monitors only the operator panel's 12 VDC feed. Its
logic side shares the exposed I2C terminal with the BME280:

| INA219 | Connection |
| --- | --- |
| `VCC` | Panel `VOUT` (3.3 V) |
| `GND` | Panel `GND` |
| `SDA` | Panel `SDA` |
| `SCL` | Panel `SCL` |
| `Vin+` | Positive lead from the 12 V supply |
| `Vin-` | Panel 9-36 V positive input |

The 12 V supply negative remains connected directly to panel `GND`. Never
connect 12 V to INA219 `VCC`, and do not place the entire TPB9000 system load
through this breakout. Initial hardware validation measured approximately
12.25 V, 0.205 A, and 2.53 W with the display active.

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

For reliable native-USB download-mode entry, disconnect the 12 V supply and
power the panel from a known data-capable USB cable only. With USB
disconnected, hold `BOOT`, reconnect USB, wait about three seconds, and then
release `BOOT`. On macOS the ROM downloader should appear as
`/dev/cu.usbmodemXXXX`. Reconnect 12 V only after flashing has completed and
the application has rebooted.

## Structure

```text
main/
  assets/ (production PNG and embedded RGB565 backgrounds)
  app_main.cpp
  board/display.cpp
  board/i2c_bus.cpp
  sensors/environment_sensor.cpp
  ui/dashboard.cpp
  network/network.cpp
  include/board_config.hpp
  include/display.hpp
  include/i2c_bus.hpp
  include/environment_sensor.hpp
  include/dashboard.hpp
  include/network.hpp
  include/private_config.example.h
  vendor/bme280/ (Bosch BME280 SensorAPI)
design/ (source artwork and dashboard iterations)
tools/png_to_rgb565.py
partitions.csv
```

Sensor, Wi-Fi, and UI modules remain separate from `app_main.cpp`.
