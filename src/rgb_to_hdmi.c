#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include "cache.h"
#include "defs.h"
#include "cpld.h"
#include "info.h"
#include "logging.h"
#include "rpi-aux.h"
#include "rpi-gpio.h"
#include "rpi-mailbox-interface.h"
#include "startup.h"
#include "rpi-mailbox.h"
#include "osd.h"
#include "cpld.h"
#include "cpld_normal.h"
#include "cpld_alternative.h"

// #define INSTRUMENT_CAL
#define NUM_CAL_PASSES 1

#define SHORT_PRESS 0
#define  LONG_PRESS 1

#ifdef DOUBLE_BUFFER
#include "rpi-interrupts.h"
#endif

typedef void (*func_ptr)();

#define GZ_CLK_BUSY    (1 << 7)
#define GP_CLK1_CTL (volatile uint32_t *)(PERIPHERAL_BASE + 0x101078)
#define GP_CLK1_DIV (volatile uint32_t *)(PERIPHERAL_BASE + 0x10107C)

// =============================================================
// Global variables
// =============================================================

cpld_t *cpld = NULL;
unsigned char *fb = NULL;
int pitch = 0;

// =============================================================
// Local variables
// =============================================================

static int width = 0;
static int height = 0;
static uint32_t cpld_version_id;
static int delay;
static int elk;
static int mode7;
static int last_mode7;
static int result;
static int chars_per_line;

// TODO: Don't hardcode pitch!
static char last[SCREEN_HEIGHT * 336] __attribute__((aligned(32)));

#ifndef USE_PROPERTY_INTERFACE_FOR_FB
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
#endif

// =============================================================
// External symbols from rgb_to_fb.S
// =============================================================

extern int rgb_to_fb(unsigned char *fb, int chars_per_line, int bytes_per_line, int mode7);
extern int measure_vsync();


// =============================================================
// Private methods
// =============================================================


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

