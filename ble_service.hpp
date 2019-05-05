/**
 * @file   ble_service.hpp
 *
 * @brief  ESP32 Bluetooth Low Energy service abstraction layer.
 * @detail This class represents a BLE service which can be included in other services or attached
 *         to a profile.
 * @date   03/04/2019
 * @author Zeyad Tamimi (ZeyadTamimi@Outlook.com)
 * @copyright Copyright (c) 2019 Zeyad Tamimi. All rights reserved.
 *            This Source Code Form is subject to the terms of the Mozilla Public
 *            License, v. 2.0. If a copy of the MPL was not distributed with this
 *            file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * @TODO Add event system
 * @TODO Add service stop
 * @TODO Add characteristic_remove
 * @TODO handle when the service start fails on startup
 * @TODO Move some LOGEs to throws
 */

#ifndef COMPONENTS_BLE_BLE_SERVICE_HPP
#define COMPONENTS_BLE_BLE_SERVICE_HPP

#include <cstdint>
#include <unordered_map>

#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "utilities.hpp"

#include "ble_characteristic.hpp"
#include "types.hpp"

namespace BLE
{

using Utilities::Notification_Manager;


class BLE_Profile;


class BLE_Service
{
public:
    enum class Status : uint8_t
    {
        STOPED,
        STARTED,
    };


    BLE_Service(esp_gatt_srvc_id_t service_id, uint16_t handle, esp_gatt_if_t gatts_if,
                bool advertise, std::weak_ptr<BLE_Profile> profile);

    /**
     * @brief Starts the service.
     * @note This function is thread safe.
     * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
     *                                     value is set to false the function is not guaranteed to
     *                                     complete by the time the call completes, in that case
     *                                     callers should use the event mechanism to determine when
     *                                     the call has finished execution.
     * @return True on success, false otherwise.
     */
    bool service_start(bool blocking=true);

    /**
     * @brief Adds a characteristic to the service.
     * @note This function is thread safe.
     * @param [in] uuid The UUID of the characteristic to be added.
     * @param [in] properties The defined operations and properties for the characteristic that will
     *             be created.
     * @param [in] permission The permissions that connecting clients will have.
     * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
     *                                     value is set to false the function is not guaranteed to
     *                                     complete by the time the call completes, in that case
     *                                     callers should use the event mechanism to determine when
     *                                     the call has finished execution.
     * @returns True on success, false otherwise.
     */
    bool characteristic_add(UUID uuid, esp_gatt_char_prop_t properties,
                            esp_gatt_perm_t permissions, bool blocking=true);

    /**
     * @brief Retrieves a characteristic that is defined on this service.
     * @note This function is thread safe.
     * @param [in] uuid The UUID of the characteristic of interest
     * @return A weak pointer to the BLE_Characteristic if the uuid is valid, else a default constructed
     *          weak pointer (nullptr).
     */
    std::weak_ptr<BLE_Characteristic> characteristic_get(UUID uuid);

    /**
     * @brief Retrieves a characteristic that is defined on this service.
     * @note This function is thread safe.
     * @param [in] uuid The UUID of the characteristic of interest
     * @return A weak pointer to the BLE_Characteristic if the uuid is valid, else a default constructed
     *          weak pointer (nullptr).
     */
    std::weak_ptr<BLE_Characteristic> characteristic_get(uint16_t handle);


    /**
     * @brief A function used internally by the framework to signal events to the profile.
     * @warning DO NOT CALL THIS FUNCTION
     * @TODO Make this private and friend the server?
     */
    void service_event_handler_gatts(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                     esp_ble_gatts_cb_param_t *param);


    const esp_gatt_srvc_id_t            service_id;
    const UUID                          uuid;
    const uint16_t                      handle;
    const esp_gatt_if_t                 gatts_if;
    const std::weak_ptr<BLE_Profile>    profile;
    bool                                advertise;

private:
    enum class OP
    {
        SERVICE_START,
        CHARACTERISTIC_ADD,
        CHARACTERISTIC_REMOVE,
    };

    using Characteristic_Map_UUID = std::unordered_map<UUID, std::shared_ptr<BLE_Characteristic>>;
    using Characteristic_Map_Handle = std::unordered_map<uint16_t, std::shared_ptr<BLE_Characteristic>>;
    using Characteristic_Creation_Map = std::unordered_map<UUID, std::pair<esp_gatt_char_prop_t,
                                                           esp_gatt_perm_t>>;


    void handle_characteristic_create(const esp_ble_gatts_cb_param_t::gatts_add_char_evt_param& param);


    BLE_Service::Status             m_status = BLE_Service::Status::STOPED;

    Characteristic_Map_UUID         m_characteristics_uuid;
    Characteristic_Map_Handle       m_characteristics_handle;
    Characteristic_Creation_Map     m_characteristics_creation;

    Notification_Manager<UUID, OP>  m_notification_mgr;
    SemaphoreHandle_t               m_characteristics_map_semaphore = xSemaphoreCreateBinary();
};

};

#endif //COMPONENTS_BLE_BLE__SERVICE_HPP

