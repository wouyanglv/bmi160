/*******************************************************************************
 * Copyright (C) 2017 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated
 * Products, Inc. shall not be used except as stated in the Maxim Integrated
 * Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all
 * ownership rights.
 *******************************************************************************
 */
 

#include <stddef.h>
#include "max30003.h"
#include "max30003_intb.h"
#include "spi.h"
#include "nrf_delay.h"



//    // Constants
//    const int EINT_STATUS =  1 << 23;
//    const int RTOR_STATUS =  1 << 10;
//    const int RTOR_REG_OFFSET = 10;
//    const float RTOR_LSB_RES = 0.0078125f;
//    const int FIFO_OVF =  0x7;
//    const int FIFO_VALID_SAMPLE =  0x0;
//    const int FIFO_FAST_SAMPLE =  0x1;
//    const int ETAG_BITS = 0x7;


static uint8_t       m_tx_buf[SPI_TX_BUF_LEN];    /**< TX buffer. */
static uint8_t       m_rx_buf[SPI_RX_BUF_LEN];    /**< RX buffer. */
static void          (*m_data_avail_callback[MAX30003_NUM_DEV_INSTANCES])(void);



static ret_code_t max30003_read_reg(max30003_dev_t *dev, uint8_t reg, uint32_t *data, bool repeated_xfer);
static ret_code_t max30003_write_reg(max30003_dev_t *dev, uint8_t reg, uint32_t data);
static void max30003_intb_handler(max30003_dev_t *dev);



