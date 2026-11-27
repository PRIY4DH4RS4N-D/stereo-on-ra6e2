/**
 * @file hal_entry.c
 * @brief SPINESENSE Dual Independent Hardware I2C IMU Telemetry on Renesas RA6E2.
 *
 * Hardware Peripherals & Pin Multiplexing:
 * - BUS 1 (Sensor 1: MPU6050):
 *     Peripheral: SCI9 (Simple I2C Master)
 *     SCL: Arduino D13 (P111 / SCK9)
 *     SDA: Arduino D11 (P109 / TXD9_SSDA9)
 *     Address: 0x68
 *
 * - BUS 2 (Sensor 2: MPU9250 / MPU6500):
 *     Peripheral: IIC_B0 (Hardware I2C Master)
 *     SCL: Arduino D3 (P408 / SCL0)
 *     SDA: PMOD2 Pin 4 (P407 / SDA0)
 *     Address: 0x68
 *
 * - UART Serial Console:
 *     Peripheral: SCI0 (P101 / TXD0) -> Segger J-Link CDC UART (COM20)
 *     115200 baud, 8-N-1
 *
 * Both sensors are initialized and read independently.
 * Output is broadcast through PuTTY every 2000 ms.
 */

#include "hal_data.h"
#include "timer_measurement.h"
#include "uart_logger.h"
#include "i2c_hw_sci9.h"
#include "i2c_hw_iic0.h"
#include "mpu_sensor.h"
#include "r_ioport.h"

/* Status LEDs on BGK-RA6E2 */
#define BOARD_LED1_PIN LED1 /* P104 - Red / Activity */
#define BOARD_LED2_PIN LED2 /* P112 - Green / Sensor Status OK */

static mpu_device_t s_sensor_bus1 = {
    .label          = "MPU6050 NECK",
    .dev_addr       = 0x68U,
    .write_reg      = i2c_hw_sci9_write_reg,
    .read_bytes     = i2c_hw_sci9_read_bytes,
    .who_am_i       = 0U,
    .model_name     = "UNKNOWN",
    .is_initialized = false
};

