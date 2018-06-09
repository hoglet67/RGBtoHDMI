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
static int default_sp_base[NUM_CHANNELS];
static int default_sp_offset[NUM_OFFSETS];

// Current calibration state for mode 7
static int mode7_sp_base[NUM_CHANNELS];
static int mode7_sp_offset[NUM_OFFSETS];

// =============================================================
// Private methods
// =============================================================

static void write_config(int *sp_base, int *sp_offset) {
   int sp = 0;
   for (int i = 0; i < NUM_CHANNELS; i++) {
      sp |= (sp_base[i] & 7) << (i * 3);
   }
   for (int i = 0; i < NUM_OFFSETS; i++) {
      switch (sp_offset[i]) {
      case -1:
         sp |= 3 << (i * 2 + 9);
         break;
      case 1:
         sp |= 1 << (i * 2 + 9);
         break;
      }
   }
   for (int i = 0; i < 21; i++) {
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

static void log_sp(int *sp_base, int *sp_offset) {
   log_info("sp_base = %d %d %d, sp_offset = %d %d %d %d %d %d",
            sp_base[CHAN_RED], sp_base[CHAN_GREEN], sp_base[CHAN_BLUE],
            sp_offset[0], sp_offset[1], sp_offset[2], sp_offset[3], sp_offset[4], sp_offset[5]);
}

// =============================================================
// Public methods
// =============================================================

static void cpld_init() {
   for (int i = 0; i < NUM_CHANNELS; i++) {
      mode7_sp_base[i] = 0;
   }
   for (int i = 0; i < NUM_OFFSETS; i++) {
      default_sp_offset[i] = 0;
      mode7_sp_offset[i] = 0;
   }
}

static void cpld_calibrate(int mode7, int elk, int chars_per_line) {
   int min_i[NUM_CHANNELS];
   int min_metric[NUM_CHANNELS];
   int *metric;
   int *sp_base;
   int *sp_offset;

   if (mode7) {
      log_info("Calibrating mode 7");
      sp_base = mode7_sp_base;
      sp_offset = mode7_sp_offset;
   } else {
      log_info("Calibrating modes 0..6");
      sp_base = default_sp_base;
      sp_offset = default_sp_offset;
   }

   for (int i = 0; i < NUM_CHANNELS; i++) {
      min_metric[i] = INT_MAX;
      min_i[i] = 0;
   }

   for (int i = 0; i < NUM_OFFSETS; i++) {
      sp_offset[i] = 0;
   }

   for (int i = 0; i <= (mode7 ? 7 : 5); i++) {
      for (int j = 0; j < NUM_CHANNELS; j++) {
         sp_base[j] = i;
      }
      write_config(sp_base, sp_offset);
      metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
      log_info("offset = %d: metric = %d %d %d\n", i, metric[CHAN_RED], metric[CHAN_GREEN], metric[CHAN_BLUE]);
      for (int j = 0; j < NUM_CHANNELS; j++) {
         if (metric[j] < min_metric[j]) {
            min_metric[j] = metric[j];
            min_i[j] = i;
         }
      }
   }
   for (int j = 0; j < NUM_CHANNELS; j++) {
      sp_base[j] = min_i[j];
   }

   // If the metric is non zero, there is scope for further optimiation in mode 7
   int ref = min_metric[CHAN_RED] + min_metric[CHAN_GREEN] + min_metric[CHAN_BLUE];
   if (mode7 && ref > 0) {
      log_info("Optimizing calibration: sp_base = %d %d %d",
               sp_base[CHAN_RED], sp_base[CHAN_GREEN], sp_base[CHAN_BLUE]);
      log_debug("ref = %d", ref);
      for (int i = 0; i < NUM_OFFSETS; i++) {
         int left = INT_MAX;
         int right = INT_MAX;
         if (sp_base[CHAN_RED] > 0 && sp_base[CHAN_GREEN] > 0 && sp_base[CHAN_BLUE] > 0) {
            sp_offset[i] = -1;
            write_config(sp_base, sp_offset);
            metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
            left = metric[CHAN_RED] + metric[CHAN_GREEN] + metric[CHAN_BLUE];
         }
         if (sp_base[CHAN_RED] < 7 && sp_base[CHAN_GREEN] < 7 && sp_base[CHAN_BLUE] < 7) {
            sp_offset[i] = 1;
            write_config(sp_base, sp_offset);
            metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
            right = metric[CHAN_RED] + metric[CHAN_GREEN] + metric[CHAN_BLUE];
         }
         if (left < right && left < ref) {
            sp_offset[i] = -1;
            ref = left;
            log_info("nudged %d left, metric = %d", i, ref);
         } else if (right < left && right < ref) {
            sp_offset[i] = 1;
            ref = right;
            log_info("nudged %d right, metric = %d", i, ref);
         } else {
            sp_offset[i] = 0;
         }
      }
   }

   log_info("Calibration complete");
   log_sp(sp_base, sp_offset);
   write_config(sp_base, sp_offset);
}

static void cpld_change_mode(int mode7) {
   if (mode7) {
      write_config(mode7_sp_base, mode7_sp_offset);
   } else {
      write_config(default_sp_base, default_sp_offset);
   }
}

static void cpld_inc_sampling_base(int mode7) {
   int *sp_base = mode7 ? mode7_sp_base : default_sp_base;
   int *sp_offset = mode7 ? mode7_sp_offset : default_sp_offset;
   int limit = mode7 ? 8 : 6;
   for (int i = 0; i < NUM_CHANNELS; i++) {
      sp_base[i] = (sp_base[i] + 1) % limit;
   }
   log_sp(sp_base, sp_offset);
   write_config(sp_base, sp_offset);
}

static void cpld_inc_sampling_offset(int mode7) {
   int *sp_base = mode7 ? mode7_sp_base : default_sp_base;
   int *sp_offset = mode7 ? mode7_sp_offset : default_sp_offset;
   for (int i = 0; i < NUM_OFFSETS; i++) {
      sp_offset[i] = ((sp_offset[i] + 2) % 3) - 1;
   }
   log_sp(sp_base, sp_offset);
   write_config(sp_base, sp_offset);
}

cpld_t cpld_alternative = {
   .name = "Alternative",
   .init = cpld_init,
   .inc_sampling_base = cpld_inc_sampling_base,
   .inc_sampling_offset = cpld_inc_sampling_offset,
   .calibrate = cpld_calibrate,
   .change_mode = cpld_change_mode
};
