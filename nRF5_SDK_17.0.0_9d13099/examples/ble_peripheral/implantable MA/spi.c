/** @spi.c
 * 
 * @brief A description of the module’s purpose. 
 *
 * @par       
 */

#include "app_config.h"
#include "spi.h"
#include "nrfx_spim.h"
#include "nrf_gpio.h"
#include "sdk_config.h"
#include "nrf_log.h"


//#define SPI_QUEUE_LENGTH     15
//#define SPI_INSTANCE_ID      0
//NRF_SPI_MNGR_DEF(m_nrf_spi_mngr, SPI_QUEUE_LENGTH, SPI_INSTANCE_ID);


/* local static variables */
static volatile bool         m_spi_xfer_done;  /**< Flag used to indicate that SPI instance completed the transfer. */
static nrfx_spim_t           m_spi = NRFX_SPIM_INSTANCE(SPI_INSTANCE_0);
static uint8_t               m_spi_ss_pins[NUM_BMI160_DEVS] = { BMI160_SPI_DEV0_SS_PIN};

//static nrf_drv_spi_config_t  m_spi_configs[] = 
//{ 
//    {
//        .sck_pin        = MAX30003_SPI_SCK_PIN,
//        .mosi_pin       = MAX30003_SPI_MOSI_PIN,
//        .miso_pin       = MAX30003_SPI_MISO_PIN,
//        .ss_pin         = MAX30003_SPI_DEV0_SS_PIN,
//        .irq_priority   = SPI_DEFAULT_CONFIG_IRQ_PRIORITY,
//        .orc            = 0xFF,
//        .frequency      = NRF_SPIM_FREQ_1M,//NRF_SPIM_FREQ_125K,//NRF_SPIM_FREQ_1M,
//        .mode           = NRF_DRV_SPI_MODE_0,
//        .bit_order      = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST
//    },
//
//    {
//        .sck_pin        = MAX30003_SPI_SCK_PIN,
//        .mosi_pin       = MAX30003_SPI_MOSI_PIN,
//        .miso_pin       = MAX30003_SPI_MISO_PIN,
//        .ss_pin         = MAX30003_SPI_DEV1_SS_PIN,
//        .irq_priority   = SPI_DEFAULT_CONFIG_IRQ_PRIORITY,
//        .orc            = 0xFF,
//        .frequency      = NRF_SPIM_FREQ_1M,//NRF_SPIM_FREQ_125K,//NRF_SPIM_FREQ_1M,
//        .mode           = NRF_DRV_SPI_MODE_0,
//        .bit_order      = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST
//    }
//};


/* static function declarations */
static void spim_event_handler(nrfx_spim_evt_t const *p_event, void *p_context);


/*!
 * @brief NR52 specific initialization of the SPI driver.
 */
