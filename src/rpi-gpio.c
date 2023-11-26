#include <stdint.h>
#include "rpi-gpio.h"
#include "defs.h"
#include "rgb_to_fb.h"
#include "rgb_to_hdmi.h"
#include "logging.h"

rpi_gpio_t* RPI_GpioBase;

void RPI_SetGpioPinFunction(rpi_gpio_pin_t gpio, rpi_gpio_alt_function_t func)
{
   RPI_GpioBase = (rpi_gpio_t*) RPI_GPIO_BASE;
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
   uint32_t result;
   RPI_GpioBase = (rpi_gpio_t*) RPI_GPIO_BASE;
   switch (gpio / 32)
   {
   case 0:
      result = RPI_GpioBase->GPLEV0 & (1 << gpio);
      break;

   case 1:
      result = RPI_GpioBase->GPLEV1 & (1 << (gpio - 32));
      break;

   default:
      return RPI_IO_UNKNOWN;
      break;
   }

   if (result) {
      return RPI_IO_HI;
   } else {
      return RPI_IO_LO;
   }
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
   RPI_GpioBase = (rpi_gpio_t*) RPI_GPIO_BASE;
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
   RPI_GpioBase = (rpi_gpio_t*) RPI_GPIO_BASE;
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

void RPI_SetGpioPullUpDown(uint32_t gpio, uint32_t pull) {
#if defined(RPI4)
   rpi_reg_rw_t* pull_reg = &RPI_GpioBase->GPPULL[gpio / 16];
   rpi_reg_rw_t pull_copy = *pull_reg;
   pull_copy &= (uint32_t)~(0x3 << ((gpio % 16) * 2));
   pull_copy |= (pull << ((gpio % 16) * 2));
   *pull_reg = pull_copy;
#else
   RPI_GpioBase = (rpi_gpio_t*) RPI_GPIO_BASE;
   //log_info("Pull Type: %08X, %02X", gpio, pull);
   RPI_GpioBase->GPPUD = pull;
   delay_in_arm_cycles_cpu_adjust(5000);
   RPI_GpioBase->GPPUDCLK0 = gpio;
   delay_in_arm_cycles_cpu_adjust(5000);
   RPI_GpioBase->GPPUD = 0x0;      //clear GPPUD
   RPI_GpioBase->GPPUDCLK0 = 0x0;  //clear GPPUDCLK0
#endif
}