static void init_gpclk(int source, int divisor) {
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

#ifdef USE_PROPERTY_INTERFACE_FOR_FB

static void init_framebuffer(int mode7) {

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

static void init_framebuffer(int mode7) {
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

static int calibrate_clock() {
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

static void init_hardware() {
   int i;
   for (i = 0; i < 12; i++) {
      RPI_SetGpioPinFunction(PIXEL_BASE + i, FS_INPUT);
   }
   RPI_SetGpioPinFunction(PSYNC_PIN, FS_INPUT);
   RPI_SetGpioPinFunction(CSYNC_PIN, FS_INPUT);
   RPI_SetGpioPinFunction(MODE7_PIN, FS_OUTPUT);
   RPI_SetGpioPinFunction(SP_CLK_PIN, FS_OUTPUT);
   RPI_SetGpioPinFunction(SP_CLKEN_PIN, FS_OUTPUT);
   RPI_SetGpioPinFunction(SP_DATA_PIN, FS_OUTPUT);
   RPI_SetGpioPinFunction(MUX_PIN, FS_OUTPUT);
   RPI_SetGpioPinFunction(LED1_PIN, FS_OUTPUT);
   RPI_SetGpioPinFunction(SW1_PIN, FS_INPUT);
   RPI_SetGpioPinFunction(LINK_PIN, FS_INPUT);
   RPI_SetGpioPinFunction(VERSION_PIN, FS_OUTPUT);

   RPI_SetGpioValue(VERSION_PIN, 1);
   RPI_SetGpioValue(SP_CLK_PIN, 1);
   RPI_SetGpioValue(SP_DATA_PIN, 0);
   RPI_SetGpioValue(MUX_PIN, 0);
   RPI_SetGpioValue(LED1_PIN, 1); // 1 is off
   RPI_SetGpioValue(SP_CLKEN_PIN, 0);

#ifdef DOUBLE_BUFFER
   // This line enables IRQ interrupts
   // Enable smi_int which is IRQ 48
   // https://github.com/raspberrypi/firmware/issues/67
   RPI_GetIrqController()->Enable_IRQs_2 = (1 << VSYNCINT);
#endif

   // Initialize hardware cycle counter
   _init_cycle_counter();

   // Measure the frame time and set a clock to 384MHz +- the error
   calibrate_clock();

   // Configure the GPCLK pin as a GPCLK
   RPI_SetGpioPinFunction(GPCLK_PIN, FS_ALT5);

   // Initialise the info system with cached values (as we break the GPU property interface)
   init_info();

#ifdef DEBUG
   dump_useful_info();
#endif
}

static void cpld_init() {
   // Assert the active low version pin
   RPI_SetGpioValue(VERSION_PIN, 0);
   // The CPLD now outputs a identifier and version number on the 12-bit pixel quad bus
   cpld_version_id = 0;
   for (int i = PIXEL_BASE + 11; i >= PIXEL_BASE; i--) {
      cpld_version_id <<= 1;
      cpld_version_id |= RPI_GetGpioValue(i) & 1;
   }
   // Release the active low version pin
   RPI_SetGpioValue(VERSION_PIN, 1);

   // Set the appropriate cpld "driver" based on the version
   if ((cpld_version_id >> VERSION_DESIGN_BIT) == DESIGN_NORMAL) {
      cpld = &cpld_normal;
   } else if ((cpld_version_id >> VERSION_DESIGN_BIT) == DESIGN_ALTERNATIVE) {
      cpld = &cpld_alternative;
   } else {
      log_info("Unknown CPLD: identifier = %03x", cpld_version_id);
      log_info("Halting\n");
      while (1);
   }
   log_info("CPLD  Design: %s", cpld->name);
   log_info("CPLD Version: %x.%x", (cpld_version_id >> VERSION_MAJOR_BIT) & 0x0f, (cpld_version_id >> VERSION_MINOR_BIT) & 0x0f);

   // Initialize the CPLD's default sampling points
   cpld->init();
}

static int test_for_elk(int mode7, int chars_per_line) {

   // If mode 7, then assume the Beeb
   if (mode7) {
      return 0;
   }

   unsigned int flags = BIT_CALIBRATE | BIT_CAL_COUNT;
   unsigned char *fb1 = fb;
   unsigned char *fb2 = fb + SCREEN_HEIGHT * pitch;

   // Grab one field
   rgb_to_fb(fb1, chars_per_line, pitch, flags);

   // Grab second field
   rgb_to_fb(fb2, chars_per_line, pitch, flags);

   unsigned int min_diff = INT_MAX;
   unsigned int min_offset = 0;

   for (int offset = -2; offset <= 2; offset += 2) {

      uint32_t *p1 = (uint32_t *)(fb1 + 2 * pitch);
      uint32_t *p2 = (uint32_t *)(fb2 + 2 * pitch + offset * pitch);
      unsigned int diff = 0;
      for (int i = 0; i < (SCREEN_HEIGHT - 4) * pitch; i += 4) {
         uint32_t d = (*p1++) ^ (*p2++);
         while (d) {
            if (d & 0x0F) {
               diff++;
            }
            d >>= 4;
         }
      }
      if (diff < min_diff) {
         min_diff = diff;
         min_offset = offset;
      }
      log_debug("offset = %d, diff = %u", offset, diff);

   }
   log_debug("min offset = %d", min_offset);
   return min_offset != 0;
}

#ifdef HAS_MULTICORE
static void start_core(int core, func_ptr func) {
   printf("starting core %d\r\n", core);
   *(unsigned int *)(0x4000008C + 0x10 * core) = (unsigned int) func;
}
#endif

// =============================================================
// Public methods
// =============================================================

int *diff_N_frames(int sp, int n, int mode7, int elk, int chars_per_line) {

   static int sum[3];
   static int min[3];
   static int max[3];
   static int diff[3];

   for (int i = 0; i < 3; i++) {
      sum[i] = 0;
      min[i] = INT_MAX;
      max[i] = INT_MIN;
   }

#ifdef INSTRUMENT_CAL
   unsigned int t;
   unsigned int t_capture = 0;
   unsigned int t_memcpy = 0;
   unsigned int t_compare = 0;
#endif

   // In mode 0..6, set BIT_CAL_COUNT to 1 (capture 1 field)
   // In mode 7, set BIT_CAL_COUNT to 0 (capture two fields, doesn't matter whether odd-even or even-odd)
   unsigned int flags = mode7 | BIT_CALIBRATE | (mode7 ? 0 : BIT_CAL_COUNT) | (elk ? BIT_ELK : 0);

#ifdef INSTRUMENT_CAL
   t = _get_cycle_counter();
#endif
   // Grab an initial frame
   rgb_to_fb(fb, chars_per_line, pitch, flags);
#ifdef INSTRUMENT_CAL
   t_capture += _get_cycle_counter() - t;
#endif

   for (int i = 0; i < n; i++) {
      diff[0] = 0;
      diff[1] = 0;
      diff[2] = 0;

#ifdef INSTRUMENT_CAL
      t = _get_cycle_counter();
#endif
      // Save the last frame
      memcpy((void *)last, (void *)fb, SCREEN_HEIGHT * pitch);
#ifdef INSTRUMENT_CAL
      t_memcpy += _get_cycle_counter() - t;
      t = _get_cycle_counter();
#endif
      // Grab the next frame
      rgb_to_fb(fb, chars_per_line, pitch, flags);
#ifdef INSTRUMENT_CAL
      t_capture += _get_cycle_counter() - t;
      t = _get_cycle_counter();
#endif
      // Compare the frames
      uint32_t *fbp = (uint32_t *)fb;
      uint32_t *lastp = (uint32_t *)last;
      for (int j = 0; j < SCREEN_HEIGHT * pitch; j += 4) {
         uint32_t d = (*fbp++) ^ (*lastp++);
         // Mask out OSD
         d &= 0x77777777;
         while (d) {
            if (d & 0x01) {
               diff[0]++;
            }
            if (d & 0x02) {
               diff[1]++;
            }
            if (d & 0x04) {
               diff[2]++;
            }
            d >>= 4;
         }
      }
#ifdef INSTRUMENT_CAL
      t_compare += _get_cycle_counter() - t;
#endif

      // Accumulate the result
      for (int j = 0; j < 3; j++) {
         sum[j] += diff[j];
         if (diff[j] < min[j]) {
            min[j] = diff[j];
         }
         if (diff[j] > max[j]) {
            max[j] = diff[j];
         }
      }
   }

   for (int j = 0; j < 3; j++) {
      int mean = sum[j] / n;
      log_debug("sample point %d channel %d: diff:  sum = %d mean = %d, min = %d, max = %d", sp, j, sum[j], mean, min[j], max[j]);
   }

#ifdef INSTRUMENT_CAL
   log_debug("t_capture total = %d, mean = %d ", t_capture, t_capture / (n + 1));
   log_debug("t_compare total = %d, mean = %d ", t_compare, t_compare / n);
   log_debug("t_memcpy  total = %d, mean = %d ", t_memcpy,  t_memcpy / n);
   log_debug("total = %d", t_capture + t_compare + t_memcpy);
#endif
   return sum;
}

#if 0
int total_N_frames(int sp, int n, int mode7, int elk, int chars_per_line) {

   int sum = 0;
   int min = INT_MAX;
   int max = INT_MIN;

#ifdef INSTRUMENT_CAL
   unsigned int t;
   unsigned int t_capture = 0;
   unsigned int t_compare = 0;
#endif

   // In mode 0..6, set BIT_CAL_COUNT to 1 (capture 1 field)
   // In mode 7, set BIT_CAL_COUNT to 0 (capture two fields, doesn't matter whether odd-even or even-odd)
   unsigned int flags = mode7 | BIT_CALIBRATE | (mode7 ? 0 : BIT_CAL_COUNT) | (elk ? BIT_ELK : 0);

   for (int i = 0; i < n; i++) {
      int total = 0;

      // Grab the next frame
      rgb_to_fb(fb, chars_per_line, pitch, flags);
#ifdef INSTRUMENT_CAL
      t_capture += _get_cycle_counter() - t;
      t = _get_cycle_counter();
#endif
      // Compare the frames
      uint32_t *fbp = (uint32_t *)fb;
      for (int j = 0; j < SCREEN_HEIGHT * pitch; j += 4) {
         uint32_t f = *fbp++;
         // Mask out OSD
         f &= 0x77777777;
         while (f) {
            if (f & 0x0F) {
               total++;
            }
            f >>= 4;
         }
      }
#ifdef INSTRUMENT_CAL
      t_compare += _get_cycle_counter() - t;
#endif

      // Accumulate the result
      sum += total;
      if (total < min) {
         min = total;
      }
      if (total > max) {
         max = total;
      }
   }

   int mean = sum / n;
   log_debug("sample point %d: total: sum = %d mean = %d, min = %d, max = %d", sp, sum, mean, min, max);
#ifdef INSTRUMENT_CAL
   log_debug("t_capture total = %d, mean = %d ", t_capture, t_capture / (n + 1));
   log_debug("t_compare total = %d, mean = %d ", t_compare, t_compare / n);
   log_debug("total = %d", t_capture + t_compare + t_memcpy);
#endif
   return sum;
}
#endif

// Wait for a switch to be released
int wait_for_sw_release(int switch_pin) {
   int bit = 0;
   int length = 0;
   volatile int j;
   log_debug("waiting for switch release (gpio%d)", switch_pin);
   do {
      bit = RPI_GetGpioValue(switch_pin);
      for (j = 0; j < 10000; j++);
      length ++;
   } while (bit == 0);
   log_debug("switch release (length %d)", length);
   return (length > 10000) ? LONG_PRESS : SHORT_PRESS;
}

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

void action_calibrate() {
   elk = test_for_elk(mode7, chars_per_line);
   log_debug("Elk mode = %d", elk);
   for (int c = 0; c < NUM_CAL_PASSES; c++) {
      cpld->calibrate(elk, chars_per_line);
   }
}

void rgb_to_hdmi_main() {

   // The divisor us now the same for both modes
   log_debug("Setting up divisor");
   init_gpclk(GPCLK_SOURCE, DEFAULT_GPCLK_DIVISOR);
   log_debug("Done setting up divisor");

   // Initialize the cpld after the gpclk generator has been started
   cpld_init();

   // Determine initial mode
   mode7 = rgb_to_fb(fb, 0, 0, BIT_PROBE);
   elk = 0;

   while (1) {
      log_debug("Setting mode7 = %d", mode7);
      RPI_SetGpioValue(MODE7_PIN, mode7);

      log_debug("Setting up frame buffer");
      init_framebuffer(mode7);
      log_debug("Done setting up frame buffer");

      log_debug("Loading sample points");
      cpld->set_mode(mode7);
      log_debug("Done loading sample points");

      chars_per_line = mode7 ? MODE7_CHARS_PER_LINE : DEFAULT_CHARS_PER_LINE;

      int clear = BIT_CLEAR;

      do {

         log_debug("Entering rgb_to_fb");
         result = rgb_to_fb(fb, chars_per_line, pitch, mode7 | BIT_INITIALIZE | (elk ? BIT_ELK : 0) | clear);
         log_debug("Leaving rgb_to_fb, result= %d", result);
         clear = 0;

         if (result & RET_SW1) {
            osd_key(OSD_SW1);
            wait_for_sw_release(SW1_PIN);
         }
         if (result & RET_SW2) {
            osd_key(OSD_SW2);
            wait_for_sw_release(SW2_PIN);
         }
         if (result & RET_SW3) {
            osd_key(OSD_SW3);
            wait_for_sw_release(SW3_PIN);
         }

         last_mode7 = mode7;
         mode7 = result & BIT_MODE7;

      } while (mode7 == last_mode7);
   }
}

void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
{
   RPI_AuxMiniUartInit( 115200, 8 );

   log_info("RGB to HDMI booted");

   init_hardware();

   enable_MMU_and_IDCaches();
   _enable_unaligned_access();

   osd_init();

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
