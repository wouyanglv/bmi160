/** @i2c.c
 * 
 * @brief A description of the module’s purpose. 
 *
 * @par       
 */

#include <string.h>
#include "nrf.h"
#include "nrf_drv_twi.h"
#include "i2c.h"
#include "SEGGER_RTT.h"


/* TWI instance ID. */
/* TWI instance ID. */
#if TWI0_ENABLED
#define TWI_INSTANCE_ID     0
#elif TWI1_ENABLED
#define TWI_INSTANCE_ID     1
#endif

#define TX_BUF_SIZE         512


/* local static variables */
static const nrf_drv_twi_t  m_twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);
static uint8_t              m_tx_buff[TX_BUF_SIZE];
static volatile bool        m_twi_xfer_done;
static volatile bool        m_twi_data_nak;
static uint16_t             m_nak_bytes_transferred;


/* static function declarations */
static void twi_event_handler(nrf_drv_twi_evt_t const * p_event, void * p_context);


/*!
 * @brief NR52 specific initialization of the TWI (I2C) driver.  Initialize 
 * the pins, bus speed, and interrupt handler.
 *
 * @return Result of the initialization operation
*/
ret_code_t i2c_init()
{
    ret_code_t ret;

    const nrf_drv_twi_config_t twi_config = {
       .scl                = I2C_SCL_PIN,
       .sda                = I2C_SDA_PIN,
       .frequency          = NRF_DRV_TWI_FREQ_100K,
       .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
       .clear_bus_init     = false
    };

    ret = nrf_drv_twi_init(&m_twi, &twi_config, twi_event_handler, NULL);
    nrf_drv_twi_enable(&m_twi);

    return ret;
}

/** @brief NR52 specific method to send one or more bytes of data using the
 * TWI (I2C) bus.
 *
 *  @param device_addr  7 bit I2C address of the device
 *  @param reg_addr     Address of the register where the data will be written (initial register
 *                      for multi-byte write)
 *  @param tx_buff      Pointer to the buffer containing the data to send
 *  @param tx_size      Number of bytes of data to send
 *
 * @return Result of the write operation (succes or reason for failure)
*/
ret_code_t i2c_write_reg(uint8_t device_addr, uint8_t reg_addr, uint8_t *tx_buff, uint16_t tx_size)
{
    ret_code_t ret;

    if (tx_size < (TX_BUF_SIZE-1))
    {
        m_tx_buff[0] = reg_addr;
        memcpy(&m_tx_buff[1], tx_buff, tx_size);

        m_twi_xfer_done = false;
        m_twi_data_nak = false;
        ret = nrf_drv_twi_tx(&m_twi, device_addr, m_tx_buff, tx_size+1, false);
        while ((m_twi_xfer_done == false) && (m_twi_data_nak == false)) {__WFE();}
        APP_ERROR_CHECK(ret);
    }
    else
    {
        ret = NRFX_ERROR_INVALID_PARAM;
    }

    return ret;
}

/** @brief NR52 specific method to read one or more bytes of data using the
 * TWI (I2C) bus.
 *
 *  @param device_addr  7 bit I2C address of the device
 *  @param reg_addr     Address of the register where the data will be read from (initial register
 *                      for multi-byte read)
 *  @param rx_buff      Pointer to the buffer to place the data read from the device
 *  @param rx_size      Number of bytes of data to read
 *
 * @return Result of the read operation (succes or reason for failure)
*/
ret_code_t i2c_read_reg(uint8_t device_addr, uint8_t reg_addr, uint8_t *rx_buff, uint16_t rx_size)
{
    ret_code_t ret;
    
    m_twi_xfer_done = false;
    ret = nrf_drv_twi_tx(&m_twi, device_addr, &reg_addr, (uint8_t)1, true);
    APP_ERROR_CHECK(ret);

    if (NRFX_SUCCESS == ret)
    {
        while (m_twi_xfer_done == false) {__WFE();}

        m_twi_xfer_done = false;
        ret = nrf_drv_twi_rx(&m_twi, device_addr, rx_buff, (uint8_t)rx_size);
        APP_ERROR_CHECK(ret);
        if (NRFX_SUCCESS == ret)
        {
            while (m_twi_xfer_done == false) {__WFE();}
        }
    }

    return ret;
}

/*!
 * @brief Query if the result of the last I2C transfer was a NAK.
 *
 * @return Whether NAK occured (true) or not (false)
 */
bool i2c_is_nak()
{
    return m_twi_data_nak;
}

/*!
 * @brief Get the number of bytes that were transferred over I2C before
 * a NAK occurred.
 *
 * @return Number of bytes transferred before NAK.
 */
uint16_t i2c_get_nak_bytes()
{
    return m_nak_bytes_transferred;
}

/**
 * @brief Handles TWI events caused by communication with the device over the bus.
 *
 * @param p_event      Event that was triggered.
 * @param p_context    Context for the event.
 */
static void twi_event_handler(nrf_drv_twi_evt_t const *p_event, void *p_context)
{
    switch (p_event->type)
    {
        case NRF_DRV_TWI_EVT_DONE:
            m_twi_xfer_done = true;
            break;
        case NRF_DRV_TWI_EVT_ADDRESS_NACK:
            break;
        case NRF_DRV_TWI_EVT_DATA_NACK:
            m_twi_data_nak = true;
            m_nak_bytes_transferred = p_event->xfer_desc.primary_length;
            break;
        default:
            break;
    }
}
