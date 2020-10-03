#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include "defs.h"
#include "cpld.h"
#include "geometry.h"
#include "osd.h"
#include "logging.h"
#include "rgb_to_hdmi.h"
#include "rgb_to_fb.h"
#include "rpi-gpio.h"

#define RANGE_SUBC_0  8
#define RANGE_SUBC_1 16

#define RANGE_MAX    16

// The number of frames to compute differences over
#define NUM_CAL_FRAMES 10

#define DAC_UPDATE 1
#define NO_DAC_UPDATE 0

typedef struct {
   int cpld_setup_mode;
   int all_offsets;
   int sp_offset[NUM_OFFSETS];
       int half_px_delay; // 0 = off, 1 = on, all modes
       int divider;       // cpld divider, 6 or 8
   int delay;
   int filter_l;
   int sub_c;
   int alt_r;
   int edge;
   int clamptype;
   int mux;
   int rate;
   int terminate;
   int coupling;
   int dac_a;
   int dac_b;
   int dac_c;
   int dac_d;
   int dac_e;
   int dac_f;
   int dac_g;
   int dac_h;
} config_t;

enum {
  YUV_RATE_UNUSED,
  YUV_RATE_6,
  YUV_RATE_6_LEVEL_4,
};

// Current calibration state for mode 0..6
static config_t default_config;

// Current configuration
static config_t *config = &default_config;

// OSD message buffer
static char message[80];

// Aggregate calibration metrics (i.e. errors summed across all channels)
static int sum_metrics[RANGE_MAX];

// Error count for final calibration values for mode 0..6
static int errors;

// The CPLD version number
static int cpld_version;

static int frontend = 0;

static int supports_invert         = 1;
static int supports_separate       = 1;
static int supports_vsync          = 1;
static int supports_sub_c          = 1; /* Supports chroma subsampling */
static int supports_alt_r          = 1; /* Supports R channel inversion on alternate lines */
static int supports_edge           = 1; /* Selection of leading rather than trailing edge */
static int supports_clamptype      = 1; /* Selection of back porch or sync tip clamping */
static int supports_delay          = 1; /* A 0-3 pixel delay */
static int supports_extended_delay = 1; /* A 0-15 pixel delay */
static int supports_four_level     = 1;
static int supports_mux            = 1; /* mux moved from pin to register bit */

// invert state (not part of config)
static int invert = 0;

// =============================================================
// Param definitions for OSD
// =============================================================

enum {
   CPLD_SETUP_MODE,
   ALL_OFFSETS,
       A_OFFSET,
       B_OFFSET,
       C_OFFSET,
       D_OFFSET,
       E_OFFSET,
       F_OFFSET,
       HALF,
       DIVIDER,
   DELAY,
   FILTER_L,
   SUB_C,
   ALT_R,
   EDGE,
   CLAMPTYPE,
   MUX,
   RATE,
   TERMINATE,
   COUPLING,
   DAC_A,
   DAC_B,
   DAC_C,
   DAC_D,
   DAC_E,
   DAC_F,
   DAC_G,
   DAC_H
};

static const char *edge_names[] = {
   "Trailing",
   "Leading"
};

static const char *cpld_setup_names[] = {
   "Normal",
   "Set Pixel H Offset"
};

static const char *clamptype_names[] = {
   "Sync Tip",
   "Back Porch Short",
   "Back Porch Medium",
   "Back Porch Long",
   "Back Porch Auto"
};

static const char *level_names[] = {
   "3 Level YUV",
   "3 Lvl Y, 4 Lvl UV",
   "4 Lvl Y, 3 Lvl UV",
   "4 Level YUV",
   "Auto 4 Level YUV"
};

static const char *rate_names[] = {
   "Unused",
   "6 Bits Per Pixel",
   "6 Bits (4 Level)",
};

static const char *coupling_names[] = {
   "DC",
   "AC With Clamp"
};

enum {
   YUV_INPUT_HI,
   YUV_INPUT_TERM,
   NUM_YUV_TERM
};

enum {
   YUV_INPUT_DC,
   YUV_INPUT_AC,
   NUM_YUV_COUPLING
};

