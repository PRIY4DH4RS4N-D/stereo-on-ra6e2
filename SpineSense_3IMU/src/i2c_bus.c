/**
 * @file i2c_bus.c
 * @brief High-reliability I2C Bus Driver implementation for Renesas BGK-RA6E2.
 */

#include "i2c_bus.h"
#include "timer_measurement.h"
#include "hal_data.h"
#include "r_ioport.h"

/* Pin definitions on Port 1 */
#define SCL_PIN BSP_IO_PORT_01_PIN_00 /* P100 - Arduino SCL */
#define SDA_PIN BSP_IO_PORT_01_PIN_01 /* P101 - Arduino SDA */

#define SCL_HIGH() (R_PORT1->PCNTR3 = (1UL << 0))
#define SCL_LOW()  (R_PORT1->PCNTR3 = (1UL << 16))

#define SDA_HIGH() (R_PORT1->PCNTR3 = (1UL << 1))
#define SDA_LOW()  (R_PORT1->PCNTR3 = (1UL << 17))

#define SDA_READ() (R_BSP_PinRead(SDA_PIN) == BSP_IO_LEVEL_HIGH)
#define SCL_READ() (R_BSP_PinRead(SCL_PIN) == BSP_IO_LEVEL_HIGH)

/* Timing cycles: Standard Mode (100 kHz) -> 5 us half-period = 1000 cycles @ 200 MHz */
static uint32_t s_half_period_cycles = 1000U;

static inline void i2c_delay(void)
{
    uint32_t start = timer_get_cycles();
    while ((timer_get_cycles() - start) < s_half_period_cycles)
    {
        __NOP();
    }
}

static inline bool i2c_scl_release(void)
{
    SCL_HIGH();
    /* Clock stretching support: wait up to 1 ms (200,000 cycles) for slave release */
    uint32_t timeout = timer_get_cycles();
    while (!SCL_READ())
    {
        if ((timer_get_cycles() - timeout) > 200000U)
        {
            return false; /* SCL stuck low by slave */
        }
    }
    return true;
}

void i2c_init(void)
{
    uint32_t clk_hz = timer_get_system_clock_hz();
    if (clk_hz > 0U)
    {
        /* 100 kHz standard mode: 5 microseconds half-period */
        s_half_period_cycles = clk_hz / 200000U;
    }
    else
    {
        s_half_period_cycles = 1000U;
    }

    /* Configure P100 (SCL) and P101 (SDA) as Open-Drain Output High with Pull-up */
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(SCL_PIN, (uint32_t)(IOPORT_CFG_NMOS_ENABLE | IOPORT_CFG_PORT_DIRECTION_OUTPUT | 
                                      IOPORT_CFG_PORT_OUTPUT_HIGH | IOPORT_CFG_PULLUP_ENABLE));
    R_BSP_PinCfg(SDA_PIN, (uint32_t)(IOPORT_CFG_NMOS_ENABLE | IOPORT_CFG_PORT_DIRECTION_OUTPUT | 
                                      IOPORT_CFG_PORT_OUTPUT_HIGH | IOPORT_CFG_PULLUP_ENABLE));
    R_BSP_PinAccessDisable();

    SCL_HIGH();
    SDA_HIGH();
    timer_delay_ms(1U);

    /* I2C Bus Recovery Sequence:
     * If a slave was interrupted during a read, SDA may be held low.
     * Clock SCL up to 9 times to let slave finish its byte and release SDA.
     */
    if (!SDA_READ())
    {
        for (int i = 0; i < 9; ++i)
        {
            SCL_LOW();
            i2c_delay();
            SCL_HIGH();
            i2c_delay();
            if (SDA_READ())
            {
                break;
            }
        }
        /* Generate STOP to reset slave state machine */
        i2c_stop();
    }
}

void i2c_start(void)
{
    SDA_HIGH();
    i2c_delay();
    i2c_scl_release();
    i2c_delay();

    /* START: SDA goes low while SCL is high */
    SDA_LOW();
    i2c_delay();
    SCL_LOW();
    i2c_delay();
}

void i2c_stop(void)
{
    SCL_LOW();
    SDA_LOW();
    i2c_delay();

    /* STOP: SCL goes high, then SDA goes high */
    i2c_scl_release();
    i2c_delay();
    SDA_HIGH();
    i2c_delay();
}

