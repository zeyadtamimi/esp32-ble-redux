/**
 * @file   ble_characteristic.cpp
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

#include <cstdint>
#include <utility>

#include "esp_log.h"
#include "utilities.hpp"

#include "ble_characteristic.hpp"
#include "ble_profile.hpp"
#include "ble_service.hpp"
#include "ble_server.hpp"
#include "ble_utilities.hpp"

namespace BLE
{

constexpr const char* LOG_TAG_BLE_CHARACTERISTIC = "BLE Characteristic";

/***************************************************************************************************
* Logging Macros
***************************************************************************************************/
#define CHARACTERISTIC_LOG(LVL, MSG, ...)\
    INTANCE_LOG(LVL, LOG_TAG_BLE_CHARACTERISTIC, "%04X --", this->handle, MSG, ##__VA_ARGS__)

#define CHARACTERISTIC_LOGE(MSG, ...) CHARACTERISTIC_LOG(ESP_LOG_ERROR, MSG, ##__VA_ARGS__)
#define CHARACTERISTIC_LOGW(MSG, ...) CHARACTERISTIC_LOG(ESP_LOG_WARN, MSG, ##__VA_ARGS__)
#define CHARACTERISTIC_LOGI(MSG, ...) CHARACTERISTIC_LOG(ESP_LOG_INFO, MSG, ##__VA_ARGS__)
#define CHARACTERISTIC_LOGD(MSG, ...) CHARACTERISTIC_LOG(ESP_LOG_DEBUG, MSG, ##__VA_ARGS__)
#define CHARACTERISTIC_LOGV(MSG, ...) CHARACTERISTIC_LOG(ESP_LOG_VERBOSE, MSG, ##__VA_ARGS__)


/***************************************************************************************************
* Characteristic Member Functions
***************************************************************************************************/

/**
 * @brief Sets a callback function to be executed whenever a write operation is completed.
 * @param callback A callback function of type RW_Callback that will be executed;
 * @note This function will be executed before the response to the device is sent.
 */
void
BLE_Characteristic::callback_write_set(BLE_Characteristic::RW_Callback callback)
{
    m_callback_write = callback;
}


/**
 * @brief Sets a callback function to be executed whenever a read operation is completed.
 * @param callback A callback function of type RW_Callback that will be executed;
 * @note This function will be executed before the response to the device is sent.
 */
void
BLE_Characteristic::callback_read_set(BLE_Characteristic::RW_Callback callback)
{
    m_callback_read = callback;
}


/***************************************************************************************************
* Event Handling
***************************************************************************************************/
inline
void
BLE_Characteristic::handle_request_write(const esp_ble_gatts_cb_param_t::gatts_write_evt_param& param)
{
    // TODO Check BDA and Conn ID
    // TODO Error checking on transactions

    if (param.handle != handle)
        return;

    CHARACTERISTIC_LOGD("Write ID from: %04X, transaction: %d%s",
                        param.conn_id,
                        param.trans_id,
                        param.is_prep ? " (prep)" : "");

    ESP_LOG_BUFFER_HEXDUMP(LOG_TAG_BLE_CHARACTERISTIC, param.value, param.len, ESP_LOG_DEBUG);

    // Only start a transaction if this is not a preparation request or if we got the first chunk
    // of a preparation request.
    if (!param.is_prep || param.offset == 0)
        m_value.transaction_write_start(param.conn_id);

    m_value.transaction_write_add(param.conn_id,
                            std::vector<uint8_t>(param.value, param.value + param.len));

    // Preparation transactions go through the ESP_GATTS_EXEC_WRITE_EVT to commit.
    if (!param.is_prep)
    {
        m_value.transaction_write_commit(param.conn_id);
        if (m_callback_write)
            m_callback_write();
    }

    if (param.need_rsp)
    {
        esp_gatt_rsp_t response;
        response.attr_value.len = param.len;
        response.attr_value.handle = handle;
        response.attr_value.offset = param.offset;
        response.attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
        memcpy(response.attr_value.value, param.value, param.len);

        esp_err_t err = esp_ble_gatts_send_response(gatts_if, param.conn_id, param.trans_id,
                                                    ESP_GATT_OK, &response);
        if (err)
            CHARACTERISTIC_LOGE("Write response failed: %s (%d)", esp_err_to_name(err), err);
    }
}


inline
void
BLE_Characteristic::handle_request_exec_write(const esp_ble_gatts_cb_param_t::gatts_exec_write_evt_param& param)
{
    // TODO Check BDA and Conn ID
    // TODO Error checking on transactions
    CHARACTERISTIC_LOGI("GATT_EXEC_WRITE_EVT, conn_id %d, trans_id %d\n",
                        param.conn_id,
                        param.trans_id);
    m_value.transaction_write_commit(param.conn_id);

    if (m_callback_write)
        m_callback_write();

    esp_err_t err = esp_ble_gatts_send_response(gatts_if, param.conn_id, param.trans_id,
                                                ESP_GATT_OK, nullptr);
    if (err)
        CHARACTERISTIC_LOGE("Write exec response failed: %s (%d)", esp_err_to_name(err), err);
}


inline
void
BLE_Characteristic::handle_request_read(const esp_ble_gatts_cb_param_t::gatts_read_evt_param& param)
{
    if (param.handle != handle)
        return;

    CHARACTERISTIC_LOGD("Read ID from: %04X, transaction: %d offt:%d rsp:%d%s",
                        param.conn_id,
                        param.trans_id,
                        param.offset,
                        param.need_rsp,
                        param.is_long ? " (long)" : "");

    if (!param.need_rsp)
        return;


    // As mentioned in a post the is_long will inform us if this is an existing transaction
    if (!param.is_long)
         m_value.transaction_read_start(param.conn_id);

    // TODO Error checking
    auto info = service.lock()->profile.lock()->server.lock()->connection_get(param.conn_id);
    auto max_size = info->mtu - ATT_FIELD_LENGTH_OPCODE;

    std::vector<uint8_t> data = m_value.transaction_read_advance(param.conn_id, max_size);
    if (data.size() < max_size)
    {
        m_value.transaction_read_abort(param.conn_id);
        if (m_callback_read)
            m_callback_read();
    }

    esp_gatt_rsp_t response;
    response.attr_value.len = data.size();
    std::copy(data.begin(), data.end(), response.attr_value.value);
    esp_ble_gatts_send_response(gatts_if,
            param.conn_id,
            param.trans_id,
            ESP_GATT_OK, &response);
}


void
BLE_Characteristic::characteristic_event_handler_gatts(esp_gatts_cb_event_t event,
                                                       esp_gatt_if_t gatts_if,
                                                       esp_ble_gatts_cb_param_t *param)
{
    CHARACTERISTIC_LOGV("GATTS event = %d, inf = 0x%04X", event, gatts_if);
    if (gatts_if != this->gatts_if)
    {
        CHARACTERISTIC_LOGE("Invalid inf received: %x %x", this->gatts_if, gatts_if);
        return;
    }

    switch (event)
    {
        case ESP_GATTS_READ_EVT:
            if (param->read.handle == handle)
                handle_request_read(param->read);
        break;
        case ESP_GATTS_WRITE_EVT:
            if (param->write.handle == handle)
                handle_request_write(param->write);
        break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            if (m_value.transaction_write_ongoing(param->exec_write.conn_id))
                handle_request_exec_write(param->exec_write);
        break;
        case ESP_GATTS_DISCONNECT_EVT:
            m_value.transaction_read_abort(param->disconnect.conn_id);
            m_value.transaction_write_abort(param->disconnect.conn_id);
        break;
        default:
        break;
    }
}

};

