/**
 * @file timer_measurement.c
 * @brief High-resolution hardware timer implementation using ARM Cortex-M33 DWT.
 */

#include "timer_measurement.h"

static uint32_t s_cycles_per_us = 200U;
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

    s_cycles_per_us = s_system_clock_hz / 1000000U;
    if (s_cycles_per_us == 0U)
    {
        s_cycles_per_us = 200U;
    }

    /* Enable CoreDebug Trace and DWT Cycle Counter */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0U;
}

uint32_t timer_cycles_delta_to_us(uint32_t start_cycles, uint32_t end_cycles)
{
    /* Unsigned 32-bit subtraction correctly handles a single timer rollover */
    uint32_t delta_cycles = end_cycles - start_cycles;
    return delta_cycles / s_cycles_per_us;
}

uint32_t timer_cycles_to_us(uint32_t cycles)
{
    return cycles / s_cycles_per_us;
}

uint32_t timer_elapsed_us(uint32_t start_cycles)
{
    uint32_t current_cycles = DWT->CYCCNT;
    return (current_cycles - start_cycles) / s_cycles_per_us;
}

void timer_delay_us(uint32_t us)
{
    if (us == 0U)
    {
        return;
    }

    uint32_t target_cycles = us * s_cycles_per_us;
    uint32_t start_cycles = DWT->CYCCNT;

    while ((DWT->CYCCNT - start_cycles) < target_cycles)
    {
        __NOP();
    }
}

void timer_delay_ms(uint32_t ms)
{
    while (ms > 0U)
    {
        timer_delay_us(1000U);
        ms--;
    }
}

uint32_t timer_get_system_clock_hz(void)
{
    return s_system_clock_hz;
}
