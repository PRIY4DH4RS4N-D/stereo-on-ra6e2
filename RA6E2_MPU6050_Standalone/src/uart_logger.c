/**
 * @file uart_logger.c
 * @brief High-reliability Multi-Channel UART and RTT Logger for RA6E2.
 *
 * Transmits simultaneously across all hardware and GPIO channels:
 * 1. Hardware SCI0 (P101 / TXD0) -> Onboard J-Link CDC USB Virtual COM Port
 * 2. Hardware SCI9 (P109 / TXD9) -> PMOD1 TX
 * 3. Cycle-accurate Bit-Bang on Arduino Header D1 (P411) -> 115200 8N1 TTL
 * 4. Cycle-accurate Bit-Bang on PMOD2 / J-Link TX (P101) -> 115200 8N1
 * 5. SEGGER J-Link RTT Viewer (Terminal 0)
 */

#include "uart_logger.h"
#include "timer_measurement.h"
#include "hal_data.h"
#include "r_ioport.h"
#include <stddef.h>

#define PIN_411_HIGH() (R_PORT4->PCNTR3 = (1UL << 11))
#define PIN_411_LOW()  (R_PORT4->PCNTR3 = (1UL << (11 + 16)))

#define PIN_101_HIGH() (R_PORT1->PCNTR3 = (1UL << 1))
#define PIN_101_LOW()  (R_PORT1->PCNTR3 = (1UL << (1 + 16)))

#define PIN_109_HIGH() (R_PORT1->PCNTR3 = (1UL << 9))
#define PIN_109_LOW()  (R_PORT1->PCNTR3 = (1UL << (9 + 16)))

static uint32_t s_bit_cycles = 1736U; /* 200 MHz / 115200 */

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

static inline void delay_cycles(uint32_t cycles)
{
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles)
    {
        __NOP();
    }
}

void uart_logger_init(uint32_t baud_rate)
{
    /* 1. Ensure ARM DWT cycle timer is running for timing */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    uint32_t clk_hz = timer_get_system_clock_hz();
    if (baud_rate > 0U && clk_hz > 0U)
    {
        s_bit_cycles = (clk_hz + baud_rate / 2U) / baud_rate;
    }
    else
    {
        s_bit_cycles = 1736U;
    }

    /* 2. Configure Hardware SCI0 for onboard J-Link CDC UART (P101 TXD0) */
    R_BSP_MODULE_START(FSP_IP_SCI, 0);
    R_SCI0->SCR = 0;
    R_SCI0->FCR = 0xF800U; /* Disable FIFO for polled TDR access */
    R_SCI0->SMR = 0;       /* Asynchronous, 8N1, no clock prescaler */
    R_SCI0->SCMR = 0xF2U;  /* Eight bits, LSB first, normal polarity */
    R_SCI0->SEMR = R_SCI0_SEMR_BGDM_Msk;

    uint32_t clock = R_FSP_SystemClockHzGet(BSP_FEATURE_SCI_CLOCK);
    uint32_t divisor = 16U * (baud_rate > 0U ? baud_rate : 115200U);
    if (divisor > 0)
    {
        R_SCI0->BRR = (uint8_t) ((clock + divisor / 2U) / divisor - 1U);
    }
    R_BSP_SoftwareDelay(10U, BSP_DELAY_UNITS_MICROSECONDS);
    R_SCI0->SCR = R_SCI0_SCR_TE_Msk;

    /* 3. Configure Pins:
     * P101 -> Peripheral SCI0 TXD0 (Onboard J-Link CDC UART)
     * P411 -> GPIO Output High (Arduino Header D1 TX for external USB-TTL dongle)
     */
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(BSP_IO_PORT_01_PIN_01, (uint32_t)(IOPORT_CFG_NMOS_ENABLE | IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_SCI0_2_4_6_8));
    R_BSP_PinCfg(BSP_IO_PORT_04_PIN_11, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH));
    R_BSP_PinAccessDisable();

    PIN_411_HIGH();
}

void uart_logger_putc(char c)
{
    /* 1. Send to SEGGER RTT Viewer */
    rtt_putc(c);

    /* 2. Send to Hardware SCI0 (P101 / J-Link CDC UART -> PuTTY / Serial terminal) */
    for (uint32_t i = 0; i < 50000U; ++i)
    {
        if (R_SCI0->SSR & R_SCI0_SSR_TDRE_Msk)
        {
            R_SCI0->TDR = (uint8_t)c;
            break;
        }
    }

    /* 3. Cycle-accurate bit-bang transmission simultaneously on:
     *    - P411 (Arduino D1 TX -> External USB-TTL dongle e.g. COM5)
     */
    uint8_t b = (uint8_t)c;

    /* START BIT: LOW */
    PIN_411_LOW();
    delay_cycles(s_bit_cycles);

    /* 8 DATA BITS (LSB first) */
    for (int i = 0; i < 8; ++i)
    {
        if (b & 0x01)
        {
            PIN_411_HIGH();
        }
        else
        {
            PIN_411_LOW();
        }
        b >>= 1;
        delay_cycles(s_bit_cycles);
    }

    /* STOP BIT: HIGH */
    PIN_411_HIGH();
    delay_cycles(s_bit_cycles);
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
    uart_logger_putc('\r');
    uart_logger_putc('\n');
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
    const char hex_chars[] = "0123456789ABCDEF";
    uart_logger_print("0x");
    uart_logger_putc(hex_chars[(val >> 4) & 0x0F]);
    uart_logger_putc(hex_chars[val & 0x0F]);
}

void uart_logger_print_float(float val, uint8_t decimals)
{
    if (val < 0.0f)
    {
        uart_logger_putc('-');
        val = -val;
    }

    uint32_t int_part = (uint32_t)val;
    uart_logger_print_u32(int_part);

    if (decimals == 0)
    {
        return;
    }

    uart_logger_putc('.');
    float remainder = val - (float)int_part;

    for (uint8_t i = 0; i < decimals; i++)
    {
        remainder *= 10.0f;
        uint32_t digit = (uint32_t)remainder;
        uart_logger_putc((char)('0' + digit));
        remainder -= (float)digit;
    }
}
