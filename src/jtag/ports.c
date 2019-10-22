/*******************************************************/
/* file: ports.c                                       */
/* abstract:  This file contains the routines to       */
/*            output values on the JTAG ports, to read */
/*            the TDO bit, and to read a byte of data  */
/*            from the prom                            */
/* Revisions:                                          */
/* 12/01/2008:  Same code as before (original v5.01).  */
/*              Updated comments to clarify instructions.*/
/*              Add print in setPort for xapp058_example.exe.*/
/*******************************************************/
#include <stdio.h>

#include "ports.h"
#include "../defs.h"
#include "../rpi-gpio.h"
#include "../fatfs/ff.h"
#include "../rgb_to_fb.h"

unsigned char *xsvf_data;

/* setPort:  Implement to set the named JTAG signal (p) to the new value (v).*/
/* if in debugging mode, then just set the variables */
void setPort(short p,short val)
{
   switch (p) {
   case TMS:
      RPI_SetGpioValue(TMS_PIN, val);
      break;
   case TDI:
      RPI_SetGpioValue(TDI_PIN, val);
      break;
   case TCK:
      RPI_SetGpioValue(TCK_PIN, val);
      break;
   default:
      break;
   }
   delay_in_arm_cycles(500);
}


/* toggle tck LH.  No need to modify this code.  It is output via setPort. */
void pulseClock()
{
    setPort(TCK,0);  /* set the TCK port to low  */
    setPort(TCK,1);  /* set the TCK port to high */
}


/* readByte:  Implement to source the next byte from your XSVF file location */
/* read in a byte of data from the prom */
void readByte(unsigned char *data)
{
   *data = *xsvf_data++;
}

/* readTDOBit:  Implement to return the current value of the JTAG TDO signal.*/
/* read the TDO bit from port */
unsigned char readTDOBit()
{
   return RPI_GetGpioValue(TDO_PIN);
}

/* waitTime:  Implement as follows: */
/* REQUIRED:  This function must consume/wait at least the specified number  */
/*            of microsec, interpreting microsec as a number of microseconds.*/
/* REQUIRED FOR SPARTAN/VIRTEX FPGAs and indirect flash programming:         */
/*            This function must pulse TCK for at least microsec times,      */
/*            interpreting microsec as an integer value.                     */
/* RECOMMENDED IMPLEMENTATION:  Pulse TCK at least microsec times AND        */
/*                              continue pulsing TCK until the microsec wait */
/*                              requirement is also satisfied.               */
void waitTime(long microsec)
{
    for (long i = 0; i < microsec; i++) {
        pulseClock();
    }
}
