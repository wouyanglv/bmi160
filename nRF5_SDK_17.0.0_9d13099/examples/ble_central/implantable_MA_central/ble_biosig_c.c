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
/**@cond To Make Doxygen skip documentation generation for this file.
 * @{
 */
#include "sdk_common.h"
#if NRF_MODULE_ENABLED(BLE_BIOSIG_SVC_C)
#include "ble_biosig_c.h"
#include "ble_db_discovery.h"
#include "ble_types.h"
#include "ble_gattc.h"

#define NRF_LOG_MODULE_NAME ble_biosig_svc_c
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();


/**@brief Function for interception of the errors of GATTC and the BLE GATT Queue.
 *
 * @param[in] nrf_error   Error code.
 * @param[in] p_ctx       Parameter from the event handler.
 * @param[in] conn_handle Connection handle.
 */
static void gatt_error_handler(uint32_t   nrf_error,
                               void     * p_ctx,
                               uint16_t   conn_handle)
{
    ble_biosig_svc_c_t * p_ble_biosig_svc_c = (ble_biosig_svc_c_t *)p_ctx;

    NRF_LOG_DEBUG("A GATT Client error has occurred on conn_handle: 0X%X", conn_handle);

    if (p_ble_biosig_svc_c->error_handler != NULL)
    {
        p_ble_biosig_svc_c->error_handler(nrf_error);
    }
}


/**@brief     Function for handling Handle Value Notification received from the SoftDevice.
 *
 * @details   This function uses the Handle Value Notification received from the SoftDevice
 *            and checks whether it is a notification of the Bio Signal measurement from the peer. If
 *            it is, this function decodes the Bio Signal measurement and sends it to the
 *            application.
 *
 * @param[in] p_ble_biosig_svc_c Pointer to the Bio Signal Client structure.
 * @param[in] p_ble_evt   Pointer to the BLE event received.
 */
static void on_hvx(ble_biosig_svc_c_t * p_ble_biosig_svc_c, const ble_evt_t * p_ble_evt)
{
    // Check if the event is on the link for this instance.
    if (p_ble_biosig_svc_c->conn_handle != p_ble_evt->evt.gattc_evt.conn_handle)
    {
        NRF_LOG_DEBUG("Received HVX on link 0x%x, not associated to this instance. Ignore.",
                      p_ble_evt->evt.gattc_evt.conn_handle);
        
        return;
    }

    NRF_LOG_DEBUG("Received HVX on link 0x%x, biosig_meas_handle 0x%x",
    p_ble_evt->evt.gattc_evt.params.hvx.handle,
    p_ble_biosig_svc_c->peer_biosig_svc_db.biosig_meas_handle);
   

    // Check if this is a Bio Signal notification.
    if (p_ble_evt->evt.gattc_evt.params.hvx.handle == p_ble_biosig_svc_c->peer_biosig_svc_db.biosig_meas_handle)
    {
        ble_biosig_svc_c_evt_t ble_biosig_svc_c_evt;
        uint32_t        index = 0;

        ble_biosig_svc_c_evt.evt_type                    = BLE_BIOSIG_SVC_C_EVT_BIOSIG_MEAS_NOTIFICATION;
        ble_biosig_svc_c_evt.conn_handle                 = p_ble_biosig_svc_c->conn_handle;
        //ble_biosig_svc_c_evt.params.biosig_meas.rr_intervals_cnt = 0;
        
        memcpy(ble_biosig_svc_c_evt.params.biosig_meas.hr_value, p_ble_evt->evt.gattc_evt.params.hvx.data, sizeof(p_ble_evt->evt.gattc_evt.params.hvx.data));
        
       /* if (!(p_ble_evt->evt.gattc_evt.params.hvx.data[index++] & BIOSIG_MEAS_FLAG_MASK_HR_16BIT))
        {
            // 8-bit Bio Signal value received.
            ble_biosig_svc_c_evt.params.biosig_meas.hr_value = p_ble_evt->evt.gattc_evt.params.hvx.data[index++];  //lint !e415 suppress Lint Warning 415: Likely access out of bond
        }
        else
        {
            // 16-bit Bio Signal value received.
            ble_biosig_svc_c_evt.params.biosig_meas.hr_value =
                uint16_decode(&(p_ble_evt->evt.gattc_evt.params.hvx.data[index]));
            index += sizeof(uint16_t);
        }

        if ((p_ble_evt->evt.gattc_evt.params.hvx.data[0] & BIOSIG_MEAS_FLAG_MASK_HR_RR_INT))
        {
            uint32_t i;
            
            for (i = 0; i < BLE_BIOSIG_SVC_C_RR_INTERVALS_MAX_CNT; i ++)
            {
                if (index >= p_ble_evt->evt.gattc_evt.params.hvx.len)
                {
                    break;
                }
                ble_biosig_svc_c_evt.params.biosig_meas.rr_intervals[i] =
                    uint16_decode(&(p_ble_evt->evt.gattc_evt.params.hvx.data[index]));
                index += sizeof(uint16_t);
            }
           
            ble_biosig_svc_c_evt.params.biosig_meas.rr_intervals_cnt = (uint8_t)i;
        }*/
        p_ble_biosig_svc_c->evt_handler(p_ble_biosig_svc_c, &ble_biosig_svc_c_evt);
    }
}


