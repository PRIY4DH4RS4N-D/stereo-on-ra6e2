/**
 * @file i2c_bus.h
 * @brief Multi-Bus Independent I2C Master Driver for Renesas BGK-RA6E2.
 *
 * Supports multiple independent I2C buses:
 * - Bus 1: Arduino Header Pin D4 (SCL, P500) & Pin D5 (SDA, P409) [Sensor 1: MPU6050_NECK]
 * - Bus 2: Arduino Header Pin D2 (SCL, P105) & Pin D3 (SDA, P408) [Sensor 2: MPU6050_CENTER]
 *
 * Both buses run independently at 100 kHz standard mode with open-drain emulation,
 * internal/external pull-up compatibility, clock stretching, and stuck-bus recovery.
 */

#ifndef I2C_BUS_H_
#define I2C_BUS_H_

#include <stdint.h>
#include <stdbool.h>
#include "bsp_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bus 1 Pin Assignments (Existing Preserved Sensor 1 Wiring) */
#define I2C_BUS1_PIN_SCL     (BSP_IO_PORT_05_PIN_00) /* Arduino Header Pin D4 */
#define I2C_BUS1_PIN_SDA     (BSP_IO_PORT_04_PIN_09) /* Arduino Header Pin D5 */

/* Bus 2 Pin Assignments (New Independent Sensor 2 Wiring) */
#define I2C_BUS2_PIN_SCL     (BSP_IO_PORT_01_PIN_05) /* Arduino Header Pin D2 */
#define I2C_BUS2_PIN_SDA     (BSP_IO_PORT_04_PIN_08) /* Arduino Header Pin D3 */

typedef enum {
    I2C_SUCCESS = 0,
    I2C_ERR_NACK,
    I2C_ERR_TIMEOUT,
    I2C_ERR_BUS_BUSY
} i2c_status_t;

typedef struct {
    uint8_t             bus_id;   /* 1 or 2 */
    bsp_io_port_pin_t   scl_pin;  /* SCL GPIO pin */
    bsp_io_port_pin_t   sda_pin;  /* SDA GPIO pin */
    const char         *name;     /* "I2C BUS 1" or "I2C BUS 2" */
} i2c_bus_t;

extern i2c_bus_t g_i2c_bus1;
extern i2c_bus_t g_i2c_bus2;

/**
 * @brief Initializes a specific I2C bus instance with bus clear recovery.
 * @param p_bus Pointer to the bus structure.
 */
void i2c_bus_init(i2c_bus_t *p_bus);

/**
 * @brief Recovers a specific I2C bus from a stuck slave.
 * @param p_bus Pointer to the bus structure.
 */
void i2c_bus_recover(i2c_bus_t *p_bus);

/**
 * @brief Writes to a slave device register on the given bus.
 * @param p_bus Pointer to the bus structure.
 * @param dev_addr 7-bit device address.
 * @param reg_addr Register address.
 * @param data Data byte to write.
 * @return I2C_SUCCESS on success.
 */
i2c_status_t i2c_write_reg(i2c_bus_t *p_bus, uint8_t dev_addr, uint8_t reg_addr, uint8_t data);

/**
 * @brief Reads multiple bytes from a slave device register on the given bus.
 * @param p_bus Pointer to the bus structure.
 * @param dev_addr 7-bit device address.
 * @param reg_addr Starting register address.
 * @param p_buf Destination buffer.
 * @param length Number of bytes to read.
 * @return I2C_SUCCESS on success.
 */
i2c_status_t i2c_read_bytes(i2c_bus_t *p_bus, uint8_t dev_addr, uint8_t reg_addr, uint8_t *p_buf, uint16_t length);

/**
 * @brief Initializes all configured I2C buses.
 */
void i2c_init_all(void);

#ifdef __cplusplus
}
#endif

#endif /* I2C_BUS_H_ */