ret_code_t spi_init()
{
    nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG;
    uint8_t instance_id;
    ret_code_t ret;

    /* initialize the SPI */
    spi_config.frequency      = NRF_SPIM_FREQ_1M;
    spi_config.ss_pin         = NRF_SPIM_PIN_NOT_CONNECTED;  //set not connected so we can control 
    spi_config.miso_pin       = BMI160_SPI_MISO_PIN;
    spi_config.mosi_pin       = BMI160_SPI_MOSI_PIN;
    spi_config.sck_pin        = BMI160_SPI_SCK_PIN;
    spi_config.ss_active_high = false;
    spi_config.mode           = NRF_SPIM_MODE_0;

    ret = nrfx_spim_init(&m_spi, &spi_config, spim_event_handler, NULL);

    for (instance_id=0; instance_id < NUM_BMI160_DEVS; instance_id++)
    {
//        nrf_gpio_cfg(m_spi_ss_pins[instance_id],                       
//                  NRF_GPIO_PIN_DIR_OUTPUT,
//                  NRF_GPIO_PIN_INPUT_DISCONNECT,
//                  NRF_GPIO_PIN_NOPULL,
//                  NRF_GPIO_PIN_S0S1,      
//                  NRF_GPIO_PIN_NOSENSE);
        nrf_gpio_cfg_output(m_spi_ss_pins[instance_id]);
        nrf_gpio_pin_set(m_spi_ss_pins[instance_id]);
//        //A Slave select must be set as high before setting it as output,
//        //because during connect it to the pin it causes glitches.
//        nrf_gpio_pin_set(m_spi_ss_pins[instance_id]);
//        nrf_gpio_cfg_output(m_spi_ss_pins[instance_id]);
//        nrf_gpio_pin_set(m_spi_ss_pins[instance_id]);
    }
    
    nrf_gpio_cfg(BMI160_SPI_SCK_PIN,                       
              NRF_GPIO_PIN_DIR_OUTPUT,
              NRF_GPIO_PIN_INPUT_DISCONNECT,
              NRF_GPIO_PIN_NOPULL,
              NRF_GPIO_PIN_H0H1,      
              NRF_GPIO_PIN_NOSENSE);
    nrf_gpio_pin_set(BMI160_SPI_SCK_PIN);
//
//    nrf_gpio_cfg(MAX30003_SPI_MOSI_PIN,                       
//              NRF_GPIO_PIN_DIR_OUTPUT,
//              NRF_GPIO_PIN_INPUT_DISCONNECT,
//              NRF_GPIO_PIN_PULLUP,
//              NRF_GPIO_PIN_H0H1,      
//              NRF_GPIO_PIN_NOSENSE);
//    nrf_gpio_pin_set(MAX30003_SPI_MOSI_PIN);
//
//    nrf_gpio_cfg(MAX30003_SPI_MISO_PIN,                       
//              NRF_GPIO_PIN_DIR_INPUT,
//              NRF_GPIO_PIN_INPUT_DISCONNECT,
//              NRF_GPIO_PIN_PULLUP,
//              NRF_GPIO_PIN_H0H1,      
//              NRF_GPIO_PIN_NOSENSE);
//    nrf_gpio_pin_set(MAX30003_SPI_MISO_PIN);

    return ret;
}

//ret_code_t spi_init()
//{
//    ret_code_t ret;
//    uint8_t i;
//
//    for (i=0; i < NUM_MAX30003_DEVS; i++)
//    {
//        //A Slave select must be set as high before setting it as output,
//        //because during connect it to the pin it causes glitches.
//        nrf_gpio_pin_set(m_spi_configs[i].ss_pin);
//        nrf_gpio_cfg_output(m_spi_configs[i].ss_pin);
//        nrf_gpio_pin_set(m_spi_configs[i].ss_pin);
//    }
//
//    ret = nrf_spi_mngr_init(&m_nrf_spi_mngr, &m_spi_configs[0]);
//
//    nrf_gpio_cfg(MAX30003_SPI_SCK_PIN,                       
//              NRF_GPIO_PIN_DIR_OUTPUT,
//              NRF_GPIO_PIN_INPUT_DISCONNECT,
//              NRF_GPIO_PIN_NOPULL,
//              NRF_GPIO_PIN_H0H1,      
//              NRF_GPIO_PIN_NOSENSE);
//    nrf_gpio_pin_set(MAX30003_SPI_SCK_PIN);
//
//    nrf_gpio_cfg(MAX30003_SPI_MOSI_PIN,                       
//              NRF_GPIO_PIN_DIR_OUTPUT,
//              NRF_GPIO_PIN_INPUT_DISCONNECT,
//              NRF_GPIO_PIN_PULLUP,
//              NRF_GPIO_PIN_H0H1,      
//              NRF_GPIO_PIN_NOSENSE);
//    nrf_gpio_pin_set(MAX30003_SPI_MOSI_PIN);
//
//    nrf_gpio_cfg(MAX30003_SPI_MISO_PIN,                       
//              NRF_GPIO_PIN_DIR_INPUT,
//              NRF_GPIO_PIN_INPUT_DISCONNECT,
//              NRF_GPIO_PIN_PULLUP,
//              NRF_GPIO_PIN_H0H1,      
//              NRF_GPIO_PIN_NOSENSE);
//    nrf_gpio_pin_set(MAX30003_SPI_MISO_PIN);
//
//    return ret;
//}
//
//
/*!
 * @brief Use the NRF52 driver to transfer data to and from the MT29F.
 * Waits for operation to complete before returning.
 *
 * @param p_tx_buffer          Buffer containing the data to write
 * @param tx_buffer_length     Length of data to write in bytes
 * @param p_rx_buffer          Buffer where the read data will be placed
 * @param rx_buffer_length     Length in bytes of the data to receive.
 * @param repeated_xfer        Flag indicating that the transfer will be executed multiple times.
 *
 * @return Result of transfer operation (success or reason for failure)
 */
