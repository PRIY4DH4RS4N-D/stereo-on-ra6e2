/**
 * @file mpu6050.h
 * @brief Multi-Instance MPU6050 6-Axis MotionTracking IMU Driver for RA6E2.
 */

#ifndef MPU6050_H_
#define MPU6050_H_

#include <stdint.h>
#include <stdbool.h>
#include "i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Default I2C Device Address (AD0 connected to GND) */
#define MPU6050_I2C_ADDR            (0x68)

/* MPU6050 Register Map */
#define MPU6050_REG_XG_OFFS_TC      (0x00)
#define MPU6050_REG_SMPLRT_DIV      (0x19)
#define MPU6050_REG_CONFIG          (0x1A)
#define MPU6050_REG_GYRO_CONFIG     (0x1B)
#define MPU6050_REG_ACCEL_CONFIG    (0x1C)
#define MPU6050_REG_ACCEL_XOUT_H    (0x3B)
#define MPU6050_REG_TEMP_OUT_H      (0x41)
#define MPU6050_REG_GYRO_XOUT_H     (0x43)
#define MPU6050_REG_PWR_MGMT_1      (0x6B)
#define MPU6050_REG_PWR_MGMT_2      (0x6C)
#define MPU6050_REG_WHO_AM_I        (0x75)

#define MPU6050_WHO_AM_I_VAL        (0x68)

/* Scaling Factors */
#define MPU6050_ACCEL_SCALE_2G      (16384.0f) /* LSB / g */
#define MPU6050_GYRO_SCALE_250DPS   (131.0f)   /* LSB / (deg/s) */

typedef enum {
    MPU6050_OK = 0,
    MPU6050_ERR_I2C,
    MPU6050_ERR_ID_MISMATCH,
    MPU6050_ERR_NOT_RESPONDING,
    MPU6050_ERR_PARAM
} mpu6050_status_t;

typedef struct {
    int16_t accel_x_raw;
    int16_t accel_y_raw;
    int16_t accel_z_raw;
    int16_t temp_raw;
    int16_t gyro_x_raw;
    int16_t gyro_y_raw;
    int16_t gyro_z_raw;

    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    float gyro_x_dps;
    float gyro_y_dps;
    float gyro_z_dps;
    float temp_c;
} mpu6050_data_t;

typedef struct {
    const char         *name;            /* "MPU6050 NECK" or "MPU6050 CENTER" */
    i2c_bus_t          *p_bus;           /* Pointer to dedicated I2C bus */
    uint8_t             dev_addr;        /* 7-bit I2C address (default 0x68) */
    uint8_t             who_am_i;        /* Read identity */
    bool                is_initialized;  /* Initialization state */
    mpu6050_data_t      data;            /* Latest telemetry data */
} mpu6050_device_t;

/**
 * @brief Reads the WHO_AM_I register (0x75) of the specified sensor instance.
 * @param p_dev Pointer to MPU6050 device structure.
 * @param p_who_am_i Output pointer for read WHO_AM_I value.
 * @return MPU6050_OK on success.
 */
mpu6050_status_t mpu6050_read_id(mpu6050_device_t *p_dev, uint8_t *p_who_am_i);

/**
 * @brief Wakes up and configures the specified MPU6050 instance (±2g, ±250 dps).
 * @param p_dev Pointer to MPU6050 device structure.
 * @return MPU6050_OK on success.
 */
mpu6050_status_t mpu6050_init(mpu6050_device_t *p_dev);

/**
 * @brief Performs 14-byte continuous burst read of all motion & temperature registers.
 * @param p_dev Pointer to MPU6050 device structure.
 * @return MPU6050_OK on success.
 */
mpu6050_status_t mpu6050_read_all(mpu6050_device_t *p_dev);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H_ */
