/**
 * @file   ble_profile.hpp
 *
 * @brief  ESP32 Bluetooth Low Energy profile abstraction layer.
 * @detail This class represents a BLE Profile/App and allows the programmer to group together
 *         similar services
 * @date   03/04/2019
 * @author Zeyad Tamimi (ZeyadTamimi@Outlook.com)
 * @copyright Copyright (c) 2019 Zeyad Tamimi. All rights reserved.
 *            This Source Code Form is subject to the terms of the Mozilla Public
 *            License, v. 2.0. If a copy of the MPL was not distributed with this
 *            file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * @TODO Add event system
 * @TODO Move some LOGEs to throws
 */

#ifndef COMPONENTS_BLE_BLE_PROFILE_HPP
#define COMPONENTS_BLE_BLE_PROFILE_HPP

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "utilities.hpp"

#include "ble_service.hpp"

namespace BLE
{

using Utilities::Notification_Manager;


class BLE_Server;


class BLE_Profile
{
public:
    BLE_Profile(uint16_t id, esp_gatt_if_t gatts_if, std::weak_ptr<BLE_Server> server);

    /**
     * @brief Adds a service to the BLE Server under this profile.
     * @note This function is thread safe.
     * @param [in] uuid The UUID of the service to be added.
     * @prama [in] advertise Wether or not to advertise this service.
     * @param [in] requested_handle (default=0x20) The number of the handle requested for this
     *                                             device.
     *                                             Please note that this is not guaranteed to be
     *                                             satisfied if the handle is already taken.
     * @param [in] primary Whether or not this service is primary.
     * @param [in] inst_id (default=0x00) The instance id.
     * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
     *                                     value is set to false the function is not guaranteed to
     *                                     complete by the time the call completes, in that case
     *                                     callers should use the event mechanism to determine when
     *                                     the call has finished execution.
     * @return True on success, false otherwise.
     */
    bool service_add(UUID uuid, bool advertise, uint16_t requested_handle=0x0020, bool primary=true,
                     uint8_t inst_id=0x00, bool blocking=true);

    /**
     * @brief Removes a service from the BLE server.
     * @detail Remove a service and all its associated characteristics and descriptors from the BLE
     *         server.
     * @note This function is thread safe.
     * @param [in] uuid The UUID of the service to remove.
     * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
     *                                     value is set to false the function is not guaranteed to
     *                                     complete by the time the call completes, in that case
     *                                     callers should use the event mechanism to determine when
     *                                     the call has finished execution.
     */
    void service_remove(UUID uuid, bool blocking=true);

    /**
     * @brief Removes a service from the BLE Server.
     * @detail Remove a service and all its associated characteristics and descriptors from the BLE
     *         server.
     * @note This function is thread safe.
     * @param [in] handle The handle of the service to remove.
     * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
     *                                     value is set to false the function is not guaranteed to
     *                                     complete by the time the call completes, in that case
     *                                     callers should use the event mechanism to determine when
     *                                     the call has finished execution.
     */
    void service_remove(uint16_t handle, bool blocking=true);

    /**
     * @brief Retrieves a server that is attached to this profile.
     * @note This function is thread safe.
     * @param [in] uuid The UUID of the service of interest.
     * @return A weak pointer to the BLE_Service if the uuid is valid, else a default constructed
     *         weak pointer (nullptr).
     */
    std::weak_ptr<BLE_Service> service_get(UUID uuid);

    /**
     * @brief Retrieves a server that is attached to this profile.
     * @note This function is thread safe.
     * @param [in] handle The handle of the service of interest.
     * @return A weak pointer to the BLE_Service if the handle is valid, else a default constructed
     *         weak pointer (nullptr).
     */
    std::weak_ptr<BLE_Service> service_get(uint16_t handle);

    /**
     * @brief Retrieves all services attached to this profile.
     * @note This function is thread safe.
     * @return A vector of weak pointer to the BLE_Service objects attached to this profile.
     */
    std::vector<std::weak_ptr<BLE_Service>> service_get_all(void);

    /**
     * @brief A function used internally by the framework to signal events to the profile.
     * @warning DO NOT CALL THIS FUNCTION
     * @TODO Make this private and friend the server?
     */
    void profile_event_handler_gatts(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                     esp_ble_gatts_cb_param_t *param);


    const uint16_t                      id;
    const esp_gatt_if_t                 gatts_if;
    const std::weak_ptr<BLE_Server>     server;

private:
    enum class OP
    {
        SERVICE_ADD,
        SERVICE_REMOVE,
    };

    using Service_Map_UUID = std::unordered_map<UUID, std::shared_ptr<BLE_Service>>;
    using Service_Map_Handle = std::unordered_map<uint16_t, std::shared_ptr<BLE_Service>>;
    using Service_Creation_Map = std::unordered_map<UUID, bool>;


    void handle_service_add(const esp_ble_gatts_cb_param_t::gatts_create_evt_param& param);
    void handle_service_remove(const esp_ble_gatts_cb_param_t::gatts_delete_evt_param& param);


    Service_Map_UUID                m_services_uuid;
    Service_Map_Handle              m_services_handle;
    Service_Creation_Map            m_services_creation;
    Notification_Manager<UUID, OP>  m_notification_mgr;

    SemaphoreHandle_t               m_service_map_semaphore = xSemaphoreCreateBinary();
};

};

#endif //  COMPONENTS_BLE_BLE_PROFILE_HPP

