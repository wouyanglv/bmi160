/**
 * Copyright (c) 2012 - 2019, Nordic Semiconductor ASA
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
#include "ble_bio_sig_svc.h"
#include <string.h>
#include "ble_srv_common.h"
#include "max30003.h"
#include "bmi160.h"

#define OPCODE_LENGTH 1                                                              /**< Length of opcode inside BioSignal Measurement packet. */
#define HANDLE_LENGTH 2                                                              /**< Length of handle inside BioSignal Measurement packet. */
#define MAX_BIOSIG_LEN   (NRF_SDH_BLE_GATT_MAX_MTU_SIZE - OPCODE_LENGTH - HANDLE_LENGTH) /**< Maximum size of a transmitted BioSignal Measurement. */


#define INITIAL_TIMESTAMP_BIOSIG                   0                                    /**< Initial BioSignal timestamp value. */

struct bmi160_sensor_data initial_value_biosig[38] = {0};
static bool m_wait_ble_tx;
static uint32_t flush = 0;

/**@brief Function for handling the Connect event.
 *
 * @param[in]   p_biosig_svc   BioSignal Service structure.
 * @param[in]   p_ble_evt      Event received from the BLE stack.
 */
static void on_connect(ble_biosig_t *p_biosig_svc, ble_evt_t const * p_ble_evt)
{
    p_biosig_svc->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}


/**@brief Function for handling the Disconnect event.
 *
 * @param[in]   p_biosig_svc   BioSignal Service structure.
 * @param[in]   p_ble_evt      Event received from the BLE stack.
 */
static void on_disconnect(ble_biosig_t *p_biosig, ble_evt_t const *p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_biosig->conn_handle = BLE_CONN_HANDLE_INVALID;
}


void ble_biosig_svc_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
    ble_biosig_t *p_biosig_svc = (ble_biosig_t *) p_context;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_biosig_svc, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_biosig_svc, p_ble_evt);
            break;

        case BLE_GATTS_EVT_HVN_TX_COMPLETE:
            m_wait_ble_tx = false;
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for encoding a BioSignal Measurement.
 *
 * @param[in]   p_biosig_svc     BioSignal Service structure.
 * @param[in]   timestamp                Timestamp of the measurement.
 * @param[in]   measurement              New bio signal measurement.
 * @param[in]   dev_type                 Identifier for the type of bio signal measurement device.
 * @param[out]  p_encoded_buffer Buffer where the encoded data will be written.
 *
 * @return      Size of encoded data.
 */

// Did not encode sampling rate, because it is not stable, setting 1600 Hz actually gets about ~1800 Hz.
static uint8_t biosignal_encode(ble_biosig_t *p_biosig_svc, uint64_t timestamp, uint8_t frame_num, struct bmi160_sensor_data * accel_data, int8_t *p_encoded_buffer)
{
    uint8_t len = 0;
    
    memcpy(&(p_encoded_buffer[len]), &timestamp, sizeof(uint64_t));
    len += sizeof(uint64_t);
    memcpy(&(p_encoded_buffer[len]), &frame_num, sizeof(uint8_t));
    
    len += sizeof(uint8_t);
    
    for(int i=0;i<frame_num;i++)
    {
       memcpy(&(p_encoded_buffer[len]), &(accel_data[i]), 3*sizeof(int16_t));
       
       len += 3*sizeof(int16_t);
       
    }

    return len;
}


uint32_t ble_biosig_svc_init(ble_biosig_t *p_biosig_svc, ble_biosig_svc_init_t const *p_biosig_init)
{
    uint32_t              err_code;
    ble_uuid_t            ble_uuid;
    ble_add_char_params_t add_char_params;
    uint8_t               encoded_initial_biosig[MAX_BIOSIG_LEN];

    // Initialize service structure
    p_biosig_svc->evt_handler                 = p_biosig_init->evt_handler;
    p_biosig_svc->conn_handle                 = BLE_CONN_HANDLE_INVALID;
    p_biosig_svc->max_biosig_len              = MAX_BIOSIG_LEN;

    // Add service
    BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_HEART_RATE_SERVICE);

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_biosig_svc->service_handle);

    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add bio signal measurement characteristic
    memset(&add_char_params, 0, sizeof(add_char_params));

    add_char_params.uuid              = BLE_UUID_HEART_RATE_MEASUREMENT_CHAR;
    add_char_params.max_len           = MAX_BIOSIG_LEN;
    add_char_params.init_len          = biosignal_encode(p_biosig_svc, INITIAL_TIMESTAMP_BIOSIG, 38, initial_value_biosig, encoded_initial_biosig);
    add_char_params.p_init_value      = encoded_initial_biosig;
    add_char_params.is_var_len        = true;
    add_char_params.char_props.notify = 1;
    add_char_params.cccd_write_access = p_biosig_init->cccd_wr_sec;
    add_char_params.read_access       = p_biosig_init->rd_sec;

    err_code = characteristic_add(p_biosig_svc->service_handle, &add_char_params, &(p_biosig_svc->biosig_handles));
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    return NRF_SUCCESS;
}


uint32_t ble_biosig_svc_measurement_send(ble_biosig_t *p_biosig_svc, uint64_t timestamp, uint8_t frame_num, 
                                          struct bmi160_sensor_data * accel_data)
{
    uint32_t err_code;

    // Send value if connected and notifying
    if (p_biosig_svc->conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        int8_t                encoded_biosig[MAX_BIOSIG_LEN];
        uint16_t               len;
        uint16_t               hvx_len;
        ble_gatts_hvx_params_t hvx_params;

        len     = biosignal_encode(p_biosig_svc, timestamp, frame_num, accel_data, encoded_biosig);
        hvx_len = len;
      //  SEGGER_RTT_printf(0, "biosignal len: %d bytes \r\n", len);
        memset(&hvx_params, 0, sizeof(hvx_params));

        // https://devzone.nordicsemi.com/f/nordic-q-a/5646/how-to-enable-indication-for-a-characteristic
        hvx_params.handle = p_biosig_svc->biosig_handles.value_handle;
        hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
        hvx_params.offset = 0;
        hvx_params.p_len  = &hvx_len;
        hvx_params.p_data = encoded_biosig;


        err_code = sd_ble_gatts_hvx(p_biosig_svc->conn_handle, &hvx_params);
       // SEGGER_RTT_printf(0,"err: %d\n", err_code);
       // if (err_code == NRF_SUCCESS) {SEGGER_RTT_printf(0, "NRF SUCCESS - Error code: %d \r\n", err_code);}
        if ((err_code == NRF_SUCCESS) && (hvx_len != len))
        {
            err_code = NRF_ERROR_DATA_SIZE;
        }
        if (err_code == NRF_ERROR_RESOURCES)
        {
            m_wait_ble_tx = true;
            while (m_wait_ble_tx)
            {
                nrf_pwr_mgmt_run();
            }
            err_code = sd_ble_gatts_hvx(p_biosig_svc->conn_handle, &hvx_params);
            return 1000; //means needing to flush fifo in order to restart watermark interrupt
        }
    }
    else
    {
        err_code = NRF_ERROR_INVALID_STATE;
    }

    return err_code;
}


void ble_biosig_svc_on_gatt_evt(ble_biosig_t *p_biosig_svc, nrf_ble_gatt_evt_t const *p_gatt_evt)
{
    if ( (p_biosig_svc->conn_handle == p_gatt_evt->conn_handle)
        &&  (p_gatt_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        p_biosig_svc->max_biosig_len = p_gatt_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
    }
}
#endif // NRF_MODULE_ENABLED(BLE_HRS)
