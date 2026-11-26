/**
 * @file uart_logger.h
 * @brief High-reliability UART and RTT logging interface for RA6E2 Ultrasonic System.
 *
 * Supports transmission over:
 * 1. Hardware Pin Arduino D1 (P411 TX) & PMOD2 Pin 6 (P101) @ 115200 8N1
 * 2. SEGGER J-Link RTT Viewer (Terminal 0)
 */

#ifndef UART_LOGGER_H_
#define UART_LOGGER_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the UART logger hardware and RTT buffers.
 * @param baud_rate Communication baud rate (typically 115200).
 */
void uart_logger_init(uint32_t baud_rate);

/**
 * @brief Transmits a single character over UART and RTT.
 * @param c Character to send.
 */
void uart_logger_putc(char c);

/**
 * @brief Transmits a null-terminated string over UART and RTT.
 * @param str String to send.
 */
void uart_logger_print(const char *str);

/**
 * @brief Transmits a string followed by CRLF.
 * @param str String to send.
 */
void uart_logger_println(const char *str);

/**
 * @brief Prints an unsigned 32-bit integer.
 * @param val Value to print.
 */
void uart_logger_print_u32(uint32_t val);

/**
 * @brief Prints a floating point number with specified decimal places.
 * @param val Value to print.
 * @param decimals Number of fractional digits (e.g. 2).
 */
void uart_logger_print_float(float val, uint8_t decimals);

/**
 * @brief Prints the standard project banner header.
 */
void uart_logger_print_banner(void);

/**
 * @brief Prints the standardized HC-SR04 measurement report block.
 *
 * Format matches specification:
 * =================================
 * RA6E2 ULTRASONIC DISTANCE SYSTEM
 * =================================
 * Sensor Status : CONNECTED / TIMEOUT / DISCONNECTED
 * Distance      : 24.65 cm / INVALID
 * Echo Time     : 1437 us
 * Measurement   : SUCCESS / TIMEOUT / OUT OF RANGE
 *
 * @param sensor_status   "CONNECTED", "TIMEOUT", "DISCONNECTED", etc.
 * @param distance_cm     Calculated distance in centimeters.
 * @param echo_time_us    Measured echo pulse duration in microseconds.
 * @param measurement_str Detailed status description ("SUCCESS", "RISING TIMEOUT", etc.)
 * @param is_valid        True if distance is valid; false if invalid/timeout.
 */
void uart_logger_print_measurement(const char *sensor_status,
                                  float distance_cm,
                                  uint32_t echo_time_us,
                                  const char *measurement_str,
                                  bool is_valid);

#ifdef __cplusplus
}
#endif

#endif /* UART_LOGGER_H_ */
