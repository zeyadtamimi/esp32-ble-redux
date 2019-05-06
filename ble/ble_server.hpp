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

#ifndef COMPONENT_BLE_BLE_SERVER
#define COMPONENT_BLE_BLE_SERVER

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "utilities.hpp"

#include "ble_profile.hpp"
#include "ble_service.hpp"
#include "ble_utilities.hpp"


namespace BLE
{

using Utilities::Notification_Manager;


struct connection_t
{
    esp_bd_addr_t bda;
    uint16_t mtu;
};


class BLE_Server
{
public:
    enum class State
    {
        STOPPED,
        IDLE,
        ADVERTISING,
    };


    static constexpr const uint16_t BLE_SERVER_CONNECTION_LATENCY_MAX = 0x01F3;
    static constexpr const uint16_t BLE_SERVER_CONNECTION_TIMEOUT_MIN = 0x000A;
    static constexpr const uint16_t BLE_SERVER_CONNECTION_TIMEOUT_MAX = 0x0C80;


    /**
     * @brief Retrieve an instance of the current active BLE GATTS server or create an return a new
     *        one if none exist.
     * @note  The first caller will get the only shared pointer to the server. Therefore, in order
     *        to avoid the server being destroyed, you must keep it alive.
     * @return A shared pointer to the BLE GATTS server
     */
    static std::shared_ptr<BLE_Server> get_instance(void);

    /**
     * @brief Starts the BLE GATTS server and enables BLE stack.
     * @return True if the operation succeeds, false otherwise.
     */
    bool server_start(void);

    /**
     * @brief Starts advertising the device to external scanners.
     * @return True if the operation succeeds, false otherwise.
     */
    bool advertising_start(void);

    /**
     * @brief Stops advertising the device to external scanners.
     */
    void advertising_stop(void);

    /**
     * @brief Adds a profile to the BLE Server.
     * @detail BLE Profiles are designed to help separate and compartmentalise application
     *         functionality, Each Application Profile describes a way to group functionalities that
     *         are designed for one client application.
     * @param [in] profile_id A unique ID to be assigned to the newly created profile.
     * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
     *                                     value is set to false the function is not guaranteed to
     *                                     complete by the time the call completes, in that case
     *                                     callers should use the event mechanism to determine when
     *                                     the call has finished execution.
     * @return True on success, false otherwise.
     */
    bool profile_add(uint16_t profile_id, bool blocking=true);


    /**
     * @brief Removes a profile and all associated services, characteristics, etc...
     * @param [in] profile_id The unique ID for the profile to be deleted.
     * @param [in] blocking (default=true) Blocks the call until the operation completes. If this
     *                                     value is set to false the function is not guaranteed to
     *                                     complete by the time the call completes, in that case
     *                                     callers should use the event mechanism to determine when
     *                                     the call has finished execution.
     */
    void profile_remove(uint16_t profile_id, bool blocking=true);

    /**
     * @brief Retrieves a weak pointer to the desired profile.
     * @param [in] profile_id The unique ID for the profile to be retrieved.
     * @return A weak pointer to the profile if the id is valid, else a default constructed weak
     *         pointer (nullptr)
     */
    std::weak_ptr<BLE_Profile> profile_get(uint16_t profile_id);

    /**
     * @brief Retrieves a weak pointer to the desired profile.
     * @param [in] profile_id The unique ID for the profile to be retrieved.
     * @return A weak pointer to the profile if the id is valid, else a default constructed weak
     *         pointer (nullptr)
     */
    std::optional<connection_t> connection_get(uint16_t connection_id);

    /**
     * @brief Sets the device information that can be seen by external scanners
     * @param [in] dev_name The device name.
     * @param [in] appearance The device appearance as defined in esp_gap_ble_api.h
     * @return True if the operation succeeds, false otherwise.
     */
    bool device_information_set(std::string dev_name, int appearance);

    /**
     * @brief Sets the device's advertising parameters.
     * @param [in] interval_min Minimum advertising interval for undirected and low duty cycle
     *                          directed advertising.
     *                          Range: 0x0020 to 0x4000. Default: N = 0x0800 (1.28 second).
     *                          Time = N * 0.625 msec. Time Range: 20 ms to 10.24 sec.
     * @param [in] interval_max Maximum advertising interval for undirected and low duty cycle
     *                          directed advertising.
     *                          Range: 0x0020 to 0x4000. Default: N = 0x0800 (1.28 second).
     *                          Time = N * 0.625 msec. Time Range: 20 ms to 10.24 sec.
     * @return True if the operation succeeds, false otherwise.
     */
    bool advertising_parameters_set(int interval_min, int interval_max);

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
    bool connection_parameters_set(std::pair<uint16_t,uint16_t> interval, uint16_t latency,
                                   uint16_t timeout);

private:
    enum class OP
    {
        PROFILE_ADD,
        PROFILE_REMOVE,
        ADV_START,
        ADV_STOP,
    };

    using Profile_Map = std::unordered_map<uint16_t, std::shared_ptr<BLE_Profile>>;
    using Connection_Map = std::unordered_map<uint16_t, connection_t>;


    BLE_Server(void) {}

    esp_ble_adv_data_t adv_data_gen(void);
    esp_ble_adv_params_t adv_params_gen(void);

    void handle_profile_add(esp_gatt_if_t gatts_if,
                            const esp_ble_gatts_cb_param_t::gatts_reg_evt_param& param);
    void handle_connection_new(const esp_ble_gatts_cb_param_t::gatts_connect_evt_param& param);
    void handle_connection_delete(const esp_ble_gatts_cb_param_t::gatts_disconnect_evt_param& param);
    void handle_connection_mtu_update(const esp_ble_gatts_cb_param_t::gatts_mtu_evt_param& param);

    void event_handler_gap(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
    void event_handler_gatts(esp_gatts_cb_event_t event, esp_gatt_if_t inf,
                             esp_ble_gatts_cb_param_t *param);


    static std::weak_ptr<BLE_Server>    instance;

    BLE_Server::State                   m_state = BLE_Server::State::STOPPED;

    std::string                         m_device_name = "ESP Device";
    int                                 m_appearance = ESP_BLE_APPEARANCE_GENERIC_WATCH;
    std::pair<int,int>                  m_advertising_interval = std::make_pair(0x20, 0x40);

    std::pair<uint16_t,uint16_t>        m_connection_interval = std::make_pair(0x10, 0x30);
    uint16_t                            m_connection_latency = 0x0;
    uint16_t                            m_connection_timeout = 400;

    Profile_Map                         m_profiles;
    Connection_Map                      m_connections;
    Notification_Manager<uint16_t, OP>  m_notification_mgr;
    std::vector<uint8_t>                m_adv_uuids;
    uint16_t                            m_server_mtu = MTU_DEFAULT_BLE_SERVER;
};

};

#endif // COMPONENT_BLE_BLE_SERVER
