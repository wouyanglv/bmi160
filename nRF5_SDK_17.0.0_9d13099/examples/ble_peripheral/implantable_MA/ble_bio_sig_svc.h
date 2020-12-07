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
/** @file
 *
 * @defgroup ble_hrs Heart Rate Service
 * @{
 * @ingroup ble_sdk_srv
 * @brief Heart Rate Service module.
 *
 * @details This module implements the Heart Rate Service with the Heart Rate Measurement,
 *          Body Sensor Location and Heart Rate Control Point characteristics.
 *          During initialization it adds the Heart Rate Service and Heart Rate Measurement
 *          characteristic to the BLE stack database. Optionally it also adds the
 *          Body Sensor Location and Heart Rate Control Point characteristics.
 *
 *          If enabled, notification of the Heart Rate Measurement characteristic is performed
 *          when the application calls ble_hrs_heart_rate_measurement_send().
 *
 *          The Heart Rate Service also provides a set of functions for manipulating the
 *          various fields in the Heart Rate Measurement characteristic, as well as setting
 *          the Body Sensor Location characteristic value.
 *
 *          If an event handler is supplied by the application, the Heart Rate Service will
 *          generate Heart Rate Service events to the application.
 *
 * @note    The application must register this module as BLE event observer using the
 *          NRF_SDH_BLE_OBSERVER macro. Example:
 *          @code
 *              ble_hrs_t instance;
 *              NRF_SDH_BLE_OBSERVER(anything, BLE_HRS_BLE_OBSERVER_PRIO,
 *                                   ble_hrs_on_ble_evt, &instance);
 *          @endcode
 *
 * @note Attention!
 *  To maintain compliance with Nordic Semiconductor ASA Bluetooth profile
 *  qualification listings, this section of source code must not be modified.
 */

#ifndef BLE_BIO_SIG_SVC_H__
#define BLE_BIO_SIG_SVC_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "bmi160.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MA_BASE_UUID  {{0x9b, 0x05, 0x00, 0x00, 0x6f, 0x92, 0x40, 0xda, 0xbf, 0xfe, 0xb0, 0x80, 0x9c, 0x5d, 0xd2, 0xa2}}
#define BLE_UUID_MA_SERVICE  0x0A00 
#define BLE_UUID_MA_MEASUREMENT_CHAR 0x0B00


/**@brief   Macro for defining a ble_biosig_svc instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_BIOSIG_SVC_DEF(_name)                                                                   \
static ble_biosig_t _name;                                                                          \
NRF_SDH_BLE_OBSERVER(_name ## _obs,                                                                 \
                     BLE_BIOSIG_SVC_BLE_OBSERVER_PRIO,                                              \
                     ble_biosig_svc_on_ble_evt, &_name)


typedef enum
{
    BLE_BIOSIG_DEV_TYPE_EEG,
    BLE_BIOSIG_DEV_TYPE_EMG
} ble_biosig_svc_dev_type;

/**@brief Heart Rate Service event type. */
typedef enum
{
    BLE_HRS_EVT_NOTIFICATION_ENABLED,   /**< Heart Rate value notification enabled event. */
    BLE_HRS_EVT_NOTIFICATION_DISABLED   /**< Heart Rate value notification disabled event. */
} ble_biosig_svc_evt_type_t;

/**@brief Heart Rate Service event. */
typedef struct
{
    ble_biosig_svc_evt_type_t   evt_type;    /**< Type of event. */
} ble_biosig_svc_evt_t;

// Forward declaration of the ble_biosig_t type.
typedef struct ble_biosig_s   ble_biosig_t;

/**@brief BioSignal Service event handler type. */
typedef void (*ble_biosig_svc_evt_handler_t) (ble_biosig_t *p_biosig_svc, ble_biosig_svc_evt_t *p_evt);

/**@brief BioSignal Service init structure. This contains all options and data needed for
 *        initialization of the service. */
typedef struct
{
    ble_biosig_svc_evt_handler_t  evt_handler;                                          /**< Event handler to be called for handling events in the BioSignal Service. */
    security_req_t                cccd_wr_sec;                                          /**< Security requirement for writing the BioSignal characteristic CCCD. */
    security_req_t                rd_sec;                                               /**< Security requirement for reading the BioSignal characteristic value. */
} ble_biosig_svc_init_t;

/**@brief BioSignal Service structure. This contains various status information for the service. */
struct ble_biosig_s
{
    uint8_t                         uuid_type;  
    ble_biosig_svc_evt_handler_t evt_handler;                                          /**< Event handler to be called for handling events in the Heart Rate Service. */
    uint16_t                     service_handle;                                       /**< Handle of BioSignal Service (as provided by the BLE stack). */
    ble_gatts_char_handles_t     biosig_handles;                                       /**< Handles related to the BioSignal Measurement characteristic. */
    uint16_t                     conn_handle;                                          /**< Handle of the current connection (as provided by the BLE stack, is BLE_CONN_HANDLE_INVALID if not in a connection). */
    uint8_t                      max_biosig_len;                                       /**< Current maximum BioSignal measurement length, adjusted according to the current ATT MTU. */
};


/**@brief Function for initializing the Heart Rate Service.
 *
 * @param[out]  p_biosig_svc     BioSignal Service structure. This structure will have to be supplied by
 *                               the application. It will be initialized by this function, and will later
 *                               be used to identify this particular service instance.
 * @param[in]   p_biosig_init    Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on successful initialization of service, otherwise an error code.
 */
uint32_t ble_biosig_svc_init(ble_biosig_t *p_biosig_svc, ble_biosig_svc_init_t const *p_biosig_init);


/**@brief Function for handling the GATT module's events.
 *
 * @details Handles all events from the GATT module of interest to the BioSignal Service.
 *
 * @param[in]   p_biosig_svc      BioSignal Service structure.
 * @param[in]   p_gatt_evt        Event received from the GATT module.
 */
void ble_biosig_svc_on_gatt_evt(ble_biosig_t *p_biosig_svc, nrf_ble_gatt_evt_t const *p_gatt_evt);


/**@brief Function for handling the Application's BLE Stack events.
 *
 * @details Handles all events from the BLE stack of interest to the Heart Rate Service.
 *
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 * @param[in]   p_context   Heart Rate Service structure.
 */
void ble_biosig_svc_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);


/**@brief Function for sending BioSignal measurement if notification has been enabled.
 *
 * @details The application calls this function after having performed a BioSignal measurement.
 *          If notification has been enabled, the BioSignal measurement data is encoded and sent to
 *          the client.
 *
 * @param[in]   p_biosig_svc             BioSignal Service structure.
 * @param[in]   timestamp                Timestamp of the measurement.
 * @param[in]   measurement              New bio signal measurement.
 * @param[in]   dev_type                 Identifier for the type of bio signal measurement device.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
uint32_t ble_biosig_svc_measurement_send(ble_biosig_t *p_biosig_svc, uint64_t timestamp, uint8_t frame_num, 
                                          struct bmi160_sensor_data * accel_data);

#ifdef __cplusplus
}
#endif

#endif // BLE_BIO_SIG_SVC_H__

/** @} */




