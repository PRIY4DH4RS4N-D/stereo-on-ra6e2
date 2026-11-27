/**
 * @file i2c_bus.h
 * @brief High-reliability I2C Bus Driver for Renesas BGK-RA6E2.
 * 
 * Pin Assignments (Arduino Header Standard):
 * - SCL: P100 (Arduino SCL / PMOD2 Pin 4)
 * - SDA: P101 (Arduino SDA / PMOD2 Pin 3)
 */

#ifndef I2C_BUS_H_
#define I2C_BUS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    I2C_SUCCESS       = 0,
    I2C_ERR_NACK      = -1,
    I2C_ERR_TIMEOUT   = -2,
    I2C_ERR_BUS_BUSY  = -3,
    I2C_ERR_PARAM     = -4
} i2c_status_t;

/**
 * @brief Initialize I2C GPIO pins with Open-Drain NMOS and internal pull-ups.
 *        Executes bus recovery sequence (9 clock pulses) if the bus is stuck.
 */
void i2c_init(void);

/**
 * @brief Generate I2C START condition.
 */
void i2c_start(void);

/**
 * @brief Generate I2C STOP condition.
 */
void i2c_stop(void);

/**
 * @brief Write a single byte to the I2C bus and check for slave ACK.
 * @param byte Byte to transmit.
 * @return true if slave returned ACK (0), false if NACK (1).
 */
bool i2c_write_byte(uint8_t byte);

/**
 * @brief Read a single byte from the I2C bus.
 * @param ack true to send ACK (more bytes to follow), false to send NACK (last byte).
 * @return Read byte.
 */
uint8_t i2c_read_byte(bool ack);

/**
 * @brief Write to a device register.
 * @param dev_addr 7-bit device I2C address (e.g. 0x68).
 * @param reg_addr Target register address.
 * @param data Data byte to write.
 * @return I2C_SUCCESS on success, error code otherwise.
 */
i2c_status_t i2c_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);

/**
 * @brief Read a single byte from a device register.
 * @param dev_addr 7-bit device I2C address (e.g. 0x68).
 * @param reg_addr Target register address.
 * @param p_data Pointer to store read byte.
 * @return I2C_SUCCESS on success, error code otherwise.
 */
i2c_status_t i2c_read_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *p_data);

/**
 * @brief Burst read multiple consecutive bytes from a device register.
 * @param dev_addr 7-bit device I2C address (e.g. 0x68).
 * @param reg_addr Starting register address.
 * @param p_buffer Pointer to receive buffer.
 * @param length Number of bytes to read.
 * @return I2C_SUCCESS on success, error code otherwise.
 */
i2c_status_t i2c_read_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *p_buffer, size_t length);

/**
 * @brief Ping device address to verify connection.
 * @param dev_addr 7-bit device I2C address.
 * @return true if device responded with ACK, false otherwise.
 */
bool i2c_ping(uint8_t dev_addr);

#ifdef __cplusplus
}
#endif

#endif /* I2C_BUS_H_ */
