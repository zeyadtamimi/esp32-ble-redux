/**
 * @file   ble_server.hpp
 *
 * @brief  ESP32 Bluetooth Low Energy server abstraction.
 * @date   03/04/2019
 * @author Zeyad Tamimi (ZeyadTamimi@Outlook.com)
 * @copyright Copyright (c) 2019 Zeyad Tamimi. All rights reserved.
 *            This Source Code Form is subject to the terms of the Mozilla Public
 *            License, v. 2.0. If a copy of the MPL was not distributed with this
 *            file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * @TODO Server Stop
 * @TODO connection_get_all
 * @TODO synchronization
 * @TODO Make the singleton maintain a shared_prt instead of a weak ptr
 * @TODO Make this file thread safe (at the cost of code size)
 * @TODO Add event system
 * @TODO Give the ability to specify blocking opts for all functions
 */

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "utilities.hpp"

#include "ble_profile.hpp"
#include "ble_server.hpp"
#include "ble_service.hpp"
#include "uuid.hpp"

namespace BLE
{

constexpr const char* LOG_TAG_BLE_SERVER = "BLE Server";
constexpr const std::size_t MAX_ADV_UUID_LEN  = std::numeric_limits<decltype(std::declval<esp_ble_adv_data_t>().service_uuid_len)>::max();

/***************************************************************************************************
* Logging Macros
***************************************************************************************************/
#define SERVER_LOG(LVL, MSG, ...)\
    INTANCE_LOG(LVL, LOG_TAG_BLE_SERVER, "%s", "", MSG, ##__VA_ARGS__)

#define SERVER_LOGE(MSG, ...) SERVER_LOG(ESP_LOG_ERROR, MSG, ##__VA_ARGS__)
#define SERVER_LOGW(MSG, ...) SERVER_LOG(ESP_LOG_WARN, MSG, ##__VA_ARGS__)
#define SERVER_LOGI(MSG, ...) SERVER_LOG(ESP_LOG_INFO, MSG, ##__VA_ARGS__)
#define SERVER_LOGD(MSG, ...) SERVER_LOG(ESP_LOG_DEBUG, MSG, ##__VA_ARGS__)
#define SERVER_LOGV(MSG, ...) SERVER_LOG(ESP_LOG_VERBOSE, MSG, ##__VA_ARGS__)

/***************************************************************************************************
* Static Singleton Functions
***************************************************************************************************/
std::weak_ptr<BLE_Server> BLE_Server::instance;


/**
 * @brief Retrieve an instance of the current active BLE GATTS server or create an return a new one
 *        if none exist.
 * @note  The first caller will get the only shared pointer to the server. Therefore, in order to
 *        avoid the server being destroyed, you must keep it alive.
 * @todo  Potentially change this behaviour.
 * @return A shared pointer to the BLE GATTS server
 */
std::shared_ptr<BLE_Server>
BLE_Server::get_instance(void)
{
    auto instance_shared_ptr = instance.lock();
    if (instance_shared_ptr)
    {
        return instance_shared_ptr;
    }
    else
    {
        std::shared_ptr<BLE_Server> server = std::shared_ptr<BLE_Server>(new BLE_Server());
        instance = server;
        return server;
    }
}


/***************************************************************************************************
* Server management
***************************************************************************************************/
/**
 * @brief Starts the BLE GATTS server and enables BLE stack.
 * @return True if the operation succeeds, false otherwise.
 */
bool
BLE_Server::server_start(void)
{
    if (m_state != BLE_Server::State::STOPPED)
    {
        SERVER_LOGE("Server already started");
        return false;
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    esp_err_t err;
    err = esp_bt_controller_init(&bt_cfg);
    if (err)
    {
        SERVER_LOGE("Enable controller failed: %s (%d)", esp_err_to_name(err), err);
        return false;
    }
    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err)
    {
        SERVER_LOGE("Enable controller failed: %s (%d)", esp_err_to_name(err), err);
        return false;
    }

    err = esp_bluedroid_init();
    if (err)
    {
        SERVER_LOGE("Init bluetooth failed: %s (%d)", esp_err_to_name(err), err);
        return false;
    }

    err = esp_bluedroid_enable();
    if (err)
    {
        SERVER_LOGE("Enable bluetooth failed: %s (%d)", esp_err_to_name(err), err);
        return false;
    }
 
    err = esp_ble_gatt_set_local_mtu(m_server_mtu);
    if (err)
    {
        SERVER_LOGE("Set local MTU failed: %s (%d)", esp_err_to_name(err), err);
        return false;
    }

    // This static decoupling guarantees that there is always a valid instance that is being
    // referenced by the callbacks. This unfortunately causes some abstraction issues.
    esp_ble_gap_register_callback([](esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
                                    {
                                        auto active_server = BLE_Server::get_instance();
                                        active_server->event_handler_gap(event, param);
                                    });
    esp_ble_gatts_register_callback([](esp_gatts_cb_event_t event, esp_gatt_if_t inf,
                                       esp_ble_gatts_cb_param_t *param)
                                    {
                                        auto active_server = BLE_Server::get_instance();
                                        active_server->event_handler_gatts(event, inf, param);
                                    });

    esp_ble_gap_set_device_name(m_device_name.c_str());

    m_state = BLE_Server::State::IDLE;
    return true;
}


/***************************************************************************************************
* Connection and advertising related functions
***************************************************************************************************/
/**
 * @brief Sets the device information that can be seen by external scanners
 * @param [in] dev_name The device name.
 * @param [in] appearance The device appearance as defined in esp_gap_ble_api.h
 * @return True if the operation succeeds, false otherwise.
 */
bool
BLE_Server::device_information_set(std::string dev_name, int appearance)
{
    // TODO Fix race condition when advertising has not yet begun
    // TODO Add blocking option
    m_device_name = dev_name;

    if (m_state != BLE_Server::State::STOPPED)
    {
        esp_err_t err = esp_ble_gap_set_device_name(m_device_name.c_str());
        if (err)
        {
            SERVER_LOGE("Device name change failed: %s", esp_err_to_name(err));
            return false;
        }
    }

    m_appearance = appearance;

    if (m_state == BLE_Server::State::ADVERTISING)
        return advertising_start();

    return true;
}


/**
 * @brief Sets the device's advertising parameters.
 * @param [in] interval_min Minimum advertising interval for undirected and low duty cycle directed
 *                          advertising. Range: 0x0020 to 0x4000. Default: N = 0x0800 (1.28 second).
 *                          Time = N * 0.625 msec. Time Range: 20 ms to 10.24 sec.
 * @param [in] interval_max Maximum advertising interval for undirected and low duty cycle directed
 *                          advertising. Range: 0x0020 to 0x4000. Default: N = 0x0800 (1.28 second).
 *                          Time = N * 0.625 msec. Time Range: 20 ms to 10.24 sec.
 * @return True if the operation succeeds, false otherwise.
 */
bool
BLE_Server::advertising_parameters_set(int interval_min, int interval_max)
{
    // TODO Fix race condition when advertising has not yet begun

    if (interval_min > interval_max)
        return false;

    m_advertising_interval = std::make_pair(interval_min, interval_max);

    if (m_state == BLE_Server::State::ADVERTISING)
        return advertising_start();

    return true;
}


/**
 * @brief Updates the connection parameters for devices connecting to this server
 * @param [in] interval The min and max connection intervals.
 * @param [in] latency Slave latency for the connection in number of connection events.
 *             Range: 0x0000 to BLE_SERVER_CONNECTION_LATENCY_MAX
 * @param [in] timeout Supervision timeout for the LE Link.
 *             Range: BLE_SERVER_CONNECTION_TIMEOUT_MIN to BLE_SERVER_CONNECTION_TIMEOUT_MAX
 *             Time = N * 10 msec
 * @return True if the operation succeeds, false otherwise.
 */
bool
BLE_Server::connection_parameters_set(std::pair<uint16_t,uint16_t> interval, uint16_t latency,
                                      uint16_t timeout)
{
    // TODO Fix race condition when advertising has not yet begun
    if (interval.first > interval.second)
        return false;

    if (latency > BLE_SERVER_CONNECTION_LATENCY_MAX)
        return false;

    if ((timeout > BLE_SERVER_CONNECTION_TIMEOUT_MAX) ||
        (timeout < BLE_SERVER_CONNECTION_TIMEOUT_MIN))
        return false;

    m_connection_interval = interval;
    m_connection_latency = latency;
    m_connection_timeout = timeout;

    if (m_state == BLE_Server::State::ADVERTISING)
        return advertising_start();

    // TODO apply this to all existing connections

    return true;

}


/**
 * @brief Starts advertising the device to external scanners.
 * @return True if the operation succeeds, false otherwise.
 */
bool
BLE_Server::advertising_start(void)
{
    // TODO Add blocking option

    esp_ble_adv_data_t adv_data = adv_data_gen();
    esp_err_t err = esp_ble_gap_config_adv_data(&adv_data);
    if (err)
    {
        SERVER_LOGE("Error: adv data set failed: %s (%d)", esp_err_to_name(err), err);
        return false;
    }

    return true;
}


/**
 * @brief Stops advertising the device to external scanners.
 */
void
BLE_Server::advertising_stop(void)
{
    esp_err_t err = esp_ble_gap_stop_advertising();
    if (err)
        SERVER_LOGE("Error: adv stop failed: %s (%d)", esp_err_to_name(err), err);
}


/**
 * @brief Generate the primary advertising parameters for the GATTS server.
 * @return An ESP-32 advertising parameters struct.
 */
esp_ble_adv_params_t
BLE_Server::adv_params_gen(void)
{
    // TODO Make this more parameterized
    esp_ble_adv_params_t adv_params;
    adv_params.adv_int_min =  std::get<0>(m_advertising_interval);
    adv_params.adv_int_max =  std::get<1>(m_advertising_interval);
    adv_params.adv_type = ADV_TYPE_IND;
    adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    adv_params.channel_map = ADV_CHNL_ALL;
    adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    return adv_params;
}


/**
 * @brief Generate the primary advertising payload for the GATTS server.
 * @return An ESP-32 advertising data struct.
 */
esp_ble_adv_data_t
BLE_Server::adv_data_gen(void)
{
     m_adv_uuids.clear();
    for (auto profile : m_profiles)
    {
        for (auto service_weak_ptr : profile.second->service_get_all())
        {
            auto service = service_weak_ptr.lock();
            if (!service->advertise)
                continue;


            std::array<uint8_t, 16> raw_uuid = service->uuid.to_raw_128();

            // Since the maximum service_UUID_length is 65536, we need to ensure that we don't
            // exceed that size.
            if ((m_adv_uuids.size() + raw_uuid.size()) > MAX_ADV_UUID_LEN)
            {
                SERVER_LOGW("Warning: Too many advertising services, truncating list");
                break;
            }
            m_adv_uuids.insert(m_adv_uuids.begin(), raw_uuid.begin(), raw_uuid.end());
        }
    }

    SERVER_LOGI("Setting advertising UUIDs:");
    ESP_LOG_BUFFER_HEX(LOG_TAG_BLE_SERVER, m_adv_uuids.data(), m_adv_uuids.size());

    // TODO Make this more parameterized
    return
    {
        set_scan_rsp: false,
        include_name: true,
        include_txpower: true,
        min_interval: m_connection_interval.first,
        max_interval: m_connection_interval.second,
        appearance: m_appearance,
        manufacturer_len: 0,
        p_manufacturer_data: NULL,
        service_data_len: 0,
        p_service_data: NULL,
        service_uuid_len: (uint16_t) m_adv_uuids.size(),
        p_service_uuid: m_adv_uuids.data(),
        flag: (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };
}


/***************************************************************************************************
* GATTS control functions
***************************************************************************************************/
/**
 * @brief Adds a profile to the BLE Server.
 * @detail BLE Profiles are designed to help separate and compartmentalise application
 *         functionality, Each Application Profile describes a way to group functionalities that are
 *         designed for one client application.
 * @param [in] profile_id A unique ID to be assigned to the newly created profile.
 * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
 *                                     value is set to false the function is not guaranteed to
 *                                     complete by the time the call completes, in that case callers
 *                                     should use the event mechanism to determine when the call
 *                                     has finished execution.
 * @return True on success, false otherwise.
 */
bool
BLE_Server::profile_add(uint16_t profile_id, bool blocking)
{
    if (m_state == BLE_Server::State::STOPPED)
        return false;

    if (m_profiles.count(profile_id))
        return false;

    auto creation_function = [&, this](){
        esp_err_t err = esp_ble_gatts_app_register(profile_id);
        if (err)
        {
            SERVER_LOGE("Profile add failed: %s (%d)", esp_err_to_name(err), err);
            return false;
        }
        return true;
    };

    if (!blocking)
        return creation_function();

    auto result_async = m_notification_mgr.wait(profile_id, OP::PROFILE_ADD, creation_function);
    if (!result_async)
        return false;

    return *result_async;
}

/**
 * @brief Removes a profile and all associated services, characteristics, etc...
 * @note This is an async function and is not guaranteed to complete by the time the call completes,
 *       callers should use the event mechanism to determine when the call has finished.
 * @param [in] profile_id The unique ID for the profile to be deleted.
 * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
 *                                     value is set to false the function is not guaranteed to
 *                                     complete by the time the call completes, in that case callers
 *                                     should use the event mechanism to determine when the call
 *                                     has finished execution.
 */
void
BLE_Server::profile_remove(uint16_t profile_id, bool blocking)
{
    if (!m_profiles.count(profile_id))
        return;

    auto deletion_function = [&, this](){
        esp_err_t err = esp_ble_gatts_app_unregister(m_profiles.at(profile_id)->gatts_if);
        if (err)
        {
            SERVER_LOGE("Profile remove failed: %s (%d)", esp_err_to_name(err), err);
            return false;
        }

        return true;
    };

    if (blocking)
        m_notification_mgr.wait(profile_id, OP::PROFILE_REMOVE, deletion_function);
    else
        deletion_function();

    m_profiles.erase(profile_id);
}


/**
 * @brief Retrieves a weak pointer to the desired profile.
 * @param [in] profile_id The unique ID for the profile to be retrieved.
 * @return A weak pointer to the profile if the id is valid, else a default constructed weak
 *         pointer (nullptr)
 */
std::weak_ptr<BLE_Profile>
BLE_Server::profile_get(uint16_t profile_id)
{
    if (!m_profiles.count(profile_id))
        return {};

    return m_profiles[profile_id];
}


/**
 * @breif Retrieves the connection information associated with a particular connection ID.
 * @param [in] connection_id The connection ID of interest.
 * @return The uint32_t value supplied by the waking task or std::nullopt if the operation timed
 *         out.
 */
std::optional<connection_t>
BLE_Server::connection_get(uint16_t connection_id)
{
    if (!m_connections.count(connection_id))
        return {};

    return m_connections.at(connection_id);
}


/***************************************************************************************************
* Event Handling
***************************************************************************************************/
void
BLE_Server::handle_connection_new(const esp_ble_gatts_cb_param_t::gatts_connect_evt_param& param)
{
    if (m_connections.count(param.conn_id))
    {
        SERVER_LOGE("Connection ID already exists: 0x%04X", param.conn_id);
        return;
    }

    connection_t new_connection;
    new_connection.mtu = MTU_DEFAULT_BLE_CLIENT;
    memcpy(new_connection.bda, param.remote_bda, sizeof(esp_bd_addr_t));
    m_connections.insert(std::make_pair(param.conn_id, new_connection));

    esp_ble_conn_update_params_t connection_params;
    connection_params.min_int = m_connection_interval.first;
    connection_params.max_int = m_connection_interval.second;
    connection_params.latency = m_connection_latency;
    connection_params.timeout = m_connection_timeout;
    memcpy(connection_params.bda, param.remote_bda, sizeof(esp_bd_addr_t));
    esp_err_t err = esp_ble_gap_update_conn_params(&connection_params);
    if (err)
        SERVER_LOGE("Connection parameter update failed: %s (%d)", esp_err_to_name(err), err);


    SERVER_LOGI("Client connected: 0x%04X", param.conn_id);
}

void
BLE_Server::handle_connection_delete(const esp_ble_gatts_cb_param_t::gatts_disconnect_evt_param& param)
{
    if (!m_connections.count(param.conn_id))
    {
        SERVER_LOGE("Cannot delete nonexistent connection ID 0x%04X", param.conn_id);
        return;
    }

    if(memcmp(m_connections.at(param.conn_id).bda, param.remote_bda, sizeof(esp_bd_addr_t)) != 0)
        SERVER_LOGW("Connection ID 0x%04X BDA miss-match", param.conn_id);

    m_connections.erase(param.conn_id);

    SERVER_LOGI("Client disconnected: 0x%04X with reason: 0x%04X", param.conn_id, param.reason);

    if (m_state == BLE_Server::State::ADVERTISING)
        advertising_start();
}


void
BLE_Server::handle_connection_mtu_update(const esp_ble_gatts_cb_param_t::gatts_mtu_evt_param& param)
{
    if (!m_connections.count(param.conn_id))
        return;

    m_connections[param.conn_id].mtu = param.mtu;
    SERVER_LOGI("Connection 0x%04X requested MTU change: %d", param.conn_id, param.mtu);
}


void
BLE_Server::handle_profile_add(esp_gatt_if_t gatts_if,
                               const esp_ble_gatts_cb_param_t::gatts_reg_evt_param& param)
{
    if (param.status == ESP_GATT_OK)
    {
        m_profiles.insert(std::make_pair(param.app_id,
                                         std::make_shared<BLE_Profile>(param.app_id, gatts_if,
                                                                       get_instance())));
        SERVER_LOGI("Profile Registration complete: id %04x", param.app_id);
    }
    else
    {
        SERVER_LOGE("Profile Registration failed id: %04X, status: %d", param.app_id, param.status);
    }

    m_notification_mgr.notify(param.app_id, OP::PROFILE_ADD, param.status == ESP_GATT_OK);
}


void
BLE_Server::event_handler_gap(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    ESP_LOGD(LOG_TAG_BLE_SERVER, "GAP event = %d", event);
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        {
            esp_ble_adv_params_t adv_params = adv_params_gen();
            esp_err_t err = esp_ble_gap_start_advertising(&adv_params);
            if (err)
                SERVER_LOGE("Could not start advertising");
        }
        break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
            {
                m_state = BLE_Server::State::ADVERTISING;
                SERVER_LOGI("Advertising started");
            }
            else
            {
                ESP_LOGE(LOG_TAG_BLE_SERVER, "Advertising start failed");
            }
        break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            SERVER_LOGI("Update connection params status = %d, min_int = %d, max_int = %d, "
                        "conn_int = %d,latency = %d, timeout = %d",
                        param->update_conn_params.status,
                        param->update_conn_params.min_int,
                        param->update_conn_params.max_int,
                        param->update_conn_params.conn_int,
                        param->update_conn_params.latency,
                        param->update_conn_params.timeout);
        break;
        default:
        break;
    }
}


void BLE_Server::event_handler_gatts(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                     esp_ble_gatts_cb_param_t *param)
{
    SERVER_LOGD("GATTS event = %d, inf = 0x%04X", event, gatts_if);

    switch (event)
    {
        case ESP_GATTS_REG_EVT:
            handle_profile_add(gatts_if, param->reg);
        break;
        case ESP_GATTS_UNREG_EVT:
            SERVER_LOGI("Profile de-registration complete");
            if (m_state == BLE_Server::State::ADVERTISING)
                advertising_start();
            m_notification_mgr.notify(OP::PROFILE_REMOVE, true);
        break;
        case ESP_GATTS_CONNECT_EVT:
            handle_connection_new(param->connect);
        break;
        case ESP_GATTS_DISCONNECT_EVT:
            handle_connection_delete(param->disconnect);
            goto forward;
        break;
        case ESP_GATTS_MTU_EVT:
            handle_connection_mtu_update(param->mtu);
        case ESP_GATTS_EXEC_WRITE_EVT:
        case ESP_GATTS_READ_EVT:
        case ESP_GATTS_WRITE_EVT:
        default:
        forward:
            // Forward the event to all profiles
            for (auto profile : m_profiles)
            {
                if ((gatts_if == ESP_GATT_IF_NONE) || (gatts_if == profile.second->gatts_if))
                    profile.second->profile_event_handler_gatts(event, gatts_if, param);
            }
        break;
    }
}

};