bool i2c_write_byte(uint8_t byte)
{
    /* 8 Data Bits, MSB first */
    for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1)
    {
        if (byte & mask)
        {
            SDA_HIGH();
        }
        else
        {
            SDA_LOW();
        }
        i2c_delay();
        i2c_scl_release();
        i2c_delay();
        SCL_LOW();
    }

    /* 9th Clock: Sample ACK bit from slave */
    SDA_HIGH(); /* Release SDA for slave to pull down */
    i2c_delay();
    i2c_scl_release();
    i2c_delay();

    bool ack = (SDA_READ() == false); /* ACK is active LOW (0) */

    SCL_LOW();
    i2c_delay();

    return ack;
}

uint8_t i2c_read_byte(bool ack)
{
    uint8_t byte = 0;
    SDA_HIGH(); /* Release line for slave data */

    for (int i = 0; i < 8; ++i)
    {
        byte <<= 1;
        i2c_delay();
        i2c_scl_release();
        i2c_delay();

        if (SDA_READ())
        {
            byte |= 1U;
        }

        SCL_LOW();
    }

    /* 9th Clock: Master sends ACK (0) or NACK (1) */
    if (ack)
    {
        SDA_LOW(); /* ACK */
    }
    else
    {
        SDA_HIGH(); /* NACK */
    }
    i2c_delay();
    i2c_scl_release();
    i2c_delay();
    SCL_LOW();
    SDA_HIGH(); /* Release SDA */
    i2c_delay();

    return byte;
}

bool i2c_ping(uint8_t dev_addr)
{
    i2c_start();
    bool ack = i2c_write_byte((uint8_t)(dev_addr << 1)); /* Write address */
    i2c_stop();
    return ack;
}

i2c_status_t i2c_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    i2c_start();

    /* Send device write address */
    if (!i2c_write_byte((uint8_t)(dev_addr << 1)))
    {
        i2c_stop();
        return I2C_ERR_NACK;
    }

    /* Send register address */
    if (!i2c_write_byte(reg_addr))
    {
        i2c_stop();
        return I2C_ERR_NACK;
    }

    /* Send data byte */
    if (!i2c_write_byte(data))
    {
        i2c_stop();
        return I2C_ERR_NACK;
    }

    i2c_stop();
    return I2C_SUCCESS;
}

i2c_status_t i2c_read_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *p_data)
{
    if (p_data == NULL)
    {
        return I2C_ERR_PARAM;
    }

    i2c_start();

    /* Send device write address */
    if (!i2c_write_byte((uint8_t)(dev_addr << 1)))
    {
        i2c_stop();
        return I2C_ERR_NACK;
    }

    /* Send register address */
    if (!i2c_write_byte(reg_addr))
    {
        i2c_stop();
        return I2C_ERR_NACK;
    }

    /* Repeated START */
    i2c_start();

    /* Send device read address */
    if (!i2c_write_byte((uint8_t)((dev_addr << 1) | 1U)))
    {
        i2c_stop();
        return I2C_ERR_NACK;
    }

    /* Read byte with NACK (only 1 byte) */
    *p_data = i2c_read_byte(false);

    i2c_stop();
    return I2C_SUCCESS;
}

i2c_status_t i2c_read_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *p_buffer, size_t length)
{
    if (p_buffer == NULL || length == 0)
    {
        return I2C_ERR_PARAM;
    }

    i2c_start();

    /* Send device write address */
    if (!i2c_write_byte((uint8_t)(dev_addr << 1)))
    {
        i2c_stop();
        return I2C_ERR_NACK;
    }

    /* Send register address */
    if (!i2c_write_byte(reg_addr))
    {
        i2c_stop();
        return I2C_ERR_NACK;
    }

    /* Repeated START */
    i2c_start();

    /* Send device read address */
    if (!i2c_write_byte((uint8_t)((dev_addr << 1) | 1U)))
    {
        i2c_stop();
        return I2C_ERR_NACK;
    }

    /* Read length - 1 bytes with ACK, last byte with NACK */
    for (size_t i = 0; i < length; ++i)
    {
        bool send_ack = (i < (length - 1));
        p_buffer[i] = i2c_read_byte(send_ack);
    }

    i2c_stop();
    return I2C_SUCCESS;
}
