#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>


#include "rpi-base.h"
#include "rpi-gpio.h"
#include "rpi-interrupts.h"

/** @brief The BCM2835/6 Interupt controller peripheral at it's base address */
static rpi_irq_controller_t* rpiIRQController;

/**
    @brief Return the IRQ Controller register set
*/
rpi_irq_controller_t* RPI_GetIrqController( void )
{
    rpiIRQController = (rpi_irq_controller_t*)RPI_INTERRUPT_CONTROLLER_BASE;
    return rpiIRQController;
}

