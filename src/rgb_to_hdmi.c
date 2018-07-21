#include "rpi-aux.h"
#include "rpi-mailbox-interface.h"
#include "info.h"
#include "logging.h"

void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
{
   RPI_AuxMiniUartInit( 115200, 8 );

   log_info("RGB to HDMI booted");

   // Switch to new core clock speed
   RPI_PropertyInit();
   RPI_PropertyAddTag( TAG_SET_CLOCK_RATE, CORE_CLK_ID, 384123456, 1);
   RPI_PropertyProcess();

   dump_useful_info();

   while(1);
   
}
