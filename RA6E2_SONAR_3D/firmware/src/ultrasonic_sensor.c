/**
 * @file ultrasonic_sensor.c
 * @brief High-reliability HC-SR04 ultrasonic distance sensor driver implementation.
 */

#include "ultrasonic_sensor.h"
#include "timer_measurement.h"
#include "r_ioport.h"
#include <math.h>

static ultrasonic_config_t s_config = {
    .trig_pin            = ULTRASONIC_DEFAULT_TRIG_PIN,
    .echo_pin            = ULTRASONIC_DEFAULT_ECHO_PIN,
    .rising_timeout_us   = ULTRASONIC_DEFAULT_RISING_TIMEOUT_US,
    .falling_timeout_us  = ULTRASONIC_DEFAULT_FALLING_TIMEOUT_US,
    .min_range_cm        = ULTRASONIC_MIN_RANGE_CM,
    .max_range_cm        = ULTRASONIC_MAX_RANGE_CM
};

static bool s_is_initialized = false;

/* Fast direct register control for TRIG (P105 / Port 1 Pin 5) */
#define TRIG_PIN_HIGH() (R_PORT1->PCNTR3 = (1UL << 5))
#define TRIG_PIN_LOW()  (R_PORT1->PCNTR3 = (1UL << (5 + 16)))

static inline uint32_t echo_pin_read(void)
{
    return R_BSP_PinRead(BSP_IO_PORT_04_PIN_08);
}

fsp_err_t ultrasonic_init(const ultrasonic_config_t *p_cfg)
{
    if (p_cfg != NULL)
    {
        s_config = *p_cfg;
    }

    /* Unlock Pin Configuration */
    R_BSP_PinAccessEnable();

    /* Configure TRIG (P105 / Arduino D2) as GPIO Output, initial LOW */
    R_BSP_PinCfg(BSP_IO_PORT_01_PIN_05, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW));

    /* Configure ECHO (P408 / Arduino D3) as GPIO Input */
    R_BSP_PinCfg(BSP_IO_PORT_04_PIN_08, (uint32_t)(IOPORT_CFG_PORT_DIRECTION_INPUT));

    /* Ensure TRIG is cleanly LOW */
    TRIG_PIN_LOW();

    s_is_initialized = true;
    return FSP_SUCCESS;
}

float ultrasonic_calculate_distance(uint32_t echo_us)
{
    /* Formula: Distance (cm) = Echo duration (us) * 0.0343 / 2 */
    return ((float)echo_us) * ULTRASONIC_US_TO_CM_FACTOR;
}

const char* ultrasonic_status_to_string(ultrasonic_status_t status)
{
    switch (status)
    {
        case ULTRASONIC_SUCCESS:
            return "SUCCESS";
        case ULTRASONIC_ERR_RISING_TIMEOUT:
            return "ECHO RISING TIMEOUT";
        case ULTRASONIC_ERR_FALLING_TIMEOUT:
            return "ECHO FALLING TIMEOUT";
        case ULTRASONIC_ERR_OUT_OF_RANGE_LOW:
            return "OUT OF RANGE (< 2 cm)";
        case ULTRASONIC_ERR_OUT_OF_RANGE_HIGH:
            return "OUT OF RANGE (> 400 cm)";
        case ULTRASONIC_ERR_INVALID_PULSE:
            return "INVALID PULSE";
        case ULTRASONIC_ERR_NO_OBJECT:
            return "NO OBJECT DETECTED";
        case ULTRASONIC_ERR_UNINITIALIZED:
            return "NOT INITIALIZED";
        default:
            return "UNKNOWN ERROR";
    }
}

