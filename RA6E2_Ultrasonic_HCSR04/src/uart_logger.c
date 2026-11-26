/**
 * @file uart_logger.c
 * @brief High-reliability UART and RTT logging implementation for RA6E2.
 */

#include "uart_logger.h"
#include "timer_measurement.h"
#include "bsp_api.h"
#include "r_ioport.h"
#include <stddef.h>

/* Hardware pin bit manipulation:
 * P411: Arduino Header D1 (TX) - standard UART TX on Arduino header
 * P101: PMOD2 Pin 6 / SCI0 TXD0
 */
#define PIN_411_HIGH() (R_PORT4->PCNTR3 = (1UL << 11))
#define PIN_411_LOW()  (R_PORT4->PCNTR3 = (1UL << (11 + 16)))

#define PIN_101_HIGH() (R_PORT1->PCNTR3 = (1UL << 1))
#define PIN_101_LOW()  (R_PORT1->PCNTR3 = (1UL << (1 + 16)))

static uint32_t s_bit_cycles = 1736U; /* Default: 200 MHz CPU / 115200 baud */

/* --- SEGGER RTT Buffers & Control Block --- */
typedef struct {
  const char* pName;
  char*       pBuffer;
  unsigned    SizeOfBuffer;
  volatile unsigned WrOff;
  volatile unsigned RdOff;
  unsigned    Flags;
} SEGGER_RTT_BUFFER_UP;

typedef struct {
  const char* pName;
  char*       pBuffer;
  unsigned    SizeOfBuffer;
  volatile unsigned WrOff;
  volatile unsigned RdOff;
  unsigned    Flags;
} SEGGER_RTT_BUFFER_DOWN;

typedef struct {
  char                    acID[16];
  int                     MaxNumUpBuffers;
  int                     MaxNumDownBuffers;
  SEGGER_RTT_BUFFER_UP    aUp[1];
  SEGGER_RTT_BUFFER_DOWN  aDown[1];
} SEGGER_RTT_CB;

static char s_rtt_up_buf[1024];

__attribute__((used))
SEGGER_RTT_CB _SEGGER_RTT = {
  "SEGGER RTT\0\0\0\0\0\0",
  1,
  1,
  { { "Terminal", s_rtt_up_buf, sizeof(s_rtt_up_buf), 0, 0, 0 } },
  { { "Terminal", NULL, 0, 0, 0, 0 } }
};

static inline void rtt_putc(char c)
{
    unsigned wr = _SEGGER_RTT.aUp[0].WrOff;
    unsigned next_wr = wr + 1;
    if (next_wr >= sizeof(s_rtt_up_buf))
    {
        next_wr = 0;
    }
    if (next_wr != _SEGGER_RTT.aUp[0].RdOff)
    {
        s_rtt_up_buf[wr] = c;
        _SEGGER_RTT.aUp[0].WrOff = next_wr;
    }
}

void uart_logger_init(uint32_t baud_rate)
{
    uint32_t clk_hz = timer_get_system_clock_hz();
    if (baud_rate > 0U && clk_hz > 0U)
    {
        s_bit_cycles = clk_hz / baud_rate;
    }
    else
    {
        s_bit_cycles = 1736U;
    }

    /* Configure P411 as GPIO Output High (UART idle state) */
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(BSP_IO_PORT_04_PIN_11, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH));
    R_BSP_PinCfg(BSP_IO_PORT_01_PIN_01, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH));
    R_BSP_PinAccessDisable();

    PIN_411_HIGH();
    PIN_101_HIGH();
}

void uart_logger_putc(char c)
{
    /* Output to SEGGER RTT Viewer */
    rtt_putc(c);

    /* Output via precision bitbang UART on P411 (Arduino D1) and P101 */
    uint32_t start = timer_get_cycles();

    /* Start bit: LOW */
    PIN_411_LOW();
    PIN_101_LOW();
    while ((timer_get_cycles() - start) < s_bit_cycles)
    {
        __NOP();
    }
    start += s_bit_cycles;

    /* 8 Data bits, LSB first */
    uint8_t data = (uint8_t)c;
    for (int i = 0; i < 8; i++)
    {
        if (data & 1U)
        {
            PIN_411_HIGH();
            PIN_101_HIGH();
        }
        else
        {
            PIN_411_LOW();
            PIN_101_LOW();
        }
        data >>= 1;

        while ((timer_get_cycles() - start) < s_bit_cycles)
        {
            __NOP();
        }
        start += s_bit_cycles;
    }

    /* Stop bit: HIGH */
    PIN_411_HIGH();
    PIN_101_HIGH();
    while ((timer_get_cycles() - start) < s_bit_cycles)
    {
        __NOP();
    }
}

void uart_logger_print(const char *str)
{
    if (str == NULL)
    {
        return;
    }
    while (*str)
    {
        if (*str == '\n')
        {
            uart_logger_putc('\r');
        }
        uart_logger_putc(*str++);
    }
}

void uart_logger_println(const char *str)
{
    uart_logger_print(str);
    uart_logger_print("\r\n");
}

void uart_logger_print_u32(uint32_t val)
{
    char buf[12];
    int idx = 0;

    if (val == 0)
    {
        uart_logger_putc('0');
        return;
    }

    while (val > 0)
    {
        buf[idx++] = (char)('0' + (val % 10));
        val /= 10;
    }

    while (idx > 0)
    {
        uart_logger_putc(buf[--idx]);
    }
}

void uart_logger_print_float(float val, uint8_t decimals)
{
    if (val < 0.0f)
    {
        uart_logger_putc('-');
        val = -val;
    }

    /* Integer part */
    uint32_t int_part = (uint32_t)val;
    uart_logger_print_u32(int_part);

    if (decimals == 0)
    {
        return;
    }

    uart_logger_putc('.');

    /* Fractional part */
    float frac_part = val - (float)int_part;
    for (uint8_t i = 0; i < decimals; i++)
    {
        frac_part *= 10.0f;
        uint32_t digit = (uint32_t)frac_part;
        uart_logger_putc((char)('0' + digit));
        frac_part -= (float)digit;
    }
}

void uart_logger_print_banner(void)
{
    uart_logger_println("=================================");
    uart_logger_println("RA6E2 ULTRASONIC DISTANCE SYSTEM ");
    uart_logger_println("=================================");
}

void uart_logger_print_measurement(const char *sensor_status,
                                  float distance_cm,
                                  uint32_t echo_time_us,
                                  const char *measurement_str,
                                  bool is_valid)
{
    uart_logger_print_banner();
    
    uart_logger_print("Sensor Status : ");
    uart_logger_println(sensor_status ? sensor_status : "UNKNOWN");

    uart_logger_print("Distance      : ");
    if (is_valid)
    {
        uart_logger_print_float(distance_cm, 2);
        uart_logger_println(" cm");
    }
    else
    {
        uart_logger_println("INVALID");
    }

    uart_logger_print("Echo Time     : ");
    uart_logger_print_u32(echo_time_us);
    uart_logger_println(" us");

    uart_logger_print("Measurement   : ");
    uart_logger_println(measurement_str ? measurement_str : "");
    uart_logger_println("");
}
