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
#include "rpi-interrupts.h"
#include "rpi-mailbox-interface.h"
#include "startup.h"
#include "rpi-mailbox.h"
#include "osd.h"
#include "cpld.h"
#include "cpld_atom.h"
#include "cpld_rgb.h"
#include "cpld_yuv.h"
#include "cpld_simple.h"
#include "cpld_null.h"
#include "geometry.h"
#include "filesystem.h"
#include "rgb_to_fb.h"
#include "jtag/update_cpld.h"

// #define INSTRUMENT_CAL
#define NUM_CAL_PASSES 1

typedef void (*func_ptr)();

// =============================================================
// Define the PLL to be used for the sampling clock
// =============================================================
//
// Choose between PLLA, PLLC and PLLD
//
// PLLA is the auxiliary PLL, used to drive the CCP2 (Compact Camera Port 2) transmitter clock.
// PLLB is the CPU clock
// PLLC is the core PLL, used to drive the core VPU clock and the UART
// PLLD is the display PLL, used to drive DSI display panels.
//
// Power-on defaults are values for the Pi Zero

// SYS_CLK_DIVIDER is the ratio between the UART source clock and the Core0 output of PLLC
// This is typically 3 or 4, depending on the Pi Model.
// We need to know this to correct serial speed when PLLC used.
// it should really be read from a register as it changes with core freq (register address not known at this time)
// however it is only needed for PLLC and all models now use PLLA

#if defined(RPI4)
#define USE_PLLA4
#define SYS_CLK_DIVIDER 5
#elif defined(RPI3)
#define USE_PLLA
#define SYS_CLK_DIVIDER 3
#elif defined(RPI2)
#define USE_PLLA
#define SYS_CLK_DIVIDER 4
#else
#define USE_PLLA
#define SYS_CLK_DIVIDER 3         // should be 4 for Pi 1 depending on core clock speed
#endif

//PLL defaults for different Pi versions
//pi0 = 2400/2000/2400/2000
//pi1 = 2000/1400/2000/2000
//pi2 = 2000/1800/2000/2000 unconfirmed
//pi3 = 2400/2400/2400/2000
//pi4 = 3000/3000/3000/3000

#ifdef USE_PLLA
#define PLL_NAME              "PLLA"      // power-on default = off
#define GPCLK_SOURCE               4      // PLLA_PER (4) used as source
#define DEFAULT_GPCLK_DIVISOR      6      // 2400MHz / 4 / 6 = 100MHz
#define PLL_CTRL           PLLA_CTRL
#define PLL_FRAC           PLLA_FRAC
#define ANA1               PLLA_ANA1
#define PER                 PLLA_PER
#define PLLA_PER_VALUE             4
#define MIN_PLL_FREQ      1200000000
#define MAX_PLL_FREQ      2400000000
#endif

#ifdef USE_PLLC
#define PLL_NAME              "PLLC"      // power-on default = 1200MHz
#define GPCLK_SOURCE               5      // PLLC_PER (2) used as source
#define DEFAULT_GPCLK_DIVISOR     12      // 2400MHz / 2 / 12 = 100MHz
#define PLL_CTRL           PLLC_CTRL
#define PLL_FRAC           PLLC_FRAC
#define ANA1               PLLC_ANA1
#define PER                 PLLC_PER
#define MIN_PLL_FREQ      1200000000
#define MAX_PLL_FREQ      2400000000
#endif

#ifdef USE_PLLD
#define PLL_NAME              "PLLD"      // power-on default = 500MHz
#define GPCLK_SOURCE               6      // PLLD_PER (4) used as source
#define DEFAULT_GPCLK_DIVISOR      6      // 2400MHz / 4 / 6 = 100MHz
#define PLL_CTRL           PLLD_CTRL
#define PLL_FRAC           PLLD_FRAC
#define ANA1               PLLD_ANA1
#define PER                 PLLD_PER
#define MIN_PLL_FREQ      1200000000
#define MAX_PLL_FREQ      2400000000
#endif


#ifdef USE_PLLA4
#define PLL_NAME              "PLLA"      // power-on default = 3000MHz
#define GPCLK_SOURCE               4      // PLLA_PER (5) used as source
#define DEFAULT_GPCLK_DIVISOR      6      // 3000MHz / 5 / 6 = 100MHz
#define PLL_CTRL           PLLA_CTRL
#define PLL_FRAC           PLLA_FRAC
#define ANA1               PLLA_ANA1
#define PER                 PLLA_PER
#define PLLA_PER_VALUE             5
#define MIN_PLL_FREQ      1500000000
#define MAX_PLL_FREQ      3000000000
#endif

#ifdef USE_PLLC4
#define PLL_NAME              "PLLC"      // power-on default = 3000MHz
#define GPCLK_SOURCE               5      // PLLC_PER (5) used as source
#define DEFAULT_GPCLK_DIVISOR      6      // 3000MHz / 5 / 6 = 100MHz
#define PLL_CTRL           PLLC_CTRL
#define PLL_FRAC           PLLC_FRAC
#define ANA1               PLLC_ANA1
#define PER                 PLLC_PER
#define MIN_PLL_FREQ      1500000000
#define MAX_PLL_FREQ      3000000000
#endif

#ifdef USE_PLLD4
#define PLL_NAME              "PLLD"      // power-on default = 3000MHz
#define GPCLK_SOURCE               6      // PLLD_PER (5) used as source
#define DEFAULT_GPCLK_DIVISOR      6      // 3000MHz / 5 / 6 = 100MHz
#define PLL_CTRL           PLLD_CTRL
#define PLL_FRAC           PLLD_FRAC
#define ANA1               PLLD_ANA1
#define PER                 PLLD_PER
#define MIN_PLL_FREQ      1500000000
#define MAX_PLL_FREQ      3000000000
#endif

enum {
   CPLD_NORMAL,
   CPLD_BLANK,
   CPLD_UNKNOWN,
   CPLD_WRONG,
   CPLD_MANUAL,
   CPLD_UPDATE
};

// =============================================================
// Forward declarations
// =============================================================

static void cpld_init();

// =============================================================
// Global variables
// =============================================================

cpld_t *cpld = NULL;
int clock_error_ppm = 0;
int vsync_time_ns = 0;
int calculated_vsync_time_ns = 0;
capture_info_t *capinfo;
clk_info_t clkinfo;

// =============================================================
// Local variables
// =============================================================

static capture_info_t default_capinfo  __attribute__((aligned(32)));
static capture_info_t mode7_capinfo    __attribute__((aligned(32)));
static uint32_t cpld_version_id;
static int mode7;
static int paletteControl = PALETTECONTROL_INBAND;
static int interlaced;
static int clear;
static volatile int delay;
static double pllh_clock = 0;
static int genlocked = 0;
static int resync_count = 0;
static int target_difference = 0;
static int source_vsync_freq_hz = 0;
static int display_vsync_freq_hz = 0;
static double source_vsync_freq = 0;
static double display_vsync_freq = 0;
static char status[256];
static int restart_profile = 0;
// =============================================================
// OSD parameters
// =============================================================
static int profile     = 0;
static int saved_config_number = 0;
static int subprofile  = 0;
static int refresh = 0;
static int old_refresh = -1;
static int resolution  = -1;
static int old_resolution = -1;
static int hdmi_mode = 0;
static int old_hdmi_mode = -1;
//static int x_resolution = 0;
//static int y_resolution = 0;
static char resolution_name[MAX_NAMES_WIDTH];
static int scaling     = -1;
static int gscaling = GSCALING_INTEGER;
static int filtering   = DEFAULT_FILTERING;
static int old_filtering = - 1;
static int frontend    = 0;
static int border      = 0;
static int ntscphase   = 0;
static int debug       = 0;
static int autoswitch  = 2;
static int scanlines   = 0;
static int scanlines_intensity = 0;
static int colour      = 0;
static int invert      = 0;
static int fontsize    = 0;
static int m7deinterlace = 6;
static int deinterlace = 0;
static int ntsccolour  = 0;
static int vsync       = 0;
static int vlockmode   = 0;
static int vlockline   = 10;
static int vlockspeed  = 2;
static int vlockadj    = 0;
static int lines_per_2_vsyncs = 0;
static int lines_per_vsync = 0;
static int one_line_time_ns = 0;
static int nlines_time_ns = 0;
static int nlines_ref_ns = 0;
static int nominal_cpld_clock = 0;
static int one_vsync_time_ns = 0;
static int adjusted_clock;
static int reboot_required = 0;
static int resolution_warning = 0;
static int vlock_limited = 0;
static int current_display_buffer = 0;
static int h_overscan = 0;
static int v_overscan = 0;
static int config_overscan_left = 0;
static int config_overscan_right = 0;
static int config_overscan_top = 0;
static int config_overscan_bottom = 0;
static int startup_overscan = 0;
static int cpuspeed = 1000;
static int cpld_fail_state = CPLD_NORMAL;
static int helper_flag = 0;
static int simple_detected = 0;
static int supports8bit = 0;
static int newanalog = 0;
static int force_genlock_range = 0;
static unsigned int pll_freq = 0;
static unsigned int new_clock = 0;
static unsigned int old_pll_freq = 0;
static unsigned int old_clock = 0;
static unsigned int prediv = 0;
static unsigned int pll_scale = 0;
static unsigned int min_pll_freq = 0;
static unsigned int max_pll_freq = 0;
static unsigned int gpclk_divisor = 0;
static int ppm_range = 1;
static int ppm_range_count = 0;
static int powerup = 1;
static int hsync_threshold_switch = 0;
static int resolution_status = 0;
static volatile uint32_t display_list_index = 0;
static volatile uint32_t* display_list = SCALER_DISPLAY_LIST;


#ifdef MULTI_BUFFER
static int nbuffers    = 0;
#endif

static int current_vlockmode = -1;
static const char *sync_names[] = {
    "-H-V",
    "+H-V",
    "-H+V",
    "+H+V",
    "Comp",
    "InvComp"
};
static const char *sync_names_long[] = {
    "Separate -H -V",
    "Separate +H -V",
    "Separate -H +V",
    "Separate +H +V",
    "Composite",
    "Inverted Composite"
};
static const char *mixed_names[] = {
    "Separate H & V CPLD",
    "Mixed H & V CPLD"
};
// Calculated so that the constants from librpitx work
static volatile uint32_t *gpioreg = (volatile uint32_t *)(PERIPHERAL_BASE + 0x101000UL);

// Temporary buffer that must be at least as large as a frame buffer
static unsigned char last[4096 * 1024] __attribute__((aligned(32)));

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
// Private methods
// =============================================================
void delay_in_arm_cycles_cpu_adjust(int cycles) {
    delay_in_arm_cycles((int) ((double)cycles * (double)cpuspeed / 1000));
}

void reboot() {
	*PM_WDOG = PM_PASSWORD | 1;
	*PM_RSTC = PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET;
	while(1);
}

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
// Source 5 = PLLC = core_freq * 3
// Source 6 = PLLD = 500MHz

