/**
 * @file   ble_characteristic.hpp
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

#ifndef COMPONENTS_BLE_BLE_CHARACTERISTIC_HPP
#define COMPONENTS_BLE_BLE_CHARACTERISTIC_HPP

#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include "ble_value.hpp"
#include "types.hpp"

namespace BLE
{

class BLE_Service;


class BLE_Characteristic
{
public:
    using RW_Callback = std::function<void()>;


    BLE_Characteristic(UUID uuid, uint16_t handle, esp_gatt_if_t gatts_if,
                       std::weak_ptr<BLE_Service> service,
                       esp_gatt_char_prop_t properties = 0,
                       esp_gatt_perm_t permissions = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE)
        : uuid(uuid),
          handle(handle),
          gatts_if(gatts_if),
          service(service),
          properties(properties),
          permissions(permissions) {}

    /**
     * @brief Sets a callback function to be executed whenever a write operation is completed.
     * @param callback A callback function of type RW_Callback that will be executed;
     * @note This function will be executed before the response to the device is sent.
     */
    void callback_write_set(RW_Callback callback);

    /**
     * @brief Sets a callback function to be executed whenever a read operation is completed.
     * @param callback A callback function of type RW_Callback that will be executed;
     * @note This function will be executed before the response to the device is sent.
     */
    void callback_read_set(RW_Callback callback);

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
    void value_set(T value, BLE_Value::Serializer<T> serializer=BLE_Value::default_serializer<T>);

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
    T value_get(BLE_Value::Deserializer<T> deserializer=BLE_Value::default_deserializer<T>) const;

    void characteristic_event_handler_gatts(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                            esp_ble_gatts_cb_param_t *param);


    const UUID                          uuid;
    const uint16_t                      handle;
    const esp_gatt_if_t                 gatts_if;
    const std::weak_ptr<BLE_Service>    service;

    const esp_gatt_char_prop_t          properties;
    const esp_gatt_perm_t               permissions;

private:
    void handle_request_write(const esp_ble_gatts_cb_param_t::gatts_write_evt_param& param);
    void handle_request_exec_write(const esp_ble_gatts_cb_param_t::gatts_exec_write_evt_param& param);
    void handle_request_read(const esp_ble_gatts_cb_param_t::gatts_read_evt_param& param);

    BLE_Value                           m_value;

    RW_Callback                         m_callback_read;
    RW_Callback                         m_callback_write;
};

#include "ble_characteristic.tpp"

};

#endif // COMPONENTS_BLE_BLE_CHARACTERISTIC_HPP

