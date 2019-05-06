/**
 * @file   ble_types.hpp
 *
 * @brief  ESP32 Bluetooth Low Energy types and type aliases.
 * @date   03/04/2019
 * @author Zeyad Tamimi (ZeyadTamimi@Outlook.com)
 * @copyright Copyright (c) 2019 Zeyad Tamimi. All rights reserved.
 *            This Source Code Form is subject to the terms of the Mozilla Public
 *            License, v. 2.0. If a copy of the MPL was not distributed with this
 *            file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef COMPONENTS_BLE_TYPES_HPP
#define COMPONENTS_BLE_TYPES_HPP

#include <variant>

// TODO Macke sure this works with non-gcc
#include "absl/numeric/int128.h"
using uint128_t = absl::uint128;

#include "uuid.hpp"
#endif // COMPONENTS_BLE_TYPES_HPP