enum {
   CPLD_SETUP_NORMAL,
   CPLD_SETUP_DELAY,
   NUM_CPLD_SETUP
};

enum {
   CLAMPTYPE_SYNC,
   CLAMPTYPE_SHORT,
   CLAMPTYPE_MEDIUM,
   CLAMPTYPE_LONG,
   CLAMPTYPE_AUTO,
   NUM_CLAMPTYPE
};

static param_t params[] = {
   {  CPLD_SETUP_MODE,  "Setup Mode", "setup_mode", 0, NUM_CPLD_SETUP-1, 1 },
   { ALL_OFFSETS,      "Sample Phase",          "offset", 0,  15, 1 },
//block of hidden RGB options for file compatibility
   {    A_OFFSET,    "A Phase",    "a_offset", 0,   0, 1 },
   {    B_OFFSET,    "B Phase",    "b_offset", 0,   0, 1 },
   {    C_OFFSET,    "C Phase",    "c_offset", 0,   0, 1 },
   {    D_OFFSET,    "D Phase",    "d_offset", 0,   0, 1 },
   {    E_OFFSET,    "E Phase",    "e_offset", 0,   0, 1 },
   {    F_OFFSET,    "F Phase",    "f_offset", 0,   0, 1 },
   {        HALF,        "Half Pixel Shift",        "half", 0,   1, 1 },
   {     DIVIDER,     "Clock Multiplier",      "divider", 6,   8, 2 },
//end of hidden block
   {       DELAY,  "Pixel H Offset",            "delay", 0,  15, 1 },
   {    FILTER_L,  "Filter Y",      "l_filter", 0,   1, 1 },
   {       SUB_C,  "Subsample UV",     "sub_c", 0,   1, 1 },
   {       ALT_R,  "PAL switch",       "alt_r", 0,   1, 1 },
   {        EDGE,  "Sync Edge",         "edge", 0,   1, 1 },
   {   CLAMPTYPE,  "Clamp Type",   "clamptype", 0,     NUM_CLAMPTYPE-1, 1 },
   {         MUX,  "Sync on Y/V",     "input_mux", 0,   1, 1 },
   {        RATE,  "Sample Mode", "sample_mode", 1,   2, 1 },
   {   TERMINATE,  "75R Termination", "termination", 0,   NUM_YUV_TERM-1, 1 },
   {    COUPLING,  "Y Coupling", "coupling", 0,   NUM_YUV_COUPLING-1, 1 },
   {       DAC_A,  "DAC-A: Y Hi",      "dac_a", 0, 255, 1 },
   {       DAC_B,  "DAC-B: Y Lo",      "dac_b", 0, 255, 1 },
   {       DAC_C,  "DAC-C: UV Hi",     "dac_c", 0, 255, 1 },
   {       DAC_D,  "DAC-D: UV Lo",     "dac_d", 0, 255, 1 },
   {       DAC_E,  "DAC-E: Sync",      "dac_e", 0, 255, 1 },
   {       DAC_F,  "DAC-F: Y/V Sync",   "dac_f", 0, 255, 1 },
   {       DAC_G,  "DAC-G: Y Clamp",   "dac_g", 0, 255, 1 },
   {       DAC_H,  "DAC-H: Unused",     "dac_h", 0, 255, 1 },
   {          -1,  NULL,                  NULL, 0,   0, 0 }
};

// =============================================================
// Private methods
// =============================================================

static int getRange() {
   if (supports_sub_c && !config->sub_c) {
      return RANGE_SUBC_0;
   } else {
      return RANGE_SUBC_1;
   }
}

