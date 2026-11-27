/**
 * @file hal_entry.c
 * @brief Standalone Dual MPU6050 Independent I2C Bus Integration for RA6E2.
 *
 * Interfacing TWO MPU6050 (GY-521) sensors simultaneously on independent I2C buses:
 * - Sensor 1 (MPU6050_NECK):   I2C Bus 1 on Arduino Header Pins D4 (SCL, P500) & D5 (SDA, P409)
 * - Sensor 2 (MPU6050_CENTER): I2C Bus 2 on Arduino Header Pins D2 (SCL, P105) & D3 (SDA, P408)
 *
 * Both sensors run at the default address 0x68 independently with zero bus collision.
 * Serial telemetry is transmitted via J-Link CDC UART (COM20 / 115200 8N1) and RTT.
 */

#include "hal_data.h"
#include "timer_measurement.h"
#include "uart_logger.h"
#include "i2c_bus.h"
#include "mpu6050.h"
#include "r_ioport.h"

/* Onboard status LEDs on BGK-RA6E2 */
#define BOARD_LED1_PIN LED1 /* P104 - Red / Activity */
#define BOARD_LED2_PIN LED2 /* P112 - Green / Sensor Status OK */

static mpu6050_device_t s_sensor_neck = {
    .name           = "MPU6050 NECK",
    .p_bus          = &g_i2c_bus1,
    .dev_addr       = MPU6050_I2C_ADDR, /* 0x68 */
    .who_am_i       = 0,
    .is_initialized = false
};

static mpu6050_device_t s_sensor_center = {
    .name           = "MPU6050 CENTER",
    .p_bus          = &g_i2c_bus2,
    .dev_addr       = MPU6050_I2C_ADDR, /* 0x68 */
    .who_am_i       = 0,
    .is_initialized = false
};

static void leds_init(void)
{
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(BOARD_LED1_PIN, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW));
    R_BSP_PinCfg(BOARD_LED2_PIN, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW));
    R_BSP_PinAccessDisable();

    R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_LOW);
    R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_LOW);
}

static void print_sensor_reading(mpu6050_device_t *p_dev, const char *bus_label)
{
    uart_logger_print("[");
    uart_logger_print(p_dev->name);
    uart_logger_print(" - ");
    uart_logger_print(bus_label);
    uart_logger_println("]");

    if (p_dev->is_initialized)
    {
        mpu6050_status_t status = mpu6050_read_all(p_dev);
        if (status == MPU6050_OK)
        {
            uart_logger_println("ACCEL:");
            uart_logger_print("AX: ");
            uart_logger_print_i16(p_dev->data.accel_x_raw);
            uart_logger_print(" AY: ");
            uart_logger_print_i16(p_dev->data.accel_y_raw);
            uart_logger_print(" AZ: ");
            uart_logger_print_i16(p_dev->data.accel_z_raw);
            uart_logger_println("");

            /* Converted physical units in g */
            uart_logger_print("AX: ");
            uart_logger_print_float(p_dev->data.accel_x_g, 2);
            uart_logger_print(" g  AY: ");
            uart_logger_print_float(p_dev->data.accel_y_g, 2);
            uart_logger_print(" g  AZ: ");
            uart_logger_print_float(p_dev->data.accel_z_g, 2);
            uart_logger_println(" g");
            uart_logger_println("");

            uart_logger_println("GYRO:");
            uart_logger_print("GX: ");
            uart_logger_print_i16(p_dev->data.gyro_x_raw);
            uart_logger_print(" GY: ");
            uart_logger_print_i16(p_dev->data.gyro_y_raw);
            uart_logger_print(" GZ: ");
            uart_logger_print_i16(p_dev->data.gyro_z_raw);
            uart_logger_println("");

            /* Converted physical units in degrees/second */
            uart_logger_print("GX: ");
            uart_logger_print_float(p_dev->data.gyro_x_dps, 2);
            uart_logger_print(" dps  GY: ");
            uart_logger_print_float(p_dev->data.gyro_y_dps, 2);
            uart_logger_print(" dps  GZ: ");
            uart_logger_print_float(p_dev->data.gyro_z_dps, 2);
            uart_logger_println(" dps");
            uart_logger_println("");

            uart_logger_print("TEMP: ");
            uart_logger_print_float(p_dev->data.temp_c, 1);
            uart_logger_println(" C");
        }
        else
        {
            uart_logger_print(p_dev->name);
            uart_logger_println(": I2C READ ERROR");
            p_dev->is_initialized = false;
            i2c_bus_recover(p_dev->p_bus);
        }
    }
    else
    {
        uart_logger_print(p_dev->name);
        uart_logger_println(": DETECTION / COMMUNICATION FAILED");
        uart_logger_println("  -> Retrying sensor re-detection...");

        /* Periodic re-detection attempt */
        uint8_t id = 0;
        if (mpu6050_read_id(p_dev, &id) == MPU6050_OK && id == MPU6050_WHO_AM_I_VAL)
        {
            if (mpu6050_init(p_dev) == MPU6050_OK)
            {
                uart_logger_print("  -> ");
                uart_logger_print(p_dev->name);
                uart_logger_println(" Reconnected and configured successfully!");
            }
        }
    }
}

