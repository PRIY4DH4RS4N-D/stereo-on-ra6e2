#include "hal_data.h"
#include "diagnostics.h"
#include "sd_test.h"
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
    /* Initialize UART diagnostics on SCI0 (115200 8N1 via onboard J-Link CDC UART on P101) */
    diag_uart_init();

    /* Configure both LED pins as outputs */
    R_IOPORT_PinCfg(&g_ioport_ctrl, LED1_PIN, IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW);
    R_IOPORT_PinCfg(&g_ioport_ctrl, LED2_PIN, IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW);

    R_BSP_SoftwareDelay(100U, BSP_DELAY_UNITS_MILLISECONDS);

    uint32_t attempt_count = 1;
    bool sd_detected_and_tested = false;

    for (;;)
    {
        diag_text("\r\n\r\n");
        diag_text("****************************************************************\r\n");
        diag_text("*         Renesas BGK-RA6E2 MicroSD Card SPI Test              *\r\n");
        diag_text("*            Ported from ESP32 SD.h / SPI.h Setup              *\r\n");
        diag_text("****************************************************************\r\n");
        diag_text("Attempt #");
        diag_u32(attempt_count++);
        diag_text(" | Target: R7FA6E2BB | Baud: 115200 8N1\r\n");

        sd_spi_reset();
        sd_detected_and_tested = sd_test_run();

        if (sd_detected_and_tested)
        {
            /* =========================================================
             * SD CARD DETECTED:
             * BOTH LEDs (LED1 and LED2) TURNED ON CONTINUOUSLY!
             * ========================================================= */
            R_IOPORT_PinWrite(&g_ioport_ctrl, LED1_PIN, BSP_IO_LEVEL_HIGH);
            R_IOPORT_PinWrite(&g_ioport_ctrl, LED2_PIN, BSP_IO_LEVEL_HIGH);

            diag_text("\r\n=======================================================\r\n");
            diag_text(" >>> SD CARD DETECTED! BOTH LEDs ARE NOW SOLID ON! <<< \r\n");
            diag_text("=======================================================\r\n");
            diag_text("[LED STATUS] LED1 = ON, LED2 = ON (BOTH LEDs Solid ON)\r\n");
            diag_text("[INFO] SD Card is fully working! Test complete.\r\n");

            /* Stay with both LEDs continuously ON */
            for (;;)
            {
                R_IOPORT_PinWrite(&g_ioport_ctrl, LED1_PIN, BSP_IO_LEVEL_HIGH);
                R_IOPORT_PinWrite(&g_ioport_ctrl, LED2_PIN, BSP_IO_LEVEL_HIGH);
                R_BSP_SoftwareDelay(1000U, BSP_DELAY_UNITS_MILLISECONDS);
            }
        }
        else
        {
            /* =========================================================
             * SD CARD NOT DETECTED:
             * LED1 TURNED ON CONTINUOUSLY (SOLID ON)!
             * LED2 REMAINS OFF (NO BLINKING)!
             * ========================================================= */
            R_IOPORT_PinWrite(&g_ioport_ctrl, LED1_PIN, BSP_IO_LEVEL_HIGH); /* LED1 Solid ON */
            R_IOPORT_PinWrite(&g_ioport_ctrl, LED2_PIN, BSP_IO_LEVEL_LOW);  /* LED2 OFF (No blinking) */

            diag_text("\r\n[LED STATUS] SD CARD NOT DETECTED -> LED1 is SOLID ON, LED2 is OFF\r\n");
            diag_text("[TROUBLESHOOTING CHECKLIST]:\r\n");
            diag_text("  1. SD VCC  -> Connect to 5V on the Arduino power header (CN1).\r\n");
            diag_text("  2. SD GND  -> Connect to PMOD1 pin 5 (or GND header).\r\n");
            diag_text("  3. SD CS   -> Connect to PMOD1 pin 1 (P301).\r\n");
            diag_text("  4. SD MOSI -> Connect to PMOD1 pin 2 (P109).\r\n");
            diag_text("  5. SD MISO -> Connect to PMOD1 pin 3 (P110).\r\n");
            diag_text("  6. SD SCK  -> Connect to PMOD1 pin 4 (P111).\r\n");
            diag_text("  7. Ensure the 32 GB microSD card is pushed firmly into the socket.\r\n");
            diag_text("[WAIT] Retrying in 3 seconds (LED1 solid ON)...\r\n\r\n");

            /* Wait 3 seconds with LED1 continuously ON and LED2 OFF */
            for (int i = 0; i < 30; i++)
            {
                R_IOPORT_PinWrite(&g_ioport_ctrl, LED1_PIN, BSP_IO_LEVEL_HIGH);
                R_IOPORT_PinWrite(&g_ioport_ctrl, LED2_PIN, BSP_IO_LEVEL_LOW);
                R_BSP_SoftwareDelay(100U, BSP_DELAY_UNITS_MILLISECONDS);
            }
        }
    }

#if (0 == _RA_CORE) && (1 == BSP_MULTICORE_PROJECT) && !BSP_TZ_NONSECURE_BUILD
#if BSP_TZ_SECURE_BUILD
    R_BSP_IpcSemaphoreTake(&g_core_start_semaphore);
#endif
    R_BSP_SecondaryCoreStart();
#if BSP_TZ_SECURE_BUILD
    while(FSP_ERR_IN_USE == R_BSP_IpcSemaphoreTake(&g_core_start_semaphore))
    {
        ;
    }
#endif
#endif

#if (1 == _RA_CORE) && (1 == BSP_MULTICORE_PROJECT) && BSP_TZ_SECURE_BUILD
    R_BSP_IpcSemaphoreGive(&g_core_start_semaphore);
#endif

#if BSP_TZ_SECURE_BUILD
    R_BSP_NonSecureEnter();
#endif
}

#if BSP_TZ_SECURE_BUILD
FSP_CPP_HEADER
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ();
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ()
{
}
FSP_CPP_FOOTER
#endif
