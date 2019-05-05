// Copyright (c) 2019 Zeyad Tamimi.  All rights reserved.

#include <cstdint>
#include <vector>
#include <algorithm>

#include "ble_value.hpp"


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
