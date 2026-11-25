#include "diagnostics.h"
#include "hal_data.h"

#define DIAG_BAUD 115200U

/* Hardware pins for UART:
 * P411: Arduino D1 (TX) - standard UART TX on Arduino header
 * P101: PMOD2 SDA / SCI0 TXD0
 */
#define PIN_411_HIGH() (R_PORT4->PCNTR3 = (1UL << 11))
#define PIN_411_LOW()  (R_PORT4->PCNTR3 = (1UL << (11 + 16)))

#define PIN_101_HIGH() (R_PORT1->PCNTR3 = (1UL << 1))
#define PIN_101_LOW()  (R_PORT1->PCNTR3 = (1UL << (1 + 16)))

static uint32_t s_bit_cycles = 1736U; /* Default for 200 MHz CPU at 115200 baud */

/* --- SEGGER RTT Support (for J-Link RTT Viewer) --- */
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
    s_rtt_up_buf[wr] = c;
    _SEGGER_RTT.aUp[0].WrOff = next_wr;
}

static inline void delay_cycles(uint32_t cycles)
{
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles)
    {
    }
}

void diag_uart_init(void)
{
    /* Enable DWT Cycle Counter on Cortex-M33 for exact microsecond timing */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* Calculate exact clock cycles per bit for 115200 baud */
    uint32_t cpu_hz = SystemCoreClock;
    if (cpu_hz == 0)
    {
        cpu_hz = 200000000U;
    }
    s_bit_cycles = (cpu_hz + DIAG_BAUD / 2U) / DIAG_BAUD;

    /* Configure P411 (Arduino D1 TX) as GPIO Output, High (Idle) */
    R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_11,
                    IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH);

    /* Configure P101 as GPIO Output, High (Idle) */
    R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_01,
                    IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH);

    PIN_411_HIGH();
    PIN_101_HIGH();

    /* Optional: Also try enabling hardware SCI0 on P101 */
    R_BSP_MODULE_START(FSP_IP_SCI, 0);
    R_SCI0->SCR = 0;
    R_SCI0->FCR = 0;
    R_SCI0->SMR = 0;
    R_SCI0->SCMR = 0xF2U;
    R_SCI0->SEMR = R_SCI0_SEMR_BGDM_Msk;
    uint32_t sci_clk = R_FSP_SystemClockHzGet(BSP_FEATURE_SCI_CLOCK);
    uint32_t div = 16U * DIAG_BAUD;
    if (sci_clk > 0)
    {
        R_SCI0->BRR = (uint8_t) ((sci_clk + div / 2U) / div - 1U);
    }
    R_BSP_SoftwareDelay(10U, BSP_DELAY_UNITS_MICROSECONDS);
    R_SCI0->SCR = R_SCI0_SCR_TE_Msk;
}

void diag_char(char character)
{
    /* Send to SEGGER RTT for J-Link RTT Viewer */
    rtt_putc(character);

    uint8_t b = (uint8_t) character;

    /* 1. Try sending to hardware SCI0 */
    if (R_SCI0->SSR & R_SCI0_SSR_TDRE_Msk)
    {
        R_SCI0->TDR = b;
    }

    /* 2. Cycle-accurate bit-bang transmission on P411 (Arduino D1) and P101 */
    /* START BIT: LOW */
    PIN_411_LOW();
    PIN_101_LOW();
    delay_cycles(s_bit_cycles);

    /* 8 DATA BITS (LSB first) */
    for (int i = 0; i < 8; ++i)
    {
        if (b & 0x01)
        {
            PIN_411_HIGH();
            PIN_101_HIGH();
        }
        else
        {
            PIN_411_LOW();
            PIN_101_LOW();
        }
        b >>= 1;
        delay_cycles(s_bit_cycles);
    }

    /* STOP BIT: HIGH */
    PIN_411_HIGH();
    PIN_101_HIGH();
    delay_cycles(s_bit_cycles);
}

void diag_text(const char *text)
{
    if (text == NULL)
    {
        return;
    }
    while (*text != '\0')
    {
        if (*text == '\n')
        {
            diag_char('\r');
        }
        diag_char(*text++);
    }
}

void diag_u32(uint32_t value)
{
    char buf[12];
    int i = 0;
    if (value == 0)
    {
        diag_char('0');
        return;
    }
    while (value > 0)
    {
        buf[i++] = (char) ('0' + (value % 10));
        value /= 10;
    }
    while (i > 0)
    {
        diag_char(buf[--i]);
    }
}

void diag_u64(uint64_t value)
{
    char buf[24];
    int i = 0;
    if (value == 0)
    {
        diag_char('0');
        return;
    }
    while (value > 0)
    {
        buf[i++] = (char) ('0' + (value % 10));
        value /= 10;
    }
    while (i > 0)
    {
        diag_char(buf[--i]);
    }
}

void diag_hex_byte(uint8_t byte)
{
    const char hex[] = "0123456789ABCDEF";
    diag_text("0x");
    diag_char(hex[(byte >> 4) & 0x0F]);
    diag_char(hex[byte & 0x0F]);
}

void diag_hex_u32(uint32_t val)
{
    const char hex[] = "0123456789ABCDEF";
    diag_text("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
    {
        diag_char(hex[(val >> shift) & 0x0F]);
    }
}