static void sendDAC(int dac, int value)
{

    int M62364_dac = 0;

    switch (dac) {
        case 0:
            M62364_dac = 0x08;
        break;
        case 1:
            M62364_dac = 0x04;
        break;
        case 2:
            M62364_dac = 0x0c;
        break;
        case 3:
            M62364_dac = 0x02;
        break;
        case 4:
            M62364_dac = 0x0a;
        break;
        case 5:
            M62364_dac = 0x06;
        break;
        case 6:
            M62364_dac = 0x0e;
            if (frontend == FRONTEND_YUV_ISSUE2_5259) {
                dac = 7;
            }
        break;
        case 7:
            M62364_dac = 0x01;
            if (frontend == FRONTEND_YUV_ISSUE2_5259) {
                dac = 6;
                switch (config->terminate) {
                      default:
                      case YUV_INPUT_HI:
                        value = 0;  //high impedance
                      break;
                      case YUV_INPUT_TERM:
                        value = 255; //termination
                      break;
                }
            }
        break;
        default:
        break;
    }

    if (new_M62364_DAC_detected()) {
        int packet = (M62364_dac << 8) | value;
        //log_info("M62364 dac:%d = %02X, %03X", dac, value, packet);
        RPI_SetGpioValue(STROBE_PIN, 0);
        for (int i = 0; i < 12; i++) {
            RPI_SetGpioValue(SP_CLKEN_PIN, 0);
            RPI_SetGpioValue(SP_DATA_PIN, (packet >> 11) & 1);
            delay_in_arm_cycles_cpu_adjust(500);
            RPI_SetGpioValue(SP_CLKEN_PIN, 1);
            delay_in_arm_cycles_cpu_adjust(500);
            packet <<= 1;
        }
    } else {
        //log_info("Issue2/3 dac:%d = %d", dac, value);
        int packet = (dac << 11) | 0x600 | value;
        RPI_SetGpioValue(STROBE_PIN, 0);
        for (int i = 0; i < 16; i++) {
            RPI_SetGpioValue(SP_CLKEN_PIN, 0);
            RPI_SetGpioValue(SP_DATA_PIN, (packet >> 15) & 1);
            delay_in_arm_cycles_cpu_adjust(500);
            RPI_SetGpioValue(SP_CLKEN_PIN, 1);
            delay_in_arm_cycles_cpu_adjust(500);
            packet <<= 1;
        }
    }
    RPI_SetGpioValue(STROBE_PIN, 1);
    RPI_SetGpioValue(SP_DATA_PIN, 0);
}

