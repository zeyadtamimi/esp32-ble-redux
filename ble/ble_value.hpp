/**
 * @file   ble_value.hpp
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

#ifndef COMPONENTS_BLE_BLE_VALUE_HPP
#define COMPONENTS_BLE_BLE_VALUE_HPP

#include <algorithm>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace BLE
{

class BLE_Value
{

public:

    template<typename T>
    using Serializer = std::function<std::vector<uint8_t>(T)>;

    template<typename T>
    using Deserializer = std::function<T(std::vector<uint8_t>)>;

    /**
     * @brief A basic serializer that copies the memory contents of the provided value into a byte
     *        array.
     * @tparam T The basic type to be serialized, it must an intrinsic numeric type.
     * @param [in] value The value to be serialized.
     * @return An std::vector of uint8_ts representing the serialized type.
     */
    template<typename T,
             typename=std::enable_if_t<std::conjunction_v<std::is_default_constructible<T>,
                                                          std::negation<std::is_pointer<T>>,
                                                          std::is_integral<T>>>>
    static std::vector<uint8_t> default_serializer(T value);

    /**
     * @brief A basic deserializer that copies the memory contents of a serialized std::vector
     *        uint8_t into the memory location of the type.
     * @tparam T The basic type to be serialized, it must an intrinsic numeric type.
     * @param [in] serialized_value An std::vector which contains the serialized contents of the
     *             supplied types.
     * @return A value of type T that represents the deserialized data.
     */
    template<typename T,
             typename=std::enable_if_t<std::conjunction_v<std::is_default_constructible<T>,
                                                          std::negation<std::is_pointer<T>>,
                                                          std::is_integral<T>>>>
    static T default_deserializer(std::vector<uint8_t> serialized_value);

    /**
     * @brief Sets the inner value of the object to the provided value.
     * @tparam T the type of the provided value.
     * @param [in] value The desired value of type T to set.
     * @param [in] serializer A function capable of accepting the provided value of type T and
     *             convert it to an std::vector of uint8_ts.
     */
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

};

#endif // COMPONENTS_BLE_BLE_VALUE_HPP

