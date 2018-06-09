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

// Current calibration state for mode 0..6
static int default_sp_offset[NUM_OFFSETS];

// Current calibration state for mode 7
static int mode7_sp_offset[NUM_OFFSETS];

// =============================================================
// Private methods
// =============================================================

static void write_config(int *sp_offset) {
   int sp = 0;
   for (int i = 0; i < NUM_OFFSETS; i++) {
      sp |= (sp_offset[i] & 7) << (i * 3);
   }
   for (int i = 0; i < 18; i++) {
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

static void log_sp(int *sp_offset) {
   log_info("sp_offset = %d %d %d %d %d %d",
            sp_offset[0], sp_offset[1], sp_offset[2], sp_offset[3], sp_offset[4], sp_offset[5]);
}

// =============================================================
// Public methods
// =============================================================

static void cpld_init() {
   for (int i = 0; i < NUM_OFFSETS; i++) {
      default_sp_offset[i] = 0;
      mode7_sp_offset[i] = 0;
   }
}

static void cpld_calibrate(int mode7, int elk, int chars_per_line) {
   int min_i;
   int min_metric;
   int *rgb_metric;
   int metric;
   int *sp_offset;

   if (mode7) {
      log_info("Calibrating mode 7");
      sp_offset = mode7_sp_offset;
   } else {
      log_info("Calibrating modes 0..6");
      sp_offset = default_sp_offset;
   }

   min_metric = INT_MAX;
   min_i = 0;
   for (int i = 0; i <= (mode7 ? 7 : 5); i++) {
      for (int j = 0; j < NUM_OFFSETS; j++) {
         sp_offset[j] = i;
      }
      write_config(sp_offset);
      rgb_metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
      metric = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
      log_info("offset = %d: metric = %d\n", i, metric);
      if (metric < min_metric) {
         min_metric = metric;
         min_i = i;
      }
   }
   for (int i = 0; i < NUM_OFFSETS; i++) {
      sp_offset[i] = min_i;
   }
   write_config(sp_offset);

   // If the metric is non zero, there is scope for further optimiation in mode7
   int ref = min_metric;
   if (mode7 && ref > 0) {
      log_info("Optimizing calibration");
      log_sp(sp_offset);
      log_debug("ref = %d", ref);
      for (int i = 0; i < NUM_CHANNELS; i++) {
         int left = INT_MAX;
         int right = INT_MAX;
         if (sp_offset[i] > 0) {
            sp_offset[i]--;
            write_config(sp_offset);
            rgb_metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
            left = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
            sp_offset[i]++;
         }
         if (sp_offset[i] < 7) {
            sp_offset[i]++;
            write_config(sp_offset);
            rgb_metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
            right = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
            sp_offset[i]--;
         }
         if (left < right && left < ref) {
            sp_offset[i]--;
            ref = left;
            log_info("nudged %d left, metric = %d", i, ref);
         } else if (right < left && right < ref) {
            sp_offset[i]++;
            ref = right;
            log_info("nudged %d right, metric = %d", i, ref);
         }
      }
   }
   log_info("Calibration complete:");
   log_sp(sp_offset);
   write_config(sp_offset);
}

static void cpld_change_mode(int mode7) {
   if (mode7) {
      write_config(mode7_sp_offset);
   } else {
      write_config(default_sp_offset);
   }
}

static void cpld_inc_sampling_base(int mode7) {
   // currently nothing to to, as design only has offsets
}

static void cpld_inc_sampling_offset(int mode7) {
   int *sp_offset = mode7 ? mode7_sp_offset : default_sp_offset;
   int limit = mode7 ? 8 : 6;
   for (int i = 0; i < NUM_OFFSETS; i++) {
      sp_offset[i] = (sp_offset[i] + 1) % limit;
   }
   log_sp(sp_offset);
   write_config(sp_offset);
}

cpld_t cpld_normal = {
   .name = "Normal",
   .init = cpld_init,
   .inc_sampling_base = cpld_inc_sampling_base,
   .inc_sampling_offset = cpld_inc_sampling_offset,
   .calibrate = cpld_calibrate,
   .change_mode = cpld_change_mode
};
