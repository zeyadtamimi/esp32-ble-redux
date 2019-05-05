/**
 * @file   ble_utilities.hpp
 *
 * @brief  ESP32 Bluetooth Low Energy utility functions and constants.
 * @date   03/04/2019
 * @author Zeyad Tamimi (ZeyadTamimi@Outlook.com)
 * @copyright Copyright (c) 2019 Zeyad Tamimi. All rights reserved.
 *            This Source Code Form is subject to the terms of the Mozilla Public
 *            License, v. 2.0. If a copy of the MPL was not distributed with this
 *            file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef COMPONENTS_BLE_BLE_UTILITIES_HPP
#define COMPONENTS_BLE_BLE_UTILITIES_HPP

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

#endif // COMPONENTS_BLE_BLE_UTILITIES_HPP