/**@brief     Function for handling Disconnected event received from the SoftDevice.
 *
 * @details   This function checks whether the disconnect event is happening on the link
 *            associated with the current instance of the module. If the event is happening, the function sets the instance's
 *            conn_handle to invalid.
 *
 * @param[in] p_ble_biosig_svc_c Pointer to the Bio Signal Client structure.
 * @param[in] p_ble_evt   Pointer to the BLE event received.
 */
static void on_disconnected(ble_biosig_svc_c_t * p_ble_biosig_svc_c, const ble_evt_t * p_ble_evt)
{
    if (p_ble_biosig_svc_c->conn_handle == p_ble_evt->evt.gap_evt.conn_handle)
    {
        p_ble_biosig_svc_c->conn_handle                 = BLE_CONN_HANDLE_INVALID;
        p_ble_biosig_svc_c->peer_biosig_svc_db.biosig_meas_cccd_handle = BLE_GATT_HANDLE_INVALID;
        p_ble_biosig_svc_c->peer_biosig_svc_db.biosig_meas_handle      = BLE_GATT_HANDLE_INVALID;
    }
}


void ble_biosig_svc_on_db_disc_evt(ble_biosig_svc_c_t * p_ble_biosig_svc_c, const ble_db_discovery_evt_t * p_evt)
{
    // Check if the Bio Signal Service was discovered.
    if (p_evt->evt_type == BLE_DB_DISCOVERY_COMPLETE &&
        p_evt->params.discovered_db.srv_uuid.uuid == BLE_UUID_BIOSIG_SERVICE &&
        p_evt->params.discovered_db.srv_uuid.type == BLE_UUID_TYPE_VENDOR_BEGIN)
    {
        // Find the CCCD Handle of the Bio Signal Measurement characteristic.
        uint32_t i;

        ble_biosig_svc_c_evt_t evt;

        evt.evt_type    = BLE_BIOSIG_SVC_C_EVT_DISCOVERY_COMPLETE;
        evt.conn_handle = p_evt->conn_handle;

        for (i = 0; i < p_evt->params.discovered_db.char_count; i++)
        {
            if (p_evt->params.discovered_db.charateristics[i].characteristic.uuid.uuid ==
                BLE_UUID_BIOSIG_MEASUREMENT_CHAR)
            {
                // Found Bio Signal characteristic. Store CCCD handle and break.
                evt.params.peer_db.biosig_meas_cccd_handle =
                    p_evt->params.discovered_db.charateristics[i].cccd_handle;
                evt.params.peer_db.biosig_meas_handle =
                    p_evt->params.discovered_db.charateristics[i].characteristic.handle_value;
                break;
            }
        }

        
        //If the instance has been assigned prior to db_discovery, assign the db_handles.
        if (p_ble_biosig_svc_c->conn_handle != BLE_CONN_HANDLE_INVALID)
        {
            if ((p_ble_biosig_svc_c->peer_biosig_svc_db.biosig_meas_cccd_handle == BLE_GATT_HANDLE_INVALID)&&
                (p_ble_biosig_svc_c->peer_biosig_svc_db.biosig_meas_handle == BLE_GATT_HANDLE_INVALID))
            {
                p_ble_biosig_svc_c->peer_biosig_svc_db = evt.params.peer_db;
            }
        }


        p_ble_biosig_svc_c->evt_handler(p_ble_biosig_svc_c, &evt);
    }
}


