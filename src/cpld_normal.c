#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include "defs.h"
#include "cpld.h"
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
   int min_metric;
   int *rgb_metric;
   int metric;

   if (mode7) {
      log_info("Calibrating mode 7");
   } else {
      log_info("Calibrating modes 0..6");
   }

   min_metric = INT_MAX;
   min_i = 0;
   config->half_px_delay = 0;
   for (int i = 0; i <= (mode7 ? 7 : 5); i++) {
      for (int j = 0; j < NUM_OFFSETS; j++) {
         config->sp_offset[j] = i;
      }
      write_config(config);
      rgb_metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
      metric = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
      log_info("offset = %d: metric = %d", i, metric);
      if (metric < min_metric) {
         min_metric = metric;
         min_i = i;
      }
   }
   // If the min metric is at the limit, make use of the half pixel delay
   if (mode7 && (min_i <= 1 || min_i >= 6)) {
      log_info("Enabling half pixel delay");
      config->half_px_delay = 1;
      min_i ^= 4;
   }
   for (int i = 0; i < NUM_OFFSETS; i++) {
      config->sp_offset[i] = min_i;
   }

   // If the metric is non zero, there is scope for further optimiation in mode7
   int ref = min_metric;
   if (mode7 && ref > 0) {
      log_info("Optimizing calibration");
      log_sp(config);
      log_debug("ref = %d", ref);
      for (int i = 0; i < NUM_CHANNELS; i++) {
         int left = INT_MAX;
         int right = INT_MAX;
         if (config->sp_offset[i] > 0) {
            config->sp_offset[i]--;
            write_config(config);
            rgb_metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
            left = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
            config->sp_offset[i]++;
         }
         if (config->sp_offset[i] < 7) {
            config->sp_offset[i]++;
            write_config(config);
            rgb_metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
            right = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
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
   log_info("Calibration complete");
   log_sp(config);
   write_config(config);
}

static void cpld_set_mode(int mode) {
   mode7 = mode;
   config = mode ? &mode7_config : &default_config;
   write_config(config);
}

enum {
   A_OFFSET,
   B_OFFSET,
   C_OFFSET,
   D_OFFSET,
   E_OFFSET,
   F_OFFSET,
   HALF
};

static param_t params[] = {
   { "A offset",    0, 7 },
   { "B offset",    0, 7 },
   { "C offset",    0, 7 },
   { "D offset",    0, 7 },
   { "E offset",    0, 7 },
   { "F offset",    0, 7 },
   { "Half",        0, 1 },
   { NULL,          0, 0 },
};


static param_t *cpld_get_params() {
   return params;
}


static int cpld_get_value(int num) {
   switch (num) {
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
