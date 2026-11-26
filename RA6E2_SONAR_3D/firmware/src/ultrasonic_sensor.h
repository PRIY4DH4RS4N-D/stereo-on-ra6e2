/**
 * @file ultrasonic_sensor.h
 * @brief High-reliability HC-SR04 ultrasonic distance sensor driver for RA6E2.
 *
 * Implements:
 * - 10 us trigger pulse generation.
 * - Hardware cycle-accurate echo pulse width measurement.
 * - Hard timeout protection on rising and falling edges (never gets stuck).
 * - Physical range validation (2.0 cm - 400.0 cm).
 * - Self-test verification independent of hardware.
 */

#ifndef ULTRASONIC_SENSOR_H_
#define ULTRASONIC_SENSOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "bsp_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Speed of sound in air at 20 deg C (m/s).
 * 343 m/s = 0.0343 cm/us.
 */
#define ULTRASONIC_SPEED_OF_SOUND_CM_US (0.0343f)

/**
 * @brief Standard distance formula multiplier:
 * Distance (cm) = Echo duration (us) * 0.0343 / 2
 */
#define ULTRASONIC_US_TO_CM_FACTOR      (ULTRASONIC_SPEED_OF_SOUND_CM_US / 2.0f) /* ~0.01715 */

/**
 * @brief Physical range limits for HC-SR04
 */
#define ULTRASONIC_MIN_RANGE_CM         (2.0f)
#define ULTRASONIC_MAX_RANGE_CM         (400.0f)

/**
 * @brief Default timeouts in microseconds
 */
#define ULTRASONIC_DEFAULT_RISING_TIMEOUT_US  (5000U)  /* 5 ms max wait for burst start */
#define ULTRASONIC_DEFAULT_FALLING_TIMEOUT_US (35000U) /* 35 ms max wait (~600 cm equivalent) */

/**
 * @brief Default Hardware Pin mapping on BGK-RA6E2:
 * TRIG: Arduino D2 (P105 / BSP_IO_PORT_01_PIN_05)
 * ECHO: Arduino D3 (P408 / BSP_IO_PORT_04_PIN_08) via 5V-to-3.3V voltage divider
 */
#define ULTRASONIC_DEFAULT_TRIG_PIN     (BSP_IO_PORT_01_PIN_05)
#define ULTRASONIC_DEFAULT_ECHO_PIN     (BSP_IO_PORT_04_PIN_08)

/**
 * @brief Detailed measurement and sensor status codes.
 */
typedef enum {
    ULTRASONIC_SUCCESS = 0,             /**< Measurement successful and in valid range */
    ULTRASONIC_ERR_RISING_TIMEOUT,      /**< ECHO did not go HIGH (sensor disconnected or unpowered) */
    ULTRASONIC_ERR_FALLING_TIMEOUT,     /**< ECHO stayed HIGH too long (no return or timeout) */
    ULTRASONIC_ERR_OUT_OF_RANGE_LOW,    /**< Object too close (< 2.0 cm) */
    ULTRASONIC_ERR_OUT_OF_RANGE_HIGH,   /**< Object too far (> 400.0 cm) */
    ULTRASONIC_ERR_INVALID_PULSE,       /**< Unphysical pulse width (< 116 us or glitch) */
    ULTRASONIC_ERR_NO_OBJECT,           /**< No obstacle detected within timeout */
    ULTRASONIC_ERR_UNINITIALIZED        /**< Driver not initialized */
} ultrasonic_status_t;

/**
 * @brief Sensor configuration parameters.
 */
typedef struct {
    bsp_io_port_pin_t trig_pin;          /**< GPIO pin for TRIG output */
    bsp_io_port_pin_t echo_pin;          /**< GPIO pin for ECHO input */
    uint32_t rising_timeout_us;         /**< Max wait for echo rising edge */
    uint32_t falling_timeout_us;        /**< Max wait for echo falling edge */
    float min_range_cm;                 /**< Minimum valid distance in cm */
    float max_range_cm;                 /**< Maximum valid distance in cm */
} ultrasonic_config_t;

/**
 * @brief Complete measurement result payload.
 */
typedef struct {
    ultrasonic_status_t status;         /**< Status code */
    uint32_t echo_time_us;              /**< Measured pulse width in microseconds */
    float distance_cm;                  /**< Calculated distance in centimeters */
    bool is_valid;                      /**< True if measurement is valid */
    const char *status_str;             /**< High-level status ("CONNECTED", "TIMEOUT", etc.) */
    const char *detail_str;             /**< Detailed status message */
} ultrasonic_result_t;

/**
 * @brief Initializes the ultrasonic sensor GPIO pins and driver.
 * @param p_cfg Pointer to configuration struct, or NULL to use defaults.
 * @return FSP_SUCCESS on successful initialization.
 */
fsp_err_t ultrasonic_init(const ultrasonic_config_t *p_cfg);

/**
 * @brief Performs a full ultrasonic measurement cycle.
 *
 * Sequence:
 * 1. Emits a clean 10 us trigger pulse on TRIG.
 * 2. Monitors ECHO for rising edge with hard timeout (non-blocking recovery).
 * 3. Records start timestamp via 32-bit hardware DWT cycle counter.
 * 4. Monitors ECHO for falling edge with hard timeout.
 * 5. Records end timestamp.
 * 6. Calculates pulse duration in microseconds.
 * 7. Applies distance formula: Distance = (Echo us * 0.0343) / 2.
 * 8. Validates range boundaries.
 *
 * @param p_result Pointer to store the measurement result.
 * @return Status code (ULTRASONIC_SUCCESS on valid reading).
 */
ultrasonic_status_t ultrasonic_measure(ultrasonic_result_t *p_result);

/**
 * @brief Calculates distance in cm from raw echo duration in microseconds.
 * Distance = echo_us * 0.0343 / 2
 *
 * @param echo_us Pulse width in microseconds.
 * @return Distance in centimeters.
 */
float ultrasonic_calculate_distance(uint32_t echo_us);

/**
 * @brief Returns human-readable string for status code.
 * @param status Status code.
 * @return Short description string.
 */
const char* ultrasonic_status_to_string(ultrasonic_status_t status);

/**
 * @brief Runs an automated self-test on distance calculation algorithms.
 * Checks known values (e.g. 1437 us -> 24.65 cm, 583 us -> 10.0 cm) and boundary conditions.
 *
 * @return True if all mathematical tests pass.
 */
bool ultrasonic_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* ULTRASONIC_SENSOR_H_ */
