// Copyright (c) 2019 Zeyad Tamimi.  All rights reserved.

#ifndef COMPONENTS_BLE_BLE_VALUE_HPP
#define COMPONENTS_BLE_BLE_VALUE_HPP

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <functional>

#include "types.hpp"

// TODO Dont just assume ESP check endianness
// Handle pointers

class BLE_Value
{

public:

    template<typename T>
    using Serializer = std::function<std::vector<uint8_t>(T)>;

    template<typename T>
    using Deserializer = std::function<T(std::vector<uint8_t>)>;

    template<typename T>
    static std::vector<uint8_t> default_serializer(T value);


    template<typename T,
             typename=std::enable_if_t<std::conjunction_v<std::is_default_constructible<T>,
                                                          std::is_integral<T>>>>
    static T default_deserializer(std::vector<uint8_t> serialized_value);

    template<typename T>
    void value_set(T value, BLE_Value::Serializer<T> serializer);

    template<typename T>
    T value_get(BLE_Value::Deserializer<T> deserializer) const;

    void transaction_write_start(uint16_t connection_id);
    bool transaction_write_add(uint16_t connection_id, std::vector<uint8_t> data);
    bool transaction_write_commit(uint16_t connection_id);
    void transaction_write_abort(uint16_t connection_id);
    bool transaction_write_ongoing(int16_t connection_id);

    void transaction_read_start(uint16_t connection_id);
    std::vector<uint8_t> transaction_read_advance(uint16_t connection_id, size_t max_len);
    void transaction_read_abort(uint16_t connection_id);

    std::vector<uint8_t> to_raw(void) const;

private:
    struct transaction_read_t
    {
        std::vector<uint8_t> value;
        size_t offset;
    };

    std::vector<uint8_t> m_value;

    std::unordered_map<uint16_t, std::vector<uint8_t>> m_transactions_write;
    std::unordered_map<uint16_t, transaction_read_t> m_transactions_read;
};


#include "ble_value.tpp"

#endif // COMPONENTS_BLE_BLE_VALUE_HPP

