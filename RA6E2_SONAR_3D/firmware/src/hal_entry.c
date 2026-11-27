/**
 * @file hal_entry.c
 * @brief Application entry point for RA6E2 SONAR-3D Ultrasonic Point Cloud Scanner.
 */

#include "hal_data.h"
#include "timer_measurement.h"
#include "ultrasonic_sensor.h"
#include "uart_protocol.h"
#include "bsp_pin_cfg.h"
#include "r_ioport.h"

/* LED Pin Definitions on BGK-RA6E2 */
#define BOARD_LED1_PIN LED1 /* P104 - Red / Trigger Activity */
#define BOARD_LED2_PIN LED2 /* P112 - Green / Valid Echo Indicator */

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
    /* 1. Initialize ARM Cortex-M33 DWT hardware timer (200 MHz, 5 ns resolution) */
    timer_init();

    /* 2. Initialize onboard status LEDs */
    leds_init();

    /* 3. Initialize bidirectional JSON UART protocol engine @ 115200 baud */
    uart_protocol_init(UART_PROTOCOL_DEFAULT_BAUD);

    /* 4. Initialize HC-SR04 ultrasonic sensor driver (TRIG: D2/P105, ECHO: D3/P408) */
    ultrasonic_init(NULL);

    /* 5. Announce system boot */
    uart_protocol_send_raw("{\"type\":\"status\",\"board\":\"BGK-RA6E2\",\"system\":\"SONAR-3D\",\"status\":\"ready\"}\r\n");

    /* Quick LED self-test blink */
    R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_HIGH);
    R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_HIGH);
    timer_delay_ms(100U);
    R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_LOW);
    R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_LOW);

    /* 6. Continuous measurement and command dispatch loop */
    uart_command_t cmd;
    ultrasonic_result_t result;
    uint32_t auto_req_id = 1U;
    uint32_t last_measure_time = timer_get_cycles();
    uint32_t measure_interval_cycles = timer_get_system_clock_hz() / 12U; /* ~83 ms */

    while (1)
    {
        /* A. Check for incoming commands from host */
        if (uart_protocol_poll_command(&cmd))
        {
            if (cmd.type == UART_CMD_PING)
            {
                uart_protocol_send_pong();
            }
            else if (cmd.type == UART_CMD_RESET)
            {
                R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_LOW);
                uart_protocol_send_raw("{\"type\":\"status\",\"status\":\"reset_ok\"}\r\n");
            }
        }

        /* B. Continuous live ultrasound measurement streaming */
        uint32_t now = timer_get_cycles();
        if ((now - last_measure_time) >= measure_interval_cycles)
        {
            last_measure_time = now;

            /* Activity pulse */
            R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_HIGH);

            ultrasonic_status_t status = ultrasonic_measure(&result);

            R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_LOW);

            if (status == ULTRASONIC_SUCCESS)
            {
                R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_HIGH);
                int32_t dist_mm = (int32_t)((result.distance_cm * 10.0f) + 0.5f);

                uart_protocol_send_measurement(
                    auto_req_id++,
                    dist_mm,
                    result.echo_time_us,
                    "ok"
                );
            }
            else if (status == ULTRASONIC_ERR_RISING_TIMEOUT || status == ULTRASONIC_ERR_FALLING_TIMEOUT)
            {
                R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_LOW);
                uart_protocol_send_measurement(
                    auto_req_id++,
                    -1,
                    0,
                    "timeout"
                );
            }
            else
            {
                R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_LOW);
                uart_protocol_send_measurement(
                    auto_req_id++,
                    -1,
                    result.echo_time_us,
                    "out_of_range"
                );
            }

            if (auto_req_id > 999999U)
            {
                auto_req_id = 1U;
            }
        }

        timer_delay_ms(1U);
    }
}
