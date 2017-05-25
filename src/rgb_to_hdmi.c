#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include "cache.h"
#include "defs.h"
#include "info.h"
#include "logging.h"
#include "rpi-aux.h"
#include "rpi-gpio.h"
#include "rpi-mailbox-interface.h"
#include "startup.h"
#include "rpi-mailbox.h"

#ifdef DOUBLE_BUFFER
#include "rpi-interrupts.h"
#endif

static int sp_mode7_A = 3;
static int sp_mode7_B = 3;
static int sp_mode7_C = 3;
static int sp_default = 3;

typedef void (*func_ptr)();

#define GZ_CLK_BUSY    (1 << 7)
#define GP_CLK1_CTL (volatile uint32_t *)(PERIPHERAL_BASE + 0x101078)
#define GP_CLK1_DIV (volatile uint32_t *)(PERIPHERAL_BASE + 0x10107C)

extern int rgb_to_fb(unsigned char *fb, int chars_per_line, int bytes_per_line, int mode7);

extern int measure_vsync();

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
   log_debug("A GP_CLK1_DIV = %08"PRIx32, *GP_CLK1_DIV);

   log_debug("B GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);

   // Stop the clock generator
   *GP_CLK1_CTL = 0x5a000000 | source;

   // Wait for BUSY low
   log_debug("C GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);
   while ((*GP_CLK1_CTL) & GZ_CLK_BUSY) {}
   log_debug("D GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);

   // Configure the clock generator
   *GP_CLK1_DIV = 0x5A000000 | (divisor << 12);
   *GP_CLK1_CTL = 0x5A000000 | source;

   log_debug("E GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);

   // Start the clock generator
   *GP_CLK1_CTL = 0x5A000010 | source;

   log_debug("F GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);
   while (!((*GP_CLK1_CTL) & GZ_CLK_BUSY)) {}    // Wait for BUSY high
   log_debug("G GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);

   log_debug("H GP_CLK1_DIV = %08"PRIx32, *GP_CLK1_DIV);
}

static unsigned char* fb = NULL;
static int width = 0, height = 0, pitch = 0;

#ifdef USE_PROPERTY_INTERFACE_FOR_FB

void init_framebuffer(int mode7) {

   rpi_mailbox_property_t *mp;

   int w = mode7 ? SCREEN_WIDTH_MODE7 : SCREEN_WIDTH_MODE06;

   /* Initialise a framebuffer... */
   RPI_PropertyInit();
   RPI_PropertyAddTag( TAG_ALLOCATE_BUFFER );
   RPI_PropertyAddTag( TAG_SET_PHYSICAL_SIZE, w, SCREEN_HEIGHT );
#ifdef DOUBLE_BUFFER
   RPI_PropertyAddTag( TAG_SET_VIRTUAL_SIZE, w, SCREEN_HEIGHT * 2 );
#else
   RPI_PropertyAddTag( TAG_SET_VIRTUAL_SIZE, w, SCREEN_HEIGHT );
#endif
   RPI_PropertyAddTag( TAG_SET_DEPTH, SCREEN_DEPTH );
   if (SCREEN_DEPTH <= 8) {
      RPI_PropertyAddTag( TAG_SET_PALETTE );
   }
   RPI_PropertyAddTag( TAG_GET_PITCH );
   RPI_PropertyAddTag( TAG_GET_PHYSICAL_SIZE );
   RPI_PropertyAddTag( TAG_GET_DEPTH );

   RPI_PropertyProcess();

   // FIXME: A small delay (like the log) is neccessary here
   // or the RPI_PropertyGet seems to return garbage
   log_info( "Initialised Framebuffer" );

   if( ( mp = RPI_PropertyGet( TAG_GET_PHYSICAL_SIZE ) ) )
   {
      width = mp->data.buffer_32[0];
      height = mp->data.buffer_32[1];

      log_info( "Size: %dx%d ", width, height );
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
}

#else

// An alternative way to initialize the framebuffer using mailbox channel 1
//
// I was hoping it would then be possible to page flip just by modifying the structure
// in-place. Unfortunately that didn't work, but the code might be useful in the future.

typedef struct {
   uint32_t width;
   uint32_t height;
   uint32_t virtual_width;
   uint32_t virtual_height;
   volatile uint32_t pitch;
   volatile uint32_t depth;
   uint32_t x_offset;
   uint32_t y_offset;
   volatile uint32_t pointer;
   volatile uint32_t size;
} framebuf;

// The + 0x10000 is to miss the property buffer

static framebuf *fbp = (framebuf *) (UNCACHED_MEM_BASE + 0x10000);

void init_framebuffer(int mode7) {
   log_info( "Framebuf struct address: %p", fbp );

   int w = mode7 ? SCREEN_WIDTH_MODE7 : SCREEN_WIDTH_MODE06;

   // Fill in the frame buffer structure
   fbp->width          = w;
   fbp->height         = SCREEN_HEIGHT;
   fbp->virtual_width  = w;
#ifdef DOUBLE_BUFFER
   fbp->virtual_height = SCREEN_HEIGHT * 2;
#else
   fbp->virtual_height = SCREEN_HEIGHT;
#endif
   fbp->pitch          = 0;
   fbp->depth          = SCREEN_DEPTH;
   fbp->x_offset       = 0;
   fbp->y_offset       = 0;
   fbp->pointer        = 0;
   fbp->size           = 0;

   // Send framebuffer struct to the mailbox
   //
   // The +0x40000000 ensures the GPU bypasses it's cache when accessing
   // the framebuffer struct. If this is not done, the screen still initializes
   // but the ARM doesn't see the updated value for a very long time
   // i.e. the commented out section of code below is needed, and this eventually
   // exits with i=4603039
   //
   // 0xC0000000 should be added if disable_l2cache=1
   RPI_Mailbox0Write(MB0_FRAMEBUFFER, ((unsigned int)fbp) + 0xC0000000 );

   // Wait for the response (0)
   RPI_Mailbox0Read( MB0_FRAMEBUFFER );

   pitch = fbp->pitch;
   fb = (unsigned char*)(fbp->pointer);
   width = fbp->width;
   height = fbp->height;

   // See comment above
   // int i  = 0;
   // while (!pitch || !fb) {
   //    pitch = fbp->pitch;
   //    fb = (unsigned char*)(fbp->pointer);
   //    i++;
   // }
   // log_info( "i=%d", i);

   log_info( "Initialised Framebuffer: %dx%d ", width, height );
   log_info( "Pitch: %d bytes", pitch );
   log_info( "Framebuffer address: %8.8X", (unsigned int)fb );

   // Initialize the palette
   if (SCREEN_DEPTH <= 8) {
      RPI_PropertyInit();
      RPI_PropertyAddTag( TAG_SET_PALETTE );
      RPI_PropertyProcess();
   }

   // On the Pi 2/3 the mailbox returns the address with bits 31..30 set, which is wrong
   fb = (unsigned char *)(((unsigned int) fb) & 0x3fffffff);
}

#endif

int delay;

int calibrate_clock() {
   int a = 13;
   unsigned int frame_ref;

   log_info("     Nominal clock = %d Hz", CORE_FREQ);

   unsigned int frame_time = measure_vsync();

   if (frame_time & INTERLACED_FLAG) {
      frame_ref = 40000000;
      log_info("Nominal frame time = %d ns (interlaced)", frame_ref);
   } else {
      frame_ref = 40000000 - 64000;
      log_info("Nominal frame time = %d ns (non-interlaced)", frame_ref);
   }
   frame_time &= ~INTERLACED_FLAG;

   log_info(" Actual frame time = %d ns", frame_time);

   double error = (double) frame_time / (double) frame_ref;
   log_info("  Frame time error = %d PPM", (int) ((error - 1.0) * 1e6));

   int new_clock = (int) (((double) CORE_FREQ) / error);
   log_info("     Optimal clock = %d Hz", new_clock);

   // Sanity check clock
   if (new_clock < 380000000 || new_clock > 388000000) {
      log_info("Clock out of range 380MHz-388MHz, defaulting to 384MHz");
      new_clock = CORE_FREQ;
   }

   // Wait a while to allow UART time to empty
   for (delay = 0; delay < 100000; delay++) {
      a = a * 13;
   }

   // Switch to new core clock speed
   RPI_PropertyInit();
   RPI_PropertyAddTag( TAG_SET_CLOCK_RATE, CORE_CLK_ID, new_clock, 1);
   RPI_PropertyProcess();

   // Re-initialize UART, as system clock rate changed
   RPI_AuxMiniUartInit( 115200, 8 );

   // Check the new clock
   int actual_clock = get_clock_rate(CORE_CLK_ID);
   log_info("       Final clock = %d Hz", actual_clock);

   return a;
}

void init_sampling_point_register(int mode7_a, int mode7_b, int mode7_c, int def) {
   int sp = (mode7_a & 7) | ((mode7_b & 7) << 3) | ((mode7_c & 7) << 6) | ((def & 7) << 9);
   int i;
   int j;
   for (i = 0; i < 12; i++) {
      RPI_SetGpioValue(SP_DATA_PIN, sp & 1);
      for (j = 0; j < 1000; j++);
      RPI_SetGpioValue(SP_CLK_PIN, 0);
      RPI_SetGpioValue(SP_CLK_PIN, 1);
      for (j = 0; j < 1000; j++);
      sp >>= 1;
   }
   RPI_SetGpioValue(SP_DATA_PIN, 0);
}

void init_hardware() {
   int i;
   for (i = 0; i < 12; i++) {
      RPI_SetGpioPinFunction(PIXEL_BASE + i, FS_INPUT);
   }
   RPI_SetGpioPinFunction(PSYNC_PIN, FS_INPUT);
   RPI_SetGpioPinFunction(CSYNC_PIN, FS_INPUT);
   RPI_SetGpioPinFunction(MODE7_PIN, FS_OUTPUT);
   RPI_SetGpioPinFunction(SP_CLK_PIN, FS_OUTPUT);
   RPI_SetGpioPinFunction(SP_DATA_PIN, FS_OUTPUT);
   RPI_SetGpioValue(SP_CLK_PIN, 1);
   RPI_SetGpioValue(SP_DATA_PIN, 0);

#ifdef DOUBLE_BUFFER
   // This line enables IRQ interrupts
   // Enable smi_int which is IRQ 48
   // https://github.com/raspberrypi/firmware/issues/67
   RPI_GetIrqController()->Enable_IRQs_2 = (1 << VSYNCINT);
#endif

   // Measure the frame time and set a clock to 384MHz +- the error
   calibrate_clock();

   // Configure the GPCLK pin as a GPCLK
   RPI_SetGpioPinFunction(GPCLK_PIN, FS_ALT5);

   // Initialize the sampling points
   init_sampling_point_register(sp_mode7_A, sp_mode7_B, sp_mode7_C, sp_default);

   // Initialise the info system with cached values (as we break the GPU property interface)
   init_info();

#ifdef DEBUG
   dump_useful_info();
#endif
}

// TODO: Clean this up

int diff_N_frames(int sp, int n, int mode7, int chars_per_line) {
   // TODO: Don't hardcode pitch!
   static char last[SCREEN_HEIGHT * 336];

//   int total_sum = 0;
//   int total_sum2 = 0;
   int diff_sum = 0;
   int diff_min = INT_MAX;
   int diff_max = INT_MIN;
//   int diff_sum2 = 0;

   // Grab an initial frame
   rgb_to_fb(fb, chars_per_line, pitch, mode7 | BIT_CALIBRATE);

   for (int i = 0; i < n; i++) {
//      int total = 0;
      int diff = 0;

      // Save the last frame
      memcpy((void *)last, (void *)fb, SCREEN_HEIGHT * pitch);

      // Grab the next frame
      rgb_to_fb(fb, chars_per_line, pitch, mode7 | BIT_CALIBRATE);

      // Compare the frames
      for (int j = 0; j < SCREEN_HEIGHT * pitch; j++) {
//         if (fb[j] & 0x0F) {
//            total++;
//         }
//         if (fb[j] & 0xF0) {
//            total++;
//         }
         int d = fb[j] ^ last[j];
         if (d & 0x0F) {
            diff++;
         }
         if (d & 0xF0) {
           diff++;
         }
      }

      // Accumulate the result
      diff_sum += diff;
      if (diff < diff_min) {
         diff_min = diff;
      }
      if (diff > diff_max) {
         diff_max = diff;
      }
//      diff_sum2 += diff * diff;
//      total_sum += total;
//      total_sum2 += total * total;
   }

//   TODO: Seeing regular random crashes with double version, suspect sqrt
//   double diff_mean = (double) diff_sum / (double) n;
//   double diff_stddev = sqrt((double) diff_sum2 / (double) n - diff_mean * diff_mean);
//   double total_mean = (double) total_sum / (double) n;
//   double total_stddev = sqrt((double) total_sum2 / (double) n - total_mean * total_mean);

   int diff_mean = diff_sum / n;
//   int diff_stddev = diff_sum2 / n - diff_mean * diff_mean;
//   int total_mean = total_sum / n;
//   int total_stddev = total_sum2 / n - total_mean * total_mean;

   // Displaying as integers, as printing of doubles seems broken
//   log_debug("total: mean = %d, variance = %d; diff: mean = %d, variance = %d",
//             (int) total_mean, (int) total_stddev, (int) diff_mean, (int) diff_stddev);


   log_debug("sample point %d: mean = %d, min = %d, max = %d", sp, diff_mean, diff_min, diff_max);
   return (int) diff_mean;
}

#define NUM_CAL_FRAMES 10

void calibrate_sampling(int mode7, int chars_per_line) {
   int i;
   int diff;
   int min_i;
   int min_diff;

   if (mode7) {
      log_info("Calibrating mode 7");

      for (int abc = 0; abc < 3; abc++) {
         min_diff = INT_MAX;
         min_i = 0;
         // TODO: investigate why 6 and 7 cause weird effects
         for (i = 0; i <= 5; i++) {
            
            switch (abc) {
            case 0:
               init_sampling_point_register(i, sp_mode7_B, sp_mode7_C, sp_default);
               break;
            case 1:
               init_sampling_point_register(sp_mode7_A, i, sp_mode7_C, sp_default);
               break;
            case 2:
               init_sampling_point_register(sp_mode7_A, sp_mode7_B, i, sp_default);
               break;
            }
            diff = diff_N_frames(i, NUM_CAL_FRAMES, mode7, chars_per_line);
            if (diff < min_diff) {
               min_i = i;
               min_diff = diff;
            }
         }
         switch (abc) {
         case 0:
            sp_mode7_A = min_i;
            log_debug("Setting sp_mode7_A = %d", min_i);
            break;
         case 1:
            sp_mode7_B = min_i;
            log_debug("Setting sp_mode7_B = %d", min_i);
            break;
         case 2:
            sp_mode7_C = min_i;
            log_debug("Setting sp_mode7_C = %d", min_i);
            break;
         }
      }

   } else {
      log_info("Calibrating modes 0..6");
      min_diff = INT_MAX;
      min_i = 0;
      for (i = 0; i <= 5; i++) {
         init_sampling_point_register(sp_mode7_A, sp_mode7_B, sp_mode7_C, i);
         diff = diff_N_frames(i, NUM_CAL_FRAMES, mode7, chars_per_line);
         if (diff < min_diff) {
            min_i = i;
            min_diff = diff;
         }
      }
      sp_default = min_i;
      log_debug("Setting sp_default = %d", min_i);
   }
   //
   log_info("Calibration complete: mode 7: %d %d %d; default: %d", 
             sp_mode7_A, sp_mode7_B, sp_mode7_C, sp_default);
   // Do a final update
   init_sampling_point_register(sp_mode7_A, sp_mode7_B, sp_mode7_C, sp_default);
}

void rgb_to_hdmi_main() {
   int mode7;

   // The divisor us now the same for both modes
   log_debug("Setting up divisor");
   init_gpclk(GPCLK_SOURCE, DEFAULT_GPCLK_DIVISOR);
   log_debug("Done setting up divisor");
   
   // Determine initial mode
   mode7 = rgb_to_fb(fb, 0, 0, BIT_PROBE);

   while (1) {
      log_debug("Setting mode7 = %d", mode7);
      RPI_SetGpioValue(MODE7_PIN, mode7);

      log_debug("Setting up frame buffer");
      init_framebuffer(mode7);
      log_debug("Done setting up frame buffer");

      int chars_per_line = mode7 ? MODE7_CHARS_PER_LINE : DEFAULT_CHARS_PER_LINE;

      calibrate_sampling(mode7, chars_per_line);

      log_debug("Entering rgb_to_fb %d", mode7);
      mode7 = rgb_to_fb(fb, chars_per_line, pitch, mode7);
      log_debug("Leaving rgb_to_fb %d", mode7);
   }
}

#ifdef HAS_MULTICORE
static void start_core(int core, func_ptr func) {
   printf("starting core %d\r\n", core);
   *(unsigned int *)(0x4000008C + 0x10 * core) = (unsigned int) func;
}
#endif

#ifdef DOUBLE_BUFFER
void swapBuffer(int buffer) {
   // Flush the previous response from the GPU->ARM mailbox
   // Doing it like this avoids stalling for the response
   RPI_Mailbox0Flush( MB0_TAGS_ARM_TO_VC  );
   RPI_PropertyInit();
   if (buffer) {
      RPI_PropertyAddTag( TAG_SET_VIRTUAL_OFFSET, 0, SCREEN_HEIGHT);
   } else {
      RPI_PropertyAddTag( TAG_SET_VIRTUAL_OFFSET, 0, 0);
   }
   // Use version that doesn't wait for the response
   RPI_PropertyProcessNoCheck();
}
#endif

void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
{
   RPI_AuxMiniUartInit( 115200, 8 );

   log_info("RGB to HDMI booted");

   init_hardware();

   enable_MMU_and_IDCaches();
   _enable_unaligned_access();

#ifdef HAS_MULTICORE
   int i;

   printf("main running on core %d\r\n", _get_core());

   for (i = 0; i < 10000000; i++);
   start_core(1, _spin_core);
   for (i = 0; i < 10000000; i++);
   start_core(2, _spin_core);
   for (i = 0; i < 10000000; i++);
   start_core(3, _spin_core);
   for (i = 0; i < 10000000; i++);
#endif

   rgb_to_hdmi_main();
}