ultrasonic_status_t ultrasonic_measure(ultrasonic_result_t *p_result)
{
    if (p_result == NULL)
    {
        return ULTRASONIC_ERR_UNINITIALIZED;
    }

    p_result->distance_cm  = 0.0f;
    p_result->echo_time_us = 0U;
    p_result->is_valid     = false;

    if (!s_is_initialized)
    {
        p_result->status     = ULTRASONIC_ERR_UNINITIALIZED;
        p_result->status_str = "DISCONNECTED";
        p_result->detail_str = ultrasonic_status_to_string(ULTRASONIC_ERR_UNINITIALIZED);
        return ULTRASONIC_ERR_UNINITIALIZED;
    }

    /* 1. Ensure TRIG is LOW before pulsing */
    TRIG_PIN_LOW();
    timer_delay_us(4U);

    /* 2. Generate 10 microsecond trigger pulse */
    TRIG_PIN_HIGH();
    timer_delay_us(10U);
    TRIG_PIN_LOW();

    /* 3. Wait for ECHO rising edge with hard timeout */
    uint32_t wait_start = timer_get_cycles();
    while (echo_pin_read() == 0U)
    {
        if (timer_elapsed_us(wait_start) > s_config.rising_timeout_us)
        {
            p_result->status     = ULTRASONIC_ERR_RISING_TIMEOUT;
            p_result->status_str = "TIMEOUT";
            p_result->detail_str = ultrasonic_status_to_string(ULTRASONIC_ERR_RISING_TIMEOUT);
            return ULTRASONIC_ERR_RISING_TIMEOUT;
        }
    }

    /* 4. Rising edge detected: start hardware timing */
    uint32_t echo_start = timer_get_cycles();

    /* 5. Wait for ECHO falling edge with hard timeout */
    while (echo_pin_read() != 0U)
    {
        if (timer_elapsed_us(echo_start) > s_config.falling_timeout_us)
        {
            p_result->status     = ULTRASONIC_ERR_FALLING_TIMEOUT;
            p_result->status_str = "TIMEOUT";
            p_result->detail_str = ultrasonic_status_to_string(ULTRASONIC_ERR_FALLING_TIMEOUT);
            return ULTRASONIC_ERR_FALLING_TIMEOUT;
        }
    }

    /* 6. Falling edge detected: capture end timestamp */
    uint32_t echo_end = timer_get_cycles();

    /* 7. Calculate pulse width in microseconds */
    uint32_t echo_us = timer_cycles_delta_to_us(echo_start, echo_end);
    p_result->echo_time_us = echo_us;

    /* 8. Glitch / invalid pulse check (< 100 us is below minimum physical capability) */
    if (echo_us < 100U)
    {
        p_result->status     = ULTRASONIC_ERR_INVALID_PULSE;
        p_result->status_str = "INVALID";
        p_result->detail_str = ultrasonic_status_to_string(ULTRASONIC_ERR_INVALID_PULSE);
        return ULTRASONIC_ERR_INVALID_PULSE;
    }

    /* 9. Calculate physical distance */
    float distance = ultrasonic_calculate_distance(echo_us);
    p_result->distance_cm = distance;

    /* 10. Validate physical range */
    if (distance < s_config.min_range_cm)
    {
        p_result->status     = ULTRASONIC_ERR_OUT_OF_RANGE_LOW;
        p_result->status_str = "OUT OF RANGE";
        p_result->detail_str = ultrasonic_status_to_string(ULTRASONIC_ERR_OUT_OF_RANGE_LOW);
        p_result->is_valid   = false;
        return ULTRASONIC_ERR_OUT_OF_RANGE_LOW;
    }
    else if (distance > s_config.max_range_cm)
    {
        p_result->status     = ULTRASONIC_ERR_OUT_OF_RANGE_HIGH;
        p_result->status_str = "OUT OF RANGE";
        p_result->detail_str = ultrasonic_status_to_string(ULTRASONIC_ERR_OUT_OF_RANGE_HIGH);
        p_result->is_valid   = false;
        return ULTRASONIC_ERR_OUT_OF_RANGE_HIGH;
    }

    /* 11. Measurement successful */
    p_result->status     = ULTRASONIC_SUCCESS;
    p_result->status_str = "CONNECTED";
    p_result->detail_str = ultrasonic_status_to_string(ULTRASONIC_SUCCESS);
    p_result->is_valid   = true;

    return ULTRASONIC_SUCCESS;
}

bool ultrasonic_self_test(void)
{
    /* Test 1: 1437 us -> ~24.65 cm (from prompt specification) */
    float dist1 = ultrasonic_calculate_distance(1437U);
    if (fabsf(dist1 - 24.64455f) > 0.05f)
    {
        return false;
    }

    /* Test 2: 583 us -> ~10.0 cm */
    float dist2 = ultrasonic_calculate_distance(583U);
    if (fabsf(dist2 - 10.0f) > 0.1f)
    {
        return false;
    }

    /* Test 3: 2915 us -> ~50.0 cm */
    float dist3 = ultrasonic_calculate_distance(2915U);
    if (fabsf(dist3 - 50.0f) > 0.2f)
    {
        return false;
    }

    /* Test 4: 0 us -> 0.0 cm */
    float dist0 = ultrasonic_calculate_distance(0U);
    if (fabsf(dist0) > 0.0001f)
    {
        return false;
    }

    return true;
}
