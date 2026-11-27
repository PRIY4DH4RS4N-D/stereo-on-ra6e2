/**
 * @file uart_logger.h
 * @brief Multi-channel UART and RTT logger for RA6E2 MPU6050 Standalone project.
 *
 * Channels:
 * 1. Hardware SCI0 (P101 / TXD0) -> Onboard J-Link CDC USB Virtual COM Port (COM5)
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

void uart_logger_init(uint32_t baud_rate);
void uart_logger_putc(char c);
void uart_logger_print(const char *str);
void uart_logger_println(const char *str);
void uart_logger_print_u32(uint32_t val);
void uart_logger_print_i32(int32_t val);
void uart_logger_print_i16(int16_t val);
void uart_logger_print_hex8(uint8_t val);
void uart_logger_print_float(float val, uint8_t decimals);

#ifdef __cplusplus
}
#endif

#endif /* UART_LOGGER_H_ */