ret_code_t max30003_init(max30003_dev_t *dev)
{
    ret_code_t ret;

    // configure interrupt pin for INTB
    max30003_init_intb_gpio(dev, max30003_intb_handler);

    // reset the chip to clear registers
    ret = max30003_reset(dev);
    if (ret != NRF_SUCCESS)
    {
        return ret;
    }
    nrf_delay_ms(100);

    // need to do another command after sw reset/power on before INFO command per data sheet
    uint32_t status;
    ret = max30003_get_status(dev, &status);

    // get INFO to verify communications
    uint32_t info;
    ret = max30003_get_info(dev, &info);

    uint32_t cal;
    ret = max30003_get_calibration(dev,&cal);

    // General config register setting
    GeneralConfiguration CNFG_GEN_r;
    CNFG_GEN_r.general_config_bits.fmstr = 0;      // Master Clock Frequency: FMSTR = 32768Hz, TRES = 15.26µs (512Hz ECG progressions)
    CNFG_GEN_r.general_config_bits.en_ecg = 1;     // Enable ECG channel
    CNFG_GEN_r.general_config_bits.rbiasn = 1;     // Enable resistive bias on negative input
    CNFG_GEN_r.general_config_bits.rbiasp = 1;     // Enable resistive bias on positive input
    CNFG_GEN_r.general_config_bits.en_rbias = 1;   // Enable resistive bias
    CNFG_GEN_r.general_config_bits.imag = 2;       // Current magnitude = 10nA
    CNFG_GEN_r.general_config_bits.en_dcloff = 1;  // Enable DC lead-off detection   
    ret = max30003_write_reg(dev, MAX30003_CNFG_GEN, CNFG_GEN_r.all);        
    
    // ECG Config register setting
    ECGConfiguration CNFG_ECG_r;
    CNFG_ECG_r.ecg_config_bits.dlpf = 1;       // Digital LPF cutoff = 40Hz
    CNFG_ECG_r.ecg_config_bits.dhpf = 1;       // Digital HPF cutoff = 0.5Hz
    CNFG_ECG_r.ecg_config_bits.gain = 3;       // ECG gain = 160V/V
    CNFG_ECG_r.ecg_config_bits.rate = 1;       // Sample rate = 256 sps (NOTE: this must match #define MAX30003_SAMPLE_RATE in .h file)
    ret = max30003_write_reg(dev, MAX30003_CNFG_ECG , CNFG_ECG_r.all);
    
//    //R-to-R configuration
//    RtoR1Configuration CNFG_RTOR_r;
//    CNFG_RTOR_r.rtor1_config_bits.wndw = 0b0011;         // WNDW = 96ms
//    CNFG_RTOR_r.rtor1_config_bits.rgain = 0b1111;        // Auto-scale gain
//    CNFG_RTOR_r.rtor1_config_bits.pavg = 0b11;           // 16-average
//    CNFG_RTOR_r.rtor1_config_bits.ptsf = 0b0011;         // PTSF = 4/16
//    CNFG_RTOR_r.rtor1_config_bits.en_rtor = 0;           // Disable R-to-R detection
//    ret = max30003_write_reg(dev, MAX30003_CNFG_RTOR1 , CNFG_RTOR_r.all);    
        
    //Manage interrupts register setting
    ManageInterrupts MNG_INT_r;
    MNG_INT_r.manage_interrupts_bits.efit =31; //0x0b11111;
    MNG_INT_r.manage_interrupts_bits.clr_rrint = 1;        // Clear R-to-R on RTOR reg. read back
    ret = max30003_write_reg(dev, MAX30003_MNGR_INT , MNG_INT_r.all);   
    
    //Enable interrupts register setting
    EnableInterrupts EN_INT_r;
    EN_INT_r.all = 0;
    EN_INT_r.enable_interrupts_bits.en_eint = 1;              // Enable EINT interrupt (Indicates that ECG records meeting/exceeding the ECG FIFO Interrupt Threshold (EFIT) are available for readback. Remains active until ECG FIFO is read back to the extent required to clear the EFIT condition.)
    EN_INT_r.enable_interrupts_bits.en_rrint = 0;             // Disable R-to-R interrupt
    EN_INT_r.enable_interrupts_bits.intb_type = 3;            // Open-drain NMOS with internal pullup
    ret = max30003_write_reg(dev, MAX30003_EN_INT , EN_INT_r.all);      
    
    //Calibration setting; MUX settings must be updated to connect calibration voltages to input.
    CalConfiguration CAL_CONF_r;
    CAL_CONF_r.cal_config_bits.thigh = 0;
    CAL_CONF_r.cal_config_bits.fifty = 1;
    CAL_CONF_r.cal_config_bits.fcal = 2;
    CAL_CONF_r.cal_config_bits.vmag = 1;
    CAL_CONF_r.cal_config_bits.vmode = 1;
    CAL_CONF_r.cal_config_bits.en_vcal = 0;
    ret = max30003_write_reg(dev, MAX30003_CNFG_CAL, CAL_CONF_r.all);

    //Dyanmic modes config
    ManageDynamicModes MNG_DYN_r;
    MNG_DYN_r.manage_dynamic_modes_bits.fast = 0;                // Fast recovery mode disabled
    ret = max30003_write_reg(dev, MAX30003_MNGR_DYN , MNG_DYN_r.all);
    
    // MUX Config
    MuxConfiguration CNFG_MUX_r;
    CNFG_MUX_r.mux_config_bits.caln_sel = 0;
    CNFG_MUX_r.mux_config_bits.calp_sel = 0;
    CNFG_MUX_r.mux_config_bits.openn = 0;          // Connect ECGN to AFE channel
    CNFG_MUX_r.mux_config_bits.openp = 0;          // Connect ECGP to AFE channel
    CNFG_MUX_r.mux_config_bits.pol = 0;
    ret = max30003_write_reg(dev, MAX30003_CNFG_EMUX , CNFG_MUX_r.all);

    ret = max30003_write_reg(dev, MAX30003_SYNCH , 0);

    return ret;

//    m_tx_buf[0] = 0x9F;
//    m_tx_buf[1] = 0xFF;  /* dummy byte */
//
//    ret = dev->spi_xfer(dev->spi_instance, m_tx_buf, 2, m_rx_buf, 4);
//    SEGGER_RTT_printf(0, "Ret code: %d :: info: %x\r\n", ret, m_rx_buf[2]);
}

ret_code_t max30003_reset(max30003_dev_t *dev)
{
    ret_code_t ret;

    SEGGER_RTT_printf(0, "Resetting MAX30003...\r\n");
    ret = max30003_write_reg(dev, MAX30003_SW_RST, 0);
    return ret;
}

ret_code_t max30003_get_status(max30003_dev_t *dev, uint32_t *status)
{
    ret_code_t ret;

    ret = max30003_read_reg(dev, MAX30003_STATUS, status, false);
    return ret;
}

ret_code_t max30003_get_info(max30003_dev_t *dev, uint32_t *info)
{
    ret_code_t ret;

    ret = max30003_read_reg(dev, MAX30003_INFO, info, false);
    uint8_t check = m_rx_buf[1] >> 4;

    SEGGER_RTT_printf(0, "Ret code: %d :: INFO: 0x%lx :: 0x%x\r\n", ret, *info, check);

    return ret;
}

