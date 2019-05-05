/**
 * @file   ble_value.cpp
 *
 * @brief  An abstraction layer over a variable data type Bluetooth Low Energy class.
 * @date   03/04/2019
 * @author Zeyad Tamimi (ZeyadTamimi@Outlook.com)
 * @copyright Copyright (c) 2019 Zeyad Tamimi. All rights reserved.
 *            This Source Code Form is subject to the terms of the Mozilla Public
 *            License, v. 2.0. If a copy of the MPL was not distributed with this
 *            file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * @TODO Handle pointers
 * @TODO Handle Arrays
 * @TODO Don't assume system endianess
 * @TODO Max BLE stack size
 */

#include <cstdint>
#include <vector>
#include <algorithm>

#include "ble_value.hpp"

namespace BLE
{

std::vector<uint8_t>
BLE_Value::to_raw(void) const
{
    return m_value;
}


void
BLE_Value::transaction_write_start(uint16_t connection_id)
{
    m_transactions_write.insert_or_assign(connection_id, std::vector<uint8_t>());
}

bool
BLE_Value::transaction_write_add(uint16_t connection_id, std::vector<uint8_t> data)
{
    if (!m_transactions_write.count(connection_id))
        return false;

    m_transactions_write[connection_id].insert(m_transactions_write[connection_id].end(),
                                               data.begin(), data.end());
    return true;
}

bool
BLE_Value::transaction_write_commit(uint16_t connection_id)
{
    if (!m_transactions_write.count(connection_id))
        return false;

    m_value = m_transactions_write[connection_id];
    m_transactions_write.erase(connection_id);

    return true;
}

bool
BLE_Value::transaction_write_ongoing(int16_t connection_id)
{
    return m_transactions_write.count(connection_id) != 0;
}


void
BLE_Value::transaction_read_start(uint16_t connection_id)
{

    transaction_read_t transaction;
    transaction.offset = 0;
    transaction.value = m_value;
    m_transactions_read.insert_or_assign(connection_id, transaction);
}

std::vector<uint8_t>
BLE_Value::transaction_read_advance(uint16_t connection_id, size_t max_length)
{
    if (!m_transactions_read.count(connection_id))
        return {};

    transaction_read_t& transaction = m_transactions_read[connection_id];
    size_t length = std::min(max_length, transaction.value.size() - transaction.offset);

    if (length == 0)
        return {};

    std::vector<uint8_t> ret(transaction.value.begin() + transaction.offset,
                             transaction.value.begin() + transaction.offset + length);
    transaction.offset += length;

    return ret;
}

void
BLE_Value::transaction_read_abort(uint16_t connection_id)
{
    if (!m_transactions_read.count(connection_id))
        return;

    m_transactions_read.erase(connection_id);
}

void
BLE_Value::transaction_write_abort(uint16_t connection_id)
{
    if (!m_transactions_write.count(connection_id))
        return;

    m_transactions_write.erase(connection_id);
}

};
