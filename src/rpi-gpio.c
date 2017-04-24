#include <stdint.h>
#include "rpi-gpio.h"

rpi_gpio_t* RPI_GpioBase = (rpi_gpio_t*) RPI_GPIO_BASE;

void RPI_SetGpioPinFunction(rpi_gpio_pin_t gpio, rpi_gpio_alt_function_t func)
{
  rpi_reg_rw_t* fsel_reg = &RPI_GpioBase->GPFSEL[gpio / 10];

  rpi_reg_rw_t fsel_copy = *fsel_reg;
  fsel_copy &= ~(FS_MASK << ((gpio % 10) * 3));
  fsel_copy |= (func << ((gpio % 10) * 3));
  *fsel_reg = fsel_copy;
}

void RPI_SetGpioOutput(rpi_gpio_pin_t gpio)
{
  RPI_SetGpioPinFunction(gpio, FS_OUTPUT);
}

void RPI_SetGpioInput(rpi_gpio_pin_t gpio)
{
  RPI_SetGpioPinFunction(gpio, FS_INPUT);
}

rpi_gpio_value_t RPI_GetGpioValue(rpi_gpio_pin_t gpio)
{
  rpi_gpio_value_t result = RPI_IO_UNKNOWN;

  switch (gpio / 32)
  {
    case 0:
      result = RPI_GpioBase->GPLEV0 & (1 << gpio);
    break;

    case 1:
      result = RPI_GpioBase->GPLEV1 & (1 << (gpio - 32));
    break;

    default:
    break;
  }

  if (result != RPI_IO_UNKNOWN)
  {
    if (result)
      result = RPI_IO_HI;
  }

  return result;
}

void RPI_ToggleGpio(rpi_gpio_pin_t gpio)
{
  if (RPI_GetGpioValue(gpio))
    RPI_SetGpioLo(gpio);
  else
    RPI_SetGpioHi(gpio);
}

void RPI_SetGpioHi(rpi_gpio_pin_t gpio)
{
  switch (gpio / 32)
  {
    case 0:
      RPI_GpioBase->GPSET0 = (1 << gpio);
    break;

    case 1:
      RPI_GpioBase->GPSET1 = (1 << (gpio - 32));
    break;

    default:
    break;
  }
}

void RPI_SetGpioLo(rpi_gpio_pin_t gpio)
{
  switch (gpio / 32)
  {
    case 0:
      RPI_GpioBase->GPCLR0 = (1 << gpio);
    break;

    case 1:
      RPI_GpioBase->GPCLR1 = (1 << (gpio - 32));
    break;

    default:
    break;
  }
}

void RPI_SetGpioValue(rpi_gpio_pin_t gpio, rpi_gpio_value_t value)
{
  if ((value == RPI_IO_LO) || (value == RPI_IO_OFF))
    RPI_SetGpioLo(gpio);
  else if ((value == RPI_IO_HI) || (value == RPI_IO_ON))
    RPI_SetGpioHi(gpio);
}
