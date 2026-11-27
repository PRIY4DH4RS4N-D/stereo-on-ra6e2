/**
 * @file mpu6050.h
 * @brief MPU6050 6-Axis MotionTracking IMU Driver for Renesas RA6E2.
 */

#ifndef MPU6050_H_
#define MPU6050_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Default I2C Address (AD0 pin connected to GND) */
#define MPU6050_DEFAULT_I2C_ADDR   (0x68U)

/* MPU6050 Register Map */
#define MPU6050_REG_SMPLRT_DIV     (0x19U)
#define MPU6050_REG_CONFIG         (0x1AU)
#define MPU6050_REG_GYRO_CONFIG    (0x1BU)
#define MPU6050_REG_ACCEL_CONFIG   (0x1CU)
#define MPU6050_REG_ACCEL_XOUT_H   (0x3BU)
#define MPU6050_REG_TEMP_OUT_H     (0x41U)
#define MPU6050_REG_GYRO_XOUT_H    (0x43U)
#define MPU6050_REG_PWR_MGMT_1     (0x6BU)
#define MPU6050_REG_PWR_MGMT_2     (0x6CU)
#define MPU6050_REG_WHO_AM_I       (0x75U)

/* Expected WHO_AM_I Identity value */
#define MPU6050_WHO_AM_I_VAL       (0x68U)

/* Sensitivities for conversion */
#define MPU6050_ACCEL_SENS_2G      (16384.0f)  /* LSB/g at ±2g */
#define MPU6050_GYRO_SENS_250DPS   (131.0f)    /* LSB/(deg/s) at ±250 dps */
#define MPU6050_TEMP_SENS          (340.0f)    /* LSB/°C */
#define MPU6050_TEMP_OFFSET        (36.53f)    /* °C offset */

typedef enum {
    MPU6050_OK                 = 0,
    MPU6050_ERR_NOT_FOUND      = -1,
    MPU6050_ERR_ID_MISMATCH    = -2,
    MPU6050_ERR_WAKEUP_FAIL    = -3,
    MPU6050_ERR_CONFIG_FAIL    = -4,
    MPU6050_ERR_READ_FAIL      = -5
} mpu6050_status_t;

typedef struct {
    /* Raw 16-bit signed sensor registers */
    int16_t accel_x_raw;
    int16_t accel_y_raw;
    int16_t accel_z_raw;
    int16_t gyro_x_raw;
    int16_t gyro_y_raw;
    int16_t gyro_z_raw;
    int16_t temp_raw;

    /* Scaled physical values */
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    float gyro_x_dps;
    float gyro_y_dps;
    float gyro_z_dps;
    float temp_c;
} mpu6050_data_t;

/**
 * @brief Initialize MPU6050 sensor:
 *        1. Verifies WHO_AM_I register (0x75) == 0x68
 *        2. Wakes up device (writes 0x00 to PWR_MGMT_1 0x6B)
 *        3. Configures Accelerometer to ±2g (ACCEL_CONFIG 0x1C = 0x00)
 *        4. Configures Gyroscope to ±250 dps (GYRO_CONFIG 0x1B = 0x00)
 * 
 * @return MPU6050_OK on success, error code otherwise.
 */
mpu6050_status_t mpu6050_init(void);

/**
 * @brief Read WHO_AM_I register directly.
 * @param p_who_am_i Pointer to store 8-bit ID.
 * @return MPU6050_OK on success, error code otherwise.
 */
mpu6050_status_t mpu6050_read_id(uint8_t *p_who_am_i);

/**
 * @brief Burst-read all accelerometer, temperature, and gyroscope registers (14 bytes)
 *        and calculate scaled physical units.
 * 
 * @param p_data Pointer to mpu6050_data_t structure.
 * @return MPU6050_OK on success, error code otherwise.
 */
mpu6050_status_t mpu6050_read_all(mpu6050_data_t *p_data);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H_ */
