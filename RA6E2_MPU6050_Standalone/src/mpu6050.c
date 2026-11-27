/**
 * @file mpu6050.c
 * @brief Multi-Instance MPU6050 6-Axis MotionTracking Driver Implementation.
 */

#include "mpu6050.h"
#include "timer_measurement.h"

/* Additional MPU6050 Register Definitions */
#define MPU6050_REG_INT_ENABLE   (0x38U)
#define MPU6050_REG_USER_CTRL    (0x6AU)

mpu6050_status_t mpu6050_read_id(mpu6050_device_t *p_dev, uint8_t *p_who_am_i)
{
    if (p_dev == NULL || p_dev->p_bus == NULL || p_who_am_i == NULL)
    {
        return MPU6050_ERR_PARAM;
    }

    uint8_t id = 0;
    i2c_status_t status = i2c_read_bytes(p_dev->p_bus, p_dev->dev_addr, MPU6050_REG_WHO_AM_I, &id, 1);
    if (status != I2C_SUCCESS)
    {
        return MPU6050_ERR_NOT_RESPONDING;
    }

    *p_who_am_i = id;
    p_dev->who_am_i = id;
    return MPU6050_OK;
}

mpu6050_status_t mpu6050_init(mpu6050_device_t *p_dev)
{
    if (p_dev == NULL || p_dev->p_bus == NULL)
    {
        return MPU6050_ERR_PARAM;
    }

    /* 1. Device Reset: Write 0x80 to PWR_MGMT_1 (0x6B) */
    i2c_write_reg(p_dev->p_bus, p_dev->dev_addr, MPU6050_REG_PWR_MGMT_1, 0x80);
    timer_delay_ms(100U); /* Allow internal reset cycle to complete */

    /* 2. Wake up MPU6050 and set clock source to PLL with X axis gyro reference (0x01).
     * NOTE: Per InvenSense datasheet, CLKSEL=0 (internal 8MHz) disables gyroscopes.
     * CLKSEL=1 (PLL X-gyro) is required for stable continuous gyro and accel sampling.
     */
    i2c_status_t err = i2c_write_reg(p_dev->p_bus, p_dev->dev_addr, MPU6050_REG_PWR_MGMT_1, 0x01);
    if (err != I2C_SUCCESS)
    {
        p_dev->is_initialized = false;
        return MPU6050_ERR_I2C;
    }
    timer_delay_ms(20U); /* Allow PLL oscillator to lock */

    /* 3. Enable all 6 axes (disable any low-power standby): Write 0x00 to PWR_MGMT_2 (0x6C) */
    i2c_write_reg(p_dev->p_bus, p_dev->dev_addr, MPU6050_REG_PWR_MGMT_2, 0x00);

    /* 4. Disable FIFO and AUX I2C master mode: Write 0x00 to USER_CTRL (0x6A)
     * Ensures real-time ADC samples directly update output registers 0x3B-0x48.
     */
    i2c_write_reg(p_dev->p_bus, p_dev->dev_addr, MPU6050_REG_USER_CTRL, 0x00);

    /* 5. Disable interrupts: Write 0x00 to INT_ENABLE (0x38) */
    i2c_write_reg(p_dev->p_bus, p_dev->dev_addr, MPU6050_REG_INT_ENABLE, 0x00);

    /* 6. Configure Accelerometer Full Scale Range: ±2g -> Write 0x00 to ACCEL_CONFIG (0x1C) */
    err = i2c_write_reg(p_dev->p_bus, p_dev->dev_addr, MPU6050_REG_ACCEL_CONFIG, 0x00);
    if (err != I2C_SUCCESS)
    {
        p_dev->is_initialized = false;
        return MPU6050_ERR_I2C;
    }

    /* 7. Configure Gyroscope Full Scale Range: ±250 dps -> Write 0x00 to GYRO_CONFIG (0x1B) */
    err = i2c_write_reg(p_dev->p_bus, p_dev->dev_addr, MPU6050_REG_GYRO_CONFIG, 0x00);
    if (err != I2C_SUCCESS)
    {
        p_dev->is_initialized = false;
        return MPU6050_ERR_I2C;
    }

    /* 8. Set Sample Rate Divider: SMPLRT_DIV = 0 (1 kHz / (1 + 0) = 1 kHz sample rate) */
    i2c_write_reg(p_dev->p_bus, p_dev->dev_addr, MPU6050_REG_SMPLRT_DIV, 0x00);

    /* 9. Set DLPF (Digital Low Pass Filter) to ~42-44 Hz bandwidth: CONFIG (0x1A) = 0x03 */
    i2c_write_reg(p_dev->p_bus, p_dev->dev_addr, MPU6050_REG_CONFIG, 0x03);

    timer_delay_ms(15U);
    p_dev->is_initialized = true;
    return MPU6050_OK;
}

mpu6050_status_t mpu6050_read_all(mpu6050_device_t *p_dev)
{
    if (p_dev == NULL || p_dev->p_bus == NULL)
    {
        return MPU6050_ERR_PARAM;
    }

    /* 14-byte continuous burst read starting from ACCEL_XOUT_H (0x3B) */
    uint8_t raw_buf[14];
    i2c_status_t err = i2c_read_bytes(p_dev->p_bus, p_dev->dev_addr, MPU6050_REG_ACCEL_XOUT_H, raw_buf, 14);
    if (err != I2C_SUCCESS)
    {
        return MPU6050_ERR_I2C;
    }

    /* Reconstruct signed 16-bit values (Big-Endian from MPU6050) */
    p_dev->data.accel_x_raw = (int16_t)(((uint16_t)raw_buf[0]  << 8) | raw_buf[1]);
    p_dev->data.accel_y_raw = (int16_t)(((uint16_t)raw_buf[2]  << 8) | raw_buf[3]);
    p_dev->data.accel_z_raw = (int16_t)(((uint16_t)raw_buf[4]  << 8) | raw_buf[5]);
    p_dev->data.temp_raw    = (int16_t)(((uint16_t)raw_buf[6]  << 8) | raw_buf[7]);
    p_dev->data.gyro_x_raw  = (int16_t)(((uint16_t)raw_buf[8]  << 8) | raw_buf[9]);
    p_dev->data.gyro_y_raw  = (int16_t)(((uint16_t)raw_buf[10] << 8) | raw_buf[11]);
    p_dev->data.gyro_z_raw  = (int16_t)(((uint16_t)raw_buf[12] << 8) | raw_buf[13]);

    /* Physical units conversions:
     * Accelerometer (±2g): g = raw / 16384.0
     * Gyroscope (±250 dps): dps = raw / 131.0
     * Temperature: C = (raw / 340.0) + 36.53
     */
    p_dev->data.accel_x_g  = (float)p_dev->data.accel_x_raw / MPU6050_ACCEL_SCALE_2G;
    p_dev->data.accel_y_g  = (float)p_dev->data.accel_y_raw / MPU6050_ACCEL_SCALE_2G;
    p_dev->data.accel_z_g  = (float)p_dev->data.accel_z_raw / MPU6050_ACCEL_SCALE_2G;

    p_dev->data.gyro_x_dps = (float)p_dev->data.gyro_x_raw / MPU6050_GYRO_SCALE_250DPS;
    p_dev->data.gyro_y_dps = (float)p_dev->data.gyro_y_raw / MPU6050_GYRO_SCALE_250DPS;
    p_dev->data.gyro_z_dps = (float)p_dev->data.gyro_z_raw / MPU6050_GYRO_SCALE_250DPS;

    p_dev->data.temp_c     = ((float)p_dev->data.temp_raw / 340.0f) + 36.53f;

    return MPU6050_OK;
}