static void write_config(config_t *config, int dac_update) {
   int sp = 0;
   int scan_len = 0;
   if (supports_delay) {
      // In the hardware we effectively have a total of 5 bits of actual offset
      // and we need to derive this from the offset and delay values
      if (config->sub_c) {
         // Use 4 bits of offset and 1 bit of delay
         sp |= (config->all_offsets & 15) << scan_len;
         scan_len += 4;
         sp |= ((~config->delay >> 1) & 1) << scan_len;
         scan_len += 1;
      } else {
         // Use 3 bits of offset and 2 bit of delay
         sp |= (config->all_offsets & 7) << scan_len;
         scan_len += 3;
         sp |= (~config->delay & 3) << scan_len;
         scan_len += 2;
      }
   } else {
      sp |= (config->all_offsets & 15) << scan_len;
      scan_len += 4;
   }

   if (supports_extended_delay) {
      sp |= ((~config->delay >> 2) & 3) << scan_len;
      scan_len += 2;
   }

   sp |= ((config->rate - 1) & 1) << scan_len;
   scan_len++;

   sp |= config->filter_l << scan_len;
   scan_len++;

   if (supports_invert) {
      sp |= invert << scan_len;
      scan_len++;
   }
   if (supports_sub_c) {
      sp |= config->sub_c << scan_len;
      scan_len++;
   }
   if (supports_alt_r) {
      sp |= config->alt_r << scan_len;
      scan_len++;
   }
   if (supports_edge) {
      sp |= config->edge << scan_len;
      scan_len++;
   }
   if (supports_clamptype) {
      int clamptype = config->clamptype;
      if (clamptype == CLAMPTYPE_AUTO) {
          if (config->rate == YUV_RATE_6) {
              clamptype = CLAMPTYPE_SHORT;
              if (geometry_get_value(CLOCK) >= 6750000) {
                  clamptype = CLAMPTYPE_MEDIUM;
              }
              if (geometry_get_value(CLOCK) >= 9750000) {
                  clamptype = CLAMPTYPE_LONG;
              }
          } else {
              clamptype = CLAMPTYPE_LONG; // force 4 level YUV in four level mode
          }
      }
      sp |= clamptype << scan_len;
      scan_len += 2;
   }

   if (supports_mux) {
       sp |= config->mux << scan_len;
       scan_len += 1;
   }

   for (int i = 0; i < scan_len; i++) {
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

   if (dac_update) {
       int dac_a = config->dac_a;
       if (dac_a == 255) dac_a = 75; //approx 1v (255 = 3.3v) so a grounded input will be detected as 0 without exceeding 1.4v difference
       int dac_b = config->dac_b;
       if (dac_b == 255) dac_b = dac_a;

       int dac_c = config->dac_c;
       if (dac_c == 255) dac_c = 75; //approx 1v (255 = 3.3v) so a grounded input will be detected as 0 without exceeding 1.4v difference
       int dac_d = config->dac_d;
       if (dac_d == 255) dac_d = dac_c;

       int dac_e = config->dac_e;
       if (dac_e == 255 && config->mux != 0) dac_e = dac_a;    //if sync level is disabled, track dac_a unless sync source is sync (stops spurious sync detection)

       int dac_f = config->dac_f;
       if (dac_f == 255 && config->mux == 0) dac_f = dac_a;   //if vsync level is disabled, track dac_a unless sync source is vsync (stops spurious sync detection)

       int dac_g = config->dac_g;
       if (dac_g == 255) {
          if (supports_four_level && config->rate >= YUV_RATE_6_LEVEL_4) {
             dac_g = dac_c;
          } else {
             dac_g = 0;
          }
       }

       int dac_h = config->dac_h;
       if (dac_h == 255) dac_h = dac_c;

       if (params[DAC_G].hidden == 1) {
           dac_g = dac_c;
       }
       if (params[DAC_H].hidden == 1) {
           dac_h = dac_c;
       }

       sendDAC(0, dac_a);
       sendDAC(1, dac_b);
       sendDAC(2, dac_c);
       sendDAC(3, dac_d);
       sendDAC(5, dac_e);   // DACs E and F positions swapped in menu compared to hardware
       sendDAC(4, dac_f);
       sendDAC(6, dac_g);
       sendDAC(7, dac_h);
   }

   switch (config->terminate) {
      default:
      case YUV_INPUT_HI:
        RPI_SetGpioValue(SP_CLKEN_PIN, 0);  //termination
      break;
      case YUV_INPUT_TERM:
        RPI_SetGpioValue(SP_CLKEN_PIN, 1);
      break;
   }

   int coupling = config->coupling;
   if (frontend == FRONTEND_ANALOG_ISSUE2_5259 || frontend == FRONTEND_ANALOG_ISSUE1_UA1 || frontend == FRONTEND_ANALOG_ISSUE1_UB1) {
       coupling = YUV_INPUT_AC;                  // always ac coupling with issue 1 or 2
   }

   switch (coupling) {
      default:
      case YUV_INPUT_DC:
        RPI_SetGpioValue(SP_DATA_PIN, 0);   //ac-dc
      break;
      case YUV_INPUT_AC:
        RPI_SetGpioValue(SP_DATA_PIN, (config->rate < YUV_RATE_6_LEVEL_4) || !supports_four_level);  // only allow clamping in 3 level mode or old cpld or 6 bit board
      break;
   }

   RPI_SetGpioValue(MUX_PIN, config->mux);
}

static int osd_sp(config_t *config, int line, int metric) {
   // Line ------
   sprintf(message,    "          Phase: %d", config->all_offsets);
   osd_set(line++, 0, message);
   // Line ------
   if (metric < 0) {
      sprintf(message, "         Errors: unknown");
   } else {
      sprintf(message, "         Errors: %d", metric);
   }
   osd_set(line++, 0, message);
   return(line);
}

static void log_sp(config_t *config) {
   log_info("phase = %d", config->all_offsets);
}

// =============================================================
// Public methods
// =============================================================

static void cpld_init(int version) {
   params[A_OFFSET].hidden = 1;
   params[B_OFFSET].hidden = 1;
   params[C_OFFSET].hidden = 1;
   params[D_OFFSET].hidden = 1;
   params[E_OFFSET].hidden = 1;
   params[F_OFFSET].hidden = 1;
   params[HALF].hidden = 1;
   params[DIVIDER].hidden = 1;
   cpld_version = version;
   config->all_offsets = 0;
   config->sp_offset[0] = 0;
   config->sp_offset[1] = 0;
   config->sp_offset[2] = 0;
   config->sp_offset[3] = 0;
   config->sp_offset[4] = 0;
   config->sp_offset[5] = 0;
   config->rate  = YUV_RATE_6;
   config->filter_l  = 1;
   config->divider = 8;
   for (int i = 0; i < RANGE_MAX; i++) {
      sum_metrics[i] = -1;
   }
   errors = -1;

   int major = (cpld_version >> VERSION_MAJOR_BIT) & 0x0F;

   // CPLDv3 adds support for inversion of video
   if (major < 3) {
      supports_invert = 0;
      supports_separate = 0;
      supports_vsync = 0;
   }
   // CPLDv4 adds support for chroma subsampling
   if (major < 4) {
      supports_sub_c = 0;
      params[SUB_C].hidden = 1;
   }
   // CPLDv5 adds support for inversion of R on alternative lines (for Spectrum)
   if (major < 5) {
      supports_alt_r = 0;
      params[ALT_R].hidden = 1;
   }
   // CPLDv6 adds support for sync edge selection and 2-bit pixel delay
   if (major < 6) {
      supports_edge = 0;
      supports_delay = 0;
      params[EDGE].hidden = 1;
      params[DELAY].hidden = 1;
   }
   // CPLDv7 adds support for programmable clamp length
   if (major < 7) {
      supports_clamptype = 0;
      params[CLAMPTYPE].hidden = 1;
   }
   // CPLDv8 addes support for 4-bit pixel delay
   if (major < 8) {
      supports_extended_delay = 0;
   }
   // CPLDv82 adds support for 4 level YUV, replacing the chroma filter
   if ((cpld_version & 0xff) < 0x82) {
       // no support for four levels and Chroma filtering available
       params[RATE].label = "Filter UV";
       params[DAC_H].hidden = 1;
       supports_four_level = 0;
   } else {
       if (!eight_bit_detected()) {
           params[RATE].max = YUV_RATE_6;   // four level YUV won't work on 6 bit boards
           params[DAC_H].hidden = 1;
           config->rate  = YUV_RATE_6;
           supports_four_level = 0;
       }
   }
   if (major < 9) {
      supports_mux = 0;
   }
   geometry_hide_pixel_sampling();
   config->cpld_setup_mode = 0;
}

static int cpld_get_version() {
   return cpld_version;
}

static void cpld_calibrate(capture_info_t *capinfo, int elk) {
   int min_i = 0;
   int metric;         // this is a point value (at one sample offset)
   int min_metric;
   int win_metric;     // this is a windowed value (over three sample offsets)
   int min_win_metric;

   int range = getRange();

   log_info("Calibrating...");

   // Measure the error metrics at all possible offset values
   min_metric = INT_MAX;
   printf("INFO:                      ");
   for (int i = 0; i < NUM_OFFSETS; i++) {
      printf("%7c", 'A' + i);
   }
   printf("   total\r\n");
   for (int value = 0; value < range; value++) {
      config->all_offsets = value;
      config->sp_offset[0] = value;
      config->sp_offset[1] = value;
      config->sp_offset[2] = value;
      config->sp_offset[3] = value;
      config->sp_offset[4] = value;
      config->sp_offset[5] = value;
      write_config(config, DAC_UPDATE);
      metric = diff_N_frames(capinfo, NUM_CAL_FRAMES, 0, elk);
      log_info("INFO: value = %d: metric = ", metric);
      sum_metrics[value] = metric;
      osd_sp(config, 2, metric);
      if (metric < min_metric) {
         min_metric = metric;
      }
   }

   // Use a 3 sample window to find the minimum and maximum
   min_win_metric = INT_MAX;
   for (int i = 0; i < range; i++) {
      int left  = (i - 1 + range) % range;
      int right = (i + 1 + range) % range;
      win_metric = sum_metrics[left] + sum_metrics[i] + sum_metrics[right];
      if (sum_metrics[i] == min_metric) {
         if (win_metric < min_win_metric) {
            min_win_metric = win_metric;
            min_i = i;
         }
      }
   }

   // In all modes, start with the min metric

   config->all_offsets = min_i;
   config->sp_offset[0] = min_i;
   config->sp_offset[1] = min_i;
   config->sp_offset[2] = min_i;
   config->sp_offset[3] = min_i;
   config->sp_offset[4] = min_i;
   config->sp_offset[5] = min_i;
   log_sp(config);
   write_config(config, DAC_UPDATE);

   // Perform a final test of errors
   log_info("Performing final test");
   errors = diff_N_frames(capinfo, NUM_CAL_FRAMES, 0, elk);
   osd_sp(config, 2, errors);
   log_sp(config);
   log_info("Calibration complete, errors = %d", errors);
}

static void cpld_set_mode(int mode) {
   write_config(config, DAC_UPDATE);
}

static void cpld_set_vsync_psync(int state) {  //state = 1 is psync (version = 1), state = 0 is vsync (version = 0, mux = 1)
   int temp_mux = config->mux;
   if (state == 0) {
       config->mux = 1;
   }
   write_config(config, NO_DAC_UPDATE);
   config->mux = temp_mux;
   RPI_SetGpioValue(VERSION_PIN, state);
}

static int cpld_analyse(int selected_sync_state, int analyse) {
   if (supports_invert) {
      if (invert ^ (selected_sync_state & SYNC_BIT_HSYNC_INVERTED)) {
         invert ^= SYNC_BIT_HSYNC_INVERTED;
         if (invert) {
            log_info("Analyze Csync: polarity changed to inverted");
         } else {
            log_info("Analyze Csync: polarity changed to non-inverted");
         }
         write_config(config, DAC_UPDATE);
      } else {
         if (invert) {
            log_info("Analyze Csync: polarity unchanged (inverted)");
         } else {
            log_info("Analyze Csync: polarity unchanged (non-inverted)");
         }
      }
      int polarity = selected_sync_state;
      if (analyse) {
          polarity = analyse_sync();
          //log_info("Raw polarity = %x %d %d %d %d", polarity, hsync_comparison_lo, hsync_comparison_hi, vsync_comparison_lo, vsync_comparison_hi);
          if (selected_sync_state & SYNC_BIT_COMPOSITE_SYNC) {
              polarity &= SYNC_BIT_HSYNC_INVERTED;
              polarity |= SYNC_BIT_COMPOSITE_SYNC;
          }
      }
      polarity ^= invert;
      if (supports_separate == 0) {
          polarity ^= ((polarity & SYNC_BIT_VSYNC_INVERTED) ? SYNC_BIT_HSYNC_INVERTED : 0);
          polarity |= SYNC_BIT_MIXED_SYNC;
      }
      if (supports_vsync) {
         return (polarity);
      } else {
         return ((polarity & SYNC_BIT_HSYNC_INVERTED) | SYNC_BIT_COMPOSITE_SYNC | SYNC_BIT_MIXED_SYNC);
      }
   } else {
       return (SYNC_BIT_COMPOSITE_SYNC | SYNC_BIT_MIXED_SYNC);
   }
}

static void cpld_update_capture_info(capture_info_t *capinfo) {
   // Update the capture info stucture, if one was passed in
   if (capinfo) {
      // Update the sample width
      capinfo->sample_width = SAMPLE_WIDTH_6;
      // Update the line capture function
      capinfo->capture_line = capture_line_normal_6bpp_table;
   }
   write_config(config, DAC_UPDATE);
}

static param_t *cpld_get_params() {
   return params;
}

static int cpld_get_value(int num) {
   switch (num) {
   case CPLD_SETUP_MODE:
      return config->cpld_setup_mode;
   case ALL_OFFSETS:
      return config->all_offsets;
   case RATE:
      return config->rate;
   case FILTER_L:
      return config->filter_l;
   case MUX:
      return config->mux;
   case DAC_A:
      return config->dac_a;
   case DAC_B:
      return config->dac_b;
   case DAC_C:
      return config->dac_c;
   case DAC_D:
      return config->dac_d;
   case DAC_E:
      return config->dac_e;
   case DAC_F:
      return config->dac_f;
   case DAC_G:
      return config->dac_g;
   case DAC_H:
      return config->dac_h;
   case SUB_C:
      return config->sub_c;
   case ALT_R:
      return config->alt_r;
   case EDGE:
      return config->edge;
   case DELAY:
      return config->delay;
   case CLAMPTYPE:
      return config->clamptype;
   case TERMINATE:
      return config->terminate;
   case COUPLING:
      return config->coupling;

   case A_OFFSET:
      return config->sp_offset[0];
   case B_OFFSET:
      return config->sp_offset[1];
   case C_OFFSET:
      return config->sp_offset[2];
   case D_OFFSET:
      return config->sp_offset[3];
   case E_OFFSET:
      return config->sp_offset[4];
   case F_OFFSET:
      return config->sp_offset[5];
   case HALF:
      return config->half_px_delay;
   case DIVIDER:
      return config->divider;

   }
   return 0;
}

static const char *cpld_get_value_string(int num) {
   if (num == EDGE) {
      return edge_names[config->edge];
   }
   if (num == RATE) {
      if ((cpld_version & 0xff) >= 0x82) {
          return rate_names[config->rate];
      }
   }
   if (num == CLAMPTYPE) {
      if ((cpld_version & 0xff) >= 0x82 && config->rate == YUV_RATE_6_LEVEL_4) {
          return level_names[config->clamptype];
      } else {
          return clamptype_names[config->clamptype];
      }
   }
   if (num == CPLD_SETUP_MODE) {
      return cpld_setup_names[config->cpld_setup_mode];
   }
   if (num == COUPLING) {
      return coupling_names[config->coupling];
   }
   return NULL;
}

static void cpld_set_value(int num, int value) {
   if (value < params[num].min) {
      value = params[num].min;
   }
   if (value > params[num].max) {
      value = params[num].max;
   }
   switch (num) {

   case CPLD_SETUP_MODE:
      config->cpld_setup_mode = value;
      set_setup_mode(value);
      break;
   case ALL_OFFSETS:
      config->all_offsets = value;
      config->all_offsets &= getRange() - 1;
      config->sp_offset[0] = config->all_offsets;
      config->sp_offset[1] = config->all_offsets;
      config->sp_offset[2] = config->all_offsets;
      config->sp_offset[3] = config->all_offsets;
      config->sp_offset[4] = config->all_offsets;
      config->sp_offset[5] = config->all_offsets;
      // Keep offset in the legal range (which depends on config->sub_c)

      break;
   case RATE:
      config->rate = value;
      if (supports_four_level) {
          params[DAC_B].label = "DAC-B: Y Lo";
          params[DAC_D].label = "DAC-D: UV Lo";
          params[DAC_F].label = "DAC-F: Y V Sync";
          params[DAC_G].label = "DAC-G: Y Clamp";
          params[DAC_H].label = "DAC-H: Unused";
          params[DAC_G].hidden = 0;
          params[DAC_H].hidden = 1;
          if (config->rate == YUV_RATE_6) {
             params[CLAMPTYPE].label = "Clamp Type";
             params[DAC_H].hidden = 1;
             params[COUPLING].hidden = 0;
          } else {
             params[CLAMPTYPE].label = "Level Type";
             params[COUPLING].hidden = 1;
             int levels = config->clamptype;
             if (levels > 3) {
                 levels = 3;
             }
             params[DAC_G].hidden = 1;
             if (levels & 2) {
                 params[DAC_B].label = "DAC-B: Y Mid";
                 params[DAC_F].label = "DAC-F: Y Lo";
             }
             if (levels & 1) {
                 params[DAC_G].hidden = 0;
                 params[DAC_H].hidden = 0;
                 params[DAC_D].label = "DAC-D: UV Mid";
                 params[DAC_G].label = "DAC-G: V Lo";
                 params[DAC_H].label = "DAC-H: U Lo";
             }
          }
          osd_refresh();
      }
      break;
   case FILTER_L:
      config->filter_l = value;
      break;
   case MUX:
      config->mux = value;
      break;
   case DAC_A:
      config->dac_a = value;
      break;
   case DAC_B:
      config->dac_b = value;
      break;
   case DAC_C:
      config->dac_c = value;
      break;
   case DAC_D:
      config->dac_d = value;
      break;
   case DAC_E:
      config->dac_e = value;
      break;
   case DAC_F:
      config->dac_f = value;
      break;
   case DAC_G:
      config->dac_g = value;
      break;
   case DAC_H:
      config->dac_h = value;
      break;
   case SUB_C:
      config->sub_c = value;
      // Keep offset in the legal range (which depends on config->sub_c)
      config->all_offsets &= getRange() - 1;
      config->sp_offset[0] = config->all_offsets;
      config->sp_offset[1] = config->all_offsets;
      config->sp_offset[2] = config->all_offsets;
      config->sp_offset[3] = config->all_offsets;
      config->sp_offset[4] = config->all_offsets;
      config->sp_offset[5] = config->all_offsets;
      break;
   case ALT_R:
      config->alt_r = value;
      break;
   case EDGE:
      config->edge = value;
      break;
   case DELAY:
      config->delay = value;
      break;
   case CLAMPTYPE:
      config->clamptype = value;
      cpld_set_value(RATE, config->rate);
      break;
   case TERMINATE:
      config->terminate = value;
      break;
   case COUPLING:
      config->coupling = value;
      break;

   case A_OFFSET:
      break;
   case B_OFFSET:
      break;
   case C_OFFSET:
      break;
   case D_OFFSET:
      break;
   case E_OFFSET:
      break;
   case F_OFFSET:
      break;
   case HALF:
      config->half_px_delay = value;
      break;
   case DIVIDER:
      config->divider = value;
      break;

   }
   write_config(config, DAC_UPDATE);
}

static int cpld_show_cal_summary(int line) {
   return osd_sp(config, line, errors);
}

static void cpld_show_cal_details(int line) {
   int range = (*sum_metrics < 0) ? 0 : getRange();
   if (range == 0) {
      sprintf(message, "No calibration data for this mode");
      osd_set(line, 0, message);
   } else {
      if (range > 8) {
         // Two column display
         int num = range >> 1;
         for (int value = 0; value < num; value++) {
            sprintf(message, "Offset %d: %6d; Offset %2d: %6d", value, sum_metrics[value], value + num, sum_metrics[value + num]);
            osd_set(line + value, 0, message);
         }
      } else {
         // One column display
         for (int value = 0; value < range; value++) {
            sprintf(message, "Offset %d: %6d", value, sum_metrics[value]);
            osd_set(line + value, 0, message);
         }
      }
   }
}

static int cpld_old_firmware_support() {
    return 0;
}

static int cpld_get_divider() {
    return 8;                        // not sure of value for atom cpld
}

static int cpld_get_delay() {
   int delay;
   if (supports_extended_delay) {
      delay = 0;
   } else {
      delay = cpld_get_value(DELAY) & 0x0c;         //mask out lower 2 bits of delay
   }
   // Compensate for change of delay with YUV CPLD v8.x
   int major = (cpld_version >> VERSION_MAJOR_BIT) & 0x0F;
   if (major < 8) {
      delay += 16;
   }
   return delay;
}

static int cpld_get_sync_edge() {
    return config->edge;
}

static int cpld_frontend_info() {
    if (new_M62364_DAC_detected()) {
        return FRONTEND_YUV_ISSUE4 | FRONTEND_YUV_ISSUE4 << 16;
    } else {
        return FRONTEND_YUV_ISSUE3_5259 | FRONTEND_YUV_ISSUE2_5259 << 16;
    }
}

static void cpld_set_frontend(int value) {
   frontend = value;
   write_config(config, DAC_UPDATE);
}

cpld_t cpld_yuv = {
   .name = "6-12_BIT_YUV_Analog",
   .default_profile = "Atom",
   .init = cpld_init,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .set_vsync_psync = cpld_set_vsync_psync,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .get_sync_edge = cpld_get_sync_edge,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details
};
