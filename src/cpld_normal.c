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
}

static void cpld_calibrate(int mode7, int elk, int chars_per_line) {
   int min_i;
   int min_metric;
   int *rgb_metric;
   int metric;
   config_t *config;

   if (mode7) {
      log_info("Calibrating mode 7");
      config = &mode7_config;
   } else {
      log_info("Calibrating modes 0..6");
      config = &default_config;
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

static void cpld_change_mode(int mode7) {
   if (mode7) {
      write_config(&mode7_config);
   } else {
      write_config(&default_config);
   }
}

static void cpld_inc_sampling_base(int mode7) {
   // currently nothing to to, as design only has offsets
}

static void cpld_inc_sampling_offset(int mode7) {
   config_t *config = mode7 ? &mode7_config : &default_config;
   int limit = mode7 ? 8 : 6;
   for (int i = 0; i < NUM_OFFSETS; i++) {
      config->sp_offset[i] = (config->sp_offset[i] + 1) % limit;
   }
   log_sp(config);
   write_config(config);
}

cpld_t cpld_normal = {
   .name = "Normal",
   .init = cpld_init,
   .inc_sampling_base = cpld_inc_sampling_base,
   .inc_sampling_offset = cpld_inc_sampling_offset,
   .calibrate = cpld_calibrate,
   .change_mode = cpld_change_mode
};
