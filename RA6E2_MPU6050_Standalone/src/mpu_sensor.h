/**
 * @file mpu_sensor.h
 * @brief Unified Independent Driver for MPU6050, MPU6500 and MPU9250 IMUs.
 */

#ifndef MPU_SENSOR_H_
#define MPU_SENSOR_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPU_REG_SMPLRT_DIV      (0x19U)
#define MPU_REG_CONFIG          (0x1AU)
#define MPU_REG_GYRO_CONFIG     (0x1BU)
#define MPU_REG_ACCEL_CONFIG    (0x1CU)
#define MPU_REG_ACCEL_XOUT_H    (0x3BU)
#define MPU_REG_TEMP_OUT_H      (0x41U)
#define MPU_REG_GYRO_XOUT_H     (0x43U)
#define MPU_REG_PWR_MGMT_1      (0x6BU)
#define MPU_REG_WHO_AM_I        (0x75U)

/* Expected WHO_AM_I Register Values */
#define MPU_ID_MPU6050          (0x68U)
#define MPU_ID_MPU6500          (0x70U)
#define MPU_ID_MPU9250          (0x71U)
#define MPU_ID_MPU9255          (0x73U)

typedef int8_t (*mpu_i2c_write_fn)(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
typedef int8_t (*mpu_i2c_read_fn)(uint8_t dev_addr, uint8_t reg_addr, uint8_t *p_buf, uint16_t length);

typedef struct {
    const char        *label;          /* Descriptive name */
    uint8_t            dev_addr;       /* 7-bit I2C Address (default 0x68) */
    mpu_i2c_write_fn   write_reg;      /* Bus-specific write function */
    mpu_i2c_read_fn    read_bytes;     /* Bus-specific read function */
    uint8_t            who_am_i;       /* Detected WHO_AM_I byte */
    const char        *model_name;     /* Model: "MPU6050", "MPU6500", "MPU9250", etc. */
    bool               is_initialized; /* Initialization state */

    /* Raw 16-bit signed registers */
    int16_t            accel_x_raw;
    int16_t            accel_y_raw;
    int16_t            accel_z_raw;
    int16_t            gyro_x_raw;
    int16_t            gyro_y_raw;
    int16_t            gyro_z_raw;
    int16_t            temp_raw;

    /* Calibrated physical engineering units */
    float              accel_x_g;      /* in g (+/- 2g range) */
    float              accel_y_g;
    float              accel_z_g;
    float              gyro_x_dps;     /* in degrees per second (+/- 250 dps) */
    float              gyro_y_dps;
    float              gyro_z_dps;
    float              temp_c;         /* in degrees Celsius */
} mpu_device_t;

/**
 * @brief Probes device, detects sensor model, wakes up sensor, and configures ±2g and ±250dps.
 * @param p_dev Pointer to mpu_device_t configuration.
 * @return 0 on success, negative error code on failure.
 */
int8_t mpu_init(mpu_device_t *p_dev);

/**
 * @brief Reads burst of Accel (6B), Temp (2B), and Gyro (6B) registers (14 bytes total).
 * @param p_dev Pointer to mpu_device_t configuration.
 * @return 0 on success, negative error code on failure.
 */
int8_t mpu_read_all(mpu_device_t *p_dev);

#ifdef __cplusplus
}
#endif

#endif /* MPU_SENSOR_H_ */
