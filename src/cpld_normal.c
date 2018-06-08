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
static int sp_default;

// Current calibration state for mode 7
static int sp_mode7[6];

// =============================================================
// Private methods
// =============================================================

static void write_config(int *sp_mode7, int def) {
   int i;
   int j;
   int sp = ((def & 7) << 18);
   for (i = 0; i <= 5; i++) {
      sp |= (sp_mode7[i] & 7) << (i * 3);
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
   sp_default = 4;
   for (int i = 0; i < 6; i++) {
      sp_mode7[i] = 1;
   }
}

static void cpld_calibrate(int mode7, int elk, int chars_per_line) {
   int i;
   int j;
   int min_i;
   int min_metric;
   int *rgb_metric;
   int metric;

   if (mode7) {
      log_info("Calibrating mode 7");

      min_metric = INT_MAX;
      min_i = 0;
      for (i = 0; i <= 7; i++) {
         for (j = 0; j <= 5; j++) {
            sp_mode7[j] = i;
         }
         write_config(sp_mode7, sp_default);
         rgb_metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
         metric = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
         if (metric < min_metric) {
            min_metric = metric;
            min_i = i;
         }
      }
      for (i = 0; i <= 5; i++) {
         sp_mode7[i] = min_i;
      }
      write_config(sp_mode7, sp_default);

      // If the metric is non zero, there is scope for further optimiation
      int ref = min_metric;
      if (ref > 0) {
         log_info("Optimizing calibration: mode 7: %d %d %d %d %d %d",
                  sp_mode7[0], sp_mode7[1], sp_mode7[2], sp_mode7[3], sp_mode7[4], sp_mode7[5]);
         log_debug("ref = %d", ref);
         for (i = 0; i <= 5; i++) {
            int left = INT_MAX;
            int right = INT_MAX;
            if (sp_mode7[i] > 0) {
               sp_mode7[i]--;
               write_config(sp_mode7, sp_default);
               rgb_metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
               left = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
               sp_mode7[i]++;
            }
            if (sp_mode7[i] < 7) {
               sp_mode7[i]++;
               write_config(sp_mode7, sp_default);
               rgb_metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
               right = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
               sp_mode7[i]--;
            }
            if (left < right && left < ref) {
               sp_mode7[i]--;
               ref = left;
               log_debug("nudged %d left, metric = %d", i, ref);
            } else if (right < left && right < ref) {
               sp_mode7[i]++;
               ref = right;
               log_debug("nudged %d right, metric = %d", i, ref);
            }
         }
         write_config(sp_mode7, sp_default);
      }

      log_info("Calibration complete: mode 7: %d %d %d %d %d %d",
               sp_mode7[0], sp_mode7[1], sp_mode7[2], sp_mode7[3], sp_mode7[4], sp_mode7[5]);
   } else {
      log_info("Calibrating modes 0..6");
      min_metric = INT_MAX;
      min_i = 0;
      for (i = 0; i <= 5; i++) {
         write_config(sp_mode7, i);
         rgb_metric = diff_N_frames(i, NUM_CAL_FRAMES, mode7, elk, chars_per_line);
         metric = rgb_metric[CHAN_RED] + rgb_metric[CHAN_GREEN] + rgb_metric[CHAN_BLUE];
         if (metric < min_metric) {
            min_metric = metric;
            min_i = i;
         }
      }
      sp_default = min_i;
      log_info("Setting sp_default = %d", min_i);
      write_config(sp_mode7, sp_default);
   }
}

static void cpld_change_mode(int mode7) {
   // currently nothing to do
}

cpld_t cpld_normal = {
   .name = "Normal",
   .init = cpld_init,
   .calibrate = cpld_calibrate,
   .change_mode = cpld_change_mode
};
