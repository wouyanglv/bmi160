/** @
 * 
 * @brief A description of the module’s purpose. 
 *
 * @par       
 */

#include "app_config.h"
#include "max30003_intb.h"
#include "sdk_config.h"
#include "nrfx_gpiote.h"
#include "nrf_log.h"


static void     (*m_gpio_int_callback[NUM_MAX30003_DEVS])(void *user_data);
static void     *m_callback_dev[NUM_MAX30003_DEVS];
static uint8_t  m_intb_pins[NUM_MAX30003_DEVS] = { MAX30003_SPI_DEV0_INTB_PIN, MAX30003_SPI_DEV1_INTB_PIN };

static void max30003_int_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);


void max30003_init_intb_gpio(max30003_dev_t *dev, void (*callbackfn)(max30003_dev_t *dev))
{
    ret_code_t err_code;
    uint8_t pin_num = m_intb_pins[dev->spi_instance_id];

    m_gpio_int_callback[dev->spi_instance_id] = callbackfn;
    m_callback_dev[dev->spi_instance_id] = dev;

    if (!nrfx_gpiote_is_init())
    {
        err_code = nrfx_gpiote_init();
        APP_ERROR_CHECK(err_code);
    }

    nrfx_gpiote_in_config_t in_config = NRFX_GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    in_config.pull = NRF_GPIO_PIN_PULLUP;

    err_code = nrfx_gpiote_in_init(pin_num, &in_config, max30003_int_handler);
    APP_ERROR_CHECK(err_code);

    nrfx_gpiote_in_event_enable(pin_num, true);
}

static void max30003_int_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    uint8_t i;

    for (i=0; i < NUM_MAX30003_DEVS; i++)
    {
        if ((m_intb_pins[i] == pin) && (m_gpio_int_callback[i] != NULL))
            m_gpio_int_callback[i](m_callback_dev[i]);
    }
}



