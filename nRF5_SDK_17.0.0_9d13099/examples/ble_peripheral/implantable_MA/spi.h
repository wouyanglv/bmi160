/** @spi.h
 * 
 * @brief 
 *
 * @par       
 */ 

#ifndef __SPI_H
#define __SPI_H


#include <stdbool.h>
#include "sdk_errors.h"


#define SPI_INSTANCE_0  0
#define SPI_INSTANCE_1  1


#define NRF_SPI_XFER_LEN            255
#define NRF_EXPANDED_SPI_XFER_LEN   1024
#define MAX_SPI_XFER_BYTES          NRF_EXPANDED_SPI_XFER_LEN

/* SPI buffer defines */
#define MAX_CMD_LEN             (5)
#define SPI_TX_BUF_LEN          (MAX_SPI_XFER_BYTES)  /* max tx data length - for commands & addresses and data to program */
#define SPI_RX_BUF_LEN          (MAX_SPI_XFER_BYTES)  /* max rx data lengths - up to page length */


/*
 * These functions must be implemented for the target MCU platform
 */

/**
 * @brief Performs any required driver or hardware initialization in
 * order to use the SPI bus.
 *
 * @return Return value indicates if initialization was successful or if not,
 * the error that occurred.
 */
extern ret_code_t spi_init();

/**
 * @brief Transfers data over the SPI bus and receives data over the
 * SPI if expected.
 *
 * @param p_tx_buffer         Pointer to buffer of data to send.
 * @param tx_buffer_length    Length in bytes of the data to send.
 * @param p_rx_buffer         Pointer to buffer to place any received data.
 * @param rx_buffer_length    Length in bytes of the data to receive.
 * @param repeated_xfer       Flag indicating that the transfer will be executed multiple times.
 *
 * @return Return value indicates if transfer was successful or if not,
 * the error that occurred.
 */
extern ret_code_t spi_transfer(uint8_t instance_id, uint8_t const *p_tx_buffer, uint8_t tx_buffer_length,
                                uint8_t *p_rx_buffer, uint8_t rx_buffer_length, bool repeated_xfer);

/**
 * @brief Determines the maximum length for a single SPI transfer based on detecting
 * the specific MCU running on.
 * 
 * @return len    Maximum SPI transfer length (for single transfer)
 */
extern uint16_t get_max_spi_xfer_len();

#endif /* __SPI_H */

/*** end of file ***/
