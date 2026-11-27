/**
 * @file uart_logger.c
 * @brief Multi-channel UART implementation for Renesas BGK-RA6E2.
 */

#include "uart_logger.h"
#include "timer_measurement.h"
#include "hal_data.h"
#include "r_ioport.h"
#include <stddef.h>

/* Pin Definitions */
#define PIN_411_HIGH() (R_PORT4->PCNTR3 = (1UL << 11))
#define PIN_411_LOW()  (R_PORT4->PCNTR3 = (1UL << (11 + 16)))

#define PIN_109_HIGH() (R_PORT1->PCNTR3 = (1UL << 9))
#define PIN_109_LOW()  (R_PORT1->PCNTR3 = (1UL << (9 + 16)))

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

    /* 1. Configure Hardware SCI9 for J-Link CDC UART (P109 TXD9) */
    R_BSP_MODULE_START(FSP_IP_SCI, 9);
    R_SCI9->SCR = 0;
    R_SCI9->FCR = 0;
    R_SCI9->SMR = 0;       /* Asynchronous, 8N1 */
    R_SCI9->SCMR = 0xF2U;  /* Eight bits, LSB first */
    R_SCI9->SEMR = R_SCI0_SEMR_BGDM_Msk;
    uint32_t clock = R_FSP_SystemClockHzGet(BSP_FEATURE_SCI_CLOCK);
    uint32_t divisor = 16U * (baud_rate > 0U ? baud_rate : 115200U);
    if (divisor > 0)
    {
        R_SCI9->BRR = (uint8_t) ((clock + divisor / 2U) / divisor - 1U);
    }
    R_BSP_SoftwareDelay(10U, BSP_DELAY_UNITS_MICROSECONDS);
    R_SCI9->SCR = R_SCI0_SCR_TE_Msk;

    /* 2. Configure P411 (Arduino D1) and P109 (PMOD1 / J-Link TX) as GPIO Output High */
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(BSP_IO_PORT_04_PIN_11, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH));
    R_BSP_PinCfg(BSP_IO_PORT_01_PIN_09, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH));
    R_BSP_PinAccessDisable();

    PIN_411_HIGH();
    PIN_109_HIGH();
}

void uart_logger_putc(char c)
{
    /* 1. Send to SEGGER RTT Viewer */
    rtt_putc(c);

    /* 2. Send to Hardware SCI9 (J-Link CDC UART) */
    for (uint32_t i = 0; i < 5000U; ++i)
    {
        if (R_SCI9->SSR & R_SCI0_SSR_TDRE_Msk)
        {
            R_SCI9->TDR = (uint8_t)c;
            break;
        }
    }

    /* 3. Send via precision bitbang on P411 (Arduino D1) and P109 */
    uint32_t start = timer_get_cycles();

    /* Start bit: LOW */
    PIN_411_LOW();
    PIN_109_LOW();
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
            PIN_109_HIGH();
        }
        else
        {
            PIN_411_LOW();
            PIN_109_LOW();
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
    PIN_109_HIGH();
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

void uart_logger_print_i32(int32_t val)
{
    if (val < 0)
    {
        uart_logger_putc('-');
        val = -val;
    }
    uart_logger_print_u32((uint32_t)val);
}

void uart_logger_print_i16(int16_t val)
{
    uart_logger_print_i32((int32_t)val);
}

void uart_logger_print_hex8(uint8_t val)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    uart_logger_print("0x");
    uart_logger_putc(hex_digits[(val >> 4) & 0x0F]);
    uart_logger_putc(hex_digits[val & 0x0F]);
}

void uart_logger_print_float(float val, uint8_t decimals)
{
    if (val < 0.0f)
    {
        uart_logger_putc('-');
        val = -val;
    }

    /* Rounding according to decimals */
    float round_offset = 0.5f;
    for (uint8_t i = 0; i < decimals; i++)
    {
        round_offset /= 10.0f;
    }
    val += round_offset;

    uint32_t int_part = (uint32_t)val;
    uart_logger_print_u32(int_part);

    if (decimals == 0)
    {
        return;
    }

    uart_logger_putc('.');

    float frac_part = val - (float)int_part;
    for (uint8_t i = 0; i < decimals; i++)
    {
        frac_part *= 10.0f;
        uint32_t digit = (uint32_t)frac_part;
        if (digit > 9)
        {
            digit = 9;
        }
        uart_logger_putc((char)('0' + digit));
        frac_part -= (float)digit;
    }
}
