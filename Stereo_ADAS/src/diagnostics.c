#include "diagnostics.h"
#include "hal_data.h"

#define DIAG_BAUD 115200U
#define SCI_BAUD_DIVISOR 16U

void diag_uart_init(void)
{
    /* SCI0 TXD0 on P101 is selected by the BGK pin configuration. */
    R_BSP_MODULE_START(FSP_IP_SCI, 0);
    R_SCI0->SCR = 0;
    R_SCI0->FCR = 0xF800U; /* Disable FIFO for polled TDR access. */
    R_SCI0->SMR = 0;      /* Asynchronous, 8N1, no clock prescaler. */
    R_SCI0->SCMR = 0xF2U;  /* Eight bits, LSB first, normal polarity. */
    R_SCI0->SEMR = R_SCI0_SEMR_BGDM_Msk;
    uint32_t clock = R_FSP_SystemClockHzGet(BSP_FEATURE_SCI_CLOCK);
    uint32_t divisor = SCI_BAUD_DIVISOR * DIAG_BAUD;
    R_SCI0->BRR = (uint8_t) ((clock + divisor / 2U) / divisor - 1U);
    R_BSP_SoftwareDelay(10U, BSP_DELAY_UNITS_MICROSECONDS);
    R_SCI0->SCR = R_SCI0_SCR_TE_Msk;
}

static void diag_char(char character)
{
    /* Release the caller even if the peripheral stalls. */
    for (uint32_t i = 0; i < 100000U; ++i)
    {
        if (R_SCI0->SSR & R_SCI0_SSR_TDRE_Msk)
        {
            R_SCI0->TDR = (uint8_t) character;
            return;
        }
    }
}

void diag_text(const char *text)
{
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
    char digits[10];
    unsigned count = 0;
    do
    {
        digits[count++] = (char) ('0' + value % 10U);
        value /= 10U;
    } while (value != 0);
    while (count > 0)
    {
        diag_char(digits[--count]);
    }
}

void diag_hex_byte(uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    diag_text("0x");
    diag_char(hex[value >> 4]);
    diag_char(hex[value & 0xFU]);
}