uint32_t ble_biosig_svc_c_init(ble_biosig_svc_c_t * p_ble_biosig_svc_c, ble_biosig_svc_c_init_t * p_ble_biosig_svc_c_init)
{
    VERIFY_PARAM_NOT_NULL(p_ble_biosig_svc_c);
    VERIFY_PARAM_NOT_NULL(p_ble_biosig_svc_c_init);

    uint32_t      err_code;
    ble_uuid_t biosig_svc_uuid;
    ble_uuid128_t ma_base_uuid = BIOSIG_BASE_UUID;
    err_code = sd_ble_uuid_vs_add(&ma_base_uuid, &p_ble_biosig_svc_c->uuid_type);

    biosig_svc_uuid.type = p_ble_biosig_svc_c->uuid_type;
    biosig_svc_uuid.uuid = BLE_UUID_BIOSIG_SERVICE;

    p_ble_biosig_svc_c->evt_handler                 = p_ble_biosig_svc_c_init->evt_handler;
    p_ble_biosig_svc_c->error_handler               = p_ble_biosig_svc_c_init->error_handler;
    p_ble_biosig_svc_c->p_gatt_queue                = p_ble_biosig_svc_c_init->p_gatt_queue;
    p_ble_biosig_svc_c->conn_handle                 = BLE_CONN_HANDLE_INVALID;
    p_ble_biosig_svc_c->peer_biosig_svc_db.biosig_meas_cccd_handle = BLE_GATT_HANDLE_INVALID;
    p_ble_biosig_svc_c->peer_biosig_svc_db.biosig_meas_handle      = BLE_GATT_HANDLE_INVALID;

    return ble_db_discovery_evt_register(&biosig_svc_uuid);
}

void ble_biosig_svc_c_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
    ble_biosig_svc_c_t * p_ble_biosig_svc_c = (ble_biosig_svc_c_t *)p_context;

    if ((p_ble_biosig_svc_c == NULL) || (p_ble_evt == NULL))
    {
        return;
    }

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GATTC_EVT_HVX:
            on_hvx(p_ble_biosig_svc_c, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnected(p_ble_biosig_svc_c, p_ble_evt);
            break;

        default:
            break;
    }
}


/**@brief Function for creating a message for writing to the CCCD.
 */
static uint32_t cccd_configure(ble_biosig_svc_c_t * p_ble_biosig_svc_c, bool enable)
{
    NRF_LOG_DEBUG("Configuring CCCD. CCCD Handle = %d, Connection Handle = %d",
                  p_ble_biosig_svc_c->peer_biosig_svc_db.biosig_meas_cccd_handle,
                  p_ble_biosig_svc_c->conn_handle);

    nrf_ble_gq_req_t biosig_svc_c_req;
    uint8_t          cccd[BLE_CCCD_VALUE_LEN];
    uint16_t         cccd_val = enable ? BLE_GATT_HVX_NOTIFICATION : 0;

    cccd[0] = LSB_16(cccd_val);
    cccd[1] = MSB_16(cccd_val);

    memset(&biosig_svc_c_req, 0, sizeof(biosig_svc_c_req));

    biosig_svc_c_req.type                        = NRF_BLE_GQ_REQ_GATTC_WRITE;
    biosig_svc_c_req.error_handler.cb            = gatt_error_handler;
    biosig_svc_c_req.error_handler.p_ctx         = p_ble_biosig_svc_c;
    biosig_svc_c_req.params.gattc_write.handle   = p_ble_biosig_svc_c->peer_biosig_svc_db.biosig_meas_cccd_handle;
    biosig_svc_c_req.params.gattc_write.len      = BLE_CCCD_VALUE_LEN;
    biosig_svc_c_req.params.gattc_write.p_value  = cccd;
    biosig_svc_c_req.params.gattc_write.write_op = BLE_GATT_OP_WRITE_REQ;

    return nrf_ble_gq_item_add(p_ble_biosig_svc_c->p_gatt_queue, &biosig_svc_c_req, p_ble_biosig_svc_c->conn_handle);
}


uint32_t ble_biosig_svc_c_biosig_meas_notif_enable(ble_biosig_svc_c_t * p_ble_biosig_svc_c)
{
    VERIFY_PARAM_NOT_NULL(p_ble_biosig_svc_c);

    return cccd_configure(p_ble_biosig_svc_c, true);
}


uint32_t ble_biosig_svc_c_handles_assign(ble_biosig_svc_c_t    * p_ble_biosig_svc_c,
                                  uint16_t         conn_handle,
                                  const biosig_svc_db_t * p_peer_biosig_svc_handles)
{
    VERIFY_PARAM_NOT_NULL(p_ble_biosig_svc_c);

    p_ble_biosig_svc_c->conn_handle = conn_handle;
    if (p_peer_biosig_svc_handles != NULL)
    {
        p_ble_biosig_svc_c->peer_biosig_svc_db = *p_peer_biosig_svc_handles;
    }

    return nrf_ble_gq_conn_handle_register(p_ble_biosig_svc_c->p_gatt_queue, conn_handle);
}
/** @}
 *  @endcond
 */
#endif // NRF_MODULE_ENABLED(BLE_BIOSIG_SVC_C)
