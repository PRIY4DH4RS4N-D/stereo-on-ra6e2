/**
 * @file i2c_bus.c
 * @brief Multi-Bus Independent I2C Master Driver Implementation for RA6E2.
 *
 * Implements robust pseudo-open-drain I2C using Renesas RA6E2 GPIO direction switching:
 * - Output Low: actively drives pin to GND.
 * - Input with Pull-up: releases pin High to 3.3V (High-Z + internal/external pull-up).
 * - Full support for clock stretching, repeated start, and bus recovery.
 */

#include "i2c_bus.h"
#include "timer_measurement.h"
#include "r_ioport.h"

#define I2C_HALF_DELAY_US   (5U) /* ~100 kHz standard mode half-period */

i2c_bus_t g_i2c_bus1 = {
    .bus_id  = 1,
    .scl_pin = I2C_BUS1_PIN_SCL,
    .sda_pin = I2C_BUS1_PIN_SDA,
    .name    = "I2C BUS 1 (D4/D5)"
};

i2c_bus_t g_i2c_bus2 = {
    .bus_id  = 2,
    .scl_pin = I2C_BUS2_PIN_SCL,
    .sda_pin = I2C_BUS2_PIN_SDA,
    .name    = "I2C BUS 2 (D2/D3)"
};

static inline void scl_low(const i2c_bus_t *p_bus)
{
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(p_bus->scl_pin, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW));
    R_BSP_PinAccessDisable();
    R_BSP_PinWrite(p_bus->scl_pin, BSP_IO_LEVEL_LOW);
}

static inline void scl_high(const i2c_bus_t *p_bus)
{
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(p_bus->scl_pin, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE));
    R_BSP_PinAccessDisable();
}

static inline uint32_t scl_read(const i2c_bus_t *p_bus)
{
    return R_BSP_PinRead(p_bus->scl_pin);
}

static inline void sda_low(const i2c_bus_t *p_bus)
{
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(p_bus->sda_pin, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW));
    R_BSP_PinAccessDisable();
    R_BSP_PinWrite(p_bus->sda_pin, BSP_IO_LEVEL_LOW);
}

static inline void sda_high(const i2c_bus_t *p_bus)
{
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(p_bus->sda_pin, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE));
    R_BSP_PinAccessDisable();
}

static inline uint32_t sda_read(const i2c_bus_t *p_bus)
{
    return R_BSP_PinRead(p_bus->sda_pin);
}

static void wait_scl_high(const i2c_bus_t *p_bus)
{
    /* Clock stretching support with timeout (~2ms) */
    uint32_t timeout = 400U;
    while (scl_read(p_bus) == 0U)
    {
        timer_delay_us(5U);
        if (--timeout == 0U)
        {
            break;
        }
    }
}

void i2c_bus_recover(i2c_bus_t *p_bus)
{
    sda_high(p_bus);
    scl_high(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);

    /* Generate up to 9 clock pulses to release stuck SDA */
    for (int i = 0; i < 9; i++)
    {
        if (sda_read(p_bus) != 0U)
        {
            break; /* SDA is free */
        }
        scl_low(p_bus);
        timer_delay_us(I2C_HALF_DELAY_US);
        scl_high(p_bus);
        timer_delay_us(I2C_HALF_DELAY_US);
    }

    /* Send STOP condition */
    sda_low(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);
    scl_high(p_bus);
    wait_scl_high(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);
    sda_high(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);
}

void i2c_bus_init(i2c_bus_t *p_bus)
{
    scl_high(p_bus);
    sda_high(p_bus);
    timer_delay_ms(5U);
    i2c_bus_recover(p_bus);
}

void i2c_init_all(void)
{
    i2c_bus_init(&g_i2c_bus1);
    i2c_bus_init(&g_i2c_bus2);
}

static void i2c_start(const i2c_bus_t *p_bus)
{
    sda_high(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);
    scl_high(p_bus);
    wait_scl_high(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);

    /* SDA goes low while SCL is high */
    sda_low(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);
    scl_low(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);
}

static void i2c_stop(const i2c_bus_t *p_bus)
{
    sda_low(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);
    scl_high(p_bus);
    wait_scl_high(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);

    /* SDA goes high while SCL is high */
    sda_high(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);
}

