/**
 * @file uart_protocol.h
 * @brief Robust line-based JSON communication protocol for RA6E2 SONAR-3D Scanner.
 *
 * Implements bidirectional line-based JSON communication over UART @ 115200 8N1.
 * Supports command parsing, request_id tracking, and standardized response generation.
 */

#ifndef UART_PROTOCOL_H_
#define UART_PROTOCOL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UART_PROTOCOL_DEFAULT_BAUD 115200U
#define UART_MAX_LINE_LENGTH       128U

typedef enum {
    UART_CMD_NONE = 0,
    UART_CMD_MEASURE,
    UART_CMD_PING,
    UART_CMD_RESET,
    UART_CMD_INVALID
} uart_cmd_type_t;

typedef struct {
    uart_cmd_type_t type;
    uint32_t request_id;
} uart_command_t;

/**
 * @brief Initializes the hardware UART peripheral (SCI0 on P101/P100 and P411) and RTT.
 * @param baud_rate Communication baud rate (115200).
 */
void uart_protocol_init(uint32_t baud_rate);

/**
 * @brief Non-blocking poll for an incoming JSON line command from the laptop.
 * @param p_cmd Pointer to store the parsed command structure.
 * @return True if a complete valid command was received and parsed.
 */
bool uart_protocol_poll_command(uart_command_t *p_cmd);

/**
 * @brief Transmits a measurement response JSON line to the laptop.
 *
 * Format:
 * {"type":"measurement","request_id":101,"distance_mm":425,"echo_us":2478,"status":"ok"}
 *
 * @param request_id  ID from the matching MEASURE command.
 * @param distance_mm Measured distance in millimeters (-1 if invalid/timeout).
 * @param echo_us     Measured pulse width in microseconds (0 if timeout).
 * @param status      Status string: "ok", "timeout", "out_of_range", etc.
 */
void uart_protocol_send_measurement(uint32_t request_id,
                                    int32_t distance_mm,
                                    uint32_t echo_us,
                                    const char *status);

/**
 * @brief Transmits a PONG response JSON line.
 * Format: {"type":"pong","status":"ready"}
 */
void uart_protocol_send_pong(void);

/**
 * @brief Transmits an error response JSON line.
 * Format: {"type":"error","message":"..."}
 */
void uart_protocol_send_error(const char *error_msg);

/**
 * @brief Transmits a raw string over UART and RTT.
 */
void uart_protocol_send_raw(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* UART_PROTOCOL_H_ */
