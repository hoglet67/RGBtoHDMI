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
   int half_px_delay;
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
   HALF
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
   { NULL,          0, 0 },
};

// =============================================================
// Private methods
// =============================================================

static void write_config(config_t *config) {
   int sp = 0;
   for (int i = 0; i < NUM_OFFSETS; i++) {
      sp |= (config->sp_offset[i] & 7) << (i * 3);
   }
   if (config->half_px_delay) {
      sp |= (1 << 18);
   }
   for (int i = 0; i < 19; i++) {
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

static void osd_sp(config_t *config, int metric) {
   sprintf(message, "Offsets: %d %d %d %d %d %d",
           config->sp_offset[0], config->sp_offset[1], config->sp_offset[2],
           config->sp_offset[3], config->sp_offset[4], config->sp_offset[5]);
   osd_set(1, 0, message);
   sprintf(message, "   Half: %d", config->half_px_delay);
   osd_set(2, 0, message);
   sprintf(message, " Errors: %d", metric);
   osd_set(3, 0, message);
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

static void cpld_init() {
   for (int i = 0; i < NUM_OFFSETS; i++) {
      default_config.sp_offset[i] = 0;
      mode7_config.sp_offset[i] = 0;
   }
   default_config.half_px_delay = 0;
   mode7_config.half_px_delay = 0;
   config = &default_config;
}

static void cpld_calibrate(int elk, int chars_per_line) {
   int min_i;
   int max_i;
   int min_metric;
   int max_metric;
   int *rgb_metric;
   int metric = 0;

   if (mode7) {
      log_info("Calibrating mode 7");
   } else {
      log_info("Calibrating modes 0..6");
   }

   min_metric = INT_MAX;
   max_metric = INT_MIN;
   min_i = 0;
   max_i = 0;
   config->half_px_delay = 0;
   for (int i = 0; i <= (mode7 ? 7 : 5); i++) {
      for (int j = 0; j < NUM_OFFSETS; j++) {
         config->sp_offset[j] = i;
      }
      write_config(config);
      rgb_metric = diff_N_frames(NUM_CAL_FRAMES, mode7, elk, chars_per_line);
      metric = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
      osd_sp(config, metric);
      log_info("offset = %d: metric = %5d", i, metric);
      if (metric < min_metric) {
         min_metric = metric;
         min_i = i;
      }
      if (metric > max_metric) {
         max_metric = metric;
         max_i = i;
      }
   }
   // If the min metric is at the limit, make use of the half pixel delay
   if (mode7 && (min_i <= 1 || min_i >= 6)) {
      log_info("Enabling half pixel delay");
      config->half_px_delay = 1;
      min_i ^= 4;
   }
   if (mode7) {
      // In mode 7, start with the min metric
      for (int i = 0; i < NUM_OFFSETS; i++) {
         config->sp_offset[i] = min_i;
      }
   } else {
      // In mode 0..6, start opposize the max metric
      for (int i = 0; i < NUM_OFFSETS; i++) {
         config->sp_offset[i] = (max_i + 3) % 6;
      }
   }

   // If the metric is non zero, there is scope for further optimiation in mode7
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
            osd_sp(config, left);
            config->sp_offset[i]++;
         }
         if (config->sp_offset[i] < 7) {
            config->sp_offset[i]++;
            write_config(config);
            rgb_metric = diff_N_frames(NUM_CAL_FRAMES, mode7, elk, chars_per_line);
            right = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
            osd_sp(config, right);
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
   metric = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
   osd_sp(config, metric);
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
   }
   write_config(config);
}

cpld_t cpld_normal = {
   .name = "Normal",
   .init = cpld_init,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .set_value = cpld_set_value
};
