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
static int default_sp_base[3] = {0, 0, 0};
static int default_sp_offset[6] = {0, 0, 0, 0, 0, 0};

// Current calibration state for mode 7
static int mode7_sp_base[3] = {0, 0, 0};
static int mode7_sp_offset[6] = {0, 0, 0, 0, 0, 0};

// =============================================================
// Private methods
// =============================================================

static void write_config(int *sp_base, int *sp_offset) {
   int i;
   int j;
   int sp = 0;
   for (i = 0; i <= 2; i++) {
      sp |= (sp_base[i] & 7) << (i * 3);
   }
   for (i = 0; i <= 5; i++) {
      switch (sp_offset[i]) {
      case -1:
         sp |= 3 << (i * 2 + 9);
         break;
      case 1:
         sp |= 1 << (i * 2 + 9);
         break;
      }
   }
   for (i = 0; i <= 20; i++) {
      RPI_SetGpioValue(SP_DATA_PIN, sp & 1);
      for (j = 0; j < 1000; j++);
      RPI_SetGpioValue(SP_CLKEN_PIN, 1);
      for (j = 0; j < 100; j++);
      RPI_SetGpioValue(SP_CLK_PIN, 0);
      RPI_SetGpioValue(SP_CLK_PIN, 1);
      for (j = 0; j < 100; j++);
      RPI_SetGpioValue(SP_CLKEN_PIN, 0);
      for (j = 0; j < 1000; j++);
      sp >>= 1;
   }
   RPI_SetGpioValue(SP_DATA_PIN, 0);
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
   int min_i[3];
   int min_metric[3];
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
      min_i[i] = 0;
      min_metric[i] = INT_MAX;
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
      for (int j = 0; j < 3; j++) {
         if (metric[j] < min_metric[j]) {
            min_metric[j] = metric[j];
            min_i[j] = i;
         }
      }
   }
   for (int j = 0; j < 3; j++) {
      sp_base[j] = min_i[j];
   }

   // If the metric is non zero, there is scope for further optimiation
   int ref = min_metric[0] + min_metric[1] + min_metric[2];
   if (mode7 && ref > 0) {
      log_info("Optimizing calibration: sp_base = %d %d %d",
               sp_base[0], sp_base[1], sp_base[2]);
      log_debug("ref = %d", ref);
      for (int i = 0; i <= 5; i++) {
         int left = INT_MAX;
         int right = INT_MAX;
         if (sp_base[0] > 0 && sp_base[1] > 0 && sp_base[2] > 0) {
            sp_offset[i] = -1;
            write_config(sp_base, sp_offset);
            metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
            left = metric[0] + metric[1] + metric[2];
         }
         if (sp_base[0] < 7 && sp_base[1] < 7 && sp_base[2] < 7) {
            sp_offset[i] = 1;
            write_config(sp_base, sp_offset);
            metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
            right = metric[0] + metric[1] + metric[2];
         }
         sp_offset[i] = 0;
         if (left < right && left < ref) {
            sp_offset[i] = -1;
            ref = left;
            log_debug("nudged %d left, metric = %d", i, ref);
         } else if (right < left && right < ref) {
            sp_offset[i] = 1;
            ref = right;
            log_debug("nudged %d right, metric = %d", i, ref);
         }
      }
   }

   log_info("Calibration complete: sp_base = %d %d %d, sp_offset = %d %d %d %d %d %d",
            sp_base[0], sp_base[1], sp_base[2],
            sp_offset[0], sp_offset[1], sp_offset[2], sp_offset[3], sp_offset[4], sp_offset[5]);
   write_config(sp_base, sp_offset);
}

static void cpld_change_mode(int mode7) {
   if (mode7) {
      write_config(mode7_sp_base, mode7_sp_offset);
   } else {
      write_config(default_sp_base, default_sp_offset);
   }
}

cpld_t cpld_alternative = {
   .name = "Alternative",
   .init = cpld_init,
   .calibrate = cpld_calibrate,
   .change_mode = cpld_change_mode
};
