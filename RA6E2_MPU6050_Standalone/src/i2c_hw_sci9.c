/**
 * @file i2c_hw_sci9.c
 * @brief Independent Hardware I2C Driver using RA6E2 SCI9 in Simple I2C Mode.
 *
 * Bus 1 Assignment:
 * - Peripheral: SCI9 (Simple I2C Master)
 * - SCL: Arduino D13 (P111 / SCK9)
 * - SDA: Arduino D11 (P109 / TXD9_SSDA9)
 * - Speed: 100 kHz Standard Mode
 */

#include "i2c_hw_sci9.h"
#include "r_ioport.h"

#define TIMEOUT_CYCLES (500000U)

static void sci9_start(void)
{
    /* Request Start Condition (IICSTAREQ = 1) */
    R_SCI9->SIMR3 = 0x51U;
    uint32_t to = TIMEOUT_CYCLES;
    while (((R_SCI9->SIMR3 & R_SCI0_SIMR3_IICSTIF_Msk) == 0U) && (--to > 0U))
    {
    }
    /* Clear condition interrupt flag */
    R_SCI9->SIMR3 = 0x00U;
}

static void sci9_stop(void)
{
    /* Request Stop Condition (IICSTOPOREQ = 1) */
    R_SCI9->SIMR3 = 0x54U;
    uint32_t to = TIMEOUT_CYCLES;
    while (((R_SCI9->SIMR3 & R_SCI0_SIMR3_IICSTIF_Msk) == 0U) && (--to > 0U))
    {
    }
    /* Clear condition interrupt flag */
    R_SCI9->SIMR3 = 0x00U;
}

static int8_t sci9_send_byte(uint8_t byte)
{
    uint32_t to = TIMEOUT_CYCLES;
    while (((R_SCI9->SSR & R_SCI0_SSR_TDRE_Msk) == 0U) && (--to > 0U))
    {
    }
    if (to == 0U)
    {
        return -1;
    }

    R_SCI9->TDR = byte;

    to = TIMEOUT_CYCLES;
    while (((R_SCI9->SSR & R_SCI0_SSR_TEND_Msk) == 0U) && (--to > 0U))
    {
    }
    if (to == 0U)
    {
        return -2;
    }

    /* Check Acknowledge: 0 = ACK, 1 = NACK */
    if (R_SCI9->SISR & R_SCI0_SISR_IICACKR_Msk)
    {
        return -3; /* NACK received */
    }

    return 0; /* ACK received */
}

static uint8_t sci9_read_byte(bool is_last)
{
    /* Configure ACK / NACK transmission:
     * Bit 5 (IICACKT): 0 = send ACK, 1 = send NACK
     * Bits [1:0] (IICCSC, IICINTM): keep enabled (0x03)
     */
    if (is_last)
    {
        R_SCI9->SIMR2 = 0x23U; /* Send NACK on 9th clock */
    }
    else
    {
        R_SCI9->SIMR2 = 0x03U; /* Send ACK on 9th clock */
    }

    /* Read RDR to clear any pending RDRF */
    volatile uint8_t dummy = R_SCI9->RDR;
    (void)dummy;

    /* Write dummy 0xFF to TDR to trigger 8 clock pulses */
    uint32_t to = TIMEOUT_CYCLES;
    while (((R_SCI9->SSR & R_SCI0_SSR_TDRE_Msk) == 0U) && (--to > 0U))
    {
    }
    R_SCI9->TDR = 0xFFU;

    to = TIMEOUT_CYCLES;
    while (((R_SCI9->SSR & R_SCI0_SSR_RDRF_Msk) == 0U) && (--to > 0U))
    {
    }

    return R_SCI9->RDR;
}