static bool i2c_write_byte(const i2c_bus_t *p_bus, uint8_t byte)
{
    for (int i = 7; i >= 0; i--)
    {
        if (byte & (1U << i))
        {
            sda_high(p_bus);
        }
        else
        {
            sda_low(p_bus);
        }
        timer_delay_us(I2C_HALF_DELAY_US);

        scl_high(p_bus);
        wait_scl_high(p_bus);
        timer_delay_us(I2C_HALF_DELAY_US);

        scl_low(p_bus);
        timer_delay_us(I2C_HALF_DELAY_US);
    }

    /* Release SDA for ACK */
    sda_high(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);

    scl_high(p_bus);
    wait_scl_high(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);

    /* Sample ACK (SDA low = ACK) */
    bool ack = (sda_read(p_bus) == 0U);

    scl_low(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);

    return ack;
}

static uint8_t i2c_read_byte(const i2c_bus_t *p_bus, bool ack)
{
    uint8_t byte = 0;
    sda_high(p_bus); /* Release SDA for incoming data */

    for (int i = 7; i >= 0; i--)
    {
        timer_delay_us(I2C_HALF_DELAY_US);
        scl_high(p_bus);
        wait_scl_high(p_bus);
        timer_delay_us(I2C_HALF_DELAY_US);

        if (sda_read(p_bus) != 0U)
        {
            byte |= (uint8_t)(1U << i);
        }

        scl_low(p_bus);
        timer_delay_us(I2C_HALF_DELAY_US);
    }

    /* Send ACK or NACK */
    if (ack)
    {
        sda_low(p_bus);
    }
    else
    {
        sda_high(p_bus);
    }
    timer_delay_us(I2C_HALF_DELAY_US);

    scl_high(p_bus);
    wait_scl_high(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);

    scl_low(p_bus);
    timer_delay_us(I2C_HALF_DELAY_US);
    sda_high(p_bus);

    return byte;
}

i2c_status_t i2c_write_reg(i2c_bus_t *p_bus, uint8_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    if (p_bus == NULL)
    {
        return I2C_ERR_BUS_BUSY;
    }

    i2c_start(p_bus);

    /* Address + Write (0) */
    if (!i2c_write_byte(p_bus, (uint8_t)(dev_addr << 1)))
    {
        i2c_stop(p_bus);
        return I2C_ERR_NACK;
    }

    /* Register address */
    if (!i2c_write_byte(p_bus, reg_addr))
    {
        i2c_stop(p_bus);
        return I2C_ERR_NACK;
    }

    /* Data byte */
    if (!i2c_write_byte(p_bus, data))
    {
        i2c_stop(p_bus);
        return I2C_ERR_NACK;
    }

    i2c_stop(p_bus);
    return I2C_SUCCESS;
}

i2c_status_t i2c_read_bytes(i2c_bus_t *p_bus, uint8_t dev_addr, uint8_t reg_addr, uint8_t *p_buf, uint16_t length)
{
    if (p_bus == NULL || p_buf == NULL || length == 0)
    {
        return I2C_ERR_BUS_BUSY;
    }

    i2c_start(p_bus);

    /* Address + Write (0) to set register pointer */
    if (!i2c_write_byte(p_bus, (uint8_t)(dev_addr << 1)))
    {
        i2c_stop(p_bus);
        return I2C_ERR_NACK;
    }

    if (!i2c_write_byte(p_bus, reg_addr))
    {
        i2c_stop(p_bus);
        return I2C_ERR_NACK;
    }

    /* STOP followed by START for guaranteed clone slave compatibility */
    i2c_stop(p_bus);
    timer_delay_us(5U);
    i2c_start(p_bus);

    /* Address + Read (1) */
    if (!i2c_write_byte(p_bus, (uint8_t)((dev_addr << 1) | 0x01)))
    {
        i2c_stop(p_bus);
        return I2C_ERR_NACK;
    }

    /* Read length bytes */
    for (uint16_t i = 0; i < length; i++)
    {
        bool ack = (i < (length - 1)); /* ACK all except last byte */
        p_buf[i] = i2c_read_byte(p_bus, ack);
    }

    i2c_stop(p_bus);
    return I2C_SUCCESS;
}