static void init_gpclk(int source, int divisor) {
   log_debug("A GP_CLK1_DIV = %08"PRIx32, *GP_CLK1_DIV);

   log_debug("B GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);

   // Stop the clock generator (retaining the existing source)
   *GP_CLK1_CTL = CM_PASSWORD | ((*GP_CLK1_CTL) & ~GZ_CLK_ENA);

   // Wait for BUSY low
   log_debug("C GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);
   while ((*GP_CLK1_CTL) & GZ_CLK_BUSY) {}
   log_debug("D GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);

   // Configure the clock generator
   *GP_CLK1_CTL = CM_PASSWORD | source;
   *GP_CLK1_DIV = CM_PASSWORD | (divisor << 12);

   log_debug("E GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);

   // Start the clock generator
   *GP_CLK1_CTL = CM_PASSWORD | (source | GZ_CLK_ENA);

   log_debug("F GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);
   while (!((*GP_CLK1_CTL) & GZ_CLK_BUSY)) {}    // Wait for BUSY high
   log_debug("G GP_CLK1_CTL = %08"PRIx32, *GP_CLK1_CTL);

   log_debug("H GP_CLK1_DIV = %08"PRIx32, *GP_CLK1_DIV);
}

#ifdef USE_PROPERTY_INTERFACE_FOR_FB
// this is the current one used
static void init_framebuffer(capture_info_t *capinfo) {
static int last_width = -1;
static int last_height = -1;

    rpi_mailbox_property_t *mp;

    if (capinfo->width != last_width || capinfo->height != last_height) {
       //if (last_width != -1 && last_height != -1) {
       //   clear_full_screen();
       //}
       // Fill in the frame buffer structure with a small dummy frame buffer first
       /* Initialise a framebuffer... */
       RPI_PropertyInit();
       RPI_PropertyAddTag(TAG_ALLOCATE_BUFFER, 0x02000000);
       RPI_PropertyAddTag(TAG_SET_PHYSICAL_SIZE, 64, 64);
    #ifdef MULTI_BUFFER
       RPI_PropertyAddTag(TAG_SET_VIRTUAL_SIZE, 64, 64);
    #else
       RPI_PropertyAddTag(TAG_SET_VIRTUAL_SIZE, 64, 64);
    #endif
       RPI_PropertyAddTag(TAG_SET_DEPTH, capinfo->bpp);

       RPI_PropertyProcess();


        // FIXME: A small delay (like the log) is neccessary here
        // or the RPI_PropertyGet seems to return garbage
        log_info("Width or Height differ from last FB: Setting dummy 64x64 framebuffer");

    }
    int adjusted_width = capinfo->width;

    //last_width = capinfo->width;
    //last_height = capinfo->height;
    /* work out if overscan needed */

    int h_size = get_hdisplay();
    int v_size = get_vdisplay();

    h_overscan = 0;
    v_overscan = 0;

    if (get_gscaling() == GSCALING_INTEGER) {
       if (!((capinfo->video_type == VIDEO_TELETEXT && get_m7scaling() == SCALING_UNEVEN)
         ||(capinfo->video_type != VIDEO_TELETEXT && get_normalscaling() == SCALING_UNEVEN)))  {
           int width = adjusted_width >> ((capinfo->sizex2 & 2) >> 1);
           int hscale = h_size / width;
           h_overscan = h_size - (hscale * width);
       }
       int height = capinfo->height >> (capinfo->sizex2 & 1);
       int vscale = v_size / height;
       v_overscan = v_size - (vscale * height);
    }

    int adj_h_overscan = h_overscan;
    int adj_v_overscan = v_overscan;
    if (adj_h_overscan != 0) {  // add 1 if non zero to work around scaler issues
       adj_h_overscan++;
    }
    if (adj_v_overscan != 0) {  // add 1 if non zero to work around scaler issues
       adj_v_overscan++;
    }

    int left_overscan = adj_h_overscan >> 1;
    int right_overscan = left_overscan + (adj_h_overscan & 1);

    int top_overscan = adj_v_overscan >> 1;
    int bottom_overscan = top_overscan + (adj_v_overscan & 1);


    left_overscan += config_overscan_left;
    right_overscan += config_overscan_right;
    top_overscan += config_overscan_top;
    bottom_overscan += config_overscan_bottom;

    log_info("Overscan L=%d, R=%d, T=%d, B=%d",left_overscan, right_overscan, top_overscan, bottom_overscan);

    /* Initialise a framebuffer... */
    RPI_PropertyInit();
    RPI_PropertyAddTag(TAG_ALLOCATE_BUFFER, 0x02000000);
    RPI_PropertyAddTag(TAG_SET_PHYSICAL_SIZE, adjusted_width, capinfo->height);
    #ifdef MULTI_BUFFER
    RPI_PropertyAddTag(TAG_SET_VIRTUAL_SIZE, adjusted_width, capinfo->height * NBUFFERS);
    #else
    RPI_PropertyAddTag(TAG_SET_VIRTUAL_SIZE, adjusted_width, capinfo->height);
    #endif
    RPI_PropertyAddTag(TAG_SET_DEPTH, capinfo->bpp);
    RPI_PropertyAddTag(TAG_SET_OVERSCAN, top_overscan, bottom_overscan, left_overscan, right_overscan);
    RPI_PropertyAddTag(TAG_GET_PITCH);
    RPI_PropertyAddTag(TAG_GET_PHYSICAL_SIZE);
    RPI_PropertyAddTag(TAG_GET_DEPTH);

    RPI_PropertyProcess();

    // FIXME: A small delay (like the log) is neccessary here
    // or the RPI_PropertyGet seems to return garbage
    delay_in_arm_cycles_cpu_adjust(4000000);
    log_info("Initialised Framebuffer");

    if ((mp = RPI_PropertyGet(TAG_GET_PHYSICAL_SIZE))) {
      int width = mp->data.buffer_32[0];
      int height = mp->data.buffer_32[1];
      log_info("Size: %dx%d (requested %dx%d)", width, height, capinfo->width, capinfo->height);
      if (width != adjusted_width || height != capinfo->height) {
          log_info("Invalid frame buffer dimensions - maybe HDMI not connected - rebooting");
          delay_in_arm_cycles_cpu_adjust(1000000000);
          reboot();
      }
    }

    if ((mp = RPI_PropertyGet(TAG_GET_PITCH))) {
      capinfo->pitch = mp->data.buffer_32[0];
      //log_info("Pitch: %d bytes", capinfo->pitch);
    }

    if ((mp = RPI_PropertyGet(TAG_ALLOCATE_BUFFER))) {
      capinfo->fb = (unsigned char*)mp->data.buffer_32[0];
      log_info("Framebuffer address: %8.8X", (unsigned int)capinfo->fb);
    }

    // On the Pi 2/3 the mailbox returns the address with bits 31..30 set, which is wrong
    capinfo->fb = (unsigned char *)(((unsigned int) capinfo->fb) & 0x3fffffff);
    //log_info("Framebuffer address masked: %8.8X", (unsigned int)capinfo->fb);
    //Initialize the palette
    osd_update_palette();

    // modify display list if 16bpp to switch from RGB 565 to ARGB 4444
    if (capinfo->bpp == 16) {
        //have to wait for field sync for display list to be updated
        wait_for_pi_fieldsync();
        wait_for_pi_fieldsync();
        //delay_in_arm_cycles_cpu_adjust(30000000); // little extra delay
        //read the index pointer into the display list RAM
        display_list_index = (uint32_t) *SCALER_DISPLIST1;

/*
        volatile uint32_t  d;
        volatile uint32_t dlist_start;
        dlist_start = *SCALER_DISPLIST1;
        log_info("SCALER_DISPLIST1 = %08X", dlist_start);
        int i = dlist_start;
        do {
        d = display_list[i++];
        log_info("%08X", d);
        } while (d != 0x80000000);
*/

        display_list[display_list_index] = (display_list[display_list_index] & ~0x600f) | (PIXEL_ORDER << 13) | PIXEL_FORMAT;
        log_info("Modified display list word at %08X = %08X", display_list_index, display_list[display_list_index]);

    }

}
#else

// An alternative way to initialize the framebuffer using mailbox channel 1
//
// I was hoping it would then be possible to page flip just by modifying the structure
// in-place. Unfortunately that didn't work, but the code might be useful in the future.


// THIS CALL IS NOT USED AND HAS NOT BEEN UPDATED
static void init_framebuffer(capture_info_t *capinfo) {
   static int last_width = -1;
   static int last_height = -1;

   log_debug("Framebuf struct address: %p", fbp);

   if (capinfo->width != last_width || capinfo->height != last_height) {
       log_info("Width or Height differ from last FB: Setting dummy 64x64 framebuffer");
       // Fill in the frame buffer structure with a small dummy frame buffer first
       fbp->width          = 64;
       fbp->height         = 64;
       fbp->virtual_width  = 64;
    #ifdef MULTI_BUFFER
       fbp->virtual_height = 64;
    #else
       fbp->virtual_height = 64;
    #endif
       fbp->pitch          = 0;
       fbp->depth          = capinfo->bpp;
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
       RPI_Mailbox0Write(MB0_FRAMEBUFFER, ((unsigned int)fbp) + 0xC0000000);

       // Wait for the response (0)
       RPI_Mailbox0Read(MB0_FRAMEBUFFER);
   }

   last_width = capinfo->width;
   last_height = capinfo->height;

   // Fill in the frame buffer structure
   fbp->width          = capinfo->width;
   fbp->height         = capinfo->height;
   fbp->virtual_width  = capinfo->width;
#ifdef MULTI_BUFFER
   fbp->virtual_height = capinfo->height * NBUFFERS;
#else
   fbp->virtual_height = capinfo->height;
#endif
   fbp->pitch          = 0;
   fbp->depth          = capinfo->bpp;
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
   RPI_Mailbox0Write(MB0_FRAMEBUFFER, ((unsigned int)fbp) + 0xC0000000);

   // Wait for the response (0)
   RPI_Mailbox0Read(MB0_FRAMEBUFFER);

   capinfo->pitch = fbp->pitch;
   capinfo->fb = (unsigned char*)(fbp->pointer);
   int width = fbp->width;
   int height = fbp->height;

   // See comment above
   // int i  = 0;
   // while (!pitch || !fb) {
   //    pitch = fbp->pitch;
   //    fb = (unsigned char*)(fbp->pointer);
   //    i++;
   // }
   // log_info("i=%d", i);

   log_info("Initialised Framebuffer: %dx%d ", width, height);
   log_info("Pitch: %d bytes", capinfo->pitch);
   log_debug("Framebuffer address: %8.8X", (unsigned int)capinfo->fb);

   // On the Pi 2/3 the mailbox returns the address with bits 31..30 set, which is wrong
   capinfo->fb = (unsigned char *)(((unsigned int) capinfo->fb) & 0x3fffffff);

   // Initialize the palette
   osd_update_palette();
}

#endif

//info about using ANA1 prediv extracted from:
//https://github.com/torvalds/linux/blob/43570f0383d6d5879ae585e6c3cf027ba321546f/drivers/clk/bcm/clk-bcm2835.c

void log_plla() {
   int ANA1_PREDIV = (gpioreg[PLLA_ANA1] >> 14) & 1;
   int NDIV = (gpioreg[PLLA_CTRL] & 0x3ff) << ANA1_PREDIV;
   int FRAC = gpioreg[PLLA_FRAC] << ANA1_PREDIV;
   double clock = CRYSTAL * ((double)NDIV + ((double)FRAC) / ((double)(1 << 20)));
   log_info("PLLA: %lf ANA1 = %08x", clock, gpioreg[PLLA_ANA1]);
   log_info("PLLA: PDIV=%d NDIV=%d CTRL=%08x FRAC=%d DSI0=%d CORE=%d PER=%d CCP2=%d",
             (gpioreg[PLLA_CTRL] >> 12) & 0x7,
             gpioreg[PLLA_CTRL] & 0x3ff,
             gpioreg[PLLA_CTRL],
             gpioreg[PLLA_FRAC],
             gpioreg[PLLA_DSI0],
             gpioreg[PLLA_CORE],
             gpioreg[PLLA_PER],
             gpioreg[PLLA_CCP2]);
}

void log_pllb() {
   int ANA1_PREDIV = (gpioreg[PLLB_ANA1] >> 14) & 1;
   int NDIV = (gpioreg[PLLB_CTRL] & 0x3ff) << ANA1_PREDIV;
   int FRAC = gpioreg[PLLB_FRAC] << ANA1_PREDIV;
   double clock = CRYSTAL * ((double)NDIV + ((double)FRAC) / ((double)(1 << 20)));
   log_info("PLLB: %lf ANA1 = %08x", clock, gpioreg[PLLB_ANA1]);
   log_info("PLLB: PDIV=%d NDIV=%d CTRL=%08x FRAC=%d ARM=%d SP0=%d SP1=%d SP2=%d",
             (gpioreg[PLLB_CTRL] >> 12) & 0x7,
             gpioreg[PLLB_CTRL] & 0x3ff,
             gpioreg[PLLB_CTRL],
             gpioreg[PLLB_FRAC],
             gpioreg[PLLB_ARM],
             gpioreg[PLLB_SP0],
             gpioreg[PLLB_SP1],
             gpioreg[PLLB_SP2]);
}

void log_pllc() {
   int ANA1_PREDIV = (gpioreg[PLLC_ANA1] >> 14) & 1;
   int NDIV = (gpioreg[PLLC_CTRL] & 0x3ff) << ANA1_PREDIV;
   int FRAC = gpioreg[PLLC_FRAC] << ANA1_PREDIV;
   double clock = CRYSTAL * ((double)NDIV + ((double)FRAC) / ((double)(1 << 20)));
   log_info("PLLC: %lf, ANA1 = %08x", clock, gpioreg[PLLC_ANA1]);
   log_info("PLLC: PDIV=%d NDIV=%d CTRL=%08x FRAC=%d CORE2=%d CORE1=%d PER=%d CORE0=%d",
             (gpioreg[PLLC_CTRL] >> 12) & 0x7,
             gpioreg[PLLC_CTRL] & 0x3ff,
             gpioreg[PLLC_CTRL],
             gpioreg[PLLC_FRAC],
             gpioreg[PLLC_CORE2],
             gpioreg[PLLC_CORE1],
             gpioreg[PLLC_PER],
             gpioreg[PLLC_CORE0]);
}

void log_plld() {
   int ANA1_PREDIV = (gpioreg[PLLD_ANA1] >> 14) & 1;
   int NDIV = (gpioreg[PLLD_CTRL] & 0x3ff) << ANA1_PREDIV;
   int FRAC = gpioreg[PLLD_FRAC] << ANA1_PREDIV;
   double clock = CRYSTAL * ((double)NDIV + ((double)FRAC) / ((double)(1 << 20)));
   log_info("PLLD: %lf ANA1 = %08x", clock, gpioreg[PLLD_ANA1]);
   log_info("PLLD: PDIV=%d NDIV=%d CTRL=%08x FRAC=%d DSI0=%d CORE=%d PER=%d DSI1=%d",
             (gpioreg[PLLD_CTRL] >> 12) & 0x7,
             gpioreg[PLLD_CTRL] & 0x3ff,
             gpioreg[PLLD_CTRL],
             gpioreg[PLLD_FRAC],
             gpioreg[PLLD_DSI0],
             gpioreg[PLLD_CORE],
             gpioreg[PLLD_PER],
             gpioreg[PLLD_DSI1]);
}

void log_pllh() {
   int ANA1_PREDIV = (gpioreg[PLLH_ANA1] >> 11) & 1; //prediv on bit 11 instead of bit 14 for pllh
   int NDIV = (gpioreg[PLLH_CTRL] & 0x3ff) << ANA1_PREDIV;
   int FRAC = gpioreg[PLLH_FRAC] << ANA1_PREDIV;
   double clock = CRYSTAL * ((double)NDIV + ((double)FRAC) / ((double)(1 << 20)));
   log_info("PLLH: %lf ANA1 = %08x", clock, gpioreg[PLLH_ANA1]);
   log_info("PLLH: PDIV=%d NDIV=%d CTRL=%08x FRAC=%d AUX=%d RCAL=%d PIX=%d STS=%d",
             (gpioreg[PLLH_CTRL] >> 12) & 0x7,
             gpioreg[PLLH_CTRL] & 0x3ff,
             gpioreg[PLLH_CTRL],
             gpioreg[PLLH_FRAC],
             gpioreg[PLLH_AUX],
             gpioreg[PLLH_RCAL],
             gpioreg[PLLH_PIX],
             gpioreg[PLLH_STS]);
}

void set_pll_frequency(double f, int pll_ctrl, int pll_fract) {
   // Calculate the new dividers
   int div = (int) (f / CRYSTAL);
   int fract = (int) ((double)(1<<20) * (f / CRYSTAL - (double) div));
   // Sanity check the range of the fractional divider (it should actually always be in range)
   if (fract < 0) {
      log_warn("PLL fraction < 0");
      fract = 0;
   }
   if (fract > (1<<20) - 1) {
      log_warn("PLL fraction > 1");
      fract = (1<<20) - 1;
   }

   // Read the existing values
   int old_ctrl = gpioreg[pll_ctrl];
   int old_div = old_ctrl & 0x3ff;
   int old_fract = gpioreg[pll_fract];

   // Check if there's been a change
   if (div != old_div || fract != old_fract) {

#ifdef USE_PLLC
      // Flush the UART, as the Core Clock is about to change
      RPI_AuxMiniUartFlush();
#endif

      // Update the integer divider
      if (div != old_div) {
         gpioreg[pll_ctrl] = CM_PASSWORD | (old_ctrl & 0x00FFFC00) | div;
      }

      // Update the fractional divider
      if (fract != old_fract) {
         gpioreg[pll_fract] = CM_PASSWORD | fract;
      }

      // Re-read the integer divider if it's changed
      if (div != old_div) {
         int new_ctrl = gpioreg[pll_ctrl];
         int new_div = new_ctrl & 0x3ff;
         if (new_div == div) {
            //log_debug("   New int divider: %d", new_div);
         } else {
            log_warn("Failed to write int divider: wrote %d, read back %d", div, new_div);
         }
      }

      // Re-read the fraction divider if it's changed
      if (fract != old_fract) {
         int new_fract = gpioreg[pll_fract];
         if (new_fract == fract) {
            //log_debug(" New fract divider: %d", new_fract);
         } else {
            log_warn("Failed to write fract divider: wrote %d, read back %d", fract, new_fract);
         }
      }
   }
}


int calibrate_sampling_clock(int profile_changed) {
   int a = 13;

   // Default values for the Beeb
   clkinfo.clock      = 16000000;
   clkinfo.line_len   = 1024;

//log_plla();
//log_pllb();
//log_pllc();
//log_plld();

   // Update from configuration
   geometry_get_clk_params(&clkinfo);

   log_info("        clkinfo.clock = %d Hz",  clkinfo.clock);
   log_info("     clkinfo.line_len = %d",     clkinfo.line_len);
   log_info("    clkinfo.clock_ppm = %d ppm", clkinfo.clock_ppm);

   int nlines = MEASURE_NLINES; // Measure over N=100 lines
   nlines_ref_ns = nlines * (int) (1e9 * ((double) clkinfo.line_len) / ((double) clkinfo.clock));

   hsync_threshold = OTHER_HSYNC_THRESHOLD * cpuspeed / 1000;                       // set to longest value for initial measurement which works with all systems
   nlines_time_ns = (int)((double) measure_n_lines(nlines) * 1000 / cpuspeed);
   hsync_threshold = ((autoswitch == AUTOSWITCH_MODE7 && hsync_width < hsync_threshold_switch) ? BBC_HSYNC_THRESHOLD : OTHER_HSYNC_THRESHOLD) * cpuspeed / 1000;  // set to bbc threshold if appropriate

   log_info("    Nominal %3d lines = %d ns", nlines, nlines_ref_ns);
   log_info("     Actual %3d lines = %d ns", nlines, nlines_time_ns);

   ppm_range = PLL_PPM_LO;
   ppm_range_count = 0;

   double error = (double) nlines_time_ns / (double) nlines_ref_ns;
   clock_error_ppm = ((error - 1.0) * 1e6);
   log_info("          Clock error = %d PPM", clock_error_ppm);

   nominal_cpld_clock = clkinfo.clock * cpld->get_divider();

    if (profile_changed) {
        old_clock = clkinfo.clock;
        if (mode7) {
            old_clock = old_clock * 16 / 12;
        }
    }
   if ((clkinfo.clock_ppm > 0 && abs(clock_error_ppm) > clkinfo.clock_ppm) || (sync_detected == 0)) {
      if (old_clock > 0 && sub_profiles_available(profile) == 0) {
         log_warn("PPM error too large, using previous clock");
         new_clock = old_clock * cpld->get_divider();
         if (autoswitch == AUTOSWITCH_MODE7 && mode7) {
             log_warn("Compensating for mode 7");
             new_clock = new_clock * 12 / 16;
         }
      } else {
         log_warn("PPM error too large, using nominal clock");
         new_clock = nominal_cpld_clock;
      }
   } else {
      new_clock = (unsigned int) (((double) nominal_cpld_clock) / error);
   }

   if (new_clock > 195000000) {
       new_clock = 195000000;
       log_warn("Clock exceeds 195Mhz - Limiting to 195Mhz");
   }

   adjusted_clock = new_clock / cpld->get_divider();

   old_clock = adjusted_clock;

   if (mode7) {
       old_clock = old_clock * 16 / 12;
   }

   log_info(" Error adjusted clock = %d Hz", adjusted_clock);

   // Pick the best value for pll_freq and gpclk_divisor
   prediv        = (gpioreg[ANA1] >> 14) & 1;
   pll_scale     = gpioreg[PER];
   min_pll_freq  = MIN_PLL_FREQ;  // defined at the top
   max_pll_freq  = MAX_PLL_FREQ;  // defined at the top
   gpclk_divisor = max_pll_freq / pll_scale / new_clock;
   pll_freq      = new_clock * pll_scale * gpclk_divisor ;

   log_info(" Target PLL frequency = %u Hz, prediv = %d, PER = %d", pll_freq, prediv, gpioreg[PER]);

   // sanity check
   if (pll_freq < min_pll_freq) {
      log_warn("PLL clock out of range, defaulting to minimum (%u Hz)", min_pll_freq);
      pll_freq = MAX_PLL_FREQ;
      gpclk_divisor = DEFAULT_GPCLK_DIVISOR;
   } else if (pll_freq > max_pll_freq) {
      log_warn("PLL clock out of range, defaulting to maxiumum (%u Hz)", max_pll_freq);
      pll_freq = MAX_PLL_FREQ;
      gpclk_divisor = DEFAULT_GPCLK_DIVISOR;
   }

   log_info(" Actual PLL frequency = %u Hz", pll_freq);
   log_info("        GPCLK Divisor = %u", gpclk_divisor);

   // If the clock has changed from it's previous value, then actually change it
   if (pll_freq != old_pll_freq) {

      set_pll_frequency(((double) (pll_freq >> prediv)) / 1e6, PLL_CTRL, PLL_FRAC);

#ifdef USE_PLLC
      // Reinitialize the UART as the Core Clock has changed
        RPI_AuxMiniUartInit_With_Freq(115200, 8, pll_freq / pll_scale / SYS_CLK_DIVIDER);
#endif

      // And remember for next time
      old_pll_freq = pll_freq;
   }

   // TODO: this should be superfluous (as the GPU is not changing the core clock)
   // However, if we remove it, the next osd_update_palette() call hangs
   get_clock_rate(CORE_CLK_ID);

   // Finally, set the new divisor
   log_debug("Setting up divisor");
   init_gpclk(GPCLK_SOURCE, gpclk_divisor);
   log_debug("Done setting up divisor");

   // Remeasure the hsync time
   nlines_time_ns = (int)((double) measure_n_lines(nlines) * 1000 / cpuspeed);

   // Remeasure the vsync time
   vsync_time_ns = (int)((double)measure_vsync() * 1000 / cpuspeed);
   if (vsync_retry_count) log_info("Vsync retry count = %d", vsync_retry_count);

   // sanity check measured values as noise on the sync input results in nonsensical values that can cause a crash
   if (vsync_time_ns < (frame_minimum << 1) || nlines_time_ns < (line_minimum * nlines)) {
       log_info("Sync times too short, clipping:  %d,%d : %d,%d", vsync_time_ns,nlines_time_ns, frame_timeout << 1, line_timeout * nlines );
       vsync_time_ns = frame_timeout << 1;
       nlines_time_ns = line_timeout * nlines;
   }

   // Instead, calculate the number of lines per frame
   double lines_per_2_vsyncs_double = ((double) vsync_time_ns) / (((double) nlines_time_ns) / ((double) nlines));

   one_line_time_ns = nlines_time_ns / nlines;

   // If number of lines is odd, then we must be interlaced
   interlaced = ((int)(lines_per_2_vsyncs_double + 0.5)) % 2;
   one_vsync_time_ns = vsync_time_ns >> 1;
   lines_per_vsync = ((int) (lines_per_2_vsyncs_double + 0.5) >> 1);
   lines_per_2_vsyncs = (int) (lines_per_2_vsyncs_double + 0.5);

   calculated_vsync_time_ns = ((double)lines_per_2_vsyncs * nlines_time_ns / MEASURE_NLINES);   // calculate vertical period from measured hsync period (two frames / fields so ~40ms)

   // Log it
   capinfo->detected_sync_type &= ~SYNC_BIT_INTERLACED;
   if (interlaced) {
      capinfo->detected_sync_type |= SYNC_BIT_INTERLACED;
      log_info("      Lines per frame = %d, (%g)", lines_per_2_vsyncs, lines_per_2_vsyncs_double);
      log_info("Actual frame time = %d ns (interlaced), line time = %d ns", vsync_time_ns, one_line_time_ns);
   } else {
      log_info("      Lines per frame = %d, (%g)", lines_per_vsync, lines_per_2_vsyncs_double / 2);
      log_info("Actual frame time = %d ns (non-interlaced), line time = %d ns", one_vsync_time_ns, one_line_time_ns);
   }

   // Invalidate the current vlock mode to force an updated, as vsync_time_ns will have changed
   current_vlockmode = -1;

   return a;
}

static void recalculate_hdmi_clock(int vlockmode, int genlock_adjust) {
   static double last_f2 = 0;
   // The very first time we get called, vsync_time_ns has not been set
   // so exit gracefully
   if (vsync_time_ns == 0) {
       return;
   }


   // ********************temp disable genlock if RPI4 for now
   #if defined(RPI4)
   return;
   #endif

   // Dump the PLLH registers
   //log_pllh();
   int PLLH_ANA1_PREDIV = ((gpioreg[PLLH_ANA1] >> 11) & 1) ? 2 : 1; //prediv on bit 11 instead of bit 14 for pllh
   // Grab the original PLLH frequency once, at it's original value
   if (pllh_clock == 0) {
      pllh_clock = (CRYSTAL * ((double)(gpioreg[PLLH_CTRL] & 0x3ff) + ((double)gpioreg[PLLH_FRAC]) / ((double)(1 << 20)))) * PLLH_ANA1_PREDIV;
   }

   //for (int i = 0; i < 32; i++) {
   //   log_debug("   PIXELVALVE2[%2d]: %08x", i, *(PIXELVALVE2_BASE + i));
   //}

   // Dump the PIXELVALVE2 registers
   //log_debug(" PIXELVALVE2_HORZA: %08x", *PIXELVALVE2_HORZA);
   //log_debug(" PIXELVALVE2_HORZB: %08x", *PIXELVALVE2_HORZB);
   //log_debug(" PIXELVALVE2_VERTA: %08x", *PIXELVALVE2_VERTA);
   //log_debug(" PIXELVALVE2_VERTB: %08x", *PIXELVALVE2_VERTB);

   // Work out the htotal and vtotal by summing the four  16-bit values:
   // A[31:16] - back porch width in pixels
   // A[15: 0] - synch width in pixels
   // B[31:16] - front porch width in pixels
   // B[15: 0] - active line width in pixels
   uint32_t htotal = (*PIXELVALVE2_HORZA) + (*PIXELVALVE2_HORZB);
   htotal = (htotal + (htotal >> 16)) & 0xFFFF;
   uint32_t vtotal = (*PIXELVALVE2_VERTA) + (*PIXELVALVE2_VERTB);
   vtotal = (vtotal + (vtotal >> 16)) & 0xFFFF;
   //log_debug("           H-Total: %d pixels", htotal);
   //log_debug("           V-Total: %d pixels", vtotal);

   // PLLH seems to use a fixed divider to generate the pixel clock
   int fixed_divider = 10;
   //log_debug("     Fixed divider: %d", fixed_divider);

   // 720x576@50    PLLH: PDIV=1 NDIV=56 FRAC=262144 AUX=256 RCAL=256 PIX=4 STS=526655
   // 1920x1080@50  PLLH: PDIV=1 NDIV=77 FRAC=360448 AUX=256 RCAL=256 PIX=1 STS=526655
   //     An additional divider is used to get very low pixel clock rates ^
   int additional_divider = gpioreg[PLLH_PIX];
   //log_debug("Additional divider: %d", additional_divider);

   // Calculate the pixel clock
   double pixel_clock = pllh_clock / ((double) fixed_divider) / ((double) additional_divider);
   //log_debug("       Pixel Clock: %lf MHz", pixel_clock);

   // Calculate the error between the HDMI VSync and the Source VSync
   source_vsync_freq = 2e9 / ((double) vsync_time_ns);
   display_vsync_freq = 1e6 * pixel_clock / ((double) htotal) / ((double) vtotal);
   double error = display_vsync_freq / source_vsync_freq;
   double error_ppm = 1e6 * (error - 1.0);

   double f2 = pllh_clock;

   if (vlockmode != HDMI_ORIGINAL && source_vsync_freq >= 48) {
      f2 /= error;
      f2 /= 1.0 + ((double) (genlock_adjust * GENLOCK_PPM_STEP) / 1000000);
   }

   // Sanity check HDMI pixel clock
   pixel_clock = f2 / ((double) fixed_divider) / ((double) additional_divider);

   vlock_limited = 0;

   switch (force_genlock_range) {
       default:
       case GENLOCK_RANGE_NORMAL:
       case GENLOCK_RANGE_INHIBIT:
       case GENLOCK_RANGE_SET_DEFAULT:
          if ((vlockadj == VLOCKADJ_NARROW) && (error_ppm < -50000 || error_ppm > 50000)) {
            f2 = pllh_clock;
            vlock_limited = 1;
          }
          break;
       case GENLOCK_RANGE_EDID:
       case GENLOCK_RANGE_FORCE_LOW:
          if (error_ppm < -50000 || source_vsync_freq_hz < 48) {       //don't go more than 5% above 60 Hz and below 48Hz
            f2 = pllh_clock;
            vlock_limited = 1;
          }
          break;
       case GENLOCK_RANGE_FORCE_ALL:                           //no limits above 60Hz, still limited below 48Hz
          if (source_vsync_freq_hz < 48) {
            f2 = pllh_clock;
            vlock_limited = 1;
          }
          break;
   }

   int max_clock = MAX_PIXEL_CLOCK;

   if (pixel_clock < MIN_PIXEL_CLOCK) {
      log_debug("Pixel clock of %.2lf MHz is too low; leaving unchanged", pixel_clock);
      f2 = pllh_clock;
      vlock_limited = 1;
   } else if (pixel_clock > max_clock) {
      log_debug("Pixel clock of %.2lf MHz is too high; leaving unchanged", pixel_clock);
      f2 = pllh_clock;
      vlock_limited = 1;
   }

   //log_debug(" Source vsync freq: %lf Hz (measured)",  source_vsync_freq);
   //log_debug("Display vsync freq: %lf Hz",  display_vsync_freq);
   //log_debug("       Vsync error: %lf ppm", error_ppm);
   //log_debug("     Original PLLH: %lf MHz", pllh_clock);
   //log_debug("       Target PLLH: %lf MHz", f2);
   source_vsync_freq_hz = (int) (source_vsync_freq + 0.5);
   display_vsync_freq_hz = (int) (display_vsync_freq + 0.5);

   if (f2 != last_f2) {
      last_f2 = f2;
      set_pll_frequency(f2 / PLLH_ANA1_PREDIV, PLLH_CTRL, PLLH_FRAC);
   }
   // Dump the the actual PLL frequency
   //log_debug("        Final PLLH: %lf MHz", (double) CRYSTAL * ((double)(gpioreg[PLLH_CTRL] & 0x3ff) + ((double)gpioreg[PLLH_FRAC]) / ((double)(1 << 20))));

   //log_pllh();
}

int __attribute__ ((aligned (64))) recalculate_hdmi_clock_line_locked_update(int force) {
    static int framecount = 0;
    static int genlock_adjust = 0;
    static int last_vlock = -1;
    static int thresholds[GENLOCK_MAX_STEPS] = GENLOCK_THRESHOLDS;

    static double line_total = 0;
    static int line_count = 0;

    static double frame_total = 0;
    static int frame_count = 0;

    static int log_flag = 0;

    static int last = 0x80000000;

    if (last != jitter_offset) {
        log_info("Jit%d", jitter_offset);
        last = jitter_offset;
        if (vlockmode != HDMI_EXACT) {
            // Return 0 if genlock disabled
            return 0;
        } else {
            // Return 1 if genlock enabled but not yet locked
            // Return 2 if genlock enabled and locked
            return 1 + genlocked;
        }
    }
 /*
    static int last_debug = -1;
    if (last_debug != debug_value) {
        log_info("%08X", debug_value);
        last_debug = debug_value;
        if (vlockmode != HDMI_EXACT) {
            // Return 0 if genlock disabled
            return 0;
        } else {
            // Return 1 if genlock enabled but not yet locked
            // Return 2 if genlock enabled and locked
            return 1 + genlocked;
        }
    }
*/
    if (!force) {
        if (sync_detected && last_sync_detected) {
            line_total += (double)total_hsync_period * 1000 * MEASURE_NLINES / ((double)capinfo->nlines - 1) / (double)cpuspeed; //adjust to MEASURE_NLINES in ns, total will be > 32 bits
            line_count++;
            if (vsync_period >= vsync_comparison_lo && vsync_period <= vsync_comparison_hi) { // using the measured vertical period is preferable but when menu is on screen or buttons being pressed the value might be wrong by multiple fields
                frame_total += (double) vsync_period * 2 * 1000 / cpuspeed ;                                  // if measured value is within window then use it (in ZX80/81 the values are always different to calculated due to one 4.7us shorter line)
                frame_count++;
                //log_info("%d %d",vsync_time_ns, vsync_period << 1);
            }
            if (line_count >= AVERAGE_VSYNC_TOTAL) {           //average over AVERAGE_VSYNC_TOTAL frames (~5 secs)
                if (frame_count != 0) {                        //will be AVERAGE_VSYNC_TOTAL or will be less if menus are used due to frame drops
                    vsync_time_ns = (int) (frame_total / (double) frame_count);
                    //log_info("%d", frame_count);
                    frame_total = 0;
                    frame_count = 0;
                    //log_info("%d %d",vsync_time_ns, calculated_vsync_time_ns);
                } else {
                    vsync_time_ns = calculated_vsync_time_ns;
                    log_flag = 1;
                }
                double recalc_nlines_time_ns = line_total / (double) line_count;   //get average new line time
                line_count = 0;
                line_total = 0;
                int ppm_lo = (int)(((double) nlines_time_ns * ppm_range / 1000000) + 0.5);
                if (ppm_lo < 1) ppm_lo = 1;
                int diff = abs(nlines_time_ns - recalc_nlines_time_ns);
                //log_info("%d, %d %d %d %d", diff,  nlines_time_ns, recalc_nlines_time_ns, ppm_lo, ppm_hi);
                if (diff >= ppm_lo) {
                    //log_info("%d, %d", ppm_lo,  ppm_hi);
                    double error = (double) recalc_nlines_time_ns / (double) nlines_ref_ns;
                    clock_error_ppm = ((error - 1.0) * 1e6);
                    if (clkinfo.clock_ppm == 0 || abs(clock_error_ppm) <= clkinfo.clock_ppm) {
                        new_clock = (unsigned int) (((double) nominal_cpld_clock) / error);
                        if (new_clock > 195000000) new_clock = 195000000;
                        adjusted_clock = new_clock / cpld->get_divider();
                        old_clock = adjusted_clock;
                        if (mode7) {
                           old_clock = old_clock * 16 / 12;
                        }
                        pll_freq = new_clock * pll_scale * gpclk_divisor ;
                        set_pll_frequency(((double) (pll_freq >> prediv)) / 1e6, PLL_CTRL, PLL_FRAC);
                        old_pll_freq = pll_freq;
                        nlines_time_ns = (int) recalc_nlines_time_ns;
                        one_line_time_ns = nlines_time_ns / MEASURE_NLINES;
                        calculated_vsync_time_ns = ((double)lines_per_2_vsyncs * recalc_nlines_time_ns / MEASURE_NLINES);   // calculate vertical period from measured hsync period (two frames / fields so ~40ms)
                        if (ppm_range != PLL_PPM_LO) {
                            if (log_flag) {
                                log_info("*VPLL%1d", ppm_range);
                            } else {
                                log_info("*PLL%1d", ppm_range);
                            }
                        } else {
                            if (log_flag) {
                                log_info("*VPLL");
                            } else {
                                log_info("*PLL");
                            }
                        }
                        log_flag = 0;
                        if (ppm_range_count < PLL_RESYNC_THRESHOLD_HI) {
                            ppm_range_count++;
                        }
                        // return without doing any genlock processing to avoid two log messages in one field.
                        if (vlockmode != HDMI_EXACT) {
                          // Return 0 if genlock disabled
                          return 0;
                        } else {
                          // Return 1 if genlock enabled but not yet locked
                          // Return 2 if genlock enabled and locked
                          return 1 + genlocked;
                        }
                    }
                }
            }
        } else {
            line_count = 0;
            line_total = 0;
            frame_count = 0;
            frame_total = 0;
        }
    } else {
        line_count = 0;
        line_total = 0;
        frame_count = 0;
        frame_total = 0;
        last_vlock = 0x80000000;
        genlocked = 0;
        return 0;
    }

  //  lock_fail = 0;
    if (sync_detected && last_sync_detected) {
        int adjustment = 0;
        if (capinfo->nlines >= GENLOCK_NLINES_THRESHOLD) {
            adjustment = 1;
        }
        if (vlockmode != HDMI_EXACT) {
            genlocked = 0;
            target_difference = 0;
            resync_count = 0;
            genlock_adjust = 0;
            switch (vlockmode) {
                case HDMI_SLOW_2000PPM:
                    genlock_adjust = 6;
                    break;
                case HDMI_SLOW_1000PPM:
                    genlock_adjust = 3;
                    break;
                case HDMI_FAST_1000PPM:
                    genlock_adjust = -3;
                    break;
                case HDMI_FAST_2000PPM:
                    genlock_adjust = -6;
                    break;
            }
            if (last_vlock != vlockmode) {
                recalculate_hdmi_clock(vlockmode, genlock_adjust);
                last_vlock = vlockmode;
                framecount = 0;
            }
        } else {
            int max_steps = GENLOCK_MAX_STEPS;
            int locked_threshold = GENLOCK_LOCKED_THRESHOLD;
            int frame_delay = GENLOCK_FRAME_DELAY;
            if (vlockspeed == VLOCKSPEED_MEDIUM) {
                max_steps >>= 1;
                locked_threshold--;
                frame_delay <<= 1;
            } else {
                if (vlockspeed == VLOCKSPEED_SLOW) {
                    max_steps = 1;
                    locked_threshold = 1;
                    frame_delay <<= 1;
                }
            }
            signed int difference = (vsync_line >> adjustment) - ((total_lines >> adjustment) - vlockline);
            if (abs(difference) > (total_lines >> (adjustment + 1))) {
                difference = -difference;
            }
            if (genlocked == 1 && abs(difference) >= thresholds[locked_threshold]) {
                genlocked = 0;
                if (difference >= 0) {
                    target_difference = -2;
                } else {
                    target_difference = 2;
                }
                if (abs(difference) > thresholds[locked_threshold]) {
                    log_info("UnLock");
                    resync_count = 0;
                    target_difference = 0;
               //     lock_fail = 1;
                } else {
                    log_info("Sync%02d", ++resync_count);
                    if (resync_count >= 99) {
                        resync_count = 0;
                    }
                }
            }
            if(framecount == 0) {
                int new_genlock_adjust = genlock_adjust;
                if (genlocked == 0) {
                    if (difference - target_difference == 0) {
                        if (genlock_adjust < 0) {
                            new_genlock_adjust++;
                        }
                        if (genlock_adjust > 0) {
                            new_genlock_adjust--;
                        }
                        if (new_genlock_adjust == 0)
                        {
                            genlocked = 1;
                            target_difference = 0;
                            log_info("Locked");
                            if (ppm_range_count <= PLL_RESYNC_THRESHOLD_LO && ppm_range > PLL_PPM_LO) {
                                ppm_range--;
                            } else {
                                if (ppm_range_count >= PLL_RESYNC_THRESHOLD_HI && ppm_range < PLL_PPM_LO_LIMIT) {
                                    ppm_range++;
                                }
                            }
                            ppm_range_count = 0;
                        }
                    } else {
                        if (difference >= target_difference) {
                            int threshold = 0;
                            if (genlock_adjust >= 0 && genlock_adjust < max_steps) {
                                threshold = thresholds[genlock_adjust];
                            }
                            if (genlock_adjust < max_steps && difference > threshold) {
                                new_genlock_adjust++;
                            }
                            if (genlock_adjust > 1 && difference <= thresholds[genlock_adjust - 1]) {
                                new_genlock_adjust--;
                            }
                        } else {
                            int threshold = 0;
                            if (genlock_adjust <= 0 && genlock_adjust > -max_steps) {
                                threshold = -thresholds[-genlock_adjust];
                            }
                            if (genlock_adjust > -max_steps && difference < threshold) {
                                new_genlock_adjust--;
                            }
                            if (genlock_adjust < -1 && difference >= -thresholds[-(genlock_adjust + 1)]) {
                                new_genlock_adjust++;
                            }
                        }
                    }
                    if (new_genlock_adjust != genlock_adjust || last_vlock != HDMI_EXACT) {
                        recalculate_hdmi_clock(HDMI_EXACT, new_genlock_adjust);
                        last_vlock = HDMI_EXACT;
                        genlock_adjust = new_genlock_adjust;
                        framecount = frame_delay;
                        //log_debug("%4d,%4d,%4d,%4d,%4d,%4d", genlocked, vlockline, vsync_line, difference, thresholds[abs(genlock_adjust)], genlock_adjust);
                    }
                }
            }
        }
    }
    if (framecount != 0) {
      framecount --;
    }
    if (vlockmode != HDMI_EXACT) {
      // Return 0 if genlock disabled
      return 0;
    } else {
      // Return 1 if genlock enabled but not yet locked
      // Return 2 if genlock enabled and locked
      return 1 + genlocked;
    }
}

// Configure PLLA so we can use it as a sampling clock source
//
// The logic to configure PLLA conmes from the Linux Kernel clk-bcm2835
// driver, specifically the following functions:
// - bcm2835_pll_divider_off
// - bcm2835_pll_divider_set_rate
// - bcm2835_pll_divider_on
// https://elixir.bootlin.com/linux/v4.4.70/source/drivers/clk/bcm/clk-bcm2835.c
#if  defined(USE_PLLA) || defined(USE_PLLA4)
static void configure_plla(int divider) {

   // Log the before register values
   // log_plla();

   // Disable PLLA_PER divider
   *CM_PLLA           = CM_PASSWORD | (((*CM_PLLA) & ~CM_PLLA_LOADPER) | CM_PLLA_HOLDPER);
   gpioreg[PLLA_PER]  = CM_PASSWORD | (A2W_PLL_CHANNEL_DISABLE);

   // Disable PLLA_CORE divider (to check it's not being used!)
   *CM_PLLA           = CM_PASSWORD | (((*CM_PLLA) & ~CM_PLLA_LOADCORE) | CM_PLLA_HOLDCORE);
   gpioreg[PLLA_CORE] = CM_PASSWORD | (A2W_PLL_CHANNEL_DISABLE);

   // Set the PLLA_PER divider to the value passed in
   gpioreg[PLLA_PER]  = CM_PASSWORD | (divider);
   *CM_PLLA           = CM_PASSWORD | ((*CM_PLLA) |  CM_PLLA_LOADPER);
   *CM_PLLA           = CM_PASSWORD | ((*CM_PLLA) & ~CM_PLLA_LOADPER);

   // Enable PLLA PER divider
   gpioreg[PLLA_PER]  = CM_PASSWORD | (gpioreg[PLLA_PER] & ~A2W_PLL_CHANNEL_DISABLE);
   *CM_PLLA           = CM_PASSWORD | (*CM_PLLA & ~CM_PLLA_HOLDPER);

   // Log the before register values
   log_plla();
}
#endif

int eight_bit_detected() {
    return supports8bit;
}

int new_DAC_detected() {
    return newanalog;
}

void set_vsync_psync(int state) {
    cpld->set_vsync_psync(state);
}

void calculate_cpu_timings() {
static int old_cpuspeed = 0;
   cpuspeed = get_clock_rate(ARM_CLK_ID)/1000000;
   if (cpuspeed != old_cpuspeed) {
       log_info("CPU speed detected as: %d Mhz", cpuspeed);
       old_cpuspeed = cpuspeed;
   }
   field_type_threshold = FIELD_TYPE_THRESHOLD * cpuspeed / 1000;
   elk_lo_field_sync_threshold = ELK_LO_FIELD_SYNC_THRESHOLD * cpuspeed / 1000;
   elk_hi_field_sync_threshold = ELK_HI_FIELD_SYNC_THRESHOLD  * cpuspeed / 1000;
   odd_threshold = ODD_THRESHOLD * cpuspeed / 1000;
   even_threshold = EVEN_THRESHOLD * cpuspeed / 1000;
   hsync_threshold_switch = (BBC_HSYNC_THRESHOLD - 400) * cpuspeed / 1000;
   hsync_threshold = ((autoswitch == AUTOSWITCH_MODE7 && hsync_width < hsync_threshold_switch) ? BBC_HSYNC_THRESHOLD : OTHER_HSYNC_THRESHOLD) * cpuspeed / 1000;
   other_hsync_threshold = OTHER_HSYNC_THRESHOLD * cpuspeed / 1000;
   if (capinfo->vsync_type == VSYNC_INTERLACED) {
      equalising_threshold = EQUALISING_THRESHOLD * cpuspeed / 1000;  // if explicitly selecting interlaced then support filtering equalising pulses
   } else {
      equalising_threshold = 100 * cpuspeed / 1000;                   // otherwise only filter very small pulses (0.1us) which might be glitches
   }
   frame_minimum = (int)((double)FRAME_MINIMUM * cpuspeed / 1000);
   frame_timeout = (int)((double)FRAME_TIMEOUT * cpuspeed / 1000);
   line_minimum = LINE_MINIMUM * cpuspeed / 1000;
   hsync_scroll = (HSYNC_SCROLL_LO * cpuspeed / 1000) | ((HSYNC_SCROLL_HI * cpuspeed / 1000) << 16);
   line_timeout = LINE_TIMEOUT * cpuspeed / 1000;  //not currently used
}

static void init_hardware() {
   int i;

      // Initialize hardware cycle counter
   _init_cycle_counter();

   RPI_SetGpioPullUpDown(SP_DATA_MASK | SW1_MASK | SW2_MASK | SW3_MASK, GPIO_PULLUP);
   RPI_SetGpioPullUpDown(STROBE_MASK | VERSION_MASK | MUX_MASK, GPIO_PULLDOWN);

   supports8bit = 0;
   newanalog = 0;
   simple_detected = 0;

   RPI_SetGpioPinFunction(PSYNC_PIN,    FS_INPUT);
   RPI_SetGpioPinFunction(CSYNC_PIN,    FS_INPUT);
   RPI_SetGpioPinFunction(SW1_PIN,      FS_INPUT);
   RPI_SetGpioPinFunction(SW2_PIN,      FS_INPUT);
   RPI_SetGpioPinFunction(SW3_PIN,      FS_INPUT);
   RPI_SetGpioPinFunction(STROBE_PIN,   FS_INPUT);
   RPI_SetGpioPinFunction(SP_DATA_PIN,  FS_INPUT);
   RPI_SetGpioPinFunction(SP_CLKEN_PIN, FS_INPUT);
   for (i = 0; i < 12; i++) {
      RPI_SetGpioPinFunction(PIXEL_BASE + i, FS_INPUT);
   }
   delay_in_arm_cycles(1000000);                     //~1ms delay

   if (RPI_GetGpioValue(SP_DATA_PIN) == 0) {
       supports8bit = 1;
   }

   RPI_SetGpioPinFunction(SP_DATA_PIN,  FS_OUTPUT);
   RPI_SetGpioPinFunction(SP_CLKEN_PIN, FS_OUTPUT); //force CLKEN low so that V5 is initially detected as V3
   RPI_SetGpioValue(SP_DATA_PIN,        0);
   RPI_SetGpioValue(SP_CLKEN_PIN,       0);

   RPI_SetGpioPinFunction(VERSION_PIN,  FS_OUTPUT);
   RPI_SetGpioValue(VERSION_PIN,        1);          //force VERSION PIN high to help weak pullup
   delay_in_arm_cycles(10000);                       //~10uS settle delay
   RPI_SetGpioPinFunction(VERSION_PIN,  FS_INPUT);   //make VERSION PIN input
   delay_in_arm_cycles(1000000);                     //~1ms delay to allow strong pulldown to take effect if fitted

   int version_state = RPI_GetGpioValue(VERSION_PIN);
   delay_in_arm_cycles(1000);
   version_state |= RPI_GetGpioValue(VERSION_PIN);
   delay_in_arm_cycles(1000);
   version_state |= RPI_GetGpioValue(VERSION_PIN);    // read three times to be sure as it maybe picking up noise from adjacent tracks with just weak pullup

   if (!version_state) {
       simple_detected = 1;
       RPI_SetGpioPinFunction(MUX_PIN,      FS_INPUT);
   } else {
       RPI_SetGpioPinFunction(MUX_PIN,      FS_OUTPUT);
       if (RPI_GetGpioValue(STROBE_PIN) == 1) {      // if high then must be V4, if low then could be (V1-3) or V5
           newanalog = 1;
       } else {
           RPI_SetGpioValue(SP_CLKEN_PIN, 1);  //force CLKEN HIGH to detect V5 analog board
           delay_in_arm_cycles(1000000);             //~1ms  settle delay
           if (RPI_GetGpioValue(STROBE_PIN) == 1) {
               newanalog = 2;
           }
       }
   }

   RPI_SetGpioPinFunction(VERSION_PIN,  FS_OUTPUT);
   RPI_SetGpioPinFunction(MODE7_PIN,    FS_OUTPUT);

   RPI_SetGpioPinFunction(SP_CLK_PIN,   FS_OUTPUT);
   RPI_SetGpioPinFunction(LED1_PIN,     FS_OUTPUT);

   RPI_SetGpioValue(VERSION_PIN,        1);
   RPI_SetGpioValue(MODE7_PIN,          1);
   RPI_SetGpioValue(MUX_PIN,            0);
   RPI_SetGpioValue(SP_CLK_PIN,         1);
   RPI_SetGpioValue(SP_DATA_PIN,        0);
   RPI_SetGpioValue(SP_CLKEN_PIN,       0);
   RPI_SetGpioValue(LED1_PIN,           0); // active high

   // This line enables IRQ interrupts
   // Enable smi_int which is IRQ 48
   // https://github.com/raspberrypi/firmware/issues/67
   RPI_GetIrqController()->Enable_IRQs_2 = (1 << VSYNCINT);

   // Configure the GPCLK pin as a GPCLK
   RPI_SetGpioPinFunction(GPCLK_PIN, FS_ALT5);

   if (simple_detected) {
       log_info("Simple board detected");
   } else {
       log_info("CPLD board detected");
       if (supports8bit) {
           log_info("8/12 bit board detected");
       } else {
           log_info("6 bit board detected");
       }
       if (newanalog == 1) {
           log_info("Issue 4 analog board detected");
       } else if (newanalog == 2) {
           log_info("Issue 5 analog board detected");
       }
   }

   log_info("Using %s as the sampling clock", PLL_NAME);

   // Log all the PLL values
   log_plla();
   log_pllb();
   log_pllc();
   log_plld();
   log_pllh();

#if  defined(USE_PLLA) || defined(USE_PLLA4)
   // Enable the PLLA_PER divider
   configure_plla(PLLA_PER_VALUE);
#endif

   // The divisor us now the same for both modes
   log_debug("Setting up divisor");
   init_gpclk(GPCLK_SOURCE, DEFAULT_GPCLK_DIVISOR);
   log_debug("Done setting up divisor");

   calculate_cpu_timings();
   // Initialize the cpld after the gpclk generator has been started
   cpld_init();

   // Initialize the On-Screen Display
   osd_init();

   // Initialise the info system with cached values (as we break the GPU property interface)
   init_info();

#ifdef DEBUG
   dump_useful_info();
#endif
}

int read_cpld_version(){
int cpld_version_id = 0;
   if (!simple_detected) {
       for (int i = PIXEL_BASE + 11; i >= PIXEL_BASE; i--) {
          cpld_version_id <<= 1;
          cpld_version_id |= RPI_GetGpioValue(i) & 1;
       }
   } else {
       cpld_version_id = (DESIGN_SIMPLE << 8); // reads as V0.0
   }
   return cpld_version_id;
}

static void cpld_init() {
// have to set mux to 0 to allow analog detection to work
// so clock out 32 bits of 0 into register chain as later CPLDs have mux as a register bit

  // int sp = 0x1180;  //15 bits sets the rate bits to 12bit capture for testing simple mode with amiga
   int sp = 0x200000;  //24 bits sets the rate bits to 12bit capture for testing simple mode with amiga
   for (int i = 0; i < 24; i++) {
      RPI_SetGpioValue(SP_DATA_PIN, sp & 1);
      delay_in_arm_cycles_cpu_adjust(250);
      RPI_SetGpioValue(SP_CLKEN_PIN, 1);
      delay_in_arm_cycles_cpu_adjust(250);
      RPI_SetGpioValue(SP_CLK_PIN, 0);
      delay_in_arm_cycles_cpu_adjust(250);
      RPI_SetGpioValue(SP_CLK_PIN, 1);
      delay_in_arm_cycles_cpu_adjust(250);
      RPI_SetGpioValue(SP_CLKEN_PIN, 0);
      delay_in_arm_cycles_cpu_adjust(250);
      sp >>= 1;
   }
   RPI_SetGpioValue(MUX_PIN, 0);   // have to set mux to 0 to allow analog detection to work (GPIO on older cplds)
   // Assert the active low version pin
   RPI_SetGpioValue(VERSION_PIN, 0);
   delay_in_arm_cycles_cpu_adjust(100);
   RPI_SetGpioPinFunction(STROBE_PIN, FS_OUTPUT);
   RPI_SetGpioValue(STROBE_PIN, 0);
   delay_in_arm_cycles_cpu_adjust(1000);
   // The CPLD now outputs an identifier and version number on the 12-bit pixel quad bus
   cpld_version_id = read_cpld_version();

   int cpld_design = cpld_version_id >> VERSION_DESIGN_BIT;
   int cpld_version = cpld_version_id & 0xff;

   // Set the appropriate cpld "driver" based on the version
   if (cpld_design == DESIGN_BBC) {
      RPI_SetGpioPinFunction(STROBE_PIN, FS_INPUT);
      if (cpld_version <= 0x20) {
         cpld = &cpld_bbcv10v20;
      } else if (cpld_version <= 0x23) {
         cpld = &cpld_bbcv21v23;
      } else if (cpld_version <= 0x24) {
         cpld = &cpld_bbcv24;
      } else if (cpld_version <= 0x62) {
         cpld = &cpld_bbcv30v62;
      } else {
         cpld = &cpld_bbc;
      }
   } else if (cpld_design == DESIGN_ATOM) {
      RPI_SetGpioPinFunction(STROBE_PIN, FS_INPUT);
      cpld = &cpld_atom;
   } else if (cpld_design == DESIGN_YUV) {
      cpld = &cpld_yuv;
   } else if (cpld_design == DESIGN_RGB_TTL) {
       RPI_SetGpioValue(STROBE_PIN, 1);
       delay_in_arm_cycles_cpu_adjust(1000);
       if (cpld_design == DESIGN_RGB_ANALOG) {       // if STROBE_PIN (GPIO22) has an effect on the version ID (P19) it means the RGB cpld has been programmed into the BBC board
           cpld = &cpld_null_6bit;
           cpld_fail_state = CPLD_WRONG;
       } else {
           if (cpld_version >= 0x70 && cpld_version < 0x80) {
               cpld = &cpld_rgb_ttl_24mhz;
           } else {
               cpld = &cpld_rgb_ttl;
           }
       }
       RPI_SetGpioPinFunction(STROBE_PIN, FS_INPUT);   // set STROBE PIN back to an input as P19 will be an ouput when VERSION_PIN set back to 1
   } else if (cpld_design == DESIGN_RGB_ANALOG) {
      if (cpld_version >= 0x70 && cpld_version < 0x80) {
             cpld = &cpld_rgb_analog_24mhz;
      } else {
             cpld = &cpld_rgb_analog;
      }
   } else if (cpld_design == DESIGN_SIMPLE) {
      cpld = &cpld_simple;
   } else {
      log_info("Unknown CPLD: identifier = %03x", cpld_version_id);
      if (cpld_version_id == 0xfff) {
         cpld_fail_state = CPLD_BLANK;
      } else {
         cpld_fail_state = CPLD_UNKNOWN;
      }
      cpld = &cpld_null;
      RPI_SetGpioPinFunction(STROBE_PIN, FS_INPUT);
   }

   if (!test_file(FORCE_UPDATE_FILE) && cpld_fail_state == CPLD_NORMAL) {
       log_info("CPLD update file not detected");
       if (cpld_design == DESIGN_RGB_TTL || cpld_design == DESIGN_RGB_ANALOG) {
           if ( ((cpld_version & 0xf0) == (BBC_VERSION & 0xf0) && (cpld_version & 0x0f) >= (BBC_VERSION & 0x0f))
              || ((cpld_version & 0xf0) == (RGB_VERSION & 0xf0) && (cpld_version & 0x0f) >= (RGB_VERSION & 0x0f)) ) {
              check_file(FORCE_UPDATE_FILE, FORCE_UPDATE_FILE_MESSAGE);
              log_info("CPLD_UPDATE state not set");
           } else {
               cpld_fail_state = CPLD_UPDATE;
              log_info("CPLD_UPDATE state set");

           }
       }
       if (cpld_design == DESIGN_YUV) {
           if ( ((cpld_version & 0xf0) == (YUV_VERSION & 0xf0) && (cpld_version & 0x0f) >= (YUV_VERSION & 0x0f)) ) {
              check_file(FORCE_UPDATE_FILE, FORCE_UPDATE_FILE_MESSAGE);
              log_info("CPLD_UPDATE state not set.");
           } else {
              cpld_fail_state = CPLD_UPDATE;
              log_info("CPLD_UPDATE state set.");
           }
       }
   }

   int keycount = key_press_reset();
   log_info("Keycount = %d", keycount);
   if (keycount == 7) {
       switch(cpld_design) {
           case DESIGN_BBC:
                cpld = &cpld_null_3bit;
                break;
           case DESIGN_RGB_TTL:
           case DESIGN_RGB_ANALOG:
           case DESIGN_YUV:
                cpld = &cpld_null_6bit;
                break;
           case DESIGN_ATOM:
                cpld = &cpld_null_atom;
                break;
           case DESIGN_SIMPLE:
                cpld = &cpld_null_simple;
                break;
           default:
                cpld = &cpld_null;
                break;
       }
      cpld_fail_state = CPLD_MANUAL;
      RPI_SetGpioPinFunction(STROBE_PIN, FS_INPUT);
   }



   // Release the active low version pin. This will damage the cpld if YUV is programmed into a BBC board but not RGB due to above safety test
   delay_in_arm_cycles_cpu_adjust(1000);
   RPI_SetGpioValue(VERSION_PIN, 1);

   log_info("CPLD  Design: %s", cpld->name);
   log_info("CPLD Version: %x.%x", (cpld_version_id >> VERSION_MAJOR_BIT) & 0x0f, (cpld_version_id >> VERSION_MINOR_BIT) & 0x0f);

   // Initialize the CPLD's default sampling points
   cpld->init(cpld_version_id);
   // Initialize the geometry
   geometry_init(cpld_version_id);
}

int extra_flags() {
   int extra = 0;
   if (cpld->old_firmware_support()) {
        extra |= BIT_OLD_FIRMWARE_SUPPORT;
   }
   if (autoswitch != AUTOSWITCH_MODE7) {
        extra |= BIT_NO_H_SCROLL;
   }
   if (autoswitch != AUTOSWITCH_PC || !sub_profiles_available(profile)) {
        extra |= BIT_NO_AUTOSWITCH;
   }
   if (!scanlines || ((capinfo->sizex2 & 1) == 0) || (capinfo->video_type == VIDEO_TELETEXT) || osd_active()) {
        extra |= BIT_NO_SCANLINES;
   }
   if (osd_active()) {
        extra |= BIT_OSD;
   }
   if (cpld->get_sync_edge()) {
        extra |= BIT_HSYNC_EDGE;
   }
return extra;
}

#ifdef HAS_MULTICORE
static void start_core(int core, func_ptr func) {
   printf("starting core %d\r\n", core);
   *(unsigned int *)(0x4000008c + 0x10 * core) = (unsigned int) func;
   asm  ( "sev" );
}
#endif

// =============================================================
// Public methods
// =============================================================

int diff_N_frames(capture_info_t *capinfo, int n, int mode7, int elk) {
   int result = 0;

   // Calculate frame differences, broken out by channel and by sample point (A..F)
   int *by_offset = diff_N_frames_by_sample(capinfo, n, mode7, elk);

   // Collapse the offset dimension
   for (int i = 0; i < NUM_OFFSETS; i++) {
      result += by_offset[i];
   }
   return result;
}

int *diff_N_frames_by_sample(capture_info_t *capinfo, int n, int mode7, int elk) {

   unsigned int ret;

   // NUM_OFFSETS is 6 (Sample Offset A..Sample Offset F)
   static int  sum[NUM_OFFSETS];
   static int  min[NUM_OFFSETS];
   static int  max[NUM_OFFSETS];
   static int diff[NUM_OFFSETS];

   for (int i = 0; i < NUM_OFFSETS; i++) {
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

   unsigned int flags = extra_flags() | mode7 | BIT_CALIBRATE | (2 << OFFSET_NBUFFERS);

   uint32_t bpp      = capinfo->bpp;
   uint32_t pix_mask;
   uint32_t osd_mask;

   uint32_t mask_BIT_OSD = -1;

   switch (bpp) {
       case 4:
            pix_mask = 0x00000007;
            osd_mask = 0x77777777;
            capinfo->ncapture = (capinfo->video_type != VIDEO_PROGRESSIVE) ? 2 : 1;
            break;
       case 8:
            pix_mask = 0x0000007F;
            osd_mask = 0x77777777;
            capinfo->ncapture = (capinfo->video_type != VIDEO_PROGRESSIVE) ? 2 : 1;
            break;
       case 16:
       default:
            pix_mask = 0x00000fff;
            osd_mask = 0xffffffff;
            //if (capinfo->video_type == VIDEO_INTERLACED && capinfo->detected_sync_type & SYNC_BIT_INTERLACED) {
            //    mask_BIT_OSD = ~BIT_OSD;
            //}
            capinfo->ncapture = 1;
            break;
   }
//capinfo->video_type == VIDEO_INTERLACED && capinfo->detected_sync_type & SYNC_BIT_INTERLACED
   geometry_get_fb_params(capinfo);            // required as calibration sets delay to 0 and the 2 high bits of that adjust the h offset

#ifdef INSTRUMENT_CAL
   t = _get_cycle_counter();
#endif
   // Grab an initial frame
   ret = rgb_to_fb(capinfo, flags & mask_BIT_OSD);
#ifdef INSTRUMENT_CAL
   t_capture += _get_cycle_counter() - t;
#endif

   for (int i = 0; i < n; i++) {

      for (int j = 0; j < NUM_OFFSETS; j++) {
         diff[j] = 0;
      }

#ifdef INSTRUMENT_CAL
      t = _get_cycle_counter();
#endif
      // Save the last frame
      memcpy((void *)last, (void *)(capinfo->fb + ((ret >> OFFSET_LAST_BUFFER) & 3) * capinfo->height * capinfo->pitch), capinfo->height * capinfo->pitch);
#ifdef INSTRUMENT_CAL
      t_memcpy += _get_cycle_counter() - t;
      t = _get_cycle_counter();
#endif
      // Grab the next frame
      ret = rgb_to_fb(capinfo, flags & mask_BIT_OSD);
#ifdef INSTRUMENT_CAL
      t_capture += _get_cycle_counter() - t;
      t = _get_cycle_counter();
#endif
     // memcpy((void *)latest, (void *)(capinfo->fb + ((ret >> OFFSET_LAST_BUFFER) & 3) * capinfo->height * capinfo->pitch), capinfo->height * capinfo->pitch);

      int single_pixel_count = 0;
      // Compare the frames
      uint32_t *fbp = (uint32_t *)(capinfo->fb + ((ret >> OFFSET_LAST_BUFFER) & 3) * capinfo->height * capinfo->pitch + capinfo->v_adjust * capinfo->pitch);
      uint32_t *lastp = (uint32_t *)last + capinfo->v_adjust * (capinfo->pitch >> 2);
      for (int y = 0; y < (capinfo->nlines << (capinfo->sizex2 & 1)); y++) {
         int skip = 0;
         // Calculate the capture scan line number (allowing for a double hight framebuffer)
         // (capinfo->height is the framebuffer height after any doubling)
         int line = (capinfo->sizex2 & 1) ? (y >> 1) : y;
         // As v_offset increases, e.g. by one, the screen image moves up one capture line
         // (the hardcoded constant of 21 relates to the BBC video format)
         line += (capinfo->v_offset - 21);
         // Skip lines that might contain flashing cursor
         // (the cursor rows were determined empirically)
         if (line >= 0) {
            if (elk) {
               // Eliminate cursor lines in 32 row modes (0,1,2,4,5)
               if (capinfo->video_type != VIDEO_TELETEXT && (line % 8) == 5) {
                  skip = 1;
               }
               // Eliminate cursor lines in 25 row modes (3, 6)
               if (capinfo->video_type != VIDEO_TELETEXT && (line % 10) == 3) {
                  skip = 1;
               }
               // Eliminate cursor lines in mode 7
               // (this case is untested as I don't have a Jafa board)
               if (capinfo->video_type == VIDEO_TELETEXT && (line % 10) == 7) {
                  skip = 1;
               }
            } else {
               // Eliminate cursor lines in 32 row modes (0,1,2,4,5)
               if (capinfo->video_type != VIDEO_TELETEXT && (line % 8) == 7) {
                  skip = 1;
               }
               // Eliminate cursor lines in 25 row modes (3, 6)
               if (capinfo->video_type != VIDEO_TELETEXT && (line % 10) >= 5 && (line % 10) <= 7) {
                  skip = 1;
               }
               // Eliminate cursor lines in mode 7
               if (capinfo->video_type == VIDEO_TELETEXT && (line % 10) == 7) {
                  skip = 1;
               }
            }
         }

         if (skip) {
            // For debugging it's useful to see if the lines being eliminated align with the cursor
            // for (int x = 0; x < capinfo->pitch; x += 4) {
            //    *fbp++ = 0x11111111;
            // }
            fbp   += (capinfo->pitch >> 2);
            lastp += (capinfo->pitch >> 2);
         } else {
               switch (bpp) {
                   case 4:
                   {
                        if (mode7) {
                            single_pixel_count += scan_for_single_pixels_4bpp(fbp, capinfo->pitch);
                        }
                        for (int x = 0; x < capinfo->pitch; x += 4) {
                            uint32_t d = osd_get_equivalence(*fbp++ & osd_mask) ^ osd_get_equivalence(*lastp++ & osd_mask);
                            int index = (x << 1) % NUM_OFFSETS;  //2 pixels per byte
                            while (d) {
                                if (d & pix_mask) {
                                   diff[index]++;
                                }
                                d >>= 4;
                                index = (index + 1) % NUM_OFFSETS;
                            }
                        }

                        break;
                   }
                   case 8:
                        for (int x = 0; x < capinfo->pitch; x += 4) {
                            uint32_t d = osd_get_equivalence(*fbp++ & osd_mask) ^ osd_get_equivalence(*lastp++ & osd_mask);
                            int index = x % NUM_OFFSETS;         //1 pixel per byte
                            while (d) {
                                if (d & pix_mask) {
                                   diff[index]++;
                                }
                                d >>= 8;
                                index = (index + 1) % NUM_OFFSETS;
                            }
                        }
                        break;
                   case 16:
                   default:
                   {
                        if (mode7) {
                            single_pixel_count += scan_for_single_pixels_12bpp(fbp, capinfo->pitch);
                        }
                        scan_for_diffs_12bpp(fbp, lastp, capinfo->pitch, diff);
                        fbp += (capinfo->pitch >> 2);
                        lastp += (capinfo->pitch >> 2);
                        break;
                   }
               }
         }
      }
#ifdef INSTRUMENT_CAL
      t_compare += _get_cycle_counter() - t;
#endif
      // At this point the diffs correspond to the sample points in
      // an unusual order: A F C B E D
      //
      // This happens for three reasons:
      // - the CPLD starts with sample point B, so you get B C D E F A
      // - the firmware skips the first quad, so you get F A B C D E
      // - if in 4bpp mode the frame buffer swaps odd and even pixels, so you get A F C B E D

     // Mutate the result to correctly order the sample points:
      // Then the downstream algorithms don't have to worry

       //log_info("count = %d, %d", mode7, single_pixel_count);
       if (mode7 && single_pixel_count == 0) {   //generate fake diff errors if thick verticals detected
              diff[0] += 1000;
              diff[1] += 1000;
              diff[2] += 1000;
              diff[3] += 1000;
              diff[4] += 1000;
              diff[5] += 1000;
       }


      if (bpp == 4) {
          // A F C B E D => A B C D E F
          int f = diff[1];
          int b = diff[3];
          int d = diff[5];
          diff[1] = b;
          diff[3] = d;
          diff[5] = f;
      } else {
          // F A B C D E => A B C D E F
          int f = diff[0];
          diff[0] = diff[1];
          diff[1] = diff[2];
          diff[2] = diff[3];
          diff[3] = diff[4];
          diff[4] = diff[5];
          diff[5] = f;
      }

      // Accumulate the result
      for (int j = 0; j < NUM_OFFSETS; j++) {
         sum[j] += diff[j];
         if (diff[j] < min[j]) {
            min[j] = diff[j];
         }
         if (diff[j] > max[j]) {
            max[j] = diff[j];
         }
      }


   }
#if 0
   for (int i = 0; i < NUM_OFFSETS; i++) {
      log_debug("offset %d diff:  sum = %d min = %d, max = %d", i, sum[i], min[i], max[i]);
   }
#endif

#ifdef INSTRUMENT_CAL
   log_debug("t_capture total = %d, mean = %d ", t_capture, t_capture / (n + 1));
   log_debug("t_compare total = %d, mean = %d ", t_compare, t_compare / n);
   log_debug("t_memcpy  total = %d, mean = %d ", t_memcpy,  t_memcpy / n);
   log_debug("total = %d", t_capture + t_compare + t_memcpy);
#endif
   return sum;
}

#define MODE7_CHAR_WIDTH 12

signed int analyze_mode7_alignment(capture_info_t *capinfo) {
    if (capinfo->video_type != VIDEO_TELETEXT) {
        return -1;
    }
   log_info("Testing mode 7 alignment");
   // mode 7 character is 12 pixels wide
   int counts[MODE7_CHAR_WIDTH];
   // bit offset pixels 0..7
   int px_offset_map[] = {4, 0, 12, 8, 20, 16, 28, 24};

   unsigned int flags = extra_flags() | BIT_MODE7 | BIT_CALIBRATE | (2 << OFFSET_NBUFFERS);

   // Capture two fields
   capinfo->ncapture = 2;

   // Grab a frame
   int ret = rgb_to_fb(capinfo, flags);

   // Work out the base address of the frame buffer that was used
   uint32_t *fbp = (uint32_t *)(capinfo->fb + ((ret >> OFFSET_LAST_BUFFER) & 3) * capinfo->height * capinfo->pitch + capinfo->v_adjust * capinfo->pitch + capinfo->h_adjust);

   // Initialize the counters
   for (int i = 0; i < MODE7_CHAR_WIDTH; i++) {
      counts[i] = 0;
   }

   // Count the pixels
   uint32_t *fbp_line;

   for (int line = 0; line < capinfo->nlines << (capinfo->sizex2 & 1); line++) {
      int index = 0;
      fbp_line = fbp;
      for (int byte = 0; byte < (capinfo->chars_per_line << 2); byte += 4) {
         uint32_t word = *fbp_line++;
         int *offset = px_offset_map;
         for (int i = 0; i < 8; i++) {
            int px = (word >> (*offset++)) & 7;
            if (px) {
               counts[index]++;
            }
            index = (index + 1) % MODE7_CHAR_WIDTH;
         }
      }
      fbp += capinfo->pitch >> 2;
   }

   // Log the raw counters
   for (int i = 0; i < MODE7_CHAR_WIDTH; i++) {
      log_info("counter %2d = %d", i, counts[i]);
   }

   // A typical distribution looks like
   // INFO: counter  0 = 647
   // INFO: counter  1 = 573
   // INFO: counter  2 = 871
   // INFO: counter  3 = 878
   // INFO: counter  4 = 572
   // INFO: counter  5 = 653
   // INFO: counter  6 = 869
   // INFO: counter  7 = 742
   // INFO: counter  8 = 2
   // INFO: counter  9 = 2
   // INFO: counter 10 = 906
   // INFO: counter 11 = 1019

   // There should be a two pixel minima
   int min_count = INT_MAX;
   int min_i = -1;
   for (int i = 0; i < MODE7_CHAR_WIDTH; i++) {
      int c = counts[i] + counts[(i + 1) % MODE7_CHAR_WIDTH];
      if (c < min_count) {
         min_count = c;
         min_i = i;
      }
   }
   log_info("minima at index: %d", min_i);

   // That minima should occur in pixels 0 and 1, so compute a delay to make this so
   return (MODE7_CHAR_WIDTH - min_i);
}

#define DEFAULT_CHAR_WIDTH 8

signed int analyze_default_alignment(capture_info_t *capinfo) {

    if (autoswitch != AUTOSWITCH_MODE7) {
        return -1;
    }
   log_info("Testing default alignment");
   // mode 0 character is 8 pixels wide
   int counts[DEFAULT_CHAR_WIDTH];
   // bit offset pixels 0..7
   int px_offset_map[] = {4, 0, 12, 8, 20, 16, 28, 24};

   unsigned int flags = extra_flags() | BIT_CALIBRATE | (2 << OFFSET_NBUFFERS);

   // Capture two fields
   capinfo->ncapture = 1;

   // Grab a frame
   int ret = rgb_to_fb(capinfo, flags);

   // Work out the base address of the frame buffer that was used
   uint32_t *fbp = (uint32_t *)(capinfo->fb + ((ret >> OFFSET_LAST_BUFFER) & 3) * capinfo->height * capinfo->pitch + capinfo->v_adjust * capinfo->pitch + capinfo->h_adjust);

   // Initialize the counters
   for (int i = 0; i < DEFAULT_CHAR_WIDTH; i++) {
      counts[i] = 0;
   }

   // Count the pixels
   uint32_t *fbp_line;


   if (capinfo->bpp == 4)
   {

    for (int line = 0; line <  capinfo->nlines << (capinfo->sizex2 & 1); line++) {
      int index = 0;
      fbp_line = fbp;
      for (int byte = 0; byte < (capinfo->chars_per_line << 2); byte += 4) {
         uint32_t word = *fbp_line++;
         int *offset = px_offset_map;
         for (int i = 0; i < 8; i++) {
            int px = (word >> (*offset++)) & 7;
            if (px) {
               counts[index]++;
            }
            index = (index + 1) % DEFAULT_CHAR_WIDTH;
         }
      }
      fbp += capinfo->pitch >> 2;
    }

   } else {
    for (int line = 0; line <  capinfo->nlines << (capinfo->sizex2 & 1); line++) {
      int index = 0;
      fbp_line = fbp;
      for (int byte = 0; byte < (capinfo->chars_per_line << 2); byte += 4) {
         uint32_t word = *fbp_line++;
         for (int i = 0; i < 4; i++) {
            int px = (word >> (i*8)) & 0x7f;
            if (px) {
               counts[index]++;
            }
            index = (index + 1) % DEFAULT_CHAR_WIDTH;
         }
         word = *fbp_line++;
         for (int i = 0; i < 4; i++) {
            int px = (word >> (i*8)) & 0x7f;
            if (px) {
               counts[index]++;
            }
            index = (index + 1) % DEFAULT_CHAR_WIDTH;
         }
      }
      fbp += capinfo->pitch >> 2;
    }
   }
   // Log the raw counters
   for (int i = 0; i < DEFAULT_CHAR_WIDTH; i++) {
      log_info("counter %2d = %d", i, counts[i]);
   }

   // A typical distribution looks like
   // INFO: counter  0 = 878
   // INFO: counter  1 = 740
   // INFO: counter  2 = 212
   // INFO: counter  3 = 2
   // INFO: counter  4 = 1036
   // INFO: counter  5 = 1224
   // INFO: counter  6 = 648
   // INFO: counter  7 = 706

   // There should be a one pixel minima
   int min_count = INT_MAX;
   int min_i = -1;
   for (int i = 0; i < DEFAULT_CHAR_WIDTH; i++) {
      int c = counts[i];
      if (c < min_count) {
         min_count = c;
         min_i = i;
      }
   }
   log_info("minima at index: %d", min_i);

   // That minima should occur in pixels 0 and 1, so compute a delay to make this so
   return DEFAULT_CHAR_WIDTH - min_i;
}

#if 0
int total_N_frames(capture_info_t *capinfo, int n, int mode7, int elk) {

   int sum = 0;
   int min = INT_MAX;
   int max = INT_MIN;

#ifdef INSTRUMENT_CAL
   unsigned int t;
   unsigned int t_capture = 0;
   unsigned int t_compare = 0;
#endif

   unsigned int flags = extra_flags() | mode7 | BIT_CALIBRATE | (2 << OFFSET_NBUFFERS);

   // In mode 0..6, capture one field
   // In mode 7,    capture two fields
   capinfo->ncapture = capinfo->video_type != VIDEO_PROGRESSIVE ? 2 : 1;

   for (int i = 0; i < n; i++) {
      int total = 0;

      // Grab the next frame
      ret = rgb_to_fb(capinfo, flags);
#ifdef INSTRUMENT_CAL
      t_capture += _get_cycle_counter() - t;
      t = _get_cycle_counter();
#endif
      // Compare the frames
      uint32_t *fbp = (uint32_t *)(capinfo->fb + ((ret >> OFFSET_LAST_BUFFER) & 3) * capinfo->height * capinfo->pitch);
      for (int j = 0; j < capinfo->height * capinfo->pitch; j += 4) {
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
   log_debug("total: sum = %d mean = %d, min = %d, max = %d", sum, mean, min, max);
#ifdef INSTRUMENT_CAL
   log_debug("t_capture total = %d, mean = %d ", t_capture, t_capture / (n + 1));
   log_debug("t_compare total = %d, mean = %d ", t_compare, t_compare / n);
   log_debug("total = %d", t_capture + t_compare + t_memcpy);
#endif
   return sum;
}
#endif

#ifdef MULTI_BUFFER
void swapBuffer(int buffer) {
  current_display_buffer = buffer;
  if (capinfo->bpp == 16) {
     // directly manipulate the display list in 16BPP mode otherwise display list gets reconstructed
     display_list[display_list_index + 5] = ((int)capinfo->fb | 0xc0000000) + (buffer * capinfo->height * capinfo->pitch);
  } else {
     RPI_PropertyInit();
     RPI_PropertyAddTag(TAG_SET_VIRTUAL_OFFSET, 0, capinfo->height * buffer);
     // Use version that doesn't wait for the response
     RPI_PropertyProcessNoCheck();
  }
}
#endif

int get_current_display_buffer() {
   if ((capinfo->video_type == VIDEO_PROGRESSIVE || (capinfo->video_type == VIDEO_INTERLACED && !interlaced))) {
       return current_display_buffer;
   } else {
       return 0;
   }
}

void set_startup_overscan(int value) {
    startup_overscan = value;
}

int get_startup_overscan() {
    return startup_overscan;
}

void set_config_overscan(int l, int r, int t, int b) {
   config_overscan_left = l;
   config_overscan_right = r;
   config_overscan_top = t;
   config_overscan_bottom = b;
}

void get_config_overscan(int *l, int *r, int *t, int *b) {
   *l = config_overscan_left;
   *r = config_overscan_right;
   *t = config_overscan_top;
   *b = config_overscan_bottom;
}

void set_profile(int val) {
   log_info("Setting profile to %d", val);
   profile = val;
}

int get_profile() {
   return profile;
}

void set_saved_config_number(int val) {
   log_info("Setting saved config number to %d", val);
   saved_config_number = val;
}

int get_saved_config_number() {
   return saved_config_number;
}

void set_subprofile(int val) {
   log_info("Setting subprofile to %d", val);
   subprofile = val;
}

int get_subprofile() {
   return subprofile;
}
void set_paletteControl(int value) {
   if (paletteControl != value) {
      paletteControl = value;
      osd_update_palette();
   }
}

int get_paletteControl() {
   return paletteControl;
}

void set_force_genlock_range(int value) {
    force_genlock_range = value;
}

void set_hdmi(int value, int reboot) {
    hdmi_mode = value;
    if (reboot == 0) {
       old_hdmi_mode = hdmi_mode;
    } else {
        if (hdmi_mode != old_hdmi_mode) {
           reboot_required |= 0x04;
           log_info("Requesting hdmi reboot %d", hdmi_mode);
           resolution_warning = 1;
        } else {
           reboot_required &= ~0x04;
           resolution_warning = 0;
        }
    }
    if (reboot) {
       file_save_config(resolution_name, refresh, scaling, filtering, frontend, hdmi_mode);
    }
}

int get_hdmi() {
    return hdmi_mode;
}

void set_refresh(int value, int reboot) {
    refresh = value;
    if (reboot == 0) {
       old_refresh = refresh;
    } else {
        if (refresh != old_refresh) {
           reboot_required |= 0x08;
           log_info("Requesting refresh reboot %d", refresh);
           resolution_warning = 1;
        } else {
           reboot_required &= ~0x08;
           resolution_warning = 0;
        }
    }
    if (reboot) {
       file_save_config(resolution_name, refresh, scaling, filtering, frontend, hdmi_mode);
    }
}

int get_refresh() {
    return refresh;
}

void set_resolution(int mode, const char *name, int reboot) {
   resolution = mode;
   strcpy(resolution_name, name);
   if (reboot == 0) {
       old_resolution = resolution;
   } else {
       if (resolution != old_resolution) {
           reboot_required |= 0x01;
           resolution_warning = 1;
       } else {
           reboot_required &= ~0x01;
           resolution_warning = 0;
       }
   }
   if (reboot) {
       file_save_config(resolution_name, refresh, scaling, filtering, frontend, hdmi_mode);
   }
}

int get_resolution() {
   return resolution;
}

void set_filtering(int filter) {
    filtering = filter;
    old_filtering = filter;
}

void set_scaling(int mode, int reboot) {
   if (mode == SCALING_AUTO) {
        geometry_set_mode(0);
        int width = geometry_get_value(MIN_H_WIDTH);
        int h_size = get_hdisplay();
        int v_size = get_vdisplay();
        double ratio = (double) h_size / v_size;
        int h_size43 = h_size;
        if (ratio > 1.34) {
           h_size43 = v_size * 4 / 3;
        }
        int video_type = geometry_get_value(VIDEO_TYPE);
        geometry_set_mode(mode7);
        if ((video_type == VIDEO_TELETEXT && get_m7scaling() == SCALING_UNEVEN)             // workaround mode 7 width so it looks like other modes
         ||( video_type != VIDEO_TELETEXT && get_normalscaling() == SCALING_UNEVEN && get_haspect() == 3 && (get_vaspect() == 2 || get_vaspect() == 4))) {
             width = width * 4 / 3;
        }
        if ((width > 340 && h_size43 < 1440 && (h_size43 % width) > (width / 3)) || (autoswitch == AUTOSWITCH_MODE7 && v_size == 1024)) {
            gscaling = GSCALING_MANUAL43;
            filtering = FILTERING_SOFT;
            set_auto_name("Auto (Interp. 4:3/Soft)");
            osd_refresh();
            log_info("Setting 4:3 soft");
        } else {
            gscaling = GSCALING_INTEGER;
            if ((autoswitch == AUTOSWITCH_MODE7) && (v_size == 600 || v_size == 576)) {
                filtering = FILTERING_SOFT;
                set_auto_name("Auto (Integer/Soft)");
                osd_refresh();
                log_info("Setting Integer soft");
            } else {
                filtering = FILTERING_NEAREST_NEIGHBOUR;
                set_auto_name("Auto (Integer/Sharp)");
                osd_refresh();
                log_info("Setting Integer Sharp");
            }
        }
   } else {
       switch (mode) {
           default:
           case SCALING_INTEGER_SHARP:
               gscaling = GSCALING_INTEGER;
               filtering = FILTERING_NEAREST_NEIGHBOUR;
           break;

           case SCALING_INTEGER_SOFT:
               gscaling = GSCALING_INTEGER;
               filtering = FILTERING_SOFT;
           break;

           case SCALING_INTEGER_VERY_SOFT:
               gscaling = GSCALING_INTEGER;
               filtering = FILTERING_VERY_SOFT;
           break;

           case SCALING_FILL43_SOFT:
               gscaling = GSCALING_MANUAL43;
               filtering = FILTERING_SOFT;
           break;

           case SCALING_FILL43_VERY_SOFT:
               gscaling = GSCALING_MANUAL43;
               filtering = FILTERING_VERY_SOFT;
           break;

           case SCALING_FILLALL_SOFT:
               gscaling = GSCALING_MANUAL;
               filtering = FILTERING_SOFT;
           break;

           case SCALING_FILLALL_VERY_SOFT:
               gscaling = GSCALING_MANUAL;
               filtering = FILTERING_VERY_SOFT;
           break;
       }
   }
   scaling = mode;
   set_gscaling(gscaling);

   if (reboot != 0 && filtering != old_filtering) {
       reboot_required |= 0x02;
       log_info("Requesting reboot %d", filtering);
   } else {
       reboot_required &= ~0x02;
   }
   if (reboot == 1 || (reboot == 2 && reboot_required)) {
       file_save_config(resolution_name, refresh, scaling, filtering, frontend, hdmi_mode);
   }
}

int get_scaling() {
   return scaling;
}

void set_frontend(int value, int save) {
   int min = cpld->frontend_info() & 0xffff;
   int max = cpld->frontend_info() >> 16;
   if (value >= min && value <= max) {
       frontend = value;
   } else {
       if (value == 0 || value > max) {
           frontend = min;
       } else {
           frontend = max;
       }
   }
   if (save != 0) {
       file_save_config(resolution_name, refresh, scaling, filtering, frontend, hdmi_mode);
   }
   cpld->set_frontend(frontend);
}

int get_frontend() {
       return frontend;
}

void set_ntsccolour(int value) {
   ntsccolour = value;
}

int get_ntsccolour() {
   return ntsccolour;
}

void set_m7deinterlace(int mode) {
   m7deinterlace = mode;
}

int get_m7deinterlace() {
   return m7deinterlace;
}

void set_deinterlace(int mode) {
   deinterlace = mode;
}

int get_deinterlace() {
   return deinterlace;
}
void set_scanlines(int on) {
   scanlines = on;
   clear = BIT_CLEAR;
}

int get_scanlines() {
   return scanlines;
}
void set_res_status(int value) {
   resolution_status = value;
}

int get_res_status() {
   return resolution_status;
}

void set_scanlines_intensity(int value) {
   scanlines_intensity = value;
}

int get_scanlines_intensity() {
   if ((geometry_get_value(FB_SIZEX2) & 1) == 0) {
      return 0;   // returns 0 depending on state of double height
   } else {
      return scanlines_intensity;
   }
}

void set_colour(int val) {
   colour = val;
}

int get_colour() {
   return colour;
}

void set_invert(int value) {
   invert =value;
}

int get_invert() {
   return invert;
}

void set_fontsize(int value) {
   fontsize=value;
}

int get_fontsize() {
   return fontsize;
}

void set_ntscphase(int value) {
   if (ntscphase != value) {
      ntscphase = value;
      osd_update_palette();
   }
}

int  get_ntscphase() {
   return ntscphase;
}

void set_border(int value) {
   border = value;
   clear = BIT_CLEAR;
}

int  get_border() {
   return border;
}

void set_vsync(int on) {
   vsync = on;
}

int get_vsync() {
   return vsync;
}

void set_vlockmode(int val) {
   vlockmode = val;
   recalculate_hdmi_clock_line_locked_update(GENLOCK_FORCE);
}

int get_vlockmode() {
   return vlockmode;
}

void set_vlockline(int val) {
   vlockline = val;
   recalculate_hdmi_clock_line_locked_update(GENLOCK_FORCE);
}

int get_vlockline() {
   return vlockline;
}

void set_vlockadj(int val) {
   vlockadj = val;
   recalculate_hdmi_clock_line_locked_update(GENLOCK_FORCE);
}

int get_vlockadj() {
   return vlockadj;
}

void set_vlockspeed(int val) {
   vlockspeed = val;
}

int get_vlockspeed() {
   return vlockspeed;
}

#ifdef MULTI_BUFFER
int get_nbuffers() {
   return nbuffers;
}

void set_nbuffers(int val) {
   nbuffers=val;
}
#endif

void set_debug(int on) {
   debug = on;
}

int get_debug() {
   return debug;
}
int get_lines_per_vsync() {
    int lines = geometry_get_value(LINES_FRAME);
    if (lines_per_vsync > (lines - 20) && lines_per_vsync <= (lines + 1)) {
       return lines_per_vsync;
    } else {
        return lines;
    }

}

int get_50hz_state() {
    if (source_vsync_freq_hz == 50) {
       return vlock_limited;
    }
    return -1;
}

void set_autoswitch(int value) {
   // Prevent autoswitch (to mode 7) being accidentally with the Atom CPLD,
   // for example by selecting the BBC_Micro profile, as this results in
   // an unusable OSD which persists even after cycling power.
   //
   // Atom timing looks like Mode 7, but as we don't have 6bpp mode 7
   // line capture code, we end up using the default line capture code,
   // which immediately overwrites the OSD with capture data. But because the
   // mode7 flag is set, the OSD is not then repainted in the blanking interval.
   // The end result is the OSD is briefly appears when a button is pressed,
   // then vanishes, making it very tricky to navigate.
   //
   // It might be better to combine this with the cpld->old_firmware() and
   // rename this to cpld->get_capabilities().
   int cpld_ver = (cpld->get_version() >> VERSION_DESIGN_BIT) & 0x0F;
   if (value == AUTOSWITCH_MODE7 && (cpld_ver == DESIGN_ATOM || cpld_ver == DESIGN_YUV)) {
      autoswitch ^= AUTOSWITCH_PC;
   } else {
      autoswitch = value;
   }

   hsync_threshold = ((autoswitch == AUTOSWITCH_MODE7 && hsync_width < hsync_threshold_switch) ? BBC_HSYNC_THRESHOLD : OTHER_HSYNC_THRESHOLD) * cpuspeed / 1000;
}

int get_autoswitch() {
   return autoswitch;
}

void action_calibrate_clocks() {
   // re-measure vsync and set the core/sampling clocks
   calibrate_sampling_clock(0);
   // set the hdmi clock property to match exactly
   set_vlockmode(HDMI_EXACT);
}

void action_calibrate_auto() {
   // re-measure vsync and set the core/sampling clocks
   calibrate_sampling_clock(0);
   // During calibration we do our best to auto-delect an Electron
   log_debug("Elk mode = %d", elk_mode);
   for (int c = 0; c < NUM_CAL_PASSES; c++) {
      cpld->calibrate(capinfo, elk_mode);
   }
}

int is_genlocked() {
   return genlocked;
}

void calculate_fb_adjustment() {
   int double_height = capinfo->sizex2 & 1;
   capinfo->v_adjust  = (capinfo->height >> double_height)  - capinfo->nlines;
   if (capinfo->v_adjust < 0) {
       capinfo->v_adjust = 0;
   }
   capinfo->v_adjust >>= (double_height ^ 1);

   capinfo->h_adjust = (capinfo->width >> 3) - capinfo->chars_per_line;
   if (capinfo->h_adjust < 0) {
       capinfo->h_adjust = 0;
   }

   capinfo->h_adjust = (capinfo->h_adjust >> 1);
   switch(capinfo->bpp) {
       case 4:
           capinfo->h_adjust <<= 2;
           break;
       case 8:
           capinfo->h_adjust <<= 3;
           break;
       case 16:
           capinfo->h_adjust <<= 4;
           break;
   }

//log_info("adjust=%d, %d", capinfo->h_adjust, capinfo->v_adjust);
}

void setup_profile(int profile_changed) {
    // Switch the the approriate capinfo structure instance
    capinfo = mode7 ? &mode7_capinfo : &default_capinfo;

    log_debug("Setting mode7 = %d", mode7);

    geometry_set_mode(mode7);
    capinfo->palette_control = paletteControl;
    if ((capinfo->palette_control == PALETTECONTROL_NTSCARTIFACT_CGA && ntsccolour == 0)
     || (capinfo->palette_control == PALETTECONTROL_NTSCARTIFACT_BW && ntsccolour == 0)) {
        capinfo->palette_control = PALETTECONTROL_OFF;
    }
    log_debug("Loading sample points");
    cpld->set_mode(mode7);
    log_debug("Done loading sample points");

    cpld->update_capture_info(capinfo);
    geometry_get_fb_params(capinfo);

    if (autoswitch == AUTOSWITCH_MODE7) {
        capinfo->detected_sync_type = cpld->analyse(capinfo->sync_type, 0);   // skips sync test if BBC and assumes non-inverted composite (saves time during mode changes)
    } else {
        capinfo->detected_sync_type = cpld->analyse(capinfo->sync_type, 1);
    }
    log_info("Detected polarity state = %X, %s (%s)", capinfo->detected_sync_type, sync_names[capinfo->detected_sync_type & SYNC_BIT_MASK], mixed_names[(capinfo->detected_sync_type & SYNC_BIT_MIXED_SYNC) ? 1 : 0]);

    rgb_to_fb(capinfo, extra_flags() | BIT_PROBE); // dummy mode7 probe to setup sync type from capinfo
    // Measure the frame time and set the sampling clock
    calibrate_sampling_clock(profile_changed);
    // force recalculation of the HDMI clock (if the vlockmode property requires this)
    recalculate_hdmi_clock_line_locked_update(GENLOCK_FORCE);
    if (powerup) {
        osd_set(0, 0, " ");    //dummy print to turn osd on so interlaced modes can display startup resolution later
    }
    geometry_get_fb_params(capinfo);

    if (autoswitch == AUTOSWITCH_PC && sub_profiles_available(profile)) {                                                   // set window around expected time from sub-profile
        double line_time = (double) clkinfo.line_len * 1000000000 / (double) clkinfo.clock;
        int window = (int) ((double) clkinfo.clock_ppm * line_time / 1000000);
        hsync_comparison_lo = (line_time - window) * cpuspeed / 1000;
        hsync_comparison_hi = (line_time + window) * cpuspeed / 1000;
        vsync_comparison_lo = (hsync_comparison_lo * clkinfo.lines_per_frame);
        vsync_comparison_hi = (hsync_comparison_hi * clkinfo.lines_per_frame);
    } else {                                                                             // set window around measured time
        int window = (int) ((double) clkinfo.clock_ppm * (double) one_line_time_ns / 1000000);
        int vwindow = (int) ((double) clkinfo.clock_ppm * (double) one_vsync_time_ns / 1000000);
        hsync_comparison_lo = (one_line_time_ns - window) * cpuspeed / 1000;
        hsync_comparison_hi = (one_line_time_ns + window) * cpuspeed / 1000;
        vsync_comparison_lo = (int)((double)(one_vsync_time_ns - vwindow) * cpuspeed / 1000);
        if (vsync_comparison_lo < frame_minimum) {
            vsync_comparison_lo = frame_minimum;
        }
        vsync_comparison_hi = (int)((double)(one_vsync_time_ns + vwindow) * cpuspeed / 1000);
        if (vsync_comparison_hi > frame_timeout) {
            vsync_comparison_hi = frame_timeout;
        }
    }
    log_info("Window: H=%d to %d, V=%d to %d, S=%s, HW=%d, HT=%d", hsync_comparison_lo * 1000 / cpuspeed, hsync_comparison_hi * 1000 / cpuspeed, (int)((double)vsync_comparison_lo * 1000 / cpuspeed)
             , (int)((double)vsync_comparison_hi * 1000 / cpuspeed), sync_names[capinfo->sync_type], hsync_width, hsync_threshold);
}

void set_status_message(char *msg) {
    strcpy(status, msg);
}

void set_helper_flag() {
    helper_flag = 2;
}

void rgb_to_hdmi_main() {
   int result = RET_SYNC_TIMING_CHANGED;   // make sure autoswitch works first time
   int last_mode7;
   int mode_changed;
   int fb_size_changed;
   int active_size_changed;
   int clk_changed = 0;
   int ncapture;
   int last_profile = -1;
   int last_subprofile = -1;
   int last_saved_config_number = -1;
   int last_divider = -1;
   int last_sync_edge = -1;
   int last_gscaling = -1;
   int refresh_osd = 0;
   char osdline[80];
   capture_info_t last_capinfo;
   clk_info_t last_clkinfo;


   // Setup defaults (these may be overridden by the CPLD)
   default_capinfo.capture_line = capture_line_normal_3bpp_table;
   mode7_capinfo.capture_line   = capture_line_normal_3bpp_table;
   capinfo = &default_capinfo;
   capinfo->v_adjust = 0;
   capinfo->h_adjust = 0;
   capinfo->border = 0;
   capinfo->sync_type = SYNC_BIT_COMPOSITE_SYNC;
   cpld->set_mode(0);
   current_display_buffer = 0;
   // Determine initial sync polarity (and correct whether inversion required or not)
   capinfo->detected_sync_type = cpld->analyse(capinfo->sync_type, 1);
   log_info("Detected polarity state at startup = %x, %s (%s)", capinfo->detected_sync_type, sync_names[capinfo->detected_sync_type & SYNC_BIT_MASK], mixed_names[(capinfo->detected_sync_type & SYNC_BIT_MIXED_SYNC) ? 1 : 0]);
   // Determine initial mode
   mode7 = rgb_to_fb(capinfo, extra_flags() | BIT_PROBE) & BIT_MODE7 & (autoswitch == AUTOSWITCH_MODE7);
   if (cpld_fail_state != CPLD_NORMAL) {
       mode7 = 0;
   }
   // Default to capturing indefinitely
   ncapture = -1;
   int keycount = key_press_reset();
   log_info("Keycount = %d", keycount);


   if (keycount == 1 || force_genlock_range == GENLOCK_RANGE_SET_DEFAULT) {
        sw1_power_up = 1;
        force_genlock_range = GENLOCK_RANGE_INHIBIT;
        if (simple_detected) {
            if ((strcmp(resolution_name, DEFAULT_RESOLUTION) != 0) || refresh != AUTO_REFRESH || hdmi_mode != DEFAULT_HDMI_MODE ) {
                log_info("Resetting output resolution/refresh to Auto/50Hz-60Hz");
                file_save_config(DEFAULT_RESOLUTION, AUTO_REFRESH, DEFAULT_SCALING, DEFAULT_FILTERING, frontend, DEFAULT_HDMI_MODE);
                // Wait a while to allow UART time to empty
                delay_in_arm_cycles_cpu_adjust(100000000);
                reboot();
            }
        } else {
            if ((strcmp(resolution_name, DEFAULT_RESOLUTION) != 0) || refresh != DEFAULT_REFRESH || hdmi_mode != DEFAULT_HDMI_MODE ) {
                log_info("Resetting output resolution/refresh to Auto/EDID");
                file_save_config(DEFAULT_RESOLUTION, DEFAULT_REFRESH, DEFAULT_SCALING, DEFAULT_FILTERING, frontend, DEFAULT_HDMI_MODE);
                // Wait a while to allow UART time to empty
                delay_in_arm_cycles_cpu_adjust(100000000);
                reboot();
            }
        }
   }
   set_scaling(get_scaling(), 2);
   resolution_warning = 0;
   clear = BIT_CLEAR;
   while (1) {
      log_info("-----------------------LOOP------------------------");
      if (profile != last_profile || last_saved_config_number != saved_config_number) {
          last_subprofile  = -1;
      }
      setup_profile(profile != last_profile || last_subprofile != subprofile || last_saved_config_number != saved_config_number);
      if ((autoswitch == AUTOSWITCH_PC) && sub_profiles_available(profile) && ((result & RET_SYNC_TIMING_CHANGED) || profile != last_profile || last_subprofile != subprofile)) {
         int new_sub_profile = autoswitch_detect(one_line_time_ns, lines_per_vsync, capinfo->detected_sync_type & SYNC_BIT_MASK);
         if (new_sub_profile >= 0) {
             if (new_sub_profile != last_subprofile || profile != last_profile || saved_config_number != last_saved_config_number) {
                 set_subprofile(new_sub_profile);
                 process_sub_profile(get_profile(), new_sub_profile);
                 setup_profile(1);
                 set_status_message("");
             }
         } else {
             set_status_message("Auto Switch: No profile matched");
             log_info("Autoswitch: No profile matched");
         }
      }
      last_divider = cpld->get_divider();
      last_sync_edge = cpld->get_sync_edge();
      last_profile = profile;
      last_subprofile = subprofile;
      last_saved_config_number = saved_config_number;
      last_gscaling = gscaling;
      log_debug("Setting up frame buffer");
      init_framebuffer(capinfo);
      log_debug("Done setting up frame buffer");
      //log_info("Peripheral base = %08X", PERIPHERAL_BASE);
      log_info("RAM benchmark: Main memory = %d ns, Screen memory = %d ns", (int) ((double) benchmarkRAM(dummyscreen) * 1000 / cpuspeed), (int) ((double) benchmarkRAM((int) capinfo->fb) * 1000 / cpuspeed));

      geometry_get_fb_params(capinfo);
      capinfo->ncapture = ncapture;
      calculate_fb_adjustment();

      log_info("Detected screen size = %dx%d",get_hdisplay() + config_overscan_left + config_overscan_right, get_vdisplay() + config_overscan_top + config_overscan_bottom);
      log_info("Pitch=%d, width=%d, height=%d, sizex2=%d, bpp=%d", capinfo->pitch, capinfo->width, capinfo->height, capinfo->sizex2, capinfo->bpp);
      log_info("chars=%d, nlines=%d, hoffset=%d, voffset=%d, ncapture=%d", capinfo->chars_per_line, capinfo->nlines, capinfo->h_offset, capinfo-> v_offset, capinfo->ncapture);
      log_info("palctrl=%d, samplewidth=%d, hadjust=%d, vadjust=%d, sync=0x%x", capinfo->palette_control, capinfo->sample_width, capinfo->h_adjust, capinfo->v_adjust, capinfo->sync_type);
      log_info("detsync=0x%x, vsync=%d, video=%d, ntsc=%d, border=%d, delay=%d", capinfo-> detected_sync_type, capinfo->vsync_type, capinfo->video_type, capinfo->ntscphase, capinfo->border, capinfo->delay);

      refresh_osd = 1;

      if (capinfo->border !=0) {
         clear = BIT_CLEAR;
      }

      do {

         geometry_get_fb_params(capinfo);
         capinfo->ncapture = ncapture;
         calculate_fb_adjustment();

         if (refresh_osd) {
            refresh_osd = 0;
            osd_refresh();
         }

         if (powerup) {
           ntsc_status = ntsccolour << 3;
           if (check_file(FORCE_BLANK_FILE, FORCE_BLANK_FILE_MESSAGE)) {
               rgb_to_fb(capinfo, extra_flags() | BIT_PROBE); // dummy mode7 probe to setup parms from capinfo
               osd_set(0, ATTR_DOUBLE_SIZE, "Erasing CPLD");
               update_cpld(BLANK_FILE);
           }

           if (cpld_fail_state == CPLD_MANUAL) {
                rgb_to_fb(capinfo, extra_flags() | BIT_PROBE); // dummy mode7 probe to setup parms from capinfo
                osd_set(0, 0, "Release buttons for CPLD recovery menu");
                do {} while (key_press_reset() != 0);
           }
           // If the CPLD is unprogrammed, operate in a degraded mode that allows the menus to work
           if (cpld_fail_state != CPLD_NORMAL) {
             rgb_to_fb(capinfo, extra_flags() | BIT_PROBE); // dummy mode7 probe to setup parms from capinfo
             status[0] = 0;
             // Immediately load the CPLD Update Menu (renamed to CPLD Recovery Menu)
             osd_show_cpld_recovery_menu(cpld_fail_state == CPLD_UPDATE);
             while (1) {
                if (status[0] != 0) {
                    osd_set(1, 0, status);
                    status[0] = 0;
                } else {
                    switch (cpld_fail_state) {
                        case CPLD_BLANK:
                            osd_set_clear(1, 0, "CPLD is unprogrammed: Select correct CPLD");
                        break;
                        case CPLD_UNKNOWN:
                            osd_set_clear(1, 0, "CPLD is unknown: Select correct CPLD");
                        break;
                        case CPLD_WRONG:
                            osd_set_clear(1, 0, "Wrong CPLD (6bit CPLD on 3bit board)");
                        break;
                        case CPLD_MANUAL:
                            osd_set_clear(1, 0, "Manual CPLD recovery: Select correct CPLD");
                        break;
                        case CPLD_UPDATE:
                            osd_set_clear(1, 0, "Please update CPLD to latest version");
                        break;
                    }
                }
                int flags = 0;
                capinfo->ncapture = ncapture;
                log_info("Entering poll_keys_only, flags=%08x", flags);
                result = poll_keys_only(capinfo, flags);
                log_info("Leaving poll_keys_only, result=%04x", result);
                osd_set_clear(1, 0, " ");
                if (result & RET_EXPIRED) {
                   ncapture = osd_key(OSD_EXPIRED);
                } else if (result & RET_SW1) {
                   ncapture = osd_key(OSD_SW1);
                } else if (result & RET_SW2) {
                   ncapture = osd_key(OSD_SW2);
                } else if (result & RET_SW3) {
                   ncapture = osd_key(OSD_SW3);
                }
             }
           }
           capinfo->ncapture = 25;
           recalculate_hdmi_clock(HDMI_ORIGINAL, 0);
         }

         // Update capture info, in case sample width has changed
         // (this also re-selects the appropriate line capture)
         cpld->update_capture_info(capinfo);

         int flags =  extra_flags() | mode7 | clear;
         if (autoswitch == AUTOSWITCH_MODE7) {
            flags |= BIT_MODE_DETECT;
         }

         if (vsync) {
            flags |= BIT_VSYNC;
         }
         if (debug) {
            flags |= BIT_DEBUG;
         }

         //paletteFlags |= BIT_MULTI_PALETTE;   // test multi palette
         if (capinfo->video_type == VIDEO_TELETEXT) {
            flags |= m7deinterlace << OFFSET_INTERLACE;
         } else {
            flags |= deinterlace << OFFSET_INTERLACE;
         }
#ifdef MULTI_BUFFER
         if ((capinfo->video_type == VIDEO_PROGRESSIVE || (capinfo->video_type == VIDEO_INTERLACED && !interlaced)) && osd_active() && (nbuffers == 0)) {
            flags |= 2 << OFFSET_NBUFFERS;
         } else {
            flags |= nbuffers << OFFSET_NBUFFERS;
         }
         //log_info("Buffers = %d", (flags & MASK_NBUFFERS) >> OFFSET_NBUFFERS);
#endif

         if (!osd_active() && reboot_required) {
             // Wait a while to allow UART time to empty
             delay_in_arm_cycles_cpu_adjust(100000000);
             if (resolution_warning != 0) {
                 osd_set_clear(0, 0, "If there is no display with new setting:");
                       osd_set(1, 0, "Hold menu button during reset until you");
                       osd_set(2, 0, "see the 50Hz disabled recovery message");


                 for (int i = 5; i > 0; i--) {
                     sprintf(osdline, "Rebooting in %d secs ", i);
                     log_info(osdline);
                     osd_set_clear(4, 0, osdline);
                     delay_in_arm_cycles_cpu_adjust(1000000000);
                  }
             }
             reboot();
         }

         if (osd_active()) {
             if (helper_flag != 0) {
                sprintf(osdline, "%d:%d %dHz %dPPM %d %s %dHz", get_haspect(), get_vaspect(), adjusted_clock, clock_error_ppm, lines_per_vsync, sync_names[capinfo->detected_sync_type & SYNC_BIT_MASK], source_vsync_freq_hz);
                osd_set(1, 0, osdline);
                helper_flag--;
             } else {
                 if (status[0] != 0) {
                     osd_set(1, 0, status);
                     status[0] = 0;
                 } else {
                     if (!reboot_required) {
                         if (sync_detected) {
                             if (vlock_limited && (vlockmode != HDMI_ORIGINAL)) {
                                 if (force_genlock_range == GENLOCK_RANGE_INHIBIT) {
                                    sprintf(osdline, "Recovery mode: 50Hz disabled until reset");
                                 } else {
                                    sprintf(osdline, "Genlock inhibited: Src=%dHz, Disp=%dHz", source_vsync_freq_hz, display_vsync_freq_hz);
                                 }
                                 osd_set(1, 0, osdline);
                             } else {
                                 osd_set(1, 0, "");
                             }
                         } else {
                             osd_set(1, 0, "No sync detected");
                         }
                    } else {
                         osd_set(1, 0, "New setting requires reboot on menu exit");
                    }
                 }
             }
         }
         capinfo->intensity = scanlines_intensity;
         log_debug("Entering rgb_to_fb, flags=%08x", flags);
         result = rgb_to_fb(capinfo, flags);
         log_debug("Leaving rgb_to_fb, result=%04x", result);

         if (result & RET_SYNC_TIMING_CHANGED) {
             log_info("Timing exceeds window: H=%d, V=%d, Lines=%d, VSync=%d", hsync_period * 1000 / cpuspeed, (int)((double)vsync_period * 1000 / cpuspeed), (int) (((double)vsync_period/hsync_period) + 0.5), (result & RET_VSYNC_POLARITY_CHANGED) ? 1 : 0);
         }

         clear = 0;

         // Possibly the size or offset has been adjusted, so update current capinfo
         memcpy(&last_capinfo, capinfo, sizeof last_capinfo);
         memcpy(&last_clkinfo, &clkinfo, sizeof last_clkinfo);

         if (result & RET_EXPIRED) {
            ncapture = osd_key(OSD_EXPIRED);
         } else if (result & RET_SW1) {
            ncapture = osd_key(OSD_SW1);
         } else if (result & RET_SW2) {
            ncapture = osd_key(OSD_SW2);
         } else if (result & RET_SW3) {
            ncapture = osd_key(OSD_SW3);
         }

         if (powerup) {
           powerup = 0;
           if (resolution_status) {
               int h_size = get_hdisplay() + config_overscan_left + config_overscan_right;
               int v_size = get_vdisplay() + config_overscan_top + config_overscan_bottom;
               if (sync_detected) {
                   if (vlock_limited || vlockmode != HDMI_EXACT) {
                       sprintf(osdline, "%d x %d @ %dHz", h_size, v_size, display_vsync_freq_hz);
                   } else {
                       sprintf(osdline, "%d x %d @ %dHz", h_size, v_size, source_vsync_freq_hz);
                   }
               } else {
                   sprintf(osdline, "%d x %d", h_size, v_size);
               }
               osd_set(0, ATTR_DOUBLE_SIZE, osdline);
               osd_display_interface(2);
               ncapture = 150;
           } else {
               ncapture = 1;
           }
         }

         cpld->update_capture_info(capinfo);
         geometry_get_fb_params(capinfo);
         capinfo->palette_control = paletteControl;
         if ((capinfo->palette_control == PALETTECONTROL_NTSCARTIFACT_CGA && ntsccolour == 0)
          || (capinfo->palette_control == PALETTECONTROL_NTSCARTIFACT_BW && ntsccolour == 0)) {
            capinfo->palette_control = PALETTECONTROL_OFF;
         }

         fb_size_changed = (capinfo->width != last_capinfo.width) || (capinfo->height != last_capinfo.height) || (capinfo->bpp != last_capinfo.bpp) || (capinfo->sample_width != last_capinfo.sample_width || last_gscaling != gscaling);

         if (result & RET_INTERLACE_CHANGED)  {
             log_info("Interlace changed, HT = %d", hsync_threshold);
             if(capinfo->video_type == VIDEO_INTERLACED) {
                fb_size_changed |= 1;
             }
         }

         active_size_changed = (capinfo->chars_per_line != last_capinfo.chars_per_line) || (capinfo->nlines != last_capinfo.nlines);

         geometry_get_clk_params(&clkinfo);
         clk_changed = (clkinfo.clock != last_clkinfo.clock) || (clkinfo.line_len != last_clkinfo.line_len || (clkinfo.clock_ppm != last_clkinfo.clock_ppm));

         last_mode7 = mode7;

         mode7 = result & BIT_MODE7 & (autoswitch == AUTOSWITCH_MODE7);

         mode_changed = mode7 != last_mode7 || capinfo->vsync_type != last_capinfo.vsync_type || capinfo->sync_type != last_capinfo.sync_type || capinfo->border != last_capinfo.border
                                            || capinfo->video_type != last_capinfo.video_type || capinfo->px_sampling != last_capinfo.px_sampling || cpld->get_sync_edge() != last_sync_edge
                                            || profile != last_profile || saved_config_number != last_saved_config_number || last_subprofile != subprofile || cpld->get_divider() != last_divider || (result & RET_SYNC_TIMING_CHANGED);

         if (active_size_changed || fb_size_changed) {
            clear = BIT_CLEAR;
         }

         if ((clk_changed || (result & RET_INTERLACE_CHANGED)) && !fb_size_changed && !mode_changed) {
            target_difference = 0;
            resync_count = 0;
            // Measure the frame time and set the sampling clock
            calibrate_sampling_clock(0);
            // Force recalculation of the HDMI clock (if the vlockmode property requires this)
            recalculate_hdmi_clock_line_locked_update(GENLOCK_FORCE);
         }

      } while (!mode_changed && !fb_size_changed && !restart_profile);
      log_info("Mode changed = %d, fb_size_changed = %d, restart_profile = %d, HT = %d", mode_changed, fb_size_changed, restart_profile, hsync_threshold);
      restart_profile = 0;
      osd_clear();
      clear_full_screen();
   }
}

void force_reinit() {
    restart_profile = 1;
    set_status_message("Configuration restored");
}

int show_detected_status(int line) {
    char message[80];
    int adjusted_width = capinfo->width;
    int pitch = capinfo->pitch;
    switch(capinfo->bpp) {
         case 4:
            pitch <<= 1;
            break;
         case 8:
            break;
         case 16:
            pitch >>= 1;
            break;
    }
    sprintf(message, "    Pixel clock: %d Hz", adjusted_clock);
    osd_set(line++, 0, message);
    sprintf(message, "     CPLD clock: %d Hz (x%d)", adjusted_clock * cpld->get_divider(), cpld->get_divider());
    osd_set(line++, 0, message);
    sprintf(message, "    Clock error: %d PPM", clock_error_ppm);
    osd_set(line++, 0, message);
    sprintf(message, "  Line duration: %d ns", one_line_time_ns);
    osd_set(line++, 0, message);
    if (interlaced) {
        sprintf(message, "Lines per frame: %d (Interlaced %d)",  lines_per_vsync, lines_per_2_vsyncs);
    } else {
        sprintf(message, "Lines per frame: %d", lines_per_vsync);
    }
    osd_set(line++, 0, message);
    sprintf(message, "     Frame rate: %d Hz (%.2f Hz)", source_vsync_freq_hz, source_vsync_freq);
    osd_set(line++, 0, message);
    sprintf(message, "      Sync type: %s", sync_names_long[capinfo->detected_sync_type & SYNC_BIT_MASK]);
    osd_set(line++, 0, message);
    sprintf(message, "   Pixel Aspect: %d:%d", get_haspect(), get_vaspect());
    osd_set(line++, 0, message);
    int double_width = (capinfo->sizex2 & 2) >> 1;
    int double_height = capinfo->sizex2 & 1;
    sprintf(message, "   Capture Size: %d x %d (%d x %d)", capinfo->chars_per_line << (3 - double_width), capinfo->nlines, capinfo->chars_per_line << 3, capinfo->nlines << double_height );
    osd_set(line++, 0, message);
    sprintf(message, "    H & V range: %d-%d x %d-%d", capinfo->h_offset, capinfo->h_offset + (capinfo->chars_per_line << (3 - double_width)) - 1, capinfo->v_offset, capinfo->v_offset + capinfo->nlines - 1);
    osd_set(line++, 0, message);

    sprintf(message, "   Frame Buffer: %d x %d (%d x %d)", adjusted_width, capinfo->height, pitch, capinfo->height);
    osd_set(line++, 0, message);
    sprintf(message, "   FB Bit Depth: %d", capinfo->bpp);
    osd_set(line++, 0, message);
    int h_size = get_hdisplay() + config_overscan_left + config_overscan_right;
    int v_size = get_vdisplay() + config_overscan_top + config_overscan_bottom;
    sprintf(message, "  Pi Resolution: %d x %d (%d x %d)", h_size, v_size, get_hdisplay() - h_overscan, get_vdisplay() - v_overscan);
    osd_set(line++, 0, message);
    if (!sync_detected || vlock_limited || vlockmode != HDMI_EXACT) {
        sprintf(message, "  Pi Frame rate: %d Hz (%.2f Hz)", display_vsync_freq_hz, display_vsync_freq);
    } else {
        sprintf(message, "  Pi Frame rate: %d Hz (%.2f Hz)", source_vsync_freq_hz, source_vsync_freq);
    }
    osd_set(line++, 0, message);
    sprintf(message, "    Pi Overscan: %d x %d", h_overscan + config_overscan_left + config_overscan_right, v_overscan + config_overscan_top + config_overscan_bottom);
    osd_set(line++, 0, message);
    sprintf(message, "        Scaling: %.2f x %.2f", ((double)(get_hdisplay() - h_overscan)) / capinfo->width, ((double)(get_vdisplay() - v_overscan)) / capinfo->height);
    osd_set(line++, 0, message);

    return (line);
}

void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
{
   RPI_AuxMiniUartInit(115200, 8);

   enable_MMU_and_IDCaches();
   _enable_unaligned_access();

   log_info("***********************RESET***********************");
   log_info("RGB to HDMI booted");

   init_hardware();

#ifdef HAS_MULTICORE
   int i;
   printf("main running on core %u\r\n", _get_core());
   for (i = 0; i < 10000000; i++);
#ifdef USE_MULTICORE
   start_core(1, _init_core);
#else
   start_core(1, _spin_core);
#endif
   for (i = 0; i < 10000000; i++);
   start_core(2, _spin_core);
   for (i = 0; i < 10000000; i++);
   start_core(3, _spin_core);
   for (i = 0; i < 10000000; i++);
#endif

   rgb_to_hdmi_main();
}
