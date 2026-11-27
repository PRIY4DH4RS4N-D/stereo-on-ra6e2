/**
 * @file uart_logger.h
 * @brief High-reliability multi-channel UART and RTT logger for RA6E2.
 * 
 * Channels:
 * 1. Hardware SCI9 TXD9 (P109) -> J-Link CDC UART virtual COM port
 * 2. Precision bitbang on Arduino D1 (P411) -> 115200 8N1 TTL
 * 3. SEGGER J-Link RTT Viewer (Terminal 0)
 */

#ifndef UART_LOGGER_H_
#define UART_LOGGER_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize UART peripherals and RTT buffers.
 * @param baud_rate Communication baud rate (e.g. 115200).
 */
void uart_logger_init(uint32_t baud_rate);

/**
 * @brief Transmit a single character over all channels.
 */
void uart_logger_putc(char c);

/**
 * @brief Transmit a null-terminated string.
 */
void uart_logger_print(const char *str);

/**
 * @brief Transmit a string followed by CRLF.
 */
void uart_logger_println(const char *str);

/**
 * @brief Print an unsigned 32-bit integer.
 */
void uart_logger_print_u32(uint32_t val);

/**
 * @brief Print a signed 32-bit integer.
 */
void uart_logger_print_i32(int32_t val);

/**
 * @brief Print a signed 16-bit integer.
 */
void uart_logger_print_i16(int16_t val);

/**
 * @brief Print an 8-bit value in hexadecimal format (e.g. "0x68").
 */
void uart_logger_print_hex8(uint8_t val);

/**
 * @brief Print a floating point value with specified decimal places.
 */
void uart_logger_print_float(float val, uint8_t decimals);

#ifdef __cplusplus
}
#endif

#endif /* UART_LOGGER_H_ */
