# Wireless hopper node (ESP-NOW development phase)

This phase replaces only the hopper measurement transport. The A02 sensor is
read by a dedicated ESP32-S3 SuperMini, which sends validated distance data to
the operator panel over ESP-NOW. Calibration, median filtering, fill percent,
bar hysteresis, API reporting, and the approved KIBBLE gauge remain on the
operator panel.

Battery power, charging, solar input, deep sleep, sensor power switching, and
retries are intentionally outside this phase.

## Repository layout

- The repository root is the existing operator-panel ESP-IDF application.
- `hopper-node/` is an independent ESP-IDF application for the SuperMini.
- `components/hopper_protocol/` contains the 16-byte packet definition used by
  both applications.

Each application has its own `CMakeLists.txt`, `sdkconfig`, build directory,
binary, flash command, and serial monitor.

## Safe development wiring

Disconnect USB and all other power before changing wiring.

The sensor vendor specifies 3.3-5 V operation, but the project sensor did not
produce readable UART data from 3.3 V. Power it from the SuperMini's **5V**
output and reduce its measured near-5 V TX signal with the divider below.

| A02 harness | Function | ESP32-S3 SuperMini |
| --- | --- | --- |
| Red | VCC | `5V` |
| Black | GND | `GND` |
| White | TX | through the voltage divider below to the `RX` pad (`GPIO 44`) |
| Yellow | RX/mode | through 10 kΩ to `GND` |

The wire functions above are based on the physically tested harness for this
project; connector pin numbers remain authoritative if the harness changes.
The tested sensor did not transmit reliably with its yellow RX/mode lead
floating. Pulling yellow low through 10 kΩ selects its 100 ms real-time output
and produced valid frames. The node listens at 9600 baud, 8 data bits, no
parity, and one stop bit.

Do not feed the white 5 V UART signal directly into an ESP32 GPIO. Connect
white through 10 kΩ to the `RX`-pad junction, and connect 20 kΩ from that
junction to ground. The measured 3.2-3.3 V junction output is safe for the
ESP32. The temporary MAX485 board is not used in the wireless design.

The tested carrier is marked `HW-747 V0.0.2`; unlike the initially supplied
14-pin pinout, it exposes dedicated `TX` and `RX` pads mapped to GPIO 43 and
GPIO 44. GPIO 44 is centralized as `TPB9000_A02_RX_GPIO` in
`hopper-node/main/include/hopper_node_config.hpp`. GPIO 43 is assigned as UART
TX so the complete hardware pair is configured, but nothing is physically
connected to the board's `TX` pad in this phase.

## MAC and channel commissioning

ESP-NOW addresses peers by their six-byte Wi-Fi station MAC address. The
operator panel also joins the TPB9000 Wi-Fi network, so its radio follows the
access point's channel. The standalone hopper node must be configured to that
same channel. If the access point later changes its 2.4 GHz channel, update and
reflash the node.

Use this one-time commissioning sequence:

1. Build and flash the hopper node with its example configuration. Its monitor
   prints `Hopper station MAC`. Record it. With an all-zero panel MAC, the node
   deliberately does not transmit.
2. Build and flash the panel. Its monitor prints `Panel station MAC`, and after
   Wi-Fi association it prints `Wi-Fi/ESP-NOW operating channel`. Record both.
3. Copy
   `hopper-node/main/include/hopper_node_config.example.hpp` to
   `hopper-node/main/include/hopper_node_config.hpp`. Put the panel MAC in
   `TPB9000_PANEL_MAC` and the reported channel in
   `TPB9000_ESPNOW_CHANNEL`. Rebuild and flash the node.
4. Initially the panel's all-zero `TPB9000_HOPPER_NODE_MAC` enables an explicit
   commissioning mode. It accepts only correctly sized/versioned TPB9000
   packets and logs `Receiving hopper telemetry from ...`. Confirm this address
   matches the node MAC recorded in step 1.
5. Add the node MAC to `TPB9000_HOPPER_NODE_MAC` in the panel's existing
   `main/include/private_config.h`, then rebuild and flash the panel. This locks
   normal operation to the expected sender.

MAC format in both private configuration files is:

```cpp
#define TPB9000_PANEL_MAC 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
#define TPB9000_HOPPER_NODE_MAC 0x11, 0x22, 0x33, 0x44, 0x55, 0x66
```

Both private files are ignored by Git. Never commit Wi-Fi credentials or
device-specific addresses.

## Build and flash

Operator panel, from the repository root:

```sh
source ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

Hopper node:

```sh
cd hopper-node
source ~/esp/esp-idf/export.sh
idf.py -B build build
idf.py -B build -p /dev/cu.usbmodemXXXX flash monitor
```

Exit either monitor with `Ctrl+]`. Use `ls /dev/cu.usbmodem*` to identify the
port. When both devices are attached, unplug one while identifying the other so
the wrong firmware cannot be flashed to the wrong board.

## Bench tests

1. **Sensor:** Run the hopper-node monitor. Confirm `Sensor ONLINE`, changing
   distance values, and `Sensor: OK`.
2. **ESP-NOW:** Confirm the node reports `ESP-NOW READY` and `Send: SUCCESS`;
   confirm the panel logs matching `RX #...` packets and distances.
3. **Existing display:** Move a flat target between the calibrated full and
   empty distances. Confirm the KIBBLE percent, segment count, and colors react
   as before.
4. **Wireless separation:** Move the USB-powered node and sensor several feet
   from the panel and confirm updates continue.
5. **Sequence:** Compare consecutive `TX #...` and `RX #...` messages. A panel
   message such as `expected #42, received #44` reveals a dropped packet.
