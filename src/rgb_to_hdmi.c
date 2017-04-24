#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "cache.h"
#include "defs.h"
#include "info.h"
#include "logging.h"
#include "rpi-aux.h"
#include "rpi-gpio.h"
#include "rpi-mailbox-interface.h"
#include "startup.h"

#define GZ_CLK_BUSY    (1 << 7)

#define GP_CLK1_CTL (uint32_t *)(PERIPHERAL_BASE + 0x101078)
#define GP_CLK1_DIV (uint32_t *)(PERIPHERAL_BASE + 0x10107C)

#define SCREEN_WIDTH    640
#define SCREEN_HEIGHT   512

#define BPP4

#ifdef BPP32
#define SCREEN_DEPTH    32
#define COLBITS         16
#endif

#ifdef BPP16
#define SCREEN_DEPTH    16
#define COLBITS         16
#endif

#ifdef BPP8
#define SCREEN_DEPTH    8
#define COLBITS         8
#endif

#ifdef BPP4
#define SCREEN_DEPTH    4
#define COLBITS         4
#endif

extern void rgb_to_fb(unsigned char *fb);


void init_gpclk(int source, int divisor) {


#if 0
   RPI_PropertyInit();
   RPI_PropertyAddTag( TAG_SET_CLOCK_RATE, PWM_CLK_ID, 64000000, 0);
   RPI_PropertyAddTag( TAG_SET_CLOCK_STATE, PWM_CLK_ID, 1);
   RPI_PropertyProcess();
#endif

   log_info("GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);
   log_info("GP_CLK1_DIV = %08"PRIx32, *GP_CLK1_DIV);

   *GP_CLK1_CTL = 0x5A000000 + source;

   while ((*GP_CLK1_CTL) & GZ_CLK_BUSY) {}    // Wait for BUSY low
   *GP_CLK1_DIV = 0x5A002000 | (divisor << 12); // set DIVI
   *GP_CLK1_CTL = 0x5A000010 | source;    // GPCLK0 on

   log_info("GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);
   log_info("GP_CLK1_DIV = %08"PRIx32, *GP_CLK1_DIV);

   log_info("Waiting for clock to start");
   while (!((*GP_CLK1_CTL) & GZ_CLK_BUSY)) {}    // Wait for BUSY high
   log_info("Done");

   log_info("GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);
   log_info("GP_CLK1_DIV = %08"PRIx32, *GP_CLK1_DIV);
}

static unsigned char* fb = NULL;
static int width = 0, height = 0, bpp = 0, pitch = 0;

void init_framebuffer() {

   int col, x, y;
   rpi_mailbox_property_t *mp;

   /* Initialise a framebuffer... */
   RPI_PropertyInit();
   RPI_PropertyAddTag( TAG_ALLOCATE_BUFFER );
   RPI_PropertyAddTag( TAG_SET_PHYSICAL_SIZE, SCREEN_WIDTH, SCREEN_HEIGHT );
   RPI_PropertyAddTag( TAG_SET_VIRTUAL_SIZE, SCREEN_WIDTH, SCREEN_HEIGHT * 2 );
   RPI_PropertyAddTag( TAG_SET_DEPTH, SCREEN_DEPTH );
   if (SCREEN_DEPTH <= 8) {
      RPI_PropertyAddTag( TAG_SET_PALETTE );
   }
   RPI_PropertyAddTag( TAG_GET_PITCH );
   RPI_PropertyAddTag( TAG_GET_PHYSICAL_SIZE );
   RPI_PropertyAddTag( TAG_GET_DEPTH );
   RPI_PropertyProcess();
   

   if( ( mp = RPI_PropertyGet( TAG_GET_PHYSICAL_SIZE ) ) )
   {
      width = mp->data.buffer_32[0];
      height = mp->data.buffer_32[1];

      log_info( "Initialised Framebuffer: %dx%d ", width, height );
   }

    if( ( mp = RPI_PropertyGet( TAG_GET_DEPTH ) ) )
    {
        bpp = mp->data.buffer_32[0];
        log_info( "%dbpp\r\n", bpp );
    }

    if( ( mp = RPI_PropertyGet( TAG_GET_PITCH ) ) )
    {
        pitch = mp->data.buffer_32[0];
        log_info( "Pitch: %d bytes\r\n", pitch );
    }

    if( ( mp = RPI_PropertyGet( TAG_ALLOCATE_BUFFER ) ) )
    {
        fb = (unsigned char*)mp->data.buffer_32[0];
        log_info( "Framebuffer address: %8.8X\r\n", (unsigned int)fb );
    }

    // On the Pi 2/3 the mailbox returns the address with bits 31..30 set, which is wrong
    fb = (unsigned char *)(((unsigned int) fb) & 0x3fffffff);

    // Change bpp from bits to bytes
    bpp >>= 3;

    //fb_clear();

    //fb_writes("\r\n\r\nACORN ATOM\r\n>");
    
#ifdef BPP32
    for (y = 0; y < 16; y++) {
       uint32_t *fbptr = (uint32_t *) (fb + pitch * y);
       for (col = 23; col >= 0; col--) {
          for (x = 0; x < 8; x++) {
             *fbptr++ = 1 << col;
          }
       }
    }
#endif
#ifdef BPP16
    for (y = 0; y < 16; y++) {
       uint16_t *fbptr = (uint16_t *) (fb + pitch * y);
       for (col = 15; col >= 0; col--) {
          for (x = 0; x < 8; x++) {
             *fbptr++ = 1 << col;
          }
       }
    }
#endif
#ifdef BPP8
    for (y = 0; y < 16; y++) {
       uint8_t *fbptr = (uint8_t *) (fb + pitch * y);
       for (col = 0; col < 8; col++) {
          for (x = 0; x < 8; x++) {
             *fbptr++ = col;
          }
       }
    }
#endif
#ifdef BPP4
    for (y = 0; y < 16; y++) {
       uint8_t *fbptr = (uint8_t *) (fb + pitch * y);
       for (col = 0; col < 8; col++) {
          for (x = 0; x < 8; x++) {
             *fbptr++ = col | col << 4;
          }
       }
    }
#endif

    

    // Uncomment this for testing from the serial port
    // while (1) {
    //    int c = RPI_AuxMiniUartRead();
    //    fb_writec(c);
    // }

}

void init_hardware() {
   int i;
   for (i = 0; i < 12; i++) {
      RPI_SetGpioPinFunction(PIXEL_BASE + i, FS_INPUT);
   }
   RPI_SetGpioPinFunction(PSYNC_PIN, FS_INPUT);
   RPI_SetGpioPinFunction(HSYNC_PIN, FS_INPUT);
   RPI_SetGpioPinFunction(VSYNC_PIN, FS_INPUT);
   RPI_SetGpioPinFunction(FIELD_PIN, FS_INPUT);

   // Configure the GPCLK to 64MHz
   RPI_SetGpioPinFunction(GPCLK_PIN, FS_ALT5);


   // 0     0 Hz     Ground
   // 1     19.2 MHz oscillator
   // 2     0 Hz     testdebug0
   // 3     0 Hz     testdebug1
   // 4     0 Hz     PLLA
   // 5     1000 MHz PLLC (changes with overclock settings)
   // 6     500 MHz  PLLD
   // 7     216 MHz  HDMI auxiliary
   // 8-15  0 Hz     Ground


   // Source 1 = OSC = 19.2MHz
   // Source 4 = PLLA = 0MHz
   // Source 5 = PLLC = core_freq * 3 (
   // Source 6 = PLLD = 500MHz

   init_gpclk(5, 18);


  // Initialise the info system with cached values (as we break the GPU property interface)
  init_info();

#ifdef DEBUG
  dump_useful_info();
#endif

}


void flash() {
   int i;
   RPI_SetGpioPinFunction(LED_PIN, FS_OUTPUT);
   while (1) {

      LED_ON();
      for (i = 0; i < 10000000; i++);
      LED_OFF();
      for (i = 0; i < 10000000; i++);
   }

}

void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
{

     // Initialise the UART to 57600 baud
   RPI_AuxMiniUartInit( 115200, 8 );

   log_info("RGB to HDMI booted\r\n");

   init_hardware();
   init_framebuffer();


   enable_MMU_and_IDCaches();
   _enable_unaligned_access();


   while (1) {
      rgb_to_fb(fb);
   }


   log_info("RGB to HDMI finished\r\n");
  
   while (1);
}
