/**
 * @file mpu6050.c
 * @brief MPU6050 6-Axis IMU Driver Implementation.
 */

#include "mpu6050.h"
#include "i2c_bus.h"
#include "timer_measurement.h"
#include <stddef.h>

mpu6050_status_t mpu6050_read_id(uint8_t *p_who_am_i)
{
    if (p_who_am_i == NULL)
    {
        return MPU6050_ERR_READ_FAIL;
    }

    i2c_status_t status = i2c_read_reg(MPU6050_DEFAULT_I2C_ADDR, MPU6050_REG_WHO_AM_I, p_who_am_i);
    if (status != I2C_SUCCESS)
    {
        return MPU6050_ERR_NOT_FOUND;
    }

    return MPU6050_OK;
}

mpu6050_status_t mpu6050_init(void)
{
    uint8_t id = 0;

    /* 1. Verify Device Presence and Identity */
    mpu6050_status_t id_status = mpu6050_read_id(&id);
    if (id_status != MPU6050_OK)
    {
        return MPU6050_ERR_NOT_FOUND;
    }

    if (id != MPU6050_WHO_AM_I_VAL)
    {
        return MPU6050_ERR_ID_MISMATCH;
    }

    /* 2. Wake up MPU6050 by writing 0x00 to PWR_MGMT_1 (clears sleep bit 6) */
    if (i2c_write_reg(MPU6050_DEFAULT_I2C_ADDR, MPU6050_REG_PWR_MGMT_1, 0x00U) != I2C_SUCCESS)
    {
        return MPU6050_ERR_WAKEUP_FAIL;
    }

    /* Allow internal clock generator to stabilize */
    timer_delay_ms(15U);

    /* 3. Configure Accelerometer Full Scale to ±2g (0x00 = ±2g) */
    if (i2c_write_reg(MPU6050_DEFAULT_I2C_ADDR, MPU6050_REG_ACCEL_CONFIG, 0x00U) != I2C_SUCCESS)
    {
        return MPU6050_ERR_CONFIG_FAIL;
    }

    /* 4. Configure Gyroscope Full Scale to ±250 dps (0x00 = ±250 deg/s) */
    if (i2c_write_reg(MPU6050_DEFAULT_I2C_ADDR, MPU6050_REG_GYRO_CONFIG, 0x00U) != I2C_SUCCESS)
    {
        return MPU6050_ERR_CONFIG_FAIL;
    }

    timer_delay_ms(10U);

    return MPU6050_OK;
}

mpu6050_status_t mpu6050_read_all(mpu6050_data_t *p_data)
{
    if (p_data == NULL)
    {
        return MPU6050_ERR_READ_FAIL;
    }

    uint8_t buffer[14];

    /* Burst read 14 consecutive bytes:
     * [0..1]  = ACCEL_XOUT_H, ACCEL_XOUT_L
     * [2..3]  = ACCEL_YOUT_H, ACCEL_YOUT_L
     * [4..5]  = ACCEL_ZOUT_H, ACCEL_ZOUT_L
     * [6..7]  = TEMP_OUT_H,  TEMP_OUT_L
     * [8..9]  = GYRO_XOUT_H,  GYRO_XOUT_L
     * [10..11]= GYRO_YOUT_H,  GYRO_YOUT_L
     * [12..13]= GYRO_ZOUT_H,  GYRO_ZOUT_L
     */
    if (i2c_read_bytes(MPU6050_DEFAULT_I2C_ADDR, MPU6050_REG_ACCEL_XOUT_H, buffer, sizeof(buffer)) != I2C_SUCCESS)
    {
        return MPU6050_ERR_READ_FAIL;
    }

    /* Reconstruct signed 16-bit integers from MSB and LSB */
    p_data->accel_x_raw = (int16_t)(((uint16_t)buffer[0] << 8) | (uint16_t)buffer[1]);
    p_data->accel_y_raw = (int16_t)(((uint16_t)buffer[2] << 8) | (uint16_t)buffer[3]);
    p_data->accel_z_raw = (int16_t)(((uint16_t)buffer[4] << 8) | (uint16_t)buffer[5]);

    p_data->temp_raw    = (int16_t)(((uint16_t)buffer[6] << 8) | (uint16_t)buffer[7]);

    p_data->gyro_x_raw  = (int16_t)(((uint16_t)buffer[8] << 8) | (uint16_t)buffer[9]);
    p_data->gyro_y_raw  = (int16_t)(((uint16_t)buffer[10] << 8) | (uint16_t)buffer[11]);
    p_data->gyro_z_raw  = (int16_t)(((uint16_t)buffer[12] << 8) | (uint16_t)buffer[13]);

    /* Convert to engineering physical units */
    p_data->accel_x_g   = (float)p_data->accel_x_raw / MPU6050_ACCEL_SENS_2G;
    p_data->accel_y_g   = (float)p_data->accel_y_raw / MPU6050_ACCEL_SENS_2G;
    p_data->accel_z_g   = (float)p_data->accel_z_raw / MPU6050_ACCEL_SENS_2G;

    p_data->gyro_x_dps  = (float)p_data->gyro_x_raw / MPU6050_GYRO_SENS_250DPS;
    p_data->gyro_y_dps  = (float)p_data->gyro_y_raw / MPU6050_GYRO_SENS_250DPS;
    p_data->gyro_z_dps  = (float)p_data->gyro_z_raw / MPU6050_GYRO_SENS_250DPS;

    /* Temperature formula from MPU6050 Register Map spec:
     * Temperature in degrees C = (TEMP_OUT Register Value as a signed quantity) / 340 + 36.53
     */
    p_data->temp_c      = ((float)p_data->temp_raw / MPU6050_TEMP_SENS) + MPU6050_TEMP_OFFSET;

    return MPU6050_OK;
}
