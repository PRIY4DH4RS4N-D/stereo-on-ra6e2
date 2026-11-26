/**
 * @file timer_measurement.h
 * @brief High-resolution hardware timer interface for HC-SR04 pulse measurement.
 *
 * Utilizes the ARM Cortex-M33 DWT (Data Watchpoint and Trace) 32-bit cycle counter
 * running at SystemCoreClock (200 MHz on Renesas BGK-RA6E2).
 * Provides cycle-accurate timing with 5 ns resolution.
 */

#ifndef TIMER_MEASUREMENT_H_
#define TIMER_MEASUREMENT_H_

#include <stdint.h>
#include <stdbool.h>
#include "bsp_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the hardware DWT cycle counter and validates system frequency.
 */
void timer_init(void);

/**
 * @brief Gets the current 32-bit hardware cycle count.
 * @return Current DWT->CYCCNT value.
 */
static inline uint32_t timer_get_cycles(void)
{
    return DWT->CYCCNT;
}

/**
 * @brief Calculates the elapsed microseconds between two cycle count timestamps.
 * Handles 32-bit unsigned arithmetic rollover seamlessly.
 *
 * @param start_cycles Timestamp captured via timer_get_cycles()
 * @param end_cycles   Timestamp captured via timer_get_cycles()
 * @return Elapsed time in microseconds.
 */
uint32_t timer_cycles_delta_to_us(uint32_t start_cycles, uint32_t end_cycles);

/**
 * @brief Converts raw cycle count delta to microseconds.
 * @param cycles Number of clock cycles.
 * @return Microseconds equivalent.
 */
uint32_t timer_cycles_to_us(uint32_t cycles);

/**
 * @brief Calculates elapsed microseconds from start_cycles to the current moment.
 * @param start_cycles Timestamp captured earlier.
 * @return Elapsed time in microseconds.
 */
uint32_t timer_elapsed_us(uint32_t start_cycles);

/**
 * @brief Cycle-accurate microsecond busy-delay.
 * @param us Duration in microseconds (e.g. 10 us for TRIG pulse).
 */
void timer_delay_us(uint32_t us);

/**
 * @brief Cycle-accurate millisecond busy-delay.
 * @param ms Duration in milliseconds (e.g. 100-200 ms between pings).
 */
void timer_delay_ms(uint32_t ms);

/**
 * @brief Gets the configured system core clock frequency in Hz.
 * @return Clock frequency in Hz (typically 200,000,000).
 */
uint32_t timer_get_system_clock_hz(void);

#ifdef __cplusplus
}
#endif

#endif /* TIMER_MEASUREMENT_H_ */
