#pragma once

// Copy this file to private_config.h and fill in the designated network.
// private_config.h is ignored by Git and must never be committed.
#define TPB9000_WIFI_SSID "replace-with-network-name"
#define TPB9000_WIFI_PASSWORD "replace-with-network-password"

// Set this after the hopper-node discovery flash prints its station MAC.
// Leaving it all-zero enables commissioning mode, which accepts any correctly
// formed TPB9000 hopper packet and logs the sender MAC.
#define TPB9000_HOPPER_NODE_MAC 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
