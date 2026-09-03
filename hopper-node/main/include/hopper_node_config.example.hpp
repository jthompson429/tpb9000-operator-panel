#pragma once

// Copy this file to hopper_node_config.hpp and replace the peer MAC and channel
// with values printed by the operator panel. The private file is ignored by Git.
#define TPB9000_PANEL_MAC 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#define TPB9000_ESPNOW_CHANNEL 1

// A02 sensor TX connects through its 10k/20k voltage divider to the RX pad
// (GPIO 44) on the HW-747 ESP32-S3 Super Mini. The adjacent TX pad is GPIO 43
// and remains electrically unconnected. Pull the sensor's RX/mode lead low
// through 10 kΩ to select the verified real-time output mode.
#define TPB9000_A02_RX_GPIO 44
#define TPB9000_A02_TX_GPIO 43
