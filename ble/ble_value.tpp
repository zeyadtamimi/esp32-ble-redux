/**
 * @file   ble_value.tpp
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

#ifndef COMPONENTS_BLE_BLE_VALUE_TPP
#define COMPONENTS_BLE_BLE_VALUE_TPP

/**
 * @brief A basic serializer that copies the memory contents of the provided value into a byte
 *        array.
 * @tparam T The basic type to be serialized, it must an intrinsic numeric type.
 * @param [in] value The value to be serialized.
 * @return An std::vector of uint8_ts representing the serialized type.
 */
template<typename T, typename=std::enable_if_t<std::conjunction_v<std::is_default_constructible<T>,
                                                                  std::negation<std::is_pointer<T>>,
                                                                  std::is_integral<T>>>>
std::vector<uint8_t>
BLE_Value::default_serializer(T value)
{
    std::vector<uint8_t> serialized_value;
    serialized_value.resize(sizeof(T));
    std::reverse_copy(reinterpret_cast<uint8_t*>(&value),
                      reinterpret_cast<uint8_t*>(&value) + sizeof(T),
                      serialized_value.begin());
    return serialized_value;
}


/**
 * @brief A basic deserializer that copies the memory contents of a serialized std::vector
 *        uint8_t into the memory location of the type.
 * @tparam T The basic type to be serialized, it must an intrinsic numeric type.
 * @param [in] An std::vector which contains the serialized contents of the supplied types.
 * @return A value of type T that represents the deserialized data.
 */
template<typename T, typename=std::enable_if_t<std::conjunction_v<std::is_default_constructible<T>,
                                                                  std::negation<std::is_pointer<T>>,
                                                                  std::is_integral<T>>>>
T
BLE_Value::default_deserializer(std::vector<uint8_t> serialized_value)
{
    T value = {};
    std::reverse_copy(serialized_value.begin(),
                      serialized_value.begin() + std::min(sizeof(T), serialized_value.size()),
                      reinterpret_cast<uint8_t*>(&value));
    return value;

}


/**
 * @brief Sets the inner value of the object to the provided value.
 * @tparam T the type of the provided value.
 * @param [in] value The desired value of type T to set.
 * @param [in] serializer A function capable of accepting the provided value of type T and
 *             convert it to an std::vector of uint8_ts.
 */
template<typename T>
void
BLE_Value::value_set(T value, BLE_Value::Serializer<T> serializer)
{
    m_value = serializer(value);
}


template<typename T>
T
BLE_Value::value_get(BLE_Value::Deserializer<T> deserializer) const
{
    return deserializer(m_value);
}

#endif // COMPONENTS_BLE_BLE_VALUE_TPP