ret_code_t max30003_get_calibration(max30003_dev_t *dev, uint32_t *cal)
{
    ret_code_t ret;

    ret = max30003_read_reg(dev, MAX30003_CNFG_CAL, cal, false);

    SEGGER_RTT_printf(0, "Ret code: %d :: CAL: 0x%lx\r\n", ret, *cal);

    return ret;
}

ret_code_t max30003_get_interuption(max30003_dev_t *dev, uint32_t *interuption)
{
    ret_code_t ret;

    ret = max30003_read_reg(dev, MAX30003_MNGR_INT, interuption, false);

    SEGGER_RTT_printf(0, "Ret code: %d :: INTERUPTION: 0x%lx\r\n", ret, *interuption);

    return ret;
}
ret_code_t max30003_get_rtor(max30003_dev_t *dev, uint32_t *rtor)
{
    ret_code_t ret;

    ret = max30003_read_reg(dev, MAX30003_RTOR, rtor, false);
    *rtor = *rtor >>  RTOR_REG_OFFSET;
    return ret;
}

ret_code_t max30003_get_fifo(max30003_dev_t *dev, uint32_t *value)
{
    ret_code_t ret;

    ret = max30003_read_reg(dev, MAX30003_ECG_FIFO, value, false);
    return ret;
}

ret_code_t max30003_start_data(max30003_dev_t *dev, void (*data_avail_callbackfn)(void))
{
    ret_code_t ret;

    m_data_avail_callback[dev->spi_instance_id] = data_avail_callbackfn;
    ret = max30003_write_reg(dev, MAX30003_SYNCH, 0);
    return ret;
}

ret_code_t max30003_reset_fifo(max30003_dev_t *dev)
{
    ret_code_t ret;

    ret = max30003_write_reg(dev, MAX30003_FIFO_RST, 0);
    return ret;
}

static ret_code_t max30003_read_reg(max30003_dev_t *dev, uint8_t reg, uint32_t *data, bool repeated_xfer)
{
    ret_code_t ret;

    m_tx_buf[0] = (reg << 1) | MAX30003_READ_REG_FLAG;
    m_tx_buf[1] = 0xFF;
    m_tx_buf[2] = 0xFF;
    m_tx_buf[3] = 0xFF;

    ret = dev->spi_xfer(dev->spi_instance_id, m_tx_buf, 
                MAX30003_READ_REG_CMD_LEN+MAX30003_READ_REG_DATA_LEN, m_rx_buf, 
                MAX30003_READ_REG_CMD_LEN+MAX30003_READ_REG_DATA_LEN, repeated_xfer);

    *data = 0;
    *data |= (m_rx_buf[1] << 16);
    *data |= (m_rx_buf[2] << 8);
    *data |= m_rx_buf[3];

//    SEGGER_RTT_printf(0, "SPI read ret code: %d :: Data: %x\r\n", ret, *data);

    return ret;
}

static ret_code_t max30003_write_reg(max30003_dev_t *dev, uint8_t reg, uint32_t data)
{
    ret_code_t ret;

    m_tx_buf[0] = (reg << 1) | MAX30003_WRITE_REG_FLAG;
    m_tx_buf[1] = (0x00FF0000 & data) >> 16;
    m_tx_buf[2] = (0x0000FF00 & data) >> 8;
    m_tx_buf[3] = (0x000000FF & data);

    ret = dev->spi_xfer(dev->spi_instance_id, m_tx_buf, MAX30003_WRITE_REG_CMD_LEN+MAX30003_WRITE_REG_DATA_LEN, NULL, 0, false);

//    SEGGER_RTT_printf(0, "SPI write ret code: %d\r\n", ret);

    return ret;
}

static void max30003_intb_handler(max30003_dev_t *dev)
{
    if (m_data_avail_callback[dev->spi_instance_id] != NULL)
        m_data_avail_callback[dev->spi_instance_id]();
}

//****************************************************************************
//uint32_t readRegister(const Registers_e reg)
//{
//    uint32_t data = 0;
//    
//    m_cs = 0;
//    m_spiBus.write((reg << 1) | 1);
//    data |= (m_spiBus.write(0xFF) << 16);
//    data |= (m_spiBus.write(0xFF) << 8);
//    data |= m_spiBus.write(0xFF);
//    m_cs = 1;
//    
//    return data;
//}
//
////****************************************************************************    
//void writeRegister(const Registers_e reg, const uint32_t data)
//{
//    m_cs = 0;
//    m_spiBus.write(reg << 1);
//    m_spiBus.write((0x00FF0000 & data) >> 16);
//    m_spiBus.write((0x0000FF00 & data) >> 8);
//    m_spiBus.write( 0x000000FF & data);
//    m_cs = 1;
//}
 