6. **Offline/recovery:** Unplug the node. Within five seconds the panel must show
   `--` and `OFFLINE` with empty segments. Reconnect it and confirm the normal
   display returns automatically.
7. **Network coexistence:** While packets arrive, confirm `NETWORK ONLINE` and
   load `http://192.168.20.204/api/status`. BME280, INA219, firmware info, and
   the local API must continue operating.

## How this implementation works

### Mental model

ESP-NOW uses the ESP32's 2.4 GHz Wi-Fi radio to deliver small device-to-device
frames without an IP connection between the two devices. Here, the hopper node
measures distance and sends one 16-byte packet per second directly to the
panel's station MAC. The panel still connects normally to its Wi-Fi access
point; ESP-NOW shares that same radio and channel.

`radio::initialize()` in `hopper-node/main/espnow_sender.cpp` starts the node's
station radio, selects `TPB9000_ESPNOW_CHANNEL`, prints its MAC, initializes
ESP-NOW, and registers the panel MAC as an unencrypted peer with
`esp_now_add_peer()`. The fixed channel and peer MAC are stored together in the
ignored `hopper_node_config.hpp`, not scattered through the program.

`HopperTelemetry` in
`components/hopper_protocol/include/hopper_protocol.hpp` is the shared wire
contract. It contains a magic value, protocol version, packet size, sequence,
distance in millimeters, sensor status, and a reserved `battery_mv` field. Both
processors are ESP32-S3 devices and compile this exact packed 16-byte structure.
Size and version checks stop incompatible firmware from silently interpreting
the wrong layout.

The node's `app_main()` in `hopper-node/main/app_main.cpp` repeatedly calls
`sensor::read_frame()`. `FrameParser::push()` in `a02_parser.cpp` recognizes the
four-byte `0xFF, high, low, checksum` sensor frame and rejects bad checksums or
out-of-range distances. Once per second, `hopper_protocol::make_telemetry()`
constructs a packet and increments `sequence`. `radio::send()` calls
`esp_now_send()`. Returning `ESP_OK` means the send was queued; the later
`send_complete()` callback logs whether the Wi-Fi layer reported success.

On the panel, `hopper_radio::initialize()` in
`main/network/hopper_receiver.cpp` prints the panel MAC, initializes ESP-NOW,
and registers `receive_callback()`. That callback runs in the Wi-Fi task, so it
does only bounded validation/copying and places an event on a FreeRTOS queue.
`hopper_radio::receive()` runs in the normal hopper task, logs errors and packet
gaps, and returns the validated distance and sensor status.

`hopper_monitor_task()` in `main/app_main.cpp` passes that distance to
`hopper::Processor::push()` in `main/sensors/hopper_processing.cpp`. The
processor retains the already-tested five-sample median, full/empty calibration,
percentage calculation, and six-bar hysteresis. It updates `system_status`, and
the unchanged dashboard path draws the percentage and segment gauge.

The receiver remembers the last sequence. Normally it sees N, N+1, N+2. A jump
means one or more packets were missed and is logged, but no retry is requested:
food level changes slowly, a new packet arrives one second later, and retry
state would add complexity without meaningful benefit.

Each successful call to `hopper_radio::receive()` starts a fresh five-second
wait for the next valid packet. A timeout makes `hopper_monitor_task()` clear
the processor history and mark hopper status unavailable. The existing UI area
then shows `--`, `OFFLINE`, and empty segments. A later valid packet immediately
starts the normal calculation/display path again.

### Actual data flow

```text
A02YYUW sensor
    |  UART 9600 8N1
    v
sensor::read_frame() / FrameParser::push()
    |
    v
hopper_protocol::make_telemetry()
    |
    v
radio::send() -> esp_now_send()
    |  ESP-NOW, same 2.4 GHz channel as panel Wi-Fi
    v
hopper_radio::receive_callback()
    |  validated FreeRTOS queue event
    v
hopper_radio::receive()
    |
    v
hopper_monitor_task()
    |
    v
hopper::Processor::push()
    |
    +--> five-sample median
    +--> calculate_percent()
    +--> six-bar hysteresis
    v
status::set_hopper() -> dashboard::show_reading() -> KIBBLE gauge
```

### Likely future edit points

- Reporting interval: `transmit_interval_us` in
  `hopper-node/main/app_main.cpp`.
- Sensor UART pin: `TPB9000_A02_RX_GPIO` in the node private configuration.
- Full/empty calibration: `full_distance_cm` and `empty_distance_cm` in
  `main/include/hopper_sensor.hpp`.
- Packet evolution: `HopperTelemetry` and `version` in `hopper_protocol.hpp`.
- Battery measurement: populate the reserved `battery_mv` field on the node and
  add corresponding panel status/API handling in a later protocol version.
- Deep sleep, sensor power switching, and wake/report scheduling belong around
  the node's measurement/transmission loop in `hopper-node/main/app_main.cpp`,
  after continuous-power reliability is proven.

## Current limitations and next-phase checks

- Transport is unencrypted and intentionally has no application acknowledgement
  or retry queue.
- Sequence restarts after a node reboot; the panel logs that as a restart or
  out-of-order event rather than a huge loss.
- The node's fixed channel must be updated if the access point changes channel.
- The send callback reports Wi-Fi-layer delivery status, not an application-level
  acknowledgement from the panel.
- The tested A02 sensor can emit zero-distance frames for roughly ten seconds
  after power-up. Those frames are reported as `NO DATA`; the panel remains
  offline until valid measurements begin and then recovers automatically.
- Before battery/solar work, verify stable UART readings at the final mounting
  angle, reliable ESP-NOW range through the hopper/enclosure materials, the
  actual active and peak current of the node plus sensor, and the chosen
  LiFePO4/charger/solar voltage budget.
