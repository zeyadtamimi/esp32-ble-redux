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

#include <cstdint>

#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "utilities.hpp"

#include "ble_service.hpp"
#include "ble_profile.hpp"
#include "types.hpp"

namespace BLE
{

using Utilities::AnchorSemaphore;
using Utilities::Notification_Manager;


constexpr const char* LOG_TAG_BLE_SERVICE = "BLE Service";

/***************************************************************************************************
* Logging Macros
***************************************************************************************************/
#define SERVICE_LOG(LVL, MSG, ...)\
    INTANCE_LOG(LVL, LOG_TAG_BLE_SERVICE, "%04X --", this->handle, MSG, ##__VA_ARGS__)

#define SERVICE_LOGE(MSG, ...) SERVICE_LOG(ESP_LOG_ERROR, MSG, ##__VA_ARGS__)
#define SERVICE_LOGW(MSG, ...) SERVICE_LOG(ESP_LOG_WARN, MSG, ##__VA_ARGS__)
#define SERVICE_LOGI(MSG, ...) SERVICE_LOG(ESP_LOG_INFO, MSG, ##__VA_ARGS__)
#define SERVICE_LOGD(MSG, ...) SERVICE_LOG(ESP_LOG_DEBUG, MSG, ##__VA_ARGS__)
#define SERVICE_LOGV(MSG, ...) SERVICE_LOG(ESP_LOG_VERBOSE, MSG, ##__VA_ARGS__)


/***************************************************************************************************
* Service Member Functions
***************************************************************************************************/
BLE_Service::BLE_Service(esp_gatt_srvc_id_t service_id, uint16_t handle, esp_gatt_if_t gatts_if,
                         bool advertise, std::weak_ptr<BLE_Profile> profile)
    : service_id(service_id),
      uuid(service_id.id.uuid.uuid.uuid128, service_id.id.uuid.len),
      handle(handle),
      gatts_if(gatts_if),
      profile(profile),
      advertise(advertise)
{
    if (m_characteristics_map_semaphore == nullptr)
        throw std::bad_alloc();

    xSemaphoreGive(m_characteristics_map_semaphore);

    // If this call is blocking a deadlock will occur
    service_start(false);
}


/**
 * @brief Starts the service.
 * @note This function is thread safe.
 * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
 *                                     value is set to false the function is not guaranteed to
 *                                     complete by the time the call completes, in that case callers
 *                                     should use the event mechanism to determine when the call
 *                                     has finished execution.
 * @return True on success, false otherwise.
 */
bool
BLE_Service::service_start(bool blocking)
{

    auto starting_function =[&,this](){
        esp_err_t err = esp_ble_gatts_start_service(handle);
        if (err)
        {
            SERVICE_LOGE("Cannot start service 0x%04X: %s (%d)", handle, esp_err_to_name(err), err);
            return false;
        }

        SERVICE_LOGI("Starting service");
        return true;
    };

    if (!blocking)
        return starting_function();

    auto result_async = m_notification_mgr.wait(uuid, OP::SERVICE_START, starting_function);
    if (!result_async)
    {
        SERVICE_LOGI("Async operation failed");
        return false;
    }

    return *result_async;
}


/**
 * @brief Adds a characteristic to the service.
 * @note This function is thread safe.
 * @param [in] uuid The UUID of the characteristic to be added.
 * @param [in] properties The defined operations and properties for the characteristic that will
 *             be created.
 * @param [in] permission The permissions that connecting clients will have.
 * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
 *                                     value is set to false the function is not guaranteed to
 *                                     complete by the time the call completes, in that case callers
 *                                     should use the event mechanism to determine when the call
 *                                     has finished execution.
 * @returns True on success, false otherwise.
 */
bool
BLE_Service::characteristic_add(UUID uuid, esp_gatt_char_prop_t properties,
                                esp_gatt_perm_t permissions, bool blocking)
{
    {
        AnchorSemaphore anchor(m_characteristics_map_semaphore);
        if (m_characteristics_uuid.count(uuid) || m_characteristics_creation.count(uuid))
        {
            SERVICE_LOGE("Characteristic already exists: %s", uuid.to_string().c_str());
            return false;
        }
    }


    auto creation_function = [&, this](){
        esp_bt_uuid_t esp_uuid = uuid.to_esp_uuid();
        esp_err_t err = esp_ble_gatts_add_char(handle, &esp_uuid, permissions, properties,
                                               nullptr, nullptr);
        if (err)
        {
            AnchorSemaphore anchor(m_characteristics_map_semaphore);
            m_characteristics_creation.erase(uuid);
            SERVICE_LOGE("Characteristic creation failed for %s: %s (%d)", uuid.to_string().c_str(),
                                                                           esp_err_to_name(err),
                                                                           err);
            return false;
        }

        return true;
    };

    {
        AnchorSemaphore anchor(m_characteristics_map_semaphore);
        m_characteristics_creation.insert(std::make_pair(uuid, std::make_pair(properties,
                                                                              permissions)));
    }

    if(!blocking)
        return creation_function();

    auto result_async = m_notification_mgr.wait(uuid, OP::CHARACTERISTIC_ADD, creation_function);
    if (!result_async)
    {
        SERVICE_LOGE("Async operation failed");
        AnchorSemaphore anchor(m_characteristics_map_semaphore);
        m_characteristics_creation.erase(uuid);
        return false;
    }

    return *result_async;
}


/**
 * @brief Retrieves a characteristic that is defined on this service.
 * @note This function is thread safe.
 * @param [in] uuid The UUID of the characteristic of interest
 * @return A weak pointer to the BLE_Characteristic if the uuid is valid, else a default constructed
 *          weak pointer (nullptr).
 */
std::weak_ptr<BLE_Characteristic>
BLE_Service::characteristic_get(UUID uuid)
{
    AnchorSemaphore anchor(m_characteristics_map_semaphore);
    if (!m_characteristics_uuid.count(uuid))
        return {};

    return m_characteristics_uuid.at(uuid);
}


/***************************************************************************************************
* Event Handling
***************************************************************************************************/
inline
void
BLE_Service::handle_characteristic_create(const esp_ble_gatts_cb_param_t::gatts_add_char_evt_param& param)
{
    UUID uuid(param.char_uuid.uuid.uuid128, param.char_uuid.len);

    if (!m_characteristics_creation.count(uuid))
    {
        SERVICE_LOGE("Received unsolicited characteristic creation event: %s",
                     uuid.to_string().c_str());
        AnchorSemaphore anchor(m_characteristics_map_semaphore);
        m_notification_mgr.notify(uuid, OP::CHARACTERISTIC_ADD, false);
        return;
    }

    if (m_characteristics_uuid.count(uuid))
    {
        SERVICE_LOGE("Received characteristic creation event for existing characteristic: %s",
                     uuid.to_string().c_str());
        AnchorSemaphore anchor(m_characteristics_map_semaphore);
        m_characteristics_creation.erase(uuid);
        m_notification_mgr.notify(uuid, OP::CHARACTERISTIC_ADD, false);
        return;
    }

    if (param.status != ESP_GATT_OK)
    {
        SERVICE_LOGE("Characteristic creation failed for %s: %s (%d)", uuid.to_string().c_str(),
                                                                       esp_err_to_name(param.status),
                                                                       param.status);
        AnchorSemaphore anchor(m_characteristics_map_semaphore);
        m_characteristics_creation.erase(uuid);
        m_notification_mgr.notify(uuid, OP::CHARACTERISTIC_ADD, false);
        return;
    }

    // TODO Do we really need all these checks? Cant we just throw? This is an unanticipated and
    // unrecoverable case?
    auto profile_instance = profile.lock();
    if (!profile_instance)
    {
        SERVICE_LOGE("Profile instance does not exist despite receiving event");
        AnchorSemaphore anchor(m_characteristics_map_semaphore);
        m_characteristics_creation.erase(uuid);
        m_notification_mgr.notify(uuid, OP::CHARACTERISTIC_ADD, false);
        return;
    }

    auto self_ptr = profile_instance->service_get(handle).lock();
    if (!self_ptr)
    {
        SERVICE_LOGE("Profile does not acknowledge that this profile exists");
        AnchorSemaphore anchor(m_characteristics_map_semaphore);
        m_characteristics_creation.erase(uuid);
        m_notification_mgr.notify(uuid, OP::CHARACTERISTIC_ADD, false);
        return;
    }

    AnchorSemaphore anchor(m_characteristics_map_semaphore);
    auto creation_data = m_characteristics_creation[uuid];
    auto characteristic = std::make_shared<BLE_Characteristic>(uuid, param.attr_handle,
                                                               gatts_if,
                                                               self_ptr,
                                                               creation_data.first,
                                                               creation_data.second);


    m_characteristics_uuid.insert(std::make_pair(uuid, characteristic));
    m_characteristics_handle.insert(std::make_pair(param.attr_handle, characteristic));
    m_characteristics_creation.erase(uuid);
    m_notification_mgr.notify(uuid, OP::CHARACTERISTIC_ADD, true);
}

void
BLE_Service::service_event_handler_gatts(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                         esp_ble_gatts_cb_param_t *param)
{
    SERVICE_LOGV("GATTS event = %d, inf = 0x%04X", event, gatts_if);
    if (gatts_if != this->gatts_if)
    {
        SERVICE_LOGE("Invalid inf received: 0x%04X", gatts_if);
        return;
    }

    switch (event)
    {
        case ESP_GATTS_START_EVT:
            if (param->start.status == ESP_GATT_OK)
            {
                m_status = BLE_Service::Status::STARTED;
                SERVICE_LOGI("Service Started");
            }
            else
            {
                SERVICE_LOGI("Service Failed to start");
            }
            m_notification_mgr.notify(uuid, OP::SERVICE_START, param->start.status == ESP_GATT_OK);
        break;
        case ESP_GATTS_ADD_CHAR_EVT:
            handle_characteristic_create(param->add_char);
        break;
        default:
            for (auto characteristic : m_characteristics_uuid)
                characteristic.second->characteristic_event_handler_gatts(event, gatts_if, param);
        break;
    }
}

};

