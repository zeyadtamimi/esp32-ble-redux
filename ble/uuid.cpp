/**
 * @file   uuid.cpp
 *
 * @brief  128-bit UUID wrapper class and utilities.
 * @date   03/04/2019
 * @author Zeyad Tamimi (ZeyadTamimi@Outlook.com)
 * @copyright Copyright (c) 2019 Zeyad Tamimi. All rights reserved.
 *            This Source Code Form is subject to the terms of the Mozilla Public
 *            License, v. 2.0. If a copy of the MPL was not distributed with this
 *            file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <iomanip>
#include <sstream>
#include <string>
#include <variant>

#include "types.hpp"
#include "uuid.hpp"

namespace BLE
{

constexpr const uint128_t BLE_BASE_UUID = absl::MakeUint128(0x1000, 0x800000805F9B34FB);


UUID::UUID(const uint8_t* raw_uuid, size_t size)
{
    // TODO Size check!
    // TODO Allow for other variants
    uint128_t uuid = 0;
    for (int i = size - 1; i > 0; i--)
    {
        uuid |= raw_uuid[i];
        uuid <<= 8;

    }
    uuid |= raw_uuid[0];

    m_uuid = uuid;
}


uint128_t
UUID::to_128(void) const
{
    uint128_t uuid;
    if (std::holds_alternative<uint16_t>(m_uuid))
    {
        uuid = std::get<uint16_t>(m_uuid);
        uuid <<= 96;
        uuid |= BLE_BASE_UUID;
    }
    else if (std::holds_alternative<uint32_t>(m_uuid))
    {
        uuid = std::get<uint32_t>(m_uuid);
        uuid <<= 96;
        uuid |= BLE_BASE_UUID;
    }
    else
        uuid = std::get<uint128_t>(m_uuid);

    return uuid;
}


std::array<uint8_t, 16>
UUID::to_raw_128(void) const
{

    uint128_t uuid = to_128();

    // We have to be careful and match the expected endian-ness
    // TODO Use std::reverse_copy
    std::array<uint8_t, 16> ret;
    for (int i = 0; i < 16; i++)
        ret[i] = (unsigned char) ((uuid >> (i*8)) & 0xFF);

    return ret;
}


esp_bt_uuid_t
UUID::to_esp_uuid(void) const
{
    auto uuid_raw_128 = to_raw_128();

    esp_bt_uuid_t esp_uuid;
    esp_uuid.len = ESP_UUID_LEN_128;
    memcpy(esp_uuid.uuid.uuid128, uuid_raw_128.data(), ESP_UUID_LEN_128);

    return esp_uuid;
}


std::string
UUID::to_string(void) const
{
    std::array<uint8_t, 16> uuid_raw = to_raw_128();
    std::reverse(uuid_raw.begin(), uuid_raw.end());

    std::stringstream string_builder;
    string_builder << std::hex << std::setfill('0') << std::setw(2) << std::uppercase
                                                                    << (unsigned) uuid_raw[0]
                                                                    << (unsigned) uuid_raw[1]
                                                                    << (unsigned) uuid_raw[2]
                                                                    << (unsigned) uuid_raw[3]
                                                                    << '-'
                                                                    << (unsigned) uuid_raw[4]
                                                                    << (unsigned) uuid_raw[5]
                                                                    << '-'
                                                                    << (unsigned) uuid_raw[6]
                                                                    << (unsigned) uuid_raw[7]
                                                                    << '-'
                                                                    << (unsigned) uuid_raw[8]
                                                                    << (unsigned) uuid_raw[9]
                                                                    << '-'
                                                                    << (unsigned) uuid_raw[10]
                                                                    << (unsigned) uuid_raw[11]
                                                                    << (unsigned) uuid_raw[12]
                                                                    << (unsigned) uuid_raw[13]
                                                                    << (unsigned) uuid_raw[14]
                                                                    << (unsigned) uuid_raw[15];
    return string_builder.str();
}


UUID&
UUID::operator=(const UUID& other)
{
    if (this != &other)
        this->m_uuid = other.m_uuid;

    return *this;
}


bool
UUID::operator==(const UUID& other)
{
    return this->m_uuid == other.m_uuid;
}

};

