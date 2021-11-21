#include <stdint.h>
#include "rpi-systimer.h"
#include "startup.h"

static rpi_sys_timer_t* rpiSystemTimer;

rpi_sys_timer_t* RPI_GetSystemTimer(void)
{
    rpiSystemTimer = (rpi_sys_timer_t*)RPI_SYSTIMER_BASE;
    return rpiSystemTimer;
}

void RPI_WaitMicroSeconds( uint32_t us )
{
    rpiSystemTimer = (rpi_sys_timer_t*)RPI_SYSTIMER_BASE;
    uint32_t ts = rpiSystemTimer->counter_lo;

    while ( ( rpiSystemTimer->counter_lo - ts ) < us )
    {
        /* BLANK */
    }
}
