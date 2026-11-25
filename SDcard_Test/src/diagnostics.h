#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdint.h>
#include <stdbool.h>

void diag_uart_init(void);
void diag_char(char character);
void diag_text(const char *text);
void diag_u32(uint32_t value);
void diag_u64(uint64_t value);
void diag_hex_byte(uint8_t value);
void diag_hex_u32(uint32_t value);

#endif /* DIAGNOSTICS_H */
