#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include "defs.h"
#include "cpld.h"
#include "osd.h"
#include "logging.h"
#include "rpi-gpio.h"

// The number of frames to compute differences over
#define NUM_CAL_FRAMES 10

typedef struct {
   int sp_offset[NUM_OFFSETS];
   int half_px_delay; // 0 = off, 1 = on, all modes
   int full_px_delay; // 0..15, mode 7 only
} config_t;

// Current calibration state for mode 0..6
static config_t default_config;

// Current calibration state for mode 7
static config_t mode7_config;

// Current configuration
static config_t *config;
static int mode7;

// OSD message buffer
static char message[80];

// Calibration metrics (i.e. errors) for mode 0..6
static int metrics_default[8]; // Last two not used

// Calibration metrics (i.e. errors) for mode 7
static int metrics_mode7[8];

// Error count for final calibration values for mode 0..6
static int errors_default;

// Error count for final calibration values for mode 7
static int errors_mode7;

// The CPLD version number
static int cpld_version;

// Indicated the CPLD supports the mode 7 delay parameter
static int supports_delay;

// =============================================================
// Param definitions for OSD
// =============================================================

enum {
   ALL_OFFSETS,
   A_OFFSET,
   B_OFFSET,
   C_OFFSET,
   D_OFFSET,
   E_OFFSET,
   F_OFFSET,
   HALF,
   DELAY
};

static param_t default_params[] = {
   { "All offsets", 0, 5 },
   { "A offset",    0, 5 },
   { "B offset",    0, 5 },
   { "C offset",    0, 5 },
   { "D offset",    0, 5 },
   { "E offset",    0, 5 },
   { "F offset",    0, 5 },
   { "Half",        0, 1 },
   { NULL,          0, 0 },
};

static param_t mode7_params[] = {
   { "All offsets", 0, 7 },
   { "A offset",    0, 7 },
   { "B offset",    0, 7 },
   { "C offset",    0, 7 },
   { "D offset",    0, 7 },
   { "E offset",    0, 7 },
   { "F offset",    0, 7 },
   { "Half",        0, 1 },
   { "Delay",       0, 15 },
   { NULL,          0, 0 },
};

// =============================================================
// Private methods
// =============================================================

