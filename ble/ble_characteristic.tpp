/**
 * @file   ble_characteristic.tpp
 *
 * @brief  ESP32 Bluetooth Low Energy service characteristic layer.
 * @detail This class represents a BLE characteristic which is able to store descriptors and values.
 * @date   03/04/2019
 * @author Zeyad Tamimi (ZeyadTamimi@Outlook.com)
 * @copyright Copyright (c) 2019 Zeyad Tamimi. All rights reserved.
 *            This Source Code Form is subject to the terms of the Mozilla Public
 *            License, v. 2.0. If a copy of the MPL was not distributed with this
 *            file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * @TODO Add event system
 * @TODO Add notify
 * @TODO Add indicate
 * @TODO Add descriptors
 * @TODO Move some LOGEs to throws
 */

#ifndef BLE_BLE_CHARACTERISTIC_TPP
#define BLE_BLE_CHARACTERISTIC_TPP

/**
 * @brief Sets the value of the characteristic.
 * @tparam T The type of the value to set.
 * @param [in] value A value of type T to set on the characteristic.
 * @param [in] serializer (default=BLE_Value::default_serializer<T>) A serializer function which
 *             can convert the value of type T into a vector of uint8_t.
 * @note The default serializer will only work on simple types like intrinsics and will simply
 *       treat the types as a byte array.
 */
template<typename T>
inline
void
BLE_Characteristic::value_set(T value, BLE_Value::Serializer<T> serializer)
{
    m_value.value_set(value, serializer);
}


/**
 * @brief Retrieves the characteristic.
 * @tparam T The type of the value to get.
 * @param [in] deserializer (default=BLE_Value::default_deserializer<T>) A deserializer function
 *             which can convert a vector of uint8_t into a value of type T.
 * @note The default deserializer will only work on simple types like intrinsics and will simply
 *       treat the types as a byte array.
 * @return a value of type T.
 */
template<typename T>
inline
T
BLE_Characteristic::value_get(BLE_Value::Deserializer<T> deserializer) const
{
    return m_value.value_get<T>(deserializer);
}

#endif // BLE_BLE_CHARACTERISTIC_TPP

