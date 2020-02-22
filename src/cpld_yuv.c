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

typedef struct {
   int cpld_setup_mode;
   int sp_offset;
   int filter_l;
   int filter_c;
   int sub_c;
   int alt_r;
   int edge;
   int clamptype;
   int delay;
   int mux;
   int terminate;
   int dac_a;
   int dac_b;
   int dac_c;
   int dac_d;
   int dac_e;
   int dac_f;
   int dac_g;
   int dac_h;
} config_t;



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

// invert state (not part of config)
static int invert = 0;

// =============================================================
// Param definitions for OSD
// =============================================================

enum {
   CPLD_SETUP_MODE,
   OFFSET,
   FILTER_L,
   FILTER_C,
   SUB_C,
   ALT_R,
   EDGE,
   CLAMPTYPE,
   DELAY,
   MUX,
   TERMINATE,
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
   "Set Delay"
};

static const char *clamptype_names[] = {
   "Sync Tip",
   "Back Porch Short",
   "Back Porch Medium",
   "Back Porch Long",
   "Back Porch Auto"
};

static const char *termination_names[] = {
   "DC/High Impedance",
   "DC/75R Termination",
   "AC/High Impedance",
   "AC/75R Termination"
};

enum {
   YUV_INPUT_DC_HI,
   YUV_INPUT_DC_TERM,
   YUV_INPUT_AC_HI,
   YUV_INPUT_AC_TERM,
   NUM_YUV_INPUT
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
   {      OFFSET,  "Offset",          "offset", 0,  15, 1 },
   {    FILTER_L,  "Filter Y",      "l_filter", 0,   1, 1 },
   {    FILTER_C,  "Filter UV",     "c_filter", 0,   1, 1 },
   {       SUB_C,  "Subsample UV",     "sub_c", 0,   1, 1 },
   {       ALT_R,  "PAL switch",       "alt_r", 0,   1, 1 },
   {        EDGE,  "Sync Edge",         "edge", 0,   1, 1 },
   {   CLAMPTYPE,  "Clamp Type",   "clamptype", 0,     NUM_CLAMPTYPE-1, 1 },
   {       DELAY,  "Delay",            "delay", 0,  15, 1 },
   {         MUX,  "Input Mux",    "input_mux", 0,   1, 1 },
   {   TERMINATE,  "Input Coupling",    "coupling", 0,   NUM_YUV_INPUT-1, 1 },
   {       DAC_A,  "DAC-A: Y Hi",      "dac_a", 0, 255, 1 },
   {       DAC_B,  "DAC-B: Y Lo",      "dac_b", 0, 255, 1 },
   {       DAC_C,  "DAC-C: UV Hi",     "dac_c", 0, 255, 1 },
   {       DAC_D,  "DAC-D: UV Lo",     "dac_d", 0, 255, 1 },
   {       DAC_E,  "DAC-E: Y Mid/VS",  "dac_f", 0, 255, 1 },
   {       DAC_F,  "DAC-F: Sync",      "dac_g", 0, 255, 1 },
   {       DAC_G,  "DAC-G: Y Clamp",   "dac_g", 0, 255, 1 },
   {       DAC_H,  "DAC-H: Spare",     "dac_h", 0, 255, 1 },
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
    if (dac == 5) {
        if (value < 2) value = 2;  // prevent sync being just high frequency noise when no sync input
    }

    if (frontend == FRONTEND_YUV_ISSUE2_5259) {
        switch (dac) {
            case 6:
                dac = 7;
            break;
            case 7:
            {
                dac = 6;
                switch (config->terminate) {
                      default:
                      case YUV_INPUT_DC_HI:
                      case YUV_INPUT_AC_HI:
                        value = 0;  //high impedance
                      break;
                      case YUV_INPUT_DC_TERM:
                      case YUV_INPUT_AC_TERM:
                        value = 255; //termination
                      break;
                  }
            }
            break;
            default:
            break;
            }
    }

    int packet = (dac << 11) | 0x600 | value;