//int main()
//{   
//    
//    // Constants
//    const int EINT_STATUS =  1 << 23;
//    const int RTOR_STATUS =  1 << 10;
//    const int RTOR_REG_OFFSET = 10;
//    const float RTOR_LSB_RES = 0.0078125f;
//    const int FIFO_OVF =  0x7;
//    const int FIFO_VALID_SAMPLE =  0x0;
//    const int FIFO_FAST_SAMPLE =  0x1;
//    const int ETAG_BITS = 0x7;
//    
//    // Ports and serial connections
//    Serial pc(USBTX, USBRX);            // Use USB debug probe for serial link
//    pc.baud(115200);                    // Baud rate = 115200
//    
//    DigitalOut rLed(LED1, LED_OFF);     // Debug LEDs
//
//    
//    InterruptIn ecgFIFO_int(P5_4);          // Config P5_4 as int. in for the
//    ecgFIFO_int.fall(&ecgFIFO_callback);    // ecg FIFO almost full interrupt
//    
//    SPI spiBus(SPI2_MOSI, SPI2_MISO, SPI2_SCK);     // SPI bus, P5_1 = MOSI, 
//                                                    // P5_2 = MISO, P5_0 = SCK
//    
//    MAX30003 ecgAFE(spiBus, P5_3);          // New MAX30003 on spiBus, CS = P5_3 
//    ecg_config( ecgAFE );                   // Config ECG 
//     
//    
//    ecgAFE.writeRegister( MAX30003::SYNCH , 0);
//    
//    uint32_t ecgFIFO, RtoR, readECGSamples, idx, ETAG[32], status;
//    int16_t ecgSample[32];
//    float BPM;
//    
//    while(1) {
//        
//        /* Read back ECG samples from the FIFO */
//        if( ecgIntFlag ) {
//            
//            ecgIntFlag = 0;
//            pc.printf("Interrupt received....\r\n");
//            status = ecgAFE.readRegister( MAX30003::STATUS );      // Read the STATUS register
//            pc.printf("Status : 0x%x\r\n"
//                      "Current BPM is %3.2f\r\n\r\n", status, BPM);
//             
//             
//            // Check if R-to-R interrupt asserted
//            if( ( status & RTOR_STATUS ) == RTOR_STATUS ){           
//            
//                pc.printf("R-to-R Interrupt \r\n");
//                
//                // Read RtoR register
//                RtoR = ecgAFE.readRegister( MAX30003::RTOR ) >>  RTOR_REG_OFFSET;   
//                
//                // Convert to BPM
//                BPM = 1.0f / ( RtoR * RTOR_LSB_RES / 60.0f );   
//                
//                // Print RtoR              
//                pc.printf("RtoR : %d\r\n\r\n", RtoR);                   
//                
//            }  
//             
//            // Check if EINT interrupt asserted
//            if ( ( status & EINT_STATUS ) == EINT_STATUS ) {     
//            
//                pc.printf("FIFO Interrupt \r\n");
//                readECGSamples = 0;                        // Reset sample counter
//                
//                do {
//                    ecgFIFO = ecgAFE.readRegister( MAX30003::ECG_FIFO );       // Read FIFO
//                    ecgSample[readECGSamples] = ecgFIFO >> 8;                  // Isolate voltage data
//                    ETAG[readECGSamples] = ( ecgFIFO >> 3 ) & ETAG_BITS;  // Isolate ETAG
//                    readECGSamples++;                                          // Increment sample counter
//                
//                // Check that sample is not last sample in FIFO                                              
//                } while ( ETAG[readECGSamples-1] == FIFO_VALID_SAMPLE || 
//                          ETAG[readECGSamples-1] == FIFO_FAST_SAMPLE ); 
//            
//                pc.printf("%d samples read from FIFO \r\n", readECGSamples);
//                
//                // Check if FIFO has overflowed
//                if( ETAG[readECGSamples - 1] == FIFO_OVF ){                  
//                    ecgAFE.writeRegister( MAX30003::FIFO_RST , 0); // Reset FIFO
//                    rLed=1;
//                }
//                
//                // Print results 
//                for( idx = 0; idx < readECGSamples; idx++ ) {
//                    pc.printf("Sample : %6d, \tETAG : 0x%x\r\n", ecgSample[idx], ETAG[idx]);           
//                }
//                pc.printf("\r\n\r\n\r\n"); 
//                                         
//            }
//        }
//    }
//}
//
//
//
//
//void ecg_config(MAX30003& ecgAFE) { 
//
//    // Reset ECG to clear registers
//    ecgAFE.writeRegister( MAX30003::SW_RST , 0);
//    
//    // General config register setting
//    MAX30003::GeneralConfiguration_u CNFG_GEN_r;
//    CNFG_GEN_r.bits.en_ecg = 1;     // Enable ECG channel
//    CNFG_GEN_r.bits.rbiasn = 1;     // Enable resistive bias on negative input
//    CNFG_GEN_r.bits.rbiasp = 1;     // Enable resistive bias on positive input
//    CNFG_GEN_r.bits.en_rbias = 1;   // Enable resistive bias
//    CNFG_GEN_r.bits.imag = 2;       // Current magnitude = 10nA
//    CNFG_GEN_r.bits.en_dcloff = 1;  // Enable DC lead-off detection   
//    ecgAFE.writeRegister( MAX30003::CNFG_GEN , CNFG_GEN_r.all);
//        
//    
//    // ECG Config register setting
//    MAX30003::ECGConfiguration_u CNFG_ECG_r;
//    CNFG_ECG_r.bits.dlpf = 1;       // Digital LPF cutoff = 40Hz
//    CNFG_ECG_r.bits.dhpf = 1;       // Digital HPF cutoff = 0.5Hz
//    CNFG_ECG_r.bits.gain = 3;       // ECG gain = 160V/V
//    CNFG_ECG_r.bits.rate = 2;       // Sample rate = 128 sps
//    ecgAFE.writeRegister( MAX30003::CNFG_ECG , CNFG_ECG_r.all);
//      
//    
//    //R-to-R configuration
//    MAX30003::RtoR1Configuration_u CNFG_RTOR_r;
//    CNFG_RTOR_r.bits.wndw = 0b0011;         // WNDW = 96ms
//    CNFG_RTOR_r.bits.rgain = 0b1111;        // Auto-scale gain
//    CNFG_RTOR_r.bits.pavg = 0b11;           // 16-average
//    CNFG_RTOR_r.bits.ptsf = 0b0011;         // PTSF = 4/16
//    CNFG_RTOR_r.bits.en_rtor = 1;           // Enable R-to-R detection
//    ecgAFE.writeRegister( MAX30003::CNFG_RTOR1 , CNFG_RTOR_r.all);
//       
//        
//    //Manage interrupts register setting
//    MAX30003::ManageInterrupts_u MNG_INT_r;
//    MNG_INT_r.bits.efit = 0b00011;          // Assert EINT w/ 4 unread samples
//    MNG_INT_r.bits.clr_rrint = 0b01;        // Clear R-to-R on RTOR reg. read back
//    ecgAFE.writeRegister( MAX30003::MNGR_INT , MNG_INT_r.all);
//    
//    
//    //Enable interrupts register setting
//    MAX30003::EnableInterrupts_u EN_INT_r;
//    EN_INT_r.bits.en_eint = 1;              // Enable EINT interrupt
//    EN_INT_r.bits.en_rrint = 1;             // Enable R-to-R interrupt
//    EN_INT_r.bits.intb_type = 3;            // Open-drain NMOS with internal pullup
//    ecgAFE.writeRegister( MAX30003::EN_INT , EN_INT_r.all);
//       
//       
//    //Dyanmic modes config
//    MAX30003::ManageDynamicModes_u MNG_DYN_r;
//    MNG_DYN_r.bits.fast = 0;                // Fast recovery mode disabled
//    ecgAFE.writeRegister( MAX30003::MNGR_DYN , MNG_DYN_r.all);
//    
//    // MUX Config
//    MAX30003::MuxConfiguration_u CNFG_MUX_r;
//    CNFG_MUX_r.bits.openn = 0;          // Connect ECGN to AFE channel
//    CNFG_MUX_r.bits.openp = 0;          // Connect ECGP to AFE channel
//    ecgAFE.writeRegister( MAX30003::CNFG_EMUX , CNFG_MUX_r.all);
//    
//    return;
//}  

