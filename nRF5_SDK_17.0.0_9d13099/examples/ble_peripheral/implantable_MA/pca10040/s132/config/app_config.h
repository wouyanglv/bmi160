#ifndef APP_CONFIG_H__
#define APP_CONFIG_H__


#include <stdint.h>



/*
 * DEV0 = EEG
 * DEV1 = EMG
 */
#define NUM_BMI160_DEVS          1


#ifdef NRF52_DK

    /* SPI */
    #define BMI160_SPI_MOSI_PIN     27
    #define BMI160_SPI_MISO_PIN     29
    #define BMI160_SPI_SCK_PIN      26

    #define BMI160_SPI_DEV0_SS_PIN  28

    /* INTB */
    #define BMI160_SPI_DEV0_INTB_PIN  25

#else

    /* SPI */
    #define BMI160_SPI_MOSI_PIN     5
    #define BMI160_SPI_MISO_PIN     3
    #define BMI160_SPI_SCK_PIN      7

    #define BMI160_SPI_DEV0_SS_PIN  8

    /* INTB */
    #define BMI160_SPI_DEV0_INTB_PIN  2

#endif






#endif // APP_CONFIG_H__