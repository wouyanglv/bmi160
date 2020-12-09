/**
 * Copyright (c) 2012 - 2020, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/* Attention!
 * To maintain compliance with Nordic Semiconductor ASA's Bluetooth profile
 * qualification listings, this section of source code must not be modified.
 */
#include "sdk_common.h"
#if NRF_MODULE_ENABLED(BLE_HRS)
#include "ble_hrs_strain_gauge_v1.h".h"
#include <string.h>
#include "ble_srv_common.h"
#include <stdio.h>

#define OPCODE_LENGTH 1                                                              /**< Length of opcode inside Heart Rate Measurement packet. */
#define HANDLE_LENGTH 2                                                              /**< Length of handle inside Heart Rate Measurement packet. */
#define MAX_HRM_LEN      (NRF_SDH_BLE_GATT_MAX_MTU_SIZE - OPCODE_LENGTH - HANDLE_LENGTH) /**< Maximum size of a transmitted Heart Rate Measurement. */

uint16_t initial_value_hrm[200]        =              { 0  };                                  /**< Initial Heart Rate Measurement value. */

// Heart Rate Measurement flag bits
#define HRM_FLAG_MASK_HR_VALUE_16BIT            (0x01 << 0)                           /**< Heart Rate Value Format bit. */
#define HRM_FLAG_MASK_SENSOR_CONTACT_DETECTED   (0x01 << 1)                           /**< Sensor Contact Detected bit. */
#define HRM_FLAG_MASK_SENSOR_CONTACT_SUPPORTED  (0x01 << 2)                           /**< Sensor Contact Supported bit. */
#define HRM_FLAG_MASK_EXPENDED_ENERGY_INCLUDED  (0x01 << 3)                           /**< Energy Expended Status bit. Feature Not Supported */
#define HRM_FLAG_MASK_RR_INTERVAL_INCLUDED      (0x01 << 4)                           /**< RR-Interval bit. */


/**@brief Function for handling the Connect event.
 *
 * @param[in]   p_hrs       Heart Rate Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_connect(ble_hrs_t * p_hrs, ble_evt_t const * p_ble_evt)
{
    p_hrs->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}


/**@brief Function for handling the Disconnect event.
 *
 * @param[in]   p_hrs       Heart Rate Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_disconnect(ble_hrs_t * p_hrs, ble_evt_t const * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_hrs->conn_handle = BLE_CONN_HANDLE_INVALID;
}


/**@brief Function for handling write events to the Heart Rate Measurement characteristic.
 *
 * @param[in]   p_hrs         Heart Rate Service structure.
 * @param[in]   p_evt_write   Write event received from the BLE stack.
 */
static void on_hrm_cccd_write(ble_hrs_t * p_hrs, ble_gatts_evt_write_t const * p_evt_write)
{
    if (p_evt_write->len == 2)
    {
        // CCCD written, update notification state
        if (p_hrs->evt_handler != NULL)
        {
            ble_hrs_evt_t evt;

            if (ble_srv_is_notification_enabled(p_evt_write->data))
            {
                evt.evt_type = BLE_HRS_EVT_NOTIFICATION_ENABLED;
            }
            else
            {
                evt.evt_type = BLE_HRS_EVT_NOTIFICATION_DISABLED;
            }

            p_hrs->evt_handler(p_hrs, &evt);
        }
    }
}


/**@brief Function for handling the Write event.
 *
 * @param[in]   p_hrs       Heart Rate Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_write(ble_hrs_t * p_hrs, ble_evt_t const * p_ble_evt)
{
    ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if (p_evt_write->handle == p_hrs->hrm_handles.cccd_handle)
    {
        on_hrm_cccd_write(p_hrs, p_evt_write);
    }
}


void ble_hrs_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
    ble_hrs_t * p_hrs = (ble_hrs_t *) p_context;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_hrs, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_hrs, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_hrs, p_ble_evt);
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for encoding a Heart Rate Measurement.
 *
 * @param[in]   p_hrs              Heart Rate Service structure.
 * @param[in]   heart_rate         Measurement to be encoded.
 * @param[out]  p_encoded_buffer   Buffer where the encoded data will be written.
 *
 * @return      Size of encoded data.
 */
