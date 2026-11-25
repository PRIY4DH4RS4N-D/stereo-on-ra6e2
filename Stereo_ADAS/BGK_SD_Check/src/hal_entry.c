#include "hal_data.h"
#include "diagnostics.h"
#include "sd_check.h"

void hal_entry(void)
{
    diag_uart_init();
    diag_text("\nBGK-RA6E2 read-only SD check | 115200 8N1\n");
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_04, BSP_IO_LEVEL_HIGH);
    sd_check_run();
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_04, BSP_IO_LEVEL_LOW);

    bool passed = g_sd_check_result > 0;
    for (;;)
    {
        sd_check_report();
        for (unsigned i = 0; i < 10; ++i)
        {
            bsp_io_level_t led = passed || (i % 2U == 0) ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW;
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_12, led);
            R_BSP_SoftwareDelay(500U, BSP_DELAY_UNITS_MILLISECONDS);
        }
    }
}
