/** @i2c.h
 * 
 * @brief A description of the module’s purpose. 
 *
 * @par       
 */ 

#ifndef __I2C_H
#define __I2C_H

#include <stdint.h>
#include <stdbool.h>


#define I2C_SCL_PIN         22    // SCL signal pin
#define I2C_SDA_PIN         23    // SDA signal pin



/* generic i2c driver function pointers */
typedef ret_code_t  (*i2c_init_fptr_t)();
typedef ret_code_t  (*i2c_read_fptr_t)(\
                                uint8_t device_addr,\
                                uint8_t reg_addr,\
                                uint8_t *rx_buff,\
                                uint16_t rx_size\
                            );
typedef ret_code_t  (*i2c_write_fptr_t)(\
                                uint8_t device_addr,\
                                uint8_t reg_addr,\
                                uint8_t *tx_buff,\
                                uint16_t tx_size\
                            );

typedef bool                (*i2c_isnak_fptr_t)(void);

typedef uint16_t            (*i2c_getnakbytes_fptr_t)(void);


/* general configuration for i2c device */
typedef struct 
{
    i2c_init_fptr_t   init;
    i2c_read_fptr_t   read_reg;
    i2c_write_fptr_t  write_reg;
    i2c_isnak_fptr_t  is_nak;
    i2c_getnakbytes_fptr_t    get_nak_bytes;

} i2c_dev_t;


extern ret_code_t i2c_init();
extern ret_code_t i2c_write_reg(uint8_t device_addr, uint8_t reg_addr, uint8_t *tx_buff, uint16_t tx_size);
extern ret_code_t i2c_read_reg(uint8_t device_addr, uint8_t reg_addr, uint8_t *rx_buff, uint16_t rx_size);
extern bool i2c_is_nak();
extern uint16_t i2c_get_nak_bytes();


#endif /* __I2C_H */

/*** end of file ***/
