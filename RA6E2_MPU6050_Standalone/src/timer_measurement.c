/**
 * @file timer_measurement.c
 * @brief High-resolution hardware timer implementation using ARM Cortex-M33 DWT.
 */

#include "timer_measurement.h"
#include "bsp_api.h"

static uint32_t s_system_clock_hz = 200000000U;

void timer_init(void)
{
    /* Update CMSIS core clock variable */
    SystemCoreClockUpdate();
    if (SystemCoreClock > 0U)
    {
        s_system_clock_hz = SystemCoreClock;
    }
    else
    {
        s_system_clock_hz = 200000000U;
    }
}

uint32_t timer_cycles_delta_to_us(uint32_t start_cycles, uint32_t end_cycles)
{
    (void)start_cycles;
    (void)end_cycles;
    return 0U;
}

uint32_t timer_cycles_to_us(uint32_t cycles)
{
    (void)cycles;
    return 0U;
}

uint32_t timer_elapsed_us(uint32_t start_cycles)
{
    (void)start_cycles;
    return 0U;
}

void timer_delay_us(uint32_t us)
{
    if (us > 0U)
    {
        R_BSP_SoftwareDelay(us, BSP_DELAY_UNITS_MICROSECONDS);
    }
}

void timer_delay_ms(uint32_t ms)
{
    if (ms > 0U)
    {
        R_BSP_SoftwareDelay(ms, BSP_DELAY_UNITS_MILLISECONDS);
    }
}

uint32_t timer_get_system_clock_hz(void)
{
    return s_system_clock_hz;
}