void i2c_hw_sci9_init(void)
{
    /* 1. Supply module clock to SCI9 */
    R_BSP_MODULE_START(FSP_IP_SCI, 9);

    /* 2. Configure GPIO Pins:
     * P111 -> SCK9 (Arduino D13 - SCL) Open-Drain with internal pull-up
     * P109 -> TXD9 (Arduino D11 - SDA) Open-Drain with internal pull-up
     */
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(BSP_IO_PORT_01_PIN_11, (uint32_t)(IOPORT_CFG_PERIPHERAL_PIN |
                                                  IOPORT_PERIPHERAL_SCI1_3_5_7_9 |
                                                  IOPORT_CFG_NMOS_ENABLE |
                                                  IOPORT_CFG_PULLUP_ENABLE));
    R_BSP_PinCfg(BSP_IO_PORT_01_PIN_09, (uint32_t)(IOPORT_CFG_PERIPHERAL_PIN |
                                                  IOPORT_PERIPHERAL_SCI1_3_5_7_9 |
                                                  IOPORT_CFG_NMOS_ENABLE |
                                                  IOPORT_CFG_PULLUP_ENABLE));
    R_BSP_PinAccessDisable();

    /* 3. Configure SCI9 in Simple I2C Mode */
    R_SCI9->SCR = 0U; /* Disable transmitter/receiver during setup */
    R_SCI9->SMR = 0U; /* Clock divisor: PCLKA / 1 (CKS = 0) */
    R_SCI9->SCMR = 0xFAU; /* Standard Non-Smartcard, MSB first */

    /* 100 kHz bit rate: BRR = (PCLKA / (32 * 100000)) - 1 = (100MHz / 3.2MHz) - 1 = 30 */
    R_SCI9->BRR = 30U;

    R_SCI9->SEMR = (uint8_t)R_SCI0_SEMR_NFEN_Msk; /* Enable digital noise filter */
    R_SCI9->SNFR = 0U;

    /* SIMR1: Enable Simple I2C mode (IICM = 1), SDA delay = 19 PCLKA cycles (0x13) */
    R_SCI9->SIMR1 = (uint8_t)((0x13U << R_SCI0_SIMR1_IICDL_Pos) | 0x01U);

    /* SIMR2: Clock sync (IICCSC=1), Interrupt select (IICINTM=1), Default ACK (IICACKT=0) */
    R_SCI9->SIMR2 = 0x23U;

    R_SCI9->SPMR = 0U;

    /* Clear SIMR3 condition flags */
    R_SCI9->SIMR3 = 0U;

    /* Enable transmitter and receiver */
    R_SCI9->SCR = (uint8_t)(R_SCI0_SCR_TE_Msk | R_SCI0_SCR_RE_Msk);
}

int8_t i2c_hw_sci9_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    sci9_start();

    /* Send device write address */
    int8_t err = sci9_send_byte((uint8_t)((dev_addr << 1) | 0x00U));
    if (err != 0)
    {
        sci9_stop();
        return err;
    }

    /* Send target register address */
    err = sci9_send_byte(reg_addr);
    if (err != 0)
    {
        sci9_stop();
        return err;
    }

    /* Send data byte */
    err = sci9_send_byte(data);
    if (err != 0)
    {
        sci9_stop();
        return err;
    }

    sci9_stop();
    return 0;
}

int8_t i2c_hw_sci9_read_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *p_buf, uint16_t length)
{
    if ((p_buf == NULL) || (length == 0U))
    {
        return -1;
    }

    sci9_start();

    /* Send device write address to set register pointer */
    int8_t err = sci9_send_byte((uint8_t)((dev_addr << 1) | 0x00U));
    if (err != 0)
    {
        sci9_stop();
        return err;
    }

    err = sci9_send_byte(reg_addr);
    if (err != 0)
    {
        sci9_stop();
        return err;
    }

    /* Restart condition for read */
    sci9_start();

    /* Send device read address */
    err = sci9_send_byte((uint8_t)((dev_addr << 1) | 0x01U));
    if (err != 0)
    {
        sci9_stop();
        return err;
    }

    for (uint16_t i = 0U; i < length; ++i)
    {
        bool is_last = (i == (length - 1U));
        p_buf[i] = sci9_read_byte(is_last);
    }

    sci9_stop();
    return 0;
}