    RPI_SetGpioValue(STROBE_PIN, 0);
    for (int i = 0; i < 16; i++) {
        RPI_SetGpioValue(SP_CLKEN_PIN, 0);
        RPI_SetGpioValue(SP_DATA_PIN, (packet >> 15) & 1);
        delay_in_arm_cycles(cpu_adjust(500));
        RPI_SetGpioValue(SP_CLKEN_PIN, 1);
        delay_in_arm_cycles(cpu_adjust(500));
        packet <<= 1;
    }
    RPI_SetGpioValue(STROBE_PIN, 1);
    RPI_SetGpioValue(SP_DATA_PIN, 0);
}

static void write_config(config_t *config) {
   int sp = 0;
   int scan_len = 0;
   if (supports_delay) {
      // In the hardware we effectively have a total of 5 bits of actual offset
      // and we need to derive this from the offset and delay values
      if (config->sub_c) {
         // Use 4 bits of offset and 1 bit of delay
         sp |= (config->sp_offset & 15) << scan_len;
         scan_len += 4;
         sp |= ((~config->delay >> 1) & 1) << scan_len;
         scan_len += 1;
      } else {
         // Use 3 bits of offset and 2 bit of delay
         sp |= (config->sp_offset & 7) << scan_len;
         scan_len += 3;
         sp |= (~config->delay & 3) << scan_len;
         scan_len += 2;
      }
   } else {
      sp |= (config->sp_offset & 15) << scan_len;
      scan_len += 4;
   }

   if (supports_extended_delay) {
      sp |= ((~config->delay >> 2) & 3) << scan_len;
      scan_len += 2;
   }

   sp |= config->filter_c << scan_len;
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
          clamptype = CLAMPTYPE_SHORT;
          if (geometry_get_value(CLOCK) >= 6750000) {
              clamptype = CLAMPTYPE_MEDIUM;
          }
          if (geometry_get_value(CLOCK) >= 9750000) {
              clamptype = CLAMPTYPE_LONG;
          }
      }
      sp |= clamptype << scan_len;
      scan_len += 2;
   }

   for (int i = 0; i < scan_len; i++) {
      RPI_SetGpioValue(SP_DATA_PIN, sp & 1);
      delay_in_arm_cycles(cpu_adjust(100));
      RPI_SetGpioValue(SP_CLKEN_PIN, 1);
      delay_in_arm_cycles(cpu_adjust(100));
      RPI_SetGpioValue(SP_CLK_PIN, 0);
      delay_in_arm_cycles(cpu_adjust(100));
      RPI_SetGpioValue(SP_CLK_PIN, 1);
      delay_in_arm_cycles(cpu_adjust(100));
      RPI_SetGpioValue(SP_CLKEN_PIN, 0);
      delay_in_arm_cycles(cpu_adjust(100));
      sp >>= 1;
   }

   sendDAC(0, config->dac_a);
   sendDAC(1, config->dac_b);
   sendDAC(2, config->dac_c);
   sendDAC(3, config->dac_d);
   sendDAC(4, config->dac_e);
   sendDAC(5, config->dac_f);
   sendDAC(6, config->dac_g);
   sendDAC(7, config->dac_h);

      switch (config->terminate) {
         default:
          case YUV_INPUT_DC_HI:
            RPI_SetGpioValue(SP_DATA_PIN, 0);   //ac-dc
            RPI_SetGpioValue(SP_CLKEN_PIN, 0);  //termination
          break;
          case YUV_INPUT_DC_TERM:
            RPI_SetGpioValue(SP_DATA_PIN, 0);
            RPI_SetGpioValue(SP_CLKEN_PIN, 1);
          break;
          case YUV_INPUT_AC_HI:
            RPI_SetGpioValue(SP_DATA_PIN, 1);
            RPI_SetGpioValue(SP_CLKEN_PIN, 0);
          break;
          case YUV_INPUT_AC_TERM:
            RPI_SetGpioValue(SP_DATA_PIN, 1);
            RPI_SetGpioValue(SP_CLKEN_PIN, 1);
          break;
      }
   RPI_SetGpioValue(MUX_PIN, config->mux);
}

