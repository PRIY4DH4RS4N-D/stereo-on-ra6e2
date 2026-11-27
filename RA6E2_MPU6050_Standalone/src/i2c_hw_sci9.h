/**
 * @file i2c_hw_sci9.h
 * @brief Independent Hardware I2C Driver using RA6E2 SCI9 in Simple I2C Mode.
 *
 * Bus 1 Assignment:
 * - Peripheral: SCI9 (Simple I2C Master)
 * - SCL: Arduino D13 (P111 / SCK9)
 * - SDA: Arduino D11 (P109 / TXD9_SSDA9)
 * - Speed: 100 kHz Standard Mode
 */

#ifndef I2C_HW_SCI9_H_
#define I2C_HW_SCI9_H_

#include <stdint.h>
#include <stdbool.h>
#include "bsp_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Hardware SCI9 in Simple I2C master mode at 100 kHz.
 */
void i2c_hw_sci9_init(void);

/**
 * @brief Write 1 byte to a register of an I2C device on SCI9.
 * @param dev_addr 7-bit device address (e.g. 0x68).
 * @param reg_addr Register sub-address.
 * @param data Data byte to write.
 * @return 0 on success, negative error code on failure.
 */
int8_t i2c_hw_sci9_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);

/**
 * @brief Read multiple sequential bytes from an I2C device on SCI9.
 * @param dev_addr 7-bit device address (e.g. 0x68).
 * @param reg_addr Starting register sub-address.
 * @param p_buf Destination buffer.
 * @param length Number of bytes to read.
 * @return 0 on success, negative error code on failure.
 */
int8_t i2c_hw_sci9_read_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *p_buf, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* I2C_HW_SCI9_H_ */
