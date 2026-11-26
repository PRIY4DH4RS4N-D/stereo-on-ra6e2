/**
 * @file hal_entry.c
 * @brief Main entry point and measurement control loop for RA6E2 Ultrasonic System.
 */

#include "hal_data.h"
#include "timer_measurement.h"
#include "uart_logger.h"
#include "ultrasonic_sensor.h"
#include "bsp_pin_cfg.h"
#include "r_ioport.h"

/* LED Pin Definitions on BGK-RA6E2 */
#define BOARD_LED1_PIN LED1 /* P104 - Red / Activity */
#define BOARD_LED2_PIN LED2 /* P112 - Green / Valid Echo */

static void leds_init(void)
{
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(BOARD_LED1_PIN, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW));
    R_BSP_PinCfg(BOARD_LED2_PIN, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW));
    R_BSP_PinAccessDisable();

    R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_LOW);
    R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_LOW);
}

void hal_entry(void)
{
    /* 1. Initialize high-resolution DWT hardware timer */
    timer_init();

    /* 2. Initialize onboard status LEDs */
    leds_init();

    /* 3. Initialize dual-channel UART and SEGGER RTT Logger @ 115200 baud */
    uart_logger_init(115200U);

    /* 4. Initialize HC-SR04 Ultrasonic Sensor driver */
    ultrasonic_init(NULL);

    /* Startup Announcement */
    uart_logger_println("");
    uart_logger_println("**************************************************");
    uart_logger_println("*     Renesas BGK-RA6E2 Ultrasonic System        *");
    uart_logger_println("*       Precision HC-SR04 Distance Measurement   *");
    uart_logger_println("**************************************************");
    
    uart_logger_print("[SYS] CPU Clock     : ");
    uart_logger_print_u32(timer_get_system_clock_hz() / 1000000U);
    uart_logger_println(" MHz");

    uart_logger_println("[PIN] TRIG Pin      : Arduino D2 (P105) -> Output");
    uart_logger_println("[PIN] ECHO Pin      : Arduino D3 (P408) -> Input (via 5V-to-3.3V divider)");
    uart_logger_println("[PIN] UART TX       : Arduino D1 (P411) & PMOD2 Pin 6 (P101) @ 115200 8N1");
    uart_logger_println("[LOG] RTT Viewer    : SEGGER J-Link RTT Terminal 0 Enabled");

    /* 5. Self-test validation of mathematical algorithms */
    uart_logger_print("[TEST] Running Math & Distance Self-Test... ");
    if (ultrasonic_self_test())
    {
        uart_logger_println("PASSED (All formula checks valid)");
    }
    else
    {
        uart_logger_println("FAILED!");
    }

    uart_logger_println("[INFO] Starting continuous measurements (150 ms interval)...\r\n");

    /* 6. Main measurement loop */
    ultrasonic_result_t result;
    uint32_t measurement_count = 0;

    while (1)
    {
        /* Pulse activity LED during measurement trigger */
        R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_HIGH);

        /* Execute non-blocking ultrasonic measurement */
        ultrasonic_measure(&result);

        R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_LOW);

        /* LED2 indicates echo received successfully */
        if (result.is_valid)
        {
            R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_HIGH);
        }
        else
        {
            R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_LOW);
        }

        /* Output standardized measurement report */
        uart_logger_print_measurement(
            result.status_str,
            result.distance_cm,
            result.echo_time_us,
            result.detail_str,
            result.is_valid
        );

        measurement_count++;

        /* 150 ms measurement delay (satisfies 100-200 ms requirement) */
        timer_delay_ms(150U);
    }
}