static mpu_device_t s_sensor_bus2 = {
    .label          = "MPU9250/MPU6500 CENTER",
    .dev_addr       = 0x68U,
    .write_reg      = i2c_hw_iic0_write_reg,
    .read_bytes     = i2c_hw_iic0_read_bytes,
    .who_am_i       = 0U,
    .model_name     = "UNKNOWN",
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

static void print_sensor_data(mpu_device_t *p_dev, const char *bus_details)
{
    uart_logger_print("[");
    uart_logger_print(p_dev->label);
    uart_logger_print(" - ");
    uart_logger_print(bus_details);
    uart_logger_println("]");

    if (p_dev->is_initialized)
    {
        int8_t err = mpu_read_all(p_dev);
        if (err == 0)
        {
            uart_logger_println("ACCEL:");
            uart_logger_print("AX: ");
            uart_logger_print_i16(p_dev->accel_x_raw);
            uart_logger_print(" AY: ");
            uart_logger_print_i16(p_dev->accel_y_raw);
            uart_logger_print(" AZ: ");
            uart_logger_print_i16(p_dev->accel_z_raw);
            uart_logger_println("");

            /* Converted physical units in g */
            uart_logger_print("AX: ");
            uart_logger_print_float(p_dev->accel_x_g, 2);
            uart_logger_print(" g  AY: ");
            uart_logger_print_float(p_dev->accel_y_g, 2);
            uart_logger_print(" g  AZ: ");
            uart_logger_print_float(p_dev->accel_z_g, 2);
            uart_logger_println(" g");
            uart_logger_println("");

            uart_logger_println("GYRO:");
            uart_logger_print("GX: ");
            uart_logger_print_i16(p_dev->gyro_x_raw);
            uart_logger_print(" GY: ");
            uart_logger_print_i16(p_dev->gyro_y_raw);
            uart_logger_print(" GZ: ");
            uart_logger_print_i16(p_dev->gyro_z_raw);
            uart_logger_println("");

            /* Converted physical units in degrees/second */
            uart_logger_print("GX: ");
            uart_logger_print_float(p_dev->gyro_x_dps, 2);
            uart_logger_print(" dps  GY: ");
            uart_logger_print_float(p_dev->gyro_y_dps, 2);
            uart_logger_print(" dps  GZ: ");
            uart_logger_print_float(p_dev->gyro_z_dps, 2);
            uart_logger_println(" dps");
            uart_logger_println("");

            uart_logger_print("TEMP: ");
            uart_logger_print_float(p_dev->temp_c, 1);
            uart_logger_println(" C");
        }
        else
        {
            uart_logger_print(p_dev->label);
            uart_logger_println(": I2C READ ERROR");
            p_dev->is_initialized = false;
        }
    }
    else
    {
        uart_logger_print(p_dev->label);
        uart_logger_println(": DETECTION / COMMUNICATION FAILED");
        uart_logger_println("  -> Retrying sensor detection...");

        /* Periodic re-detection attempt */
        if (mpu_init(p_dev) == 0)
        {
            uart_logger_print("  -> ");
            uart_logger_print(p_dev->label);
            uart_logger_print(" (");
            uart_logger_print(p_dev->model_name);
            uart_logger_println(") Reconnected successfully!");
        }
    }
}

void hal_entry(void)
{
    /* 1. Initialize ARM Cortex-M33 DWT microsecond timer */
    timer_init();

    /* 2. Initialize onboard status LEDs */
    leds_init();

    /* 3. Initialize UART logger (SCI0 P101 -> COM20 @ 115200 8N1) */
    uart_logger_init(115200U);

    /* 4. Print Startup Banner */
    uart_logger_println("");
    uart_logger_println("========================================");
    uart_logger_println("       SPINESENSE DUAL MPU6050 TEST     ");
    uart_logger_println("========================================");
    uart_logger_println("");

    /* 5. Initialize Hardware BUS 1: SCI9 (Arduino D13 SCL, D11 SDA) */
    i2c_hw_sci9_init();
    uart_logger_println("I2C BUS 1: INITIALIZED (SCI9: D13=SCL, D11=SDA)");

    if (mpu_init(&s_sensor_bus1) == 0)
    {
        uart_logger_print("MPU6050 NECK: DETECTED (");
        uart_logger_print(s_sensor_bus1.model_name);
        uart_logger_println(")");

        uart_logger_print("WHO_AM_I: ");
        uart_logger_print_hex8(s_sensor_bus1.who_am_i);
        uart_logger_println("");

        uart_logger_println("MPU6050 NECK Configuration: OK (±2g, ±250dps)");
    }
    else
    {
        uart_logger_println("MPU6050 NECK: NOT DETECTED (Address 0x68)");
        uart_logger_println("  -> Check Wiring: SCL=Arduino D13, SDA=Arduino D11, VCC=3.3V, GND=GND, AD0=GND");
    }
    uart_logger_println("");

    /* 6. Initialize Hardware BUS 2: IIC_B0 (Arduino D3 SCL, PMOD2 Pin 4 SDA) */
    i2c_hw_iic0_init();
    uart_logger_println("I2C BUS 2: INITIALIZED (IIC_B0: D3=SCL, PMOD2-Pin4=SDA)");

    if (mpu_init(&s_sensor_bus2) == 0)
    {
        uart_logger_print("MPU9250/MPU6500 CENTER: DETECTED (");
        uart_logger_print(s_sensor_bus2.model_name);
        uart_logger_println(")");

        uart_logger_print("WHO_AM_I: ");
        uart_logger_print_hex8(s_sensor_bus2.who_am_i);
        uart_logger_println("");

        uart_logger_println("MPU9250/MPU6500 CENTER Configuration: OK (±2g, ±250dps)");
    }
    else
    {
        uart_logger_println("MPU9250/MPU6500 CENTER: NOT DETECTED (Address 0x68)");
        uart_logger_println("  -> Check Wiring: SCL=Arduino D3, SDA=PMOD2 Pin 4, VCC=3.3V, GND=GND, AD0=GND");
    }
    uart_logger_println("");

    uart_logger_println("========================================");
    uart_logger_println("         LIVE SENSOR READINGS           ");
    uart_logger_println("========================================");
    uart_logger_println("");

    /* 7. Continuous Acquisition Loop (2000 ms interval) */
    while (1)
    {
        /* Pulse Activity LED */
        R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_HIGH);

        /* Sensor 1: MPU6050 NECK on Hardware BUS 1 */
        print_sensor_data(&s_sensor_bus1, "BUS 1 (SCI9: SCL=D13, SDA=D11)");
        uart_logger_println("----------------------------------------");

        /* Sensor 2: MPU9250 / MPU6500 CENTER on Hardware BUS 2 */
        print_sensor_data(&s_sensor_bus2, "BUS 2 (IIC_B0: SCL=D3, SDA=PMOD2-Pin4)");
        uart_logger_println("========================================");
        uart_logger_println("");

        /* Update green status LED: ON if at least one sensor is active */
        if (s_sensor_bus1.is_initialized || s_sensor_bus2.is_initialized)
        {
            R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_HIGH);
        }
        else
        {
            R_BSP_PinWrite(BOARD_LED2_PIN, BSP_IO_LEVEL_LOW);
        }

        R_BSP_PinWrite(BOARD_LED1_PIN, BSP_IO_LEVEL_LOW);

        /* Wait 2000 ms before next live update */
        timer_delay_ms(2000U);
    }
}
