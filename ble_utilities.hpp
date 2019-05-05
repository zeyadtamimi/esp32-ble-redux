// Copyright (c) 2019 Zeyad Tamimi.  All rights reserved.

#ifndef COMPONENTS_BLE_UTILITIES_HPP
#define COMPONENTS_BLE_UTILITIES_HPP

#include <cstddef>

// The Bluetooth v4.0 and v4.1 standards both define a maximum BLE data length of 27 bytes (newer
// standards have increased this), of this 4 bytes go to the link layer L2CAP. Therefore we only
// have 23 bytes left for ATT data.
constexpr const size_t MTU_DEFAULT_BLE_CLIENT = 23;

// The ESP32 support high MTU value, so by default we set it to a high number such that it is never
// the bottleneck.
constexpr const size_t MTU_DEFAULT_BLE_SERVER = 512;


// The Bluetooth v4.0 specification states that the data field must contain 1 byte for the opcode.
constexpr const size_t ATT_FIELD_LENGTH_OPCODE = 1;

#endif // COMPONENTS_BLE_UTILITIES_HPP