static int osd_sp(config_t *config, int line, int metric) {
   // Line ------
   sprintf(message, "         Offset: %d", config->sp_offset);
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
   log_info("sp_offset = %d", config->sp_offset);
}

// =============================================================
// Public methods
// =============================================================

static void cpld_init(int version) {
   cpld_version = version;
   config->sp_offset = 0;
   config->filter_c  = 1;
   config->filter_l  = 1;
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
   geometry_hide_pixel_sampling();
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
      config->sp_offset = value;
      write_config(config);
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
   config->sp_offset = min_i;
   log_sp(config);
   write_config(config);

   // Perform a final test of errors
   log_info("Performing final test");
   errors = diff_N_frames(capinfo, NUM_CAL_FRAMES, 0, elk);
   osd_sp(config, 1, errors);
   log_sp(config);
   log_info("Calibration complete, errors = %d", errors);
}

static void cpld_set_mode(int mode) {
   write_config(config);
}

static int cpld_analyse(int manual_setting) {
   if (supports_invert) {
      int polarity;
      if (manual_setting < 0) {
         polarity = analyse_sync(); // returns lsb set to 1 if sync polarity incorrect
      } else {
         polarity = manual_setting ^ invert;
      }
      if (polarity & SYNC_BIT_HSYNC_INVERTED) {
         invert ^= (polarity & SYNC_BIT_HSYNC_INVERTED);
         if (invert) {
            log_info("Analyze Csync: polarity changed to inverted");
         } else {
            log_info("Analyze Csync: polarity changed to non-inverted");
         }
         write_config(config);
      } else {
         if (invert) {
            log_info("Analyze Csync: polarity unchanged (inverted)");
         } else {
            log_info("Analyze Csync: polarity unchanged (non-inverted)");
         }
      }
      polarity &= ~SYNC_BIT_HSYNC_INVERTED;
      polarity |= (invert ? SYNC_BIT_HSYNC_INVERTED : 0);
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
      capinfo->sample_width = 1; // 1 = 6 bits
      // Update the line capture function
      capinfo->capture_line = capture_line_normal_6bpp_table;
   }
}

static param_t *cpld_get_params() {
   return params;
}

static int cpld_get_value(int num) {
   switch (num) {
   case CPLD_SETUP_MODE:
      return config->cpld_setup_mode;
   case OFFSET:
      return config->sp_offset;
   case FILTER_C:
      return config->filter_c;
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
   }
   return 0;
}

static const char *cpld_get_value_string(int num) {
   if (num == EDGE) {
      return edge_names[config->edge];
   }
   if (num == CLAMPTYPE) {
      return clamptype_names[config->clamptype];
   }
   if (num == CPLD_SETUP_MODE) {
      return cpld_setup_names[config->cpld_setup_mode];
   }
   if (num == TERMINATE) {
      return termination_names[config->terminate];
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
   case OFFSET:
      config->sp_offset = value;
      // Keep offset in the legal range (which depends on config->sub_c)
      config->sp_offset &= getRange() - 1;
      break;
   case FILTER_C:
      config->filter_c = value;
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
      config->sp_offset &= getRange() - 1;
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
      break;
   case TERMINATE:
      config->terminate = value;
      break;
   }
   write_config(config);
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
      delay = cpld_get_value(DELAY);
   }
   // Compensate for change of delay with YUV CPLD v8.x
   int major = (cpld_version >> VERSION_MAJOR_BIT) & 0x0F;
   if (major < 8) {
      delay += 16;
   }
   return delay;
}

static int cpld_frontend_info() {
    return FRONTEND_YUV_ISSUE3_5259 | FRONTEND_YUV_ISSUE2_5259 << 16;
}

static void cpld_set_frontend(int value) {
   frontend = value;
   write_config(config);
}


cpld_t cpld_yuv = {
   .name = "6BIT_YUV_Analog",
   .default_profile = "Atom",
   .init = cpld_init,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details
};