ret_code_t spi_transfer(uint8_t instance_id,
                                uint8_t const *p_tx_buffer,
                                uint8_t      tx_buffer_length,
                                uint8_t     *p_rx_buffer,
                                uint8_t      rx_buffer_length,
                                bool         repeated_xfer)
{
    nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TRX(p_tx_buffer, tx_buffer_length, p_rx_buffer, rx_buffer_length);
    ret_code_t ret;
    uint32_t flags;

    if (repeated_xfer)
        flags = NRFX_SPIM_FLAG_REPEATED_XFER;
    else
        flags = 0;

    m_spi_xfer_done = false;
    nrf_gpio_pin_clear(m_spi_ss_pins[instance_id]);
    ret = nrfx_spim_xfer(&m_spi, &xfer_desc, flags);
    while (!m_spi_xfer_done)
    {
        __WFE();
    }

    nrf_gpio_pin_set(m_spi_ss_pins[instance_id]);

    return ret;
}

/*!
 * @brief Use the NRF52 driver to transfer data to and from the MT29F.
 * Waits for operation to complete before returning.
 *
 * @param p_tx_buffer          Buffer containing the data to write
 * @param tx_buffer_length     Length of data to write in bytes
 * @param p_rx_buffer          Buffer where the read data will be placed
 * @param rx_buffer_length     Length in bytes of the data to receive.
 * @param repeated_xfer        Flag indicating that the transfer will be executed multiple times.
 *
 * @return Result of transfer operation (success or reason for failure)
 */
//ret_code_t spi_transfer(uint8_t instance_id,
//                                uint8_t const *p_tx_buffer,
//                                uint8_t      tx_buffer_length,
//                                uint8_t     *p_rx_buffer,
//                                uint8_t      rx_buffer_length,
//                                bool         repeated_xfer)
//{
//    nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TRX(p_tx_buffer, tx_buffer_length, p_rx_buffer, rx_buffer_length);
//    ret_code_t ret;
//    uint32_t flags;
//
//    if (repeated_xfer)
//        flags = NRFX_SPIM_FLAG_REPEATED_XFER;
//    else
//        flags = 0;
//
//    m_spi_xfer_done = false;
//    ret = nrfx_spim_xfer(&(m_spi[instance_id]), &xfer_desc, flags);
//
//    while (!m_spi_xfer_done)
//    {
//        __WFE();
//    }
//
//    return ret;
//}

/**
 * @brief SPI user event handler called by the SPI driver when transfer is complete.
 *
 * @param p_event     Information on the event
 * @param p_context   Context information for the event
 *
 */
static void spim_event_handler(nrfx_spim_evt_t const *p_event, void *p_context)
{
    if (NRFX_SPIM_EVENT_DONE == p_event->type)
        m_spi_xfer_done = true;
}

//static void spim_xfer_done_cb(ret_code_t result, void * p_user_data)
//{
//    m_spi_xfer_done = true;
//}