void hal_entry(void)
{
    /* 1. Initialize ARM Cortex-M33 DWT cycle timer for microsecond precision */
    timer_init();

    /* 2. Initialize onboard status LEDs */
    leds_init();

    /* 3. Initialize UART logger (115200 8N1) */
    uart_logger_init(115200U);

    /* 4. Print Startup Banner */
    uart_logger_println("");
    uart_logger_println("========================================");
    uart_logger_println("       SPINESENSE DUAL MPU6050 TEST     ");
    uart_logger_println("========================================");
    uart_logger_println("");

    /* 5. Initialize I2C Bus 1 (Arduino D4 SCL, D5 SDA) */
    i2c_bus_init(s_sensor_neck.p_bus);
    uart_logger_println("I2C BUS 1: INITIALIZED");

    uint8_t id1 = 0;
    if (mpu6050_read_id(&s_sensor_neck, &id1) == MPU6050_OK && id1 == MPU6050_WHO_AM_I_VAL)
    {
        uart_logger_println("MPU6050 NECK: DETECTED");
        uart_logger_print("WHO_AM_I: ");
        uart_logger_print_hex8(id1);
        uart_logger_println("");
        if (mpu6050_init(&s_sensor_neck) == MPU6050_OK)
        {
            uart_logger_println("MPU6050 NECK Configuration: OK");
        }
    }
    else
    {
        uart_logger_println("MPU6050 NECK: NOT DETECTED (Address 0x68)");
        uart_logger_println("  -> Check Wiring: SCL=D4, SDA=D5, VCC=3.3V, GND=GND, AD0=GND");
    }
    uart_logger_println("");

    /* 6. Initialize I2C Bus 2 (Arduino D2 SCL, D3 SDA) */
    i2c_bus_init(s_sensor_center.p_bus);
    uart_logger_println("I2C BUS 2: INITIALIZED");

    uint8_t id2 = 0;
    if (mpu6050_read_id(&s_sensor_center, &id2) == MPU6050_OK && id2 == MPU6050_WHO_AM_I_VAL)
    {
        uart_logger_println("MPU6050 CENTER: DETECTED");
        uart_logger_print("WHO_AM_I: ");
        uart_logger_print_hex8(id2);
        uart_logger_println("");
        if (mpu6050_init(&s_sensor_center) == MPU6050_OK)
        {
            uart_logger_println("MPU6050 CENTER Configuration: OK");
        }
    }
    else
    {
        uart_logger_println("MPU6050 CENTER: NOT DETECTED (Address 0x68)");
        uart_logger_println("  -> Check Wiring: SCL=D2, SDA=D3, VCC=3.3V, GND=GND, AD0=GND");
    }
    uart_logger_println("");

    uart_logger_println("========================================");
    uart_logger_println("         LIVE SENSOR READINGS           ");
    uart_logger_println("========================================");
    uart_logger_println("");

    /* 7. Continuous Acquisition Loop (200 ms interval) */
    while (1)
    {
        /* Pulse activity LED */
        R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_HIGH);

        /* Sensor 1: MPU6050 NECK on Bus 1 */
        print_sensor_reading(&s_sensor_neck, "BUS 1");
        uart_logger_println("----------------------------------------");

        /* Sensor 2: MPU6050 CENTER on Bus 2 */
        print_sensor_reading(&s_sensor_center, "BUS 2");
        uart_logger_println("========================================");
        uart_logger_println("");

        /* Update green status LED: ON if at least one sensor is communicating */
        if (s_sensor_neck.is_initialized || s_sensor_center.is_initialized)
        {
            R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_HIGH);
        }
        else
        {
            R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_LOW);
        }

        R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_LOW);

        /* 200 ms acquisition period as requested */
        timer_delay_ms(200U);
    }
}
