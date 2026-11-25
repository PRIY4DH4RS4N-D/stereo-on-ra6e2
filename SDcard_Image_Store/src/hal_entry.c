#include "hal_data.h"
#include "diagnostics.h"
#include "image_loader.h"
#include "sd_spi.h"

/* Onboard LEDs on Renesas BGK-RA6E2:
 * LED1: P104 (Port 1, Pin 4)
 * LED2: P112 (Port 1, Pin 12)
 */
#define LED1_PIN BSP_IO_PORT_01_PIN_04
#define LED2_PIN BSP_IO_PORT_01_PIN_12

#if (1 == BSP_MULTICORE_PROJECT) && BSP_TZ_SECURE_BUILD
bsp_ipc_semaphore_handle_t g_core_start_semaphore =
{
    .semaphore_num = 0
};
#endif

void hal_entry(void)
{
    /* Initialize UART & RTT diagnostics */
    diag_uart_init();

    /* Configure both LED pins as outputs */
    R_IOPORT_PinCfg(&g_ioport_ctrl, LED1_PIN, IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW);
    R_IOPORT_PinCfg(&g_ioport_ctrl, LED2_PIN, IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW);

    R_BSP_SoftwareDelay(100U, BSP_DELAY_UNITS_MILLISECONDS);

    uint32_t attempt_count = 1;
    bool image_loaded = false;

    for (;;)
    {
        diag_text("\r\n\r\n");
        diag_text("****************************************************************\r\n");
        diag_text("*       Renesas BGK-RA6E2 SD Card Image Storage System         *\r\n");
        diag_text("*         Loads 'D:\\LEFT\\F0000001.PNG' directly to RAM         *\r\n");
        diag_text("****************************************************************\r\n");
        diag_text("Attempt #");
        diag_u32(attempt_count++);
        diag_text(" | Target: R7FA6E2BB (ARM Cortex-M33) | 115200 8N1\r\n");

        sd_spi_reset();
        image_loaded = image_loader_run();

        if (image_loaded)
        {
            /* =========================================================
             * IMAGE SUCCESSFULLY LOADED & STORED IN MCU RAM:
             * BOTH LEDs (LED1 and LED2) TURNED ON CONTINUOUSLY!
             * ========================================================= */
            R_IOPORT_PinWrite(&g_ioport_ctrl, LED1_PIN, BSP_IO_LEVEL_HIGH);
            R_IOPORT_PinWrite(&g_ioport_ctrl, LED2_PIN, BSP_IO_LEVEL_HIGH);

            diag_text("[LED STATUS] LED1 = ON, LED2 = ON (BOTH LEDs Solid ON)\r\n");
            diag_text("[SUCCESS] Image is residing in board memory. System Ready.\r\n");

            /* Keep LEDs Solid ON and periodically log status */
            for (;;)
            {
                R_IOPORT_PinWrite(&g_ioport_ctrl, LED1_PIN, BSP_IO_LEVEL_HIGH);
                R_IOPORT_PinWrite(&g_ioport_ctrl, LED2_PIN, BSP_IO_LEVEL_HIGH);
                R_BSP_SoftwareDelay(2000U, BSP_DELAY_UNITS_MILLISECONDS);
            }
        }
        else
        {
            /* If image loading failed: LED1 Solid ON, LED2 OFF */
            R_IOPORT_PinWrite(&g_ioport_ctrl, LED1_PIN, BSP_IO_LEVEL_HIGH);
            R_IOPORT_PinWrite(&g_ioport_ctrl, LED2_PIN, BSP_IO_LEVEL_LOW);

            diag_text("\r\n[LED STATUS] NOT LOADED -> LED1 Solid ON, LED2 OFF\r\n");
            diag_text("[WAIT] Retrying in 3 seconds...\r\n\r\n");

            for (int i = 0; i < 30; i++)
            {
                R_IOPORT_PinWrite(&g_ioport_ctrl, LED1_PIN, BSP_IO_LEVEL_HIGH);
                R_IOPORT_PinWrite(&g_ioport_ctrl, LED2_PIN, BSP_IO_LEVEL_LOW);
                R_BSP_SoftwareDelay(100U, BSP_DELAY_UNITS_MILLISECONDS);
            }
        }
    }
}