static uint8_t hrm_encode(ble_hrs_t * p_hrs, int16_t * p_buffer, uint8_t buffer_len, uint32_t tot_count, uint8_t timer_period, uint8_t * p_encoded_buffer)
{
    uint8_t len = 0;

//    int16_t test_dataset [32] ={0, 0,
//                                        1, -1,
//                                        5, -5,
//                                        10, -10,
//                                        16, -16,
//                                        50, -50,
//                                        100, -100,
//                                        256, -256,
//                                        500, -500,
//                                        1000, -1000,
//                                        1024, -1024,
//                                        5000, -5000,
//                                        8000, -8000,
//                                        10000, -10000,
//                                        30000, -30000,
//                                        32767, -32768
//                                        };

    memcpy(&(p_encoded_buffer[len]), &tot_count, sizeof(uint32_t));
    len += sizeof(uint32_t);
    memcpy(&(p_encoded_buffer[len]), &timer_period, sizeof(uint8_t));
    len += sizeof(uint8_t);
    memcpy(&(p_encoded_buffer[len]), &buffer_len, sizeof(uint8_t));
    len += sizeof(uint8_t);
    for(int i=0;i<buffer_len;i++)
    {
       memcpy(&(p_encoded_buffer[len]), &(p_buffer[i]), sizeof(uint16_t));
       len += sizeof(uint16_t);
    }
    printf(" data[48] = %d \r\n", p_buffer[30]);
    printf(" packet len: %d\r\n", len);

    return len;
}


uint32_t ble_hrs_init(ble_hrs_t * p_hrs, const ble_hrs_init_t * p_hrs_init)
{
    uint32_t              err_code;
    ble_uuid_t            ble_uuid;
    ble_add_char_params_t add_char_params;
    uint8_t               encoded_initial_hrm[MAX_HRM_LEN];

    // Initialize service structure
    p_hrs->evt_handler                 = p_hrs_init->evt_handler;
    p_hrs->is_sensor_contact_supported = p_hrs_init->is_sensor_contact_supported;
    p_hrs->conn_handle                 = BLE_CONN_HANDLE_INVALID;
    p_hrs->is_sensor_contact_detected  = false;
    p_hrs->rr_interval_count           = 0;
    p_hrs->max_hrm_len                 = MAX_HRM_LEN;

    // Add service
    BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_HEART_RATE_SERVICE);

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_hrs->service_handle);

    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add heart rate measurement characteristic
    memset(&add_char_params, 0, sizeof(add_char_params));

    add_char_params.uuid              = BLE_UUID_HEART_RATE_MEASUREMENT_CHAR;
    add_char_params.max_len           = MAX_HRM_LEN;
    add_char_params.init_len          = hrm_encode(p_hrs, initial_value_hrm, 200, 0, 5, encoded_initial_hrm);
    add_char_params.p_init_value      = encoded_initial_hrm;
    add_char_params.is_var_len        = true;
    add_char_params.char_props.notify = 1;
    add_char_params.cccd_write_access = p_hrs_init->hrm_cccd_wr_sec;

    err_code = characteristic_add(p_hrs->service_handle, &add_char_params, &(p_hrs->hrm_handles));
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    if (p_hrs_init->p_body_sensor_location != NULL)
    {
        // Add body sensor location characteristic
        memset(&add_char_params, 0, sizeof(add_char_params));

        add_char_params.uuid            = BLE_UUID_BODY_SENSOR_LOCATION_CHAR;
        add_char_params.max_len         = sizeof(uint8_t);
        add_char_params.init_len        = sizeof(uint8_t);
        add_char_params.p_init_value    = p_hrs_init->p_body_sensor_location;
        add_char_params.char_props.read = 1;
        add_char_params.read_access     = p_hrs_init->bsl_rd_sec;

        err_code = characteristic_add(p_hrs->service_handle, &add_char_params, &(p_hrs->bsl_handles));
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }

    return NRF_SUCCESS;
}


