/**
 * @file   ble_profile.cpp
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

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "esp_log.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "freertos/FreeRTOS.h"
#include "utilities.hpp"

#include "ble_profile.hpp"
#include "ble_server.hpp"
#include "ble_service.hpp"
#include "types.hpp"

namespace BLE
{

using Utilities::AnchorSemaphore;
using Utilities::Notification_Manager;


constexpr const char* LOG_TAG_BLE_PROFILE = "BLE Profile";

/***************************************************************************************************
* Logging Macros
***************************************************************************************************/
#define PROFILE_LOG(LVL, MSG, ...)\
    INTANCE_LOG(LVL, LOG_TAG_BLE_PROFILE, "%04X --", id, MSG, ##__VA_ARGS__)

#define PROFILE_LOGE(MSG, ...) PROFILE_LOG(ESP_LOG_ERROR, MSG, ##__VA_ARGS__)
#define PROFILE_LOGW(MSG, ...) PROFILE_LOG(ESP_LOG_WARN, MSG, ##__VA_ARGS__)
#define PROFILE_LOGI(MSG, ...) PROFILE_LOG(ESP_LOG_INFO, MSG, ##__VA_ARGS__)
#define PROFILE_LOGD(MSG, ...) PROFILE_LOG(ESP_LOG_DEBUG, MSG, ##__VA_ARGS__)
#define PROFILE_LOGV(MSG, ...) PROFILE_LOG(ESP_LOG_VERBOSE, MSG, ##__VA_ARGS__)


/***************************************************************************************************
* Profile Member Functions
***************************************************************************************************/
BLE_Profile::BLE_Profile(uint16_t id, esp_gatt_if_t gatts_if, std::weak_ptr<BLE_Server> server)
    : id(id),
      gatts_if(gatts_if),
      server(server)
{

    if (m_service_map_semaphore == nullptr)
        throw std::bad_alloc();

    xSemaphoreGive(m_service_map_semaphore);
}


/**
 * @brief Adds a service to the BLE Server under this profile.
 * @note This function is thread safe.
 * @param [in] uuid The UUID of the service to be added.
 * @prama [in] advertise Wether or not to advertise this service.
 * @param [in] requested_handle (default=0x20) The number of the handle requested for this device.
 *                                             Please note that this is not guaranteed to be
 *                                             satisfied if the handle is already taken.
 * @param [in] primary Whether or not this service is primary.
 * @param [in] inst_id (default=0x00) The instance id.
 * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
 *                                     value is set to false the function is not guaranteed to
 *                                     complete by the time the call completes, in that case callers
 *                                     should use the event mechanism to determine when the call
 *                                     has finished execution.
 * @return True on success, false otherwise.
 */
bool
BLE_Profile::service_add(UUID uuid, bool advertise, uint16_t requested_handle, bool primary,
                         uint8_t inst_id, bool blocking)
{

    {
        AnchorSemaphore anchor(m_service_map_semaphore);
        if (m_services_uuid.count(uuid) || m_services_creation.count(uuid))
        {
            PROFILE_LOGE("Service already exists: %s", uuid.to_string().c_str());
            return false;
        }
    }

    esp_gatt_srvc_id_t service_id;
    service_id.is_primary = primary;
    service_id.id.inst_id = inst_id;
    service_id.id.uuid = uuid.to_esp_uuid();

    auto creation_function = [&, this](){
        esp_err_t err = esp_ble_gatts_create_service(gatts_if, &service_id, requested_handle);
        if (err)
        {
            PROFILE_LOGE("Service add failed for %s: %s (%d)", uuid.to_string().c_str(),
                                                               esp_err_to_name(err), err);
            AnchorSemaphore anchor(m_service_map_semaphore);
            m_services_creation.erase(uuid);
            return false;
        }
        return true;
    };

    {
        AnchorSemaphore anchor(m_service_map_semaphore);
        m_services_creation.insert(std::make_pair(uuid, advertise));
    }

    if (!blocking)
        return creation_function();

    auto result_async = m_notification_mgr.wait(uuid, OP::SERVICE_ADD, creation_function);
    if (!result_async)
    {
        AnchorSemaphore anchor(m_service_map_semaphore);
        m_services_creation.erase(uuid);
        PROFILE_LOGE("Service add failed for %s", uuid.to_string().c_str());
        return false;
    }

    return *result_async;
}


/**
 * @brief Removes a service from the BLE server.
 * @detail Remove a service and all its associated characteristics and descriptors from the BLE
 *         server.
 * @note This function is thread safe.
 * @param [in] uuid The UUID of the service to remove.
 * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
 *                                     value is set to false the function is not guaranteed to
 *                                     complete by the time the call completes, in that case callers
 *                                     should use the event mechanism to determine when the call
 *                                     has finished execution.
 */
void
BLE_Profile::service_remove(UUID uuid, bool blocking)
{
    {
        AnchorSemaphore anchor(m_service_map_semaphore);
        if (!m_services_uuid.count(uuid))
            return;
    }

    auto deletion_function = [&, this](){
        esp_err_t err = esp_ble_gatts_delete_service(m_services_uuid[uuid]->handle);
        if (err)
        {
            PROFILE_LOGE("Service remove failed for %s: %s (%d)", uuid.to_string().c_str(),
                                                                  esp_err_to_name(err), err);
            return false;
        }

        return true;
    };

    // TODO Should we handle remove failures? or ignore them?
    if (blocking)
        m_notification_mgr.wait(uuid, OP::SERVICE_REMOVE, deletion_function);
    else
        deletion_function();

    AnchorSemaphore anchor(m_service_map_semaphore);
    m_services_handle.erase(m_services_uuid[uuid]->handle);
    m_services_uuid.erase(uuid);
}


/**
 * @brief Removes a service from the BLE Server.
 * @detail Remove a service and all its associated characteristics and descriptors from the BLE
 *         server.
 * @note This function is thread safe.
 * @param [in] handle The handle of the service to remove.
 * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
 *                                     value is set to false the function is not guaranteed to
 *                                     complete by the time the call completes, in that case callers
 *                                     should use the event mechanism to determine when the call
 *                                     has finished execution.
 */
void
BLE_Profile::service_remove(uint16_t handle, bool blocking)
{
    UUID service_uuid;
    {
        AnchorSemaphore anchor(m_service_map_semaphore);
        if (!m_services_handle.count(handle))
            return;

        service_uuid = m_services_handle[handle]->uuid;
    }

    service_remove(service_uuid);
}


/**
 * @brief Retrieves a server that is attached to this profile.
 * @note This function is thread safe.
 * @param [in] uuid The UUID of the service of interest.
 * @return A weak pointer to the BLE_Service if the uuid is valid, else a default constructed
 *         weak pointer (nullptr).
 */
std::weak_ptr<BLE_Service>
BLE_Profile::service_get(UUID uuid)
{
    AnchorSemaphore anchor(m_service_map_semaphore);
    if (!m_services_uuid.count(uuid))
        return {};

    return m_services_uuid.at(uuid);
}


/**
 * @brief Retrieves a server that is attached to this profile.
 * @note This function is thread safe.
 * @param [in] handle The handle of the service of interest.
 * @return A weak pointer to the BLE_Service if the handle is valid, else a default constructed
 *         weak pointer (nullptr).
 */
std::weak_ptr<BLE_Service>
BLE_Profile::service_get(uint16_t handle)
{
    AnchorSemaphore anchor(m_service_map_semaphore);
    if (!m_services_handle.count(handle))
        return {};

    return m_services_handle.at(handle);
}


/**
 * @brief Retrieves all services attached to this profile.
 * @note This function is thread safe.
 * @return A vector of weak pointer to the BLE_Service objects attached to this profile.
 */
std::vector<std::weak_ptr<BLE_Service>>
BLE_Profile::service_get_all(void)
{
    AnchorSemaphore anchor(m_service_map_semaphore);
    std::vector<std::weak_ptr<BLE_Service>> ret;
    for (auto service : m_services_uuid)
        ret.push_back(service.second);

    return ret;
}


/***************************************************************************************************
* Event Handling
***************************************************************************************************/
inline
void
BLE_Profile::handle_service_add(const esp_ble_gatts_cb_param_t::gatts_create_evt_param& param)
{
    UUID uuid(param.service_id.id.uuid.uuid.uuid128, param.service_id.id.uuid.len);

    if (param.status != ESP_GATT_OK)
    {
        PROFILE_LOGE("Service creation Failed: 0x%04X", param.status);
        AnchorSemaphore anchor(m_service_map_semaphore);
        m_services_creation.erase(uuid);
        m_notification_mgr.notify(uuid, OP::SERVICE_ADD, false);
        return;
    }

    if (m_services_handle.count(param.service_handle))
    {
        PROFILE_LOGE("Duplicate service creation event: 0x%04X", param.service_handle);
        AnchorSemaphore anchor(m_service_map_semaphore);
        m_services_creation.erase(uuid);
        m_notification_mgr.notify(uuid, OP::SERVICE_ADD, false);
        return;
    }

    // TODO Do we really need all these checks? Cant we just throw? This is an unanticipated and
    // unrecoverable case?
    auto server_instance = server.lock();
    if (!server_instance)
    {
        PROFILE_LOGE("Server instance does not exist despite receiving event");
        AnchorSemaphore anchor(m_service_map_semaphore);
        m_services_creation.erase(uuid);
        m_notification_mgr.notify(uuid, OP::SERVICE_ADD, false);
        return;
    }

    auto self_ptr = server_instance->profile_get(id).lock();
    if (!self_ptr)
    {
        PROFILE_LOGE("Server does not acknowledge that this profile exists");
        AnchorSemaphore anchor(m_service_map_semaphore);
        m_services_creation.erase(uuid);
        m_notification_mgr.notify(uuid, OP::SERVICE_ADD, false);
        return;
    }

    AnchorSemaphore anchor(m_service_map_semaphore);
    auto service = std::make_shared<BLE_Service>(param.service_id,
                                                 param.service_handle,
                                                 gatts_if,
                                                 m_services_creation[uuid],
                                                 self_ptr);

    m_services_uuid.insert(std::make_pair(uuid, service));
    m_services_handle.insert(std::make_pair(param.service_handle, service));
    m_services_creation.erase(uuid);

    PROFILE_LOGI("Service successfully created: 0x%04X", param.service_handle);
    m_notification_mgr.notify(uuid, OP::SERVICE_ADD, true);
}

inline
void
BLE_Profile::handle_service_remove(const esp_ble_gatts_cb_param_t::gatts_delete_evt_param& param)
{

    if (param.status == ESP_GATT_OK)
        PROFILE_LOGI("Service 0x%04X removed successfully", param.service_handle);
    else
        PROFILE_LOGI("Service 0x%04X remove failed", param.service_handle);

    // If this was a result of a blocking call, the service entry should still be in the maps
    AnchorSemaphore anchor(m_service_map_semaphore);
    if (!m_services_handle.count(param.service_handle))
        return;

    m_notification_mgr.notify(m_services_handle[param.service_handle]->uuid,
                              OP::SERVICE_REMOVE, param.status == ESP_GATT_OK);
}

void
BLE_Profile::profile_event_handler_gatts(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                         esp_ble_gatts_cb_param_t *param)
{
    PROFILE_LOGV("GATTS event = %d, inf = 0x%04X", event, gatts_if);
    if (gatts_if != this->gatts_if)
    {
        PROFILE_LOGE("Invalid inf received: 0x%04X", this->gatts_if);
        return;
    }

    switch (event)
    {
        case ESP_GATTS_CREATE_EVT:
            handle_service_add(param->create);
        break;
        case ESP_GATTS_DELETE_EVT:
            handle_service_remove(param->del);
        break;
        default:
            for (auto services : m_services_uuid)
                services.second->service_event_handler_gatts(event, gatts_if, param);
        break;
    }
}

};

