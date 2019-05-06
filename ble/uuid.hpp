/**
 * @file   uuid.hpp
 *
 * @brief  128-bit UUID wrapper class and utilities.
 * @date   03/04/2019
 * @author Zeyad Tamimi (ZeyadTamimi@Outlook.com)
 * @copyright Copyright (c) 2019 Zeyad Tamimi. All rights reserved.
 *            This Source Code Form is subject to the terms of the Mozilla Public
 *            License, v. 2.0. If a copy of the MPL was not distributed with this
 *            file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef COMPONENTS_BLE_UUID_HPP
#define COMPONENTS_BLE_UUID_HPP

#include <array>
#include <cstdint>
#include <string>
#include <variant>

#include "absl/hash/hash.h"
#include "esp_gatts_api.h"

#include "types.hpp"

namespace BLE
{

class UUID
{
public:
    UUID() {}
    UUID(uint16_t uuid) : m_uuid(uuid) {}
    UUID(uint32_t uuid) : m_uuid(uuid) {}
    UUID(uint128_t uuid) : m_uuid(uuid) {}
    UUID(const uint8_t* raw_uuid, size_t size);
    UUID(const UUID& uuid) : m_uuid(uuid.m_uuid) {}

    std::string to_string(void) const;

    uint128_t to_128(void) const;

    std::array<uint8_t, 16> to_raw_128(void) const;
    std::array<uint8_t, 4> to_raw_32(void);
    std::array<uint8_t, 2> to_raw_16(void);
    esp_bt_uuid_t to_esp_uuid(void) const;

    bool operator==(const UUID& other);
    UUID& operator=(const UUID& other);

    friend inline bool operator==(const UUID& lhs, const UUID& rhs);

private:
    std::variant<uint16_t, uint32_t, uint128_t> m_uuid;
};


inline bool operator==(const UUID& lhs, const UUID& rhs){ return rhs.to_128() == lhs.to_128(); }

};


namespace std
{

  template <>
  struct hash<BLE::UUID>
  {
    std::size_t operator()(const BLE::UUID& k) const
    {
        return absl::Hash<uint128_t>()(k.to_128());
    }
  };

};

#endif // COMPONENTS_BLE_UUID_HPP

