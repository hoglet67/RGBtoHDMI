#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include "defs.h"
#include "cpld.h"
#include "osd.h"
#include "logging.h"
#include "rgb_to_fb.h"
#include "rpi-gpio.h"

#define RANGE 16

// The number of frames to compute differences over
#define NUM_CAL_FRAMES 10

typedef struct {
   int sp_offset;
   int filter_c;
   int filter_l;
   int invert_l;
   int mux;
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
static int sum_metrics[RANGE];

// Error count for final calibration values for mode 0..6
static int errors;

// The CPLD version number
static int cpld_version;

static int frontend = 0;

static int supports_invert_luma = 0;
static int supports_invert = 0;
static int supports_separate = 0;
static int supports_vsync = 0;

// invert state (not part of config)
static int invert = 0;

// =============================================================
// Param definitions for OSD
// =============================================================

enum {
   OFFSET,
   FILTER_C,
   FILTER_L,
   INVERT_L,
   MUX,
   DAC_A,
   DAC_B,
   DAC_C,
   DAC_D,
   DAC_E,
   DAC_F,
   DAC_G,
   DAC_H

};

static param_t params[] = {
   {      OFFSET,      "Offset",      "offset", 0, 15, 1 },
   {    FILTER_C,    "C Filter",    "c_filter", 0,  1, 1 },
   {    FILTER_L,    "L Filter",    "l_filter", 0,  1, 1 },
   {    INVERT_L,    "L Invert",    "l_invert", 0,  1, 1 },
   {         MUX,   "Input Mux",   "input_mux", 0,   1, 1 },
   {       DAC_A,  "DAC-A (G/Y Hi)",   "dac_a", 0, 255, 1 },
   {       DAC_B,  "DAC-B (G/Y Lo)",   "dac_b", 0, 255, 1 },
   {       DAC_C,  "DAC-C (RB/UV Hi)", "dac_c", 0, 255, 1 },
   {       DAC_D,  "DAC-D (RB/UV Lo)", "dac_d", 0, 255, 1 },
   {       DAC_E,  "DAC-E (G/Y Sync)", "dac_f", 0, 255, 1 },
   {       DAC_F,  "DAC-F (Sync)",     "dac_g", 0, 255, 1 },
   {       DAC_G,  "DAC-G (Terminate)","dac_g", 0, 255, 1 },
   {       DAC_H,  "DAC-H (G/Y Clamp)","dac_h", 0, 255, 1 },
   {          -1,          NULL,       NULL, 0,  0, 0 }
};

// =============================================================
// Private methods
// =============================================================

static void sendDAC(int dac, int value)
{
    if (dac == 6) {
        if (value >=1) value = 255;
    }
    int packet = (dac << 11) | 0x600 | value;

    RPI_SetGpioValue(STROBE_PIN, 0);
    for (int i = 0; i < 16; i++) {
        RPI_SetGpioValue(SP_CLKEN_PIN, 0);
        RPI_SetGpioValue(SP_DATA_PIN, (packet >> 15) & 1);
        delay_in_arm_cycles(500);
        RPI_SetGpioValue(SP_CLKEN_PIN, 1);
        delay_in_arm_cycles(500);
        packet <<= 1;
    }
    RPI_SetGpioValue(STROBE_PIN, 1);
    RPI_SetGpioValue(SP_DATA_PIN, 0);
}

static void write_config(config_t *config) {
   int sp = 0;
   int scan_len = 6;
   sp |= config->sp_offset & 15;
   sp |= config->filter_c << 4;
   sp |= config->filter_l << 5;

   if (supports_invert_luma) {
      sp |= config->invert_l << 6;
      scan_len++;
   }

   if (supports_invert) {
      sp |= invert << 7;
      scan_len++;
   }

   for (int i = 0; i < scan_len; i++) {
      RPI_SetGpioValue(SP_DATA_PIN, sp & 1);
      for (int j = 0; j < 1000; j++);
      RPI_SetGpioValue(SP_CLKEN_PIN, 1);
      for (int j = 0; j < 100; j++);
      RPI_SetGpioValue(SP_CLK_PIN, 0);
      RPI_SetGpioValue(SP_CLK_PIN, 1);
      for (int j = 0; j < 100; j++);
      RPI_SetGpioValue(SP_CLKEN_PIN, 0);
      for (int j = 0; j < 1000; j++);
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

   RPI_SetGpioValue(SP_DATA_PIN, 0);
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
   for (int i = 0; i < RANGE; i++) {
      sum_metrics[i] = -1;
   }
   errors = -1;

   int major = (cpld_version >> VERSION_MAJOR_BIT) & 0x0F;
   //int minor = (cpld_version >> VERSION_MINOR_BIT) & 0x0F;

   // Optional invert parameter
   // CPLDv3 and beyond support an invertion of video
   if (major >= 3) {
      supports_invert_luma = 1;
      supports_invert = 1;
      supports_separate = 1;
      supports_vsync = 1;
   } else {
      supports_invert_luma = 0;
      supports_invert = 0;
      supports_separate = 0;
      supports_vsync = 0;
   }

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
   int range = RANGE;  // 0..15 for the Atom

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
   case OFFSET:
      return config->sp_offset;
   case FILTER_C:
      return config->filter_c;
   case FILTER_L:
      return config->filter_l;
   case INVERT_L:
      return config->invert_l;
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

   }
   return 0;
}

static const char *cpld_get_value_string(int num) {
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
   case OFFSET:
      config->sp_offset = value;
      break;
   case FILTER_C:
      config->filter_c = value;
      break;
   case FILTER_L:
      config->filter_l = value;
      break;
   case INVERT_L:
      config->invert_l = value;
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
   }
   write_config(config);
}

static int cpld_show_cal_summary(int line) {
   return osd_sp(config, line, errors);
}

static void cpld_show_cal_details(int line) {
   int range = (*sum_metrics < 0) ? 0 : RANGE;
   if (range == 0) {
      sprintf(message, "No calibration data for this mode");
      osd_set(line, 0, message);
   } else {
      int num = range >> 1;
      for (int value = 0; value < num; value++) {
         sprintf(message, "Offset %d: %6d; Offset %2d: %6d", value, sum_metrics[value], value + num, sum_metrics[value + num]);
         osd_set(line + value, 0, message);
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
    return 0;
}

static int cpld_frontend_info() {
    return FRONTEND_YUV_5259 | FRONTEND_YUV_5259 << 16;
}

static void cpld_set_frontend(int value) {
   frontend = value;
   write_config(config);
}


cpld_t cpld_yuv = {
   .name = "YUV",
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
