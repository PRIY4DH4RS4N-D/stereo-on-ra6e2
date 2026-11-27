/**
 * @file mpu_sensor.c
 * @brief Unified Independent Driver for MPU6050, MPU6500 and MPU9250 IMUs.
 */

#include "mpu_sensor.h"
#include "timer_measurement.h"

int8_t mpu_init(mpu_device_t *p_dev)
{
    if ((p_dev == NULL) || (p_dev->write_reg == NULL) || (p_dev->read_bytes == NULL))
    {
        return -1;
    }

    p_dev->is_initialized = false;
    p_dev->who_am_i = 0U;
    p_dev->model_name = "UNKNOWN";

    /* 1. Read WHO_AM_I register */
    uint8_t who = 0U;
    int8_t err = p_dev->read_bytes(p_dev->dev_addr, MPU_REG_WHO_AM_I, &who, 1U);
    if (err != 0)
    {
        return -2;
    }

    p_dev->who_am_i = who;

    /* Classify detected sensor model */
    switch (who)
    {
        case MPU_ID_MPU6050:
            p_dev->model_name = "MPU6050";
            break;
        case MPU_ID_MPU6500:
            p_dev->model_name = "MPU6500";
            break;
        case MPU_ID_MPU9250:
            p_dev->model_name = "MPU9250";
            break;
        case MPU_ID_MPU9255:
            p_dev->model_name = "MPU9255";
            break;
        default:
            p_dev->model_name = "MPU_COMPATIBLE";
            break;
    }

    /* 2. Reset device / Wake up from sleep (PWR_MGMT_1: 0x00) */
    err = p_dev->write_reg(p_dev->dev_addr, MPU_REG_PWR_MGMT_1, 0x00U);
    if (err != 0)
    {
        return -3;
    }
    timer_delay_ms(15U);

    /* 3. Set Sample Rate Divider = 0 (1 kHz internal sample rate) */
    p_dev->write_reg(p_dev->dev_addr, MPU_REG_SMPLRT_DIV, 0x00U);

    /* 4. Configure Digital Low Pass Filter (DLPF: ~44Hz bandwidth) */
    p_dev->write_reg(p_dev->dev_addr, MPU_REG_CONFIG, 0x03U);

    /* 5. Configure Gyroscope Full Scale: ±250 dps (FS_SEL = 0) */
    err = p_dev->write_reg(p_dev->dev_addr, MPU_REG_GYRO_CONFIG, 0x00U);
    if (err != 0)
    {
        return -4;
    }

    /* 6. Configure Accelerometer Full Scale: ±2g (AFS_SEL = 0) */
    err = p_dev->write_reg(p_dev->dev_addr, MPU_REG_ACCEL_CONFIG, 0x00U);
    if (err != 0)
    {
        return -5;
    }

    p_dev->is_initialized = true;
    return 0;
}

int8_t mpu_read_all(mpu_device_t *p_dev)
{
    if ((p_dev == NULL) || (!p_dev->is_initialized))
    {
        return -1;
    }

    uint8_t raw_buf[14];
    int8_t err = p_dev->read_bytes(p_dev->dev_addr, MPU_REG_ACCEL_XOUT_H, raw_buf, 14U);
    if (err != 0)
    {
        return err;
    }

    /* Extract big-endian signed 16-bit accelerometer readings */
    p_dev->accel_x_raw = (int16_t)(((uint16_t)raw_buf[0] << 8) | (uint16_t)raw_buf[1]);
    p_dev->accel_y_raw = (int16_t)(((uint16_t)raw_buf[2] << 8) | (uint16_t)raw_buf[3]);
    p_dev->accel_z_raw = (int16_t)(((uint16_t)raw_buf[4] << 8) | (uint16_t)raw_buf[5]);

    /* Extract big-endian signed 16-bit temperature reading */
    p_dev->temp_raw    = (int16_t)(((uint16_t)raw_buf[6] << 8) | (uint16_t)raw_buf[7]);

    /* Extract big-endian signed 16-bit gyroscope readings */
    p_dev->gyro_x_raw  = (int16_t)(((uint16_t)raw_buf[8] << 8) | (uint16_t)raw_buf[9]);
    p_dev->gyro_y_raw  = (int16_t)(((uint16_t)raw_buf[10] << 8) | (uint16_t)raw_buf[11]);
    p_dev->gyro_z_raw  = (int16_t)(((uint16_t)raw_buf[12] << 8) | (uint16_t)raw_buf[13]);

    /* Convert Accelerometer: Sensitivity = 16384 LSB/g for ±2g */
    p_dev->accel_x_g = (float)p_dev->accel_x_raw / 16384.0f;
    p_dev->accel_y_g = (float)p_dev->accel_y_raw / 16384.0f;
    p_dev->accel_z_g = (float)p_dev->accel_z_raw / 16384.0f;

    /* Convert Gyroscope: Sensitivity = 131.0 LSB/(deg/s) for ±250 dps */
    p_dev->gyro_x_dps = (float)p_dev->gyro_x_raw / 131.0f;
    p_dev->gyro_y_dps = (float)p_dev->gyro_y_raw / 131.0f;
    p_dev->gyro_z_dps = (float)p_dev->gyro_z_raw / 131.0f;

    /* Convert Temperature based on sensor family:
     * MPU6050: Temp = (raw / 340.0) + 36.53
     * MPU6500 / MPU9250: Temp = ((raw - 0) / 333.87) + 21.0
     */
    if (p_dev->who_am_i == MPU_ID_MPU6050)
    {
        p_dev->temp_c = ((float)p_dev->temp_raw / 340.0f) + 36.53f;
    }
    else
    {
        p_dev->temp_c = (((float)p_dev->temp_raw) / 333.87f) + 21.0f;
    }

    return 0;
}