static void write_config(config_t *config) {
   int sp = 0;
   int scan_len = 19;
   for (int i = 0; i < NUM_OFFSETS; i++) {
      sp |= (config->sp_offset[i] & 7) << (i * 3);
   }
   if (config->half_px_delay) {
      sp |= (1 << 18);
   }
   if (supports_delay) {
      scan_len = 23;
      sp |= (config->full_px_delay << 19);
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
   RPI_SetGpioValue(SP_DATA_PIN, 0);
}

static void osd_sp(config_t *config, int line, int metric) {
   if (mode7) {
      osd_set(line, 0, "   Mode: 7");
   } else {
      osd_set(line, 0, "   Mode: 0..6");
   }
   sprintf(message, "Offsets: %d %d %d %d %d %d",
           config->sp_offset[0], config->sp_offset[1], config->sp_offset[2],
           config->sp_offset[3], config->sp_offset[4], config->sp_offset[5]);
   osd_set(line + 1, 0, message);
   sprintf(message, "   Half: %d", config->half_px_delay);
   osd_set(line + 2, 0, message);
   if (metric < 0) {
      sprintf(message, " Errors: unknown");
   } else {
      sprintf(message, " Errors: %d", metric);
   }
   osd_set(line + 3, 0, message);
}

static void log_sp(config_t *config) {
   log_info("sp_offset = %d %d %d %d %d %d; half = %d",
            config->sp_offset[0], config->sp_offset[1], config->sp_offset[2],
            config->sp_offset[3], config->sp_offset[4], config->sp_offset[5],
            config->half_px_delay);
}

// =============================================================
// Public methods
// =============================================================

static void cpld_init(int version) {
   cpld_version = version;
   // Version 2 CPLD supports the delay parameter
   supports_delay = ((cpld_version >> VERSION_MAJOR_BIT) & 0x0F) >= 2;
   if (!supports_delay) {
      mode7_params[DELAY].name = NULL; 
   }
   for (int i = 0; i < NUM_OFFSETS; i++) {
      default_config.sp_offset[i] = 0;
      mode7_config.sp_offset[i] = 0;
   }
   default_config.half_px_delay = 0;
   mode7_config.half_px_delay = 0;
   mode7_config.full_px_delay = 4; // Correct for the master
   config = &default_config;
   for (int i = 0; i < 8; i++) {
      metrics_default[i] = -1;
      metrics_mode7[i] = -1;
   }
   errors_default = -1;
   errors_mode7 = -1;
}

static int cpld_get_version() {
   return cpld_version;
}

static void cpld_calibrate(int elk, int chars_per_line) {
   int min_i = 0;
   int metric;         // this is a point value (at one sample offset)
   int min_metric;
   int win_metric;     // this is a windowed value (over three sample offsets)
   int min_win_metric;
   int *rgb_metric;

   int range;          // 0..5 in Modes 0..6, 0..7 in Mode 7
   int *metrics;
   int *errors;

   if (mode7) {
      log_info("Calibrating mode 7");
      range   = 8;
      metrics = metrics_mode7;
      errors  = &errors_mode7;
   } else {
      log_info("Calibrating modes 0..6");
      range   = 6;
      metrics = metrics_default;
      errors  = &errors_default;
   }

   min_metric = INT_MAX;
   config->half_px_delay = 0;
   for (int i = 0; i < range; i++) {
      for (int j = 0; j < NUM_OFFSETS; j++) {
         config->sp_offset[j] = i;
      }
      write_config(config);
      rgb_metric = diff_N_frames(NUM_CAL_FRAMES, mode7, elk, chars_per_line);
      metric = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
      metrics[i] = metric;
      osd_sp(config, 1, metric);
      log_info("offset = %d: metric = %5d", i, metric);
      if (metric < min_metric) {
         min_metric = metric;
      }
   }
   // Use a 3 sample window to find the minimum and maximum
   min_win_metric = INT_MAX;
   for (int i = 0; i < range; i++) {
      int left  = (i - 1 + range) % range;
      int right = (i + 1 + range) % range;
      win_metric = metrics[left] + metrics[i] + metrics[right];
      if (metrics[i] == min_metric) {
         if (win_metric < min_win_metric) {
            min_win_metric = win_metric;
            min_i = i;
         }
      }
   }
   // If the min metric is at the limit, make use of the half pixel delay
   if (mode7 && min_metric > 0 && (min_i <= 1 || min_i >= 6)) {
      log_info("Enabling half pixel delay");
      config->half_px_delay = 1;
      min_i ^= 4;
   }
   // In all modes, start with the min metric
   for (int i = 0; i < NUM_OFFSETS; i++) {
      config->sp_offset[i] = min_i;
   }

   // If the metric is non zero, there is scope for further optimization in mode7
   if (mode7 && min_metric > 0) {
      int ref = min_metric;
      log_info("Optimizing calibration");
      log_sp(config);
      log_debug("ref = %d", ref);
      for (int i = 0; i < NUM_OFFSETS; i++) {
         int left = INT_MAX;
         int right = INT_MAX;
         if (config->sp_offset[i] > 0) {
            config->sp_offset[i]--;
            write_config(config);
            rgb_metric = diff_N_frames(NUM_CAL_FRAMES, mode7, elk, chars_per_line);
            left = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
            osd_sp(config, 1, left);
            config->sp_offset[i]++;
         }
         if (config->sp_offset[i] < 7) {
            config->sp_offset[i]++;
            write_config(config);
            rgb_metric = diff_N_frames(NUM_CAL_FRAMES, mode7, elk, chars_per_line);
            right = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
            osd_sp(config, 1, right);
            config->sp_offset[i]--;
         }
         if (left < right && left < ref) {
            config->sp_offset[i]--;
            ref = left;
            log_info("nudged %d left, metric = %d", i, ref);
         } else if (right < left && right < ref) {
            config->sp_offset[i]++;
            ref = right;
            log_info("nudged %d right, metric = %d", i, ref);
         }
      }
   }
   write_config(config);
   rgb_metric = diff_N_frames(NUM_CAL_FRAMES, mode7, elk, chars_per_line);
   *errors = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
   osd_sp(config, 1, *errors);
   log_info("Calibration complete");
   log_sp(config);
}

static void cpld_set_mode(int mode) {
   mode7 = mode;
   config = mode ? &mode7_config : &default_config;
   write_config(config);
}

static param_t *cpld_get_params() {
   if (mode7) {
      return mode7_params;
   } else {
      return default_params;
   }
}

static int cpld_get_value(int num) {
   switch (num) {
   case ALL_OFFSETS:
      return config->sp_offset[0];
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
   case DELAY:
      return config->full_px_delay;
   }
   return 0;
}

static void cpld_set_value(int num, int value) {
   switch (num) {
   case ALL_OFFSETS:
      config->sp_offset[0] = value;
      config->sp_offset[1] = value;
      config->sp_offset[2] = value;
      config->sp_offset[3] = value;
      config->sp_offset[4] = value;
      config->sp_offset[5] = value;
      break;
   case A_OFFSET:
      config->sp_offset[0] = value;
      break;
   case B_OFFSET:
      config->sp_offset[1] = value;
      break;
   case C_OFFSET:
      config->sp_offset[2] = value;
      break;
   case D_OFFSET:
      config->sp_offset[3] = value;
      break;
   case E_OFFSET:
      config->sp_offset[4] = value;
      break;
   case F_OFFSET:
      config->sp_offset[5] = value;
      break;
   case HALF:
      config->half_px_delay = value;
      break;
   case DELAY:
      config->full_px_delay = value;
      break;
   }
   write_config(config);
}

static void cpld_show_cal_summary(int line) {
   osd_sp(config, line, mode7 ? errors_mode7 : errors_default);
}

static void cpld_show_cal_details(int line) {
   int *metrics = mode7 ? metrics_mode7 : metrics_default;
   int num = (*metrics < 0) ? 0 : mode7 ? 8 : 6;
   if (num == 0) {
      sprintf(message, "No calibration data for this mode");
      osd_set(line, 0, message);
   } else {
      for (int i = 0; i < num; i++) {
         sprintf(message, "Offset %d: Errors = %6d", i, metrics[i]);
         osd_set(line + i, 0, message);
      }
   }
}

cpld_t cpld_normal = {
   .name = "Normal",
   .init = cpld_init,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details
};