uint32_t ble_hrs_heart_rate_measurement_send(ble_hrs_t * p_hrs, uint16_t * p_buffer, uint8_t buffer_len, uint32_t tot_count, uint8_t timer_period)
{
    uint32_t err_code;

    // Send value if connected and notifying
    if (p_hrs->conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        uint8_t                encoded_hrm[MAX_HRM_LEN];
        uint16_t               len;
        uint16_t               hvx_len;
        ble_gatts_hvx_params_t hvx_params;

        len     = hrm_encode(p_hrs, p_buffer, buffer_len, tot_count, timer_period, encoded_hrm);
        hvx_len = len;

        memset(&hvx_params, 0, sizeof(hvx_params));

        hvx_params.handle = p_hrs->hrm_handles.value_handle;
        hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
        hvx_params.offset = 0;
        hvx_params.p_len  = &hvx_len;
        hvx_params.p_data = encoded_hrm;

        err_code = sd_ble_gatts_hvx(p_hrs->conn_handle, &hvx_params);
        if ((err_code == NRF_SUCCESS) && (hvx_len != len))
        {
            err_code = NRF_ERROR_DATA_SIZE;
        }
    }
    else
    {
        err_code = NRF_ERROR_INVALID_STATE;
    }

    return err_code;
}


void ble_hrs_rr_interval_add(ble_hrs_t * p_hrs, uint16_t rr_interval)
{
    if (p_hrs->rr_interval_count == BLE_HRS_MAX_BUFFERED_RR_INTERVALS)
    {
        // The rr_interval buffer is full, delete the oldest value
        memmove(&p_hrs->rr_interval[0],
                &p_hrs->rr_interval[1],
                (BLE_HRS_MAX_BUFFERED_RR_INTERVALS - 1) * sizeof(uint16_t));
        p_hrs->rr_interval_count--;
    }

    // Add new value
    p_hrs->rr_interval[p_hrs->rr_interval_count++] = rr_interval;
}


bool ble_hrs_rr_interval_buffer_is_full(ble_hrs_t * p_hrs)
{
    return (p_hrs->rr_interval_count == BLE_HRS_MAX_BUFFERED_RR_INTERVALS);
}


uint32_t ble_hrs_sensor_contact_supported_set(ble_hrs_t * p_hrs, bool is_sensor_contact_supported)
{
    // Check if we are connected to peer
    if (p_hrs->conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        p_hrs->is_sensor_contact_supported = is_sensor_contact_supported;
        return NRF_SUCCESS;
    }
    else
    {
        return NRF_ERROR_INVALID_STATE;
    }
}


void ble_hrs_sensor_contact_detected_update(ble_hrs_t * p_hrs, bool is_sensor_contact_detected)
{
    p_hrs->is_sensor_contact_detected = is_sensor_contact_detected;
}


uint32_t ble_hrs_body_sensor_location_set(ble_hrs_t * p_hrs, uint8_t body_sensor_location)
{
    ble_gatts_value_t gatts_value;

    // Initialize value struct.
    memset(&gatts_value, 0, sizeof(gatts_value));

    gatts_value.len     = sizeof(uint8_t);
    gatts_value.offset  = 0;
    gatts_value.p_value = &body_sensor_location;

    return sd_ble_gatts_value_set(p_hrs->conn_handle, p_hrs->bsl_handles.value_handle, &gatts_value);
}


void ble_hrs_on_gatt_evt(ble_hrs_t * p_hrs, nrf_ble_gatt_evt_t const * p_gatt_evt)
{
    if (    (p_hrs->conn_handle == p_gatt_evt->conn_handle)
        &&  (p_gatt_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        p_hrs->max_hrm_len = p_gatt_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
    }
}
#endif // NRF_MODULE_ENABLED(BLE_HRS)
