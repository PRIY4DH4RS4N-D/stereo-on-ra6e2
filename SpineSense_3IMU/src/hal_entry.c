/**
 * @file hal_entry.c
 * @brief Main application entry point for SpineSense MPU6050 UART Bring-up Test.
 */

#include "hal_data.h"
#include "timer_measurement.h"
#include "uart_logger.h"
#include "i2c_bus.h"
#include "mpu6050.h"
#include "r_ioport.h"

/* Onboard status LEDs */
#define BOARD_LED1_PIN LED1 /* P104 - Red / Activity */
#define BOARD_LED2_PIN LED2 /* P112 - Green / MPU6050 OK */

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
    /* 1. Initialize ARM Cortex-M33 DWT cycle timer for microsecond precision */
    timer_init();

    /* 2. Initialize onboard LEDs */
    leds_init();

    /* 3. Initialize UART logging interface @ 115200 8N1 */
    uart_logger_init(115200U);

    /* 4. Initialize I2C Bus on Arduino Header (SCL: P100, SDA: P101) */
    i2c_init();

    /* 5. Print Standard Startup Header */
    uart_logger_println("");
    uart_logger_println("=====================================");
    uart_logger_println("       SPINESENSE - MPU6050 TEST     ");
    uart_logger_println("=====================================");
    uart_logger_println("RA6E2 I2C Initialization: OK");
    uart_logger_println("UART Initialization: OK");

    /* 6. Detect and Initialize MPU6050 */
    uint8_t who_am_i = 0;
    bool sensor_ready = false;

    while (!sensor_ready)
    {
        mpu6050_status_t id_err = mpu6050_read_id(&who_am_i);
        if (id_err == MPU6050_OK && who_am_i == MPU6050_WHO_AM_I_VAL)
        {
            uart_logger_println("MPU6050 Detection: SUCCESS");
            uart_logger_print("WHO_AM_I: ");
            uart_logger_print_hex8(who_am_i);
            uart_logger_println("");

            mpu6050_status_t init_err = mpu6050_init();
            if (init_err == MPU6050_OK)
            {
                uart_logger_println("Sensor Configuration: OK");
                sensor_ready = true;
                R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_HIGH);
            }
            else
            {
                uart_logger_println("[ERROR] MPU6050 Wakeup / Configuration FAILED!");
                timer_delay_ms(1000U);
            }
        }
        else if (id_err == MPU6050_OK && who_am_i != MPU6050_WHO_AM_I_VAL)
        {
            uart_logger_print("[ERROR] MPU6050 WHO_AM_I Mismatch! Expected 0x68, read: ");
            uart_logger_print_hex8(who_am_i);
            uart_logger_println("");
            timer_delay_ms(1000U);
        }
        else
        {
            uart_logger_println("[WARNING] MPU6050 Not Detected at I2C Address 0x68!");
            uart_logger_println("  -> Check Wiring: VCC=3.3V/5V, GND=GND, SCL=P100, SDA=P101, AD0=GND");
            uart_logger_println("  -> Retrying detection in 1 second...\r\n");
            timer_delay_ms(1000U);
        }
    }

    uart_logger_println("\r\nStarting continuous sensor readings (200 ms interval)...\r\n");

    /* 7. Continuous measurement loop (200 ms period) */
    mpu6050_data_t data;

    while (1)
    {
        /* Pulse activity LED */
        R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_HIGH);

        mpu6050_status_t status = mpu6050_read_all(&data);

        R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_LOW);

        if (status == MPU6050_OK)
        {
            R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_HIGH);

            /* Exact output structure specified in requirements */
            uart_logger_println("-------------------------------------");
            uart_logger_println("ACCELEROMETER");
            uart_logger_print("AX: ");
            uart_logger_print_i16(data.accel_x_raw);
            uart_logger_print("  AY: ");
            uart_logger_print_i16(data.accel_y_raw);
            uart_logger_print("  AZ: ");
            uart_logger_print_i16(data.accel_z_raw);
            uart_logger_println("");

            /* Scaled physical units in g */
            uart_logger_print("AX: ");
            uart_logger_print_float(data.accel_x_g, 2);
            uart_logger_print(" g  AY: ");
            uart_logger_print_float(data.accel_y_g, 2);
            uart_logger_print(" g  AZ: ");
            uart_logger_print_float(data.accel_z_g, 2);
            uart_logger_println(" g");
            uart_logger_println("");

            uart_logger_println("GYROSCOPE");
            uart_logger_print("GX: ");
            uart_logger_print_i16(data.gyro_x_raw);
            uart_logger_print("  GY: ");
            uart_logger_print_i16(data.gyro_y_raw);
            uart_logger_print("  GZ: ");
            uart_logger_print_i16(data.gyro_z_raw);
            uart_logger_println("");

            /* Scaled physical units in degrees/second */
            uart_logger_print("GX: ");
            uart_logger_print_float(data.gyro_x_dps, 2);
            uart_logger_print(" dps  GY: ");
            uart_logger_print_float(data.gyro_y_dps, 2);
            uart_logger_print(" dps  GZ: ");
            uart_logger_print_float(data.gyro_z_dps, 2);
            uart_logger_println(" dps");
            uart_logger_println("");

            /* Temperature in Celsius */
            uart_logger_print("TEMPERATURE: ");
            uart_logger_print_float(data.temp_c, 1);
            uart_logger_println(" C");
            uart_logger_println("-------------------------------------");
        }
        else
        {
            R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_LOW);

            uart_logger_println("-------------------------------------");
            uart_logger_println("[ERROR] MPU6050 Read Failure / Disconnected!");
            uart_logger_println("Sensor Status: NO ACK ON I2C BUS");
            uart_logger_println("Attempting bus recovery and sensor re-detection...");
            uart_logger_println("-------------------------------------");

            /* Re-initialize bus and check if sensor is restored */
            i2c_init();
            timer_delay_ms(200U);
            if (mpu6050_init() == MPU6050_OK)
            {
                uart_logger_println("[INFO] MPU6050 Re-connected and re-configured successfully!");
            }
        }

        /* 200 ms interval between readings */
        timer_delay_ms(200U);
    }
}
