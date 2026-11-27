/**
 * @file i2c_hw_iic0.c
 * @brief Independent Hardware I2C Driver using RA6E2 IIC_B0 (I3C/IIC) in I2C Master Mode.
 *
 * Bus 2 Assignment:
 * - Peripheral: IIC_B0 (I2C Master)
 * - SCL: Arduino D3 (P408 / SCL0)
 * - SDA: PMOD2 Pin 4 (P407 / SDA0)
 * - Speed: 100 kHz Standard Mode
 */

#include "i2c_hw_iic0.h"
#include "r_ioport.h"

#define IIC0_TIMEOUT_CYCLES (500000U)

void i2c_hw_iic0_init(void)
{
    /* 1. Supply module clock to I3C/IIC peripheral */
    R_BSP_MODULE_START(FSP_IP_I3C, 0);

    /* 2. Configure Pin Multiplexing:
     * P408 -> I3C/IIC SCL0 (Arduino D3)
     * P407 -> I3C/IIC SDA0 (PMOD2 Pin 4)
     * Both configured as Open-Drain NMOS with pull-ups enabled
     */
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(BSP_IO_PORT_04_PIN_08, (uint32_t)(IOPORT_CFG_PERIPHERAL_PIN |
                                                  IOPORT_PERIPHERAL_IIC |
                                                  IOPORT_CFG_NMOS_ENABLE |
                                                  IOPORT_CFG_PULLUP_ENABLE));
    R_BSP_PinCfg(BSP_IO_PORT_04_PIN_07, (uint32_t)(IOPORT_CFG_PERIPHERAL_PIN |
                                                  IOPORT_PERIPHERAL_IIC |
                                                  IOPORT_CFG_NMOS_ENABLE |
                                                  IOPORT_CFG_PULLUP_ENABLE));
    R_BSP_PinAccessDisable();

    /* 3. Hardware Peripheral Reset & Clock Configuration */
    R_I3C0->CECTL = (uint32_t) R_I3C0_CECTL_CLKE_Msk; /* Enable internal clock */
    R_I3C0->BCTL = 0U; /* Disable bus during configuration */

    R_I3C0->RSTCTL = (uint32_t) R_I3C0_RSTCTL_RI3CRST_Msk; /* Perform software reset */
    uint32_t to = IIC0_TIMEOUT_CYCLES;
    while (((R_I3C0->RSTCTL & R_I3C0_RSTCTL_RI3CRST_Msk) != 0U) && (--to > 0U))
    {
    }

    /* Set protocol mode to I2C (PRTMD = 1) */
    R_I3C0->PRTS = 1U;

    /* Bit Rate calculation for 100 kHz standard mode:
     * REFCKCTL = 0 (Internal clock / 1)
     * STDBR = low-level and high-level period
     */
    R_I3C0->REFCKCTL = 0U;
    R_I3C0->STDBR = (uint32_t) ((124U << R_I3C0_STDBR_SBRLO_Pos) | (124U << R_I3C0_STDBR_SBRHO_Pos));

    /* Enable Bus */
    R_I3C0->BCTL = (uint32_t) R_I3C0_BCTL_BUSE_Msk;
}

static int8_t iic0_start(void)
{
    /* Issue Start Condition */
    R_I3C0->CNDCTL = (uint32_t) R_I3C0_CNDCTL_STCND_Msk;

    uint32_t to = IIC0_TIMEOUT_CYCLES;
    while (((R_I3C0->BCST & R_I3C0_BCST_BFREF_Msk) != 0U) && (--to > 0U))
    {
    }
    return (to > 0U) ? 0 : -1;
}

static void iic0_stop(void)
{
    /* Issue Stop Condition */
    R_I3C0->CNDCTL = (uint32_t) R_I3C0_CNDCTL_SPCND_Msk;

    uint32_t to = IIC0_TIMEOUT_CYCLES;
    while (((R_I3C0->BCST & R_I3C0_BCST_BFREF_Msk) == 0U) && (--to > 0U))
    {
    }
}

static int8_t iic0_send_byte(uint8_t byte)
{
    /* Wait for Transmit Data Buffer Empty */
    uint32_t to = IIC0_TIMEOUT_CYCLES;
    while (((R_I3C0->NTST & R_I3C0_NTST_TDBEF0_Msk) == 0U) && (--to > 0U))
    {
    }
    if (to == 0U)
    {
        return -1;
    }

    /* Clear NACK flag if set */
    if (R_I3C0->BST & R_I3C0_BST_NACKDF_Msk)
    {
        R_I3C0->BSTFC = (uint32_t) R_I3C0_BSTFC_NACKDFC_Msk;
    }

    R_I3C0->NTDTBP0 = (uint32_t) byte;

    /* Wait for completion / ACK */
    to = IIC0_TIMEOUT_CYCLES;
    while (((R_I3C0->BST & R_I3C0_BST_TENDF_Msk) == 0U) && (--to > 0U))
    {
        if (R_I3C0->BST & R_I3C0_BST_NACKDF_Msk)
        {
            return -2; /* Slave NACK */
        }
    }

    return (to > 0U) ? 0 : -3;
}

static uint8_t iic0_read_byte(bool is_last)
{
    FSP_PARAMETER_NOT_USED(is_last);

    /* Wait for Receive Data Buffer Full */
    uint32_t to = IIC0_TIMEOUT_CYCLES;
    while (((R_I3C0->NTST & R_I3C0_NTST_RDBFF0_Msk) == 0U) && (--to > 0U))
    {
    }

    return (uint8_t)(R_I3C0->NTDTBP0 & 0xFFU);
}

int8_t i2c_hw_iic0_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    if (iic0_start() != 0)
    {
        iic0_stop();
        return -1;
    }

    /* Send device write address */
    int8_t err = iic0_send_byte((uint8_t)((dev_addr << 1) | 0x00U));
    if (err != 0)
    {
        iic0_stop();
        return err;
    }

    /* Send target register address */
    err = iic0_send_byte(reg_addr);
    if (err != 0)
    {
        iic0_stop();
        return err;
    }

    /* Send data byte */
    err = iic0_send_byte(data);
    if (err != 0)
    {
        iic0_stop();
        return err;
    }

    iic0_stop();
    return 0;
}

int8_t i2c_hw_iic0_read_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *p_buf, uint16_t length)
{
    if ((p_buf == NULL) || (length == 0U))
    {
        return -1;
    }

    if (iic0_start() != 0)
    {
        iic0_stop();
        return -1;
    }

    /* Send device write address to set internal register pointer */
    int8_t err = iic0_send_byte((uint8_t)((dev_addr << 1) | 0x00U));
    if (err != 0)
    {
        iic0_stop();
        return err;
    }

    err = iic0_send_byte(reg_addr);
    if (err != 0)
    {
        iic0_stop();
        return err;
    }

    /* Issue repeated start condition */
    R_I3C0->CNDCTL = (uint32_t) R_I3C0_CNDCTL_SRCND_Msk;
    uint32_t to = IIC0_TIMEOUT_CYCLES;
    while (((R_I3C0->BCST & R_I3C0_BCST_BFREF_Msk) != 0U) && (--to > 0U))
    {
    }

    /* Send device read address */
    err = iic0_send_byte((uint8_t)((dev_addr << 1) | 0x01U));
    if (err != 0)
    {
        iic0_stop();
        return err;
    }

    for (uint16_t i = 0U; i < length; ++i)
    {
        bool is_last = (i == (length - 1U));
        p_buf[i] = iic0_read_byte(is_last);
    }

    iic0_stop();
    return 0;
}
