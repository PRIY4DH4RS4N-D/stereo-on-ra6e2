/**
 * @file uart_protocol.c
 * @brief Robust line-based JSON communication protocol implementation.
 */

#include "uart_protocol.h"
#include "timer_measurement.h"
#include "hal_data.h"
#include "r_ioport.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define PIN_411_HIGH() (R_PORT4->PCNTR3 = (1UL << 11))
#define PIN_411_LOW()  (R_PORT4->PCNTR3 = (1UL << (11 + 16)))

static uint32_t s_bit_cycles = 1736U; /* 200 MHz / 115200 */
static char s_rx_buf[UART_MAX_LINE_LENGTH];
static size_t s_rx_len = 0;

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

void uart_protocol_init(uint32_t baud_rate)
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

    /* 1. Configure Hardware SCI0 for bidirectional J-Link CDC UART (COM port) */
    R_BSP_MODULE_START(FSP_IP_SCI, 0);
    R_SCI0->SCR = 0;
    R_SCI0->FCR = 0xF800U; /* Disable FIFO for direct polled access */
    R_SCI0->SMR = 0;       /* Asynchronous, 8N1, no clock prescaler */
    R_SCI0->SCMR = 0xF2U;  /* Eight bits, LSB first, normal polarity */
    R_SCI0->SEMR = R_SCI0_SEMR_BGDM_Msk;
    uint32_t clock = R_FSP_SystemClockHzGet(BSP_FEATURE_SCI_CLOCK);
    uint32_t divisor = 16U * (baud_rate > 0U ? baud_rate : 115200U);
    R_SCI0->BRR = (uint8_t) ((clock + divisor / 2U) / divisor - 1U);
    R_BSP_SoftwareDelay(10U, BSP_DELAY_UNITS_MICROSECONDS);
    /* Enable both Transmitter (TE) and Receiver (RE) */
    R_SCI0->SCR = (uint8_t)(R_SCI0_SCR_TE_Msk | R_SCI0_SCR_RE_Msk);

    /* 2. Configure P411 as GPIO Output High for Arduino D1 UART TX */
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(BSP_IO_PORT_04_PIN_11, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH));
    R_BSP_PinAccessDisable();

    PIN_411_HIGH();

    s_rx_len = 0;
}

static inline void uart_tx_char(char c)
{
    /* 1. Output to SEGGER RTT Viewer */
    rtt_putc(c);

    /* 2. Output to Hardware SCI0 */
    for (uint32_t i = 0; i < 50000U; ++i)
    {
        if (R_SCI0->SSR & R_SCI0_SSR_TDRE_Msk)
        {
            R_SCI0->TDR = (uint8_t)c;
            break;
        }
    }

    /* 3. Output to P411 (Arduino D1 Header) */
    uint32_t start = timer_get_cycles();
    PIN_411_LOW();
    while ((timer_get_cycles() - start) < s_bit_cycles)
    {
        __NOP();
    }
    start += s_bit_cycles;

    uint8_t data = (uint8_t)c;
    for (int i = 0; i < 8; i++)
    {
        if (data & 1U)
        {
            PIN_411_HIGH();
        }
        else
        {
            PIN_411_LOW();
        }
        data >>= 1;

        while ((timer_get_cycles() - start) < s_bit_cycles)
        {
            __NOP();
        }
        start += s_bit_cycles;
    }

    PIN_411_HIGH();
    while ((timer_get_cycles() - start) < s_bit_cycles)
    {
        __NOP();
    }
}

void uart_protocol_send_raw(const char *str)
{
    if (str == NULL)
    {
        return;
    }
    while (*str)
    {
        uart_tx_char(*str++);
    }
}

static inline bool uart_rx_char(char *p_c)
{
    /* Check for and recover from any framing/overrun/parity errors */
    if (R_SCI0->SSR & (R_SCI0_SSR_ORER_Msk | R_SCI0_SSR_FER_Msk | R_SCI0_SSR_PER_Msk))
    {
        /* To clear receive errors on RA SCI, toggle RE and reset error bits */
        R_SCI0->SCR = (uint8_t)(R_SCI0->SCR & ~R_SCI0_SCR_RE_Msk);
        R_SCI0->SSR = 0xC0U; /* Clear ORER, FER, PER */
        R_SCI0->SCR = (uint8_t)(R_SCI0->SCR | R_SCI0_SCR_RE_Msk);
    }

    if (R_SCI0->SSR & R_SCI0_SSR_RDRF_Msk)
    {
        *p_c = (char)R_SCI0->RDR;
        return true;
    }
    return false;
}

static bool parse_json_command(const char *json_str, uart_command_t *p_cmd)
{
    if (json_str == NULL || p_cmd == NULL)
    {
        return false;
    }

    p_cmd->type = UART_CMD_INVALID;
    p_cmd->request_id = 0;

    /* Check for "cmd":"MEASURE" */
    const char *cmd_key = strstr(json_str, "\"cmd\"");
    if (cmd_key == NULL)
    {
        return false;
    }

    if (strstr(cmd_key, "\"MEASURE\"") != NULL)
    {
        p_cmd->type = UART_CMD_MEASURE;

        /* Extract request_id */
        const char *req_key = strstr(json_str, "\"request_id\"");
        if (req_key != NULL)
        {
            const char *colon = strchr(req_key, ':');
            if (colon != NULL)
            {
                p_cmd->request_id = (uint32_t)strtoul(colon + 1, NULL, 10);
            }
        }
        return true;
    }
    else if (strstr(cmd_key, "\"PING\"") != NULL)
    {
        p_cmd->type = UART_CMD_PING;
        return true;
    }
    else if (strstr(cmd_key, "\"RESET\"") != NULL)
    {
        p_cmd->type = UART_CMD_RESET;
        return true;
    }

    return false;
}

bool uart_protocol_poll_command(uart_command_t *p_cmd)
{
    char c = 0;
    while (uart_rx_char(&c))
    {
        if (c == '\r' || c == '\n')
        {
            if (s_rx_len > 0)
            {
                s_rx_buf[s_rx_len] = '\0';
                s_rx_len = 0;
                return parse_json_command(s_rx_buf, p_cmd);
            }
        }
        else
        {
            if (s_rx_len < (sizeof(s_rx_buf) - 1))
            {
                s_rx_buf[s_rx_len++] = c;
            }
            else
            {
                /* Buffer overflow protection: reset line */
                s_rx_len = 0;
            }
        }
    }
    return false;
}

void uart_protocol_send_measurement(uint32_t request_id,
                                    int32_t distance_mm,
                                    uint32_t echo_us,
                                    const char *status)
{
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"measurement\",\"request_id\":%lu,\"distance_mm\":%ld,\"echo_us\":%lu,\"status\":\"%s\"}\r\n",
             (unsigned long)request_id,
             (long)distance_mm,
             (unsigned long)echo_us,
             status ? status : "error");
    uart_protocol_send_raw(buf);
}

void uart_protocol_send_pong(void)
{
    uart_protocol_send_raw("{\"type\":\"pong\",\"status\":\"ready\"}\r\n");
}

void uart_protocol_send_error(const char *error_msg)
{
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"error\",\"message\":\"%s\"}\r\n",
             error_msg ? error_msg : "unknown");
    uart_protocol_send_raw(buf);
}
