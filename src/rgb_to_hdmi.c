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

#define CORE_FREQ          383800000

#define DEFAULT_GPCLK_DIVISOR     18      // 64MHz
#define DEFAULT_CHARS_PER_LINE    80
#define DEFAULT_BYTES_PER_LINE   320

#define MODE7_GPCLK_DIVISOR       24      // 48MHz
#define MODE7_CHARS_PER_LINE      63
#define MODE7_BYTES_PER_LINE     320

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

extern int rgb_to_fb(unsigned char *fb, int chars_per_line, int bytes_per_line, int mode7);

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
// Source 5 = PLLC = core_freq * 3 = (384 * 3) = 1152
// Source 6 = PLLD = 500MHz

void init_gpclk(int source, int divisor) {
   log_info("A GP_CLK1_DIV = %08"PRIx32, *GP_CLK1_DIV);

   log_info("B GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);

   // Stop the clock generator
   *GP_CLK1_CTL = 0x5a000000 | source;
   
   // Wait for BUSY low
   log_info("C GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);
   while ((*GP_CLK1_CTL) & GZ_CLK_BUSY) {}
   log_info("D GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);

   // Configure the clock generator
   *GP_CLK1_DIV = 0x5A000000 | (divisor << 12);
   *GP_CLK1_CTL = 0x5A000000 | source;

   log_info("E GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);

   // Start the clock generator
   *GP_CLK1_CTL = 0x5A000010 | source;

   log_info("F GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);
   while (!((*GP_CLK1_CTL) & GZ_CLK_BUSY)) {}    // Wait for BUSY high
   log_info("G GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);

   log_info("H GP_CLK1_DIV = %08"PRIx32, *GP_CLK1_DIV);
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
   RPI_PropertyAddTag( TAG_SET_VIRTUAL_SIZE, SCREEN_WIDTH, SCREEN_HEIGHT );
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
        log_info( "%dbpp", bpp );
    }

    if( ( mp = RPI_PropertyGet( TAG_GET_PITCH ) ) )
    {
        pitch = mp->data.buffer_32[0];
        log_info( "Pitch: %d bytes", pitch );
    }

    if( ( mp = RPI_PropertyGet( TAG_ALLOCATE_BUFFER ) ) )
    {
        fb = (unsigned char*)mp->data.buffer_32[0];
        log_info( "Framebuffer address: %8.8X", (unsigned int)fb );
    }

    // On the Pi 2/3 the mailbox returns the address with bits 31..30 set, which is wrong
    fb = (unsigned char *)(((unsigned int) fb) & 0x3fffffff);
    
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
}

void init_hardware() {
   int i;
   for (i = 0; i < 12; i++) {
      RPI_SetGpioPinFunction(PIXEL_BASE + i, FS_INPUT);
   }
   RPI_SetGpioPinFunction(PSYNC_PIN, FS_INPUT);
   RPI_SetGpioPinFunction(CSYNC_PIN, FS_INPUT);
   RPI_SetGpioPinFunction(MODE7_PIN, FS_OUTPUT);

   // Tune the clock to a multiple of 48MHz and 64MHz (384MHz)
   RPI_PropertyInit();
   RPI_PropertyAddTag( TAG_SET_CLOCK_RATE, CORE_CLK_ID, CORE_FREQ, 1);
   RPI_PropertyProcess();

   // Configure the GPCLK to 64MHz
   RPI_SetGpioPinFunction(GPCLK_PIN, FS_ALT5);

   // Re-initialize UART, as system clock rate changed
   RPI_AuxMiniUartInit( 115200, 8 );

   // Initialise the info system with cached values (as we break the GPU property interface)
   init_info();

#ifdef DEBUG
   dump_useful_info();
#endif

}

void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
{
   int mode7  = 1;

   RPI_AuxMiniUartInit( 115200, 8 );

   log_info("RGB to HDMI booted");

   init_hardware();
   init_framebuffer();

   enable_MMU_and_IDCaches();
   _enable_unaligned_access();
   
   while (1) {
      int divisor = mode7 ? MODE7_GPCLK_DIVISOR : DEFAULT_GPCLK_DIVISOR;
      int chars_per_line = mode7 ? MODE7_CHARS_PER_LINE : DEFAULT_CHARS_PER_LINE;
      int bytes_per_line = mode7 ? MODE7_BYTES_PER_LINE : DEFAULT_BYTES_PER_LINE;
      RPI_SetGpioValue(MODE7_PIN, mode7);
      if (mode7) {
         log_info("Setting up for mode 7, divisor = %d", divisor);
      } else {
         log_info("Setting up for modes 0..6, divisor = %d", divisor);
      }
      init_gpclk(5, divisor);
      log_info("Done setting up");
      log_info("Entering rgb_to_fb");
      mode7 = rgb_to_fb(fb, chars_per_line, bytes_per_line, mode7);
      log_info("Leaving rgb_to_fb %d", mode7);
   }

   log_info("RGB to HDMI finished");
  
   while (1);
}
