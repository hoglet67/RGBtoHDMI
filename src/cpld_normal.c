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
#include "rgb_to_fb.h"
#include "rpi-gpio.h"

// The number of frames to compute differences over
#define NUM_CAL_FRAMES 10

typedef struct {
   int sp_offset[NUM_OFFSETS];
   int half_px_delay; // 0 = off, 1 = on, all modes
   int divider;       // cpld divider, 6 or 8
   int full_px_delay; // 0..15
   int rate;          // 0 = normal psync rate (3 bpp), 1 = double psync rate (6 bpp)
} config_t;

// Current calibration state for mode 0..6
static config_t default_config;

// Current calibration state for mode 7
static config_t mode7_config;

// Current configuration
static config_t *config;
static int mode7;

// OSD message buffer
static char message[256];

// Per-Offset calibration metrics (i.e. errors) for mode 0..6
static int raw_metrics_default[8][NUM_OFFSETS];

// Per-offset calibration metrics (i.e. errors) for mode 7
static int raw_metrics_mode7[8][NUM_OFFSETS];

// Aggregate calibration metrics (i.e. errors summed across all offsets) for mode 0..6
static int sum_metrics_default[8]; // Last two not used

// Aggregate calibration metrics (i.e. errors summver across all offsets) for mode 7
static int sum_metrics_mode7[8];

// Error count for final calibration values for mode 0..6
static int errors_default;

// Error count for final calibration values for mode 7
static int errors_mode7;

// The CPLD version number
static int cpld_version;

// Indicated the CPLD supports the delay parameter
static int supports_delay;

// Indicated the CPLD supports the rate parameter
static int supports_rate;

// =============================================================
// Param definitions for OSD
// =============================================================


static param_t params[] = {
   { ALL_OFFSETS, "All offsets", 0,   0, 1 },
   {    A_OFFSET,    "A offset", 0,   0, 1 },
   {    B_OFFSET,    "B offset", 0,   0, 1 },
   {    C_OFFSET,    "C offset", 0,   0, 1 },
   {    D_OFFSET,    "D offset", 0,   0, 1 },
   {    E_OFFSET,    "E offset", 0,   0, 1 },
   {    F_OFFSET,    "F offset", 0,   0, 1 },
   {        HALF,        "Half", 0,   1, 1 },
   {     DIVIDER,     "Divider", 6,   8, 2 },
   {       DELAY,       "Delay", 0,  15, 1 },
   {      SIXBIT,"Six bits", 0,   1, 1 },
   {          -1,          NULL, 0,   0, 1 }
};

// =============================================================
// Private methods
// =============================================================

static void write_config(config_t *config) {
   int sp = 0;
   int scan_len = 19;
   for (int i = 0; i < NUM_OFFSETS; i++) {
      // Cycle the sample points taking account the pixel delay value
      // (if the CPLD support this, and we are in mode 7
      // delay = 0 : use ABCDEF
      // delay = 1 : use BCDEFA
      // delay = 2 : use CDEFAB
      // etc
      int offset = (supports_delay && mode7) ? (i + config->full_px_delay) % NUM_OFFSETS : i;
      sp |= (config->sp_offset[i] & 7) << (offset * 3);
   }
   if (config->half_px_delay) {
      sp |= (1 << 18);
   }
   if (supports_delay) {
      sp |= (config->full_px_delay << scan_len);
      scan_len += 4;
   }
   if (supports_rate) {
      sp |= (config->rate << scan_len);
      scan_len += 1;
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
   // Line ------
   if (mode7) {
      osd_set(line, 0, "   Mode: 7");
   } else {
      osd_set(line, 0, "   Mode: default");
   }
   line++;
   // Line ------
   sprintf(message, "Offsets: %d %d %d %d %d %d",
           config->sp_offset[0], config->sp_offset[1], config->sp_offset[2],
           config->sp_offset[3], config->sp_offset[4], config->sp_offset[5]);
   osd_set(line, 0, message);
   line++;
   // Line ------
   sprintf(message, "   Half: %d", config->half_px_delay);
   osd_set(line, 0, message);
   line++;
   // Line ------
   if (supports_delay) {
      sprintf(message, "  Delay: %d", config->full_px_delay);
      osd_set(line, 0, message);
      line++;
   }
   // Line ------
   if (supports_rate) {
      sprintf(message, "   Rate: %d", config->rate);
      osd_set(line, 0, message);
      line++;
   }
   // Line ------
   if (metric < 0) {
      sprintf(message, " Errors: unknown");
   } else {
      sprintf(message, " Errors: %d", metric);
   }
   osd_set(line, 0, message);
}

static void log_sp(config_t *config) {
   char *mp = message;
   mp += sprintf(mp, "sp_offset = %d %d %d %d %d %d; half = %d",
                 config->sp_offset[0], config->sp_offset[1], config->sp_offset[2],
                 config->sp_offset[3], config->sp_offset[4], config->sp_offset[5],
                 config->half_px_delay);
   if (supports_delay) {
      mp += sprintf(mp, "; delay = %d", config->full_px_delay);
   }
   if (supports_rate) {
      mp += sprintf(mp, "; rate = %d", config->rate);
   }
   log_info("%s", message);
}

// =============================================================
// Public methods
// =============================================================

static void cpld_init(int version) {
   cpld_version = version;
   // Setup default frame buffer params
   //
   // Nominal width should be 640x512 or 480x504, but making this a bit larger deals with two problems:
   // 1. Slight differences in the horizontal placement in the different screen modes
   // 2. Slight differences in the vertical placement due to *TV settings
   // Version 2 CPLD supports the delay parameter, and starts sampling earlier
   supports_delay = ((cpld_version >> VERSION_MAJOR_BIT) & 0x0F) >= 2;
   if (!supports_delay) {
      params[DELAY].key = -1;
   }
   supports_rate = ((cpld_version >> VERSION_MAJOR_BIT) & 0x0F) >= 3;
   if (!supports_rate) {
      params[SIXBIT].key = -1;
   }
   for (int i = 0; i < NUM_OFFSETS; i++) {
      default_config.sp_offset[i] = 0;
      mode7_config.sp_offset[i] = 0;
   }
   default_config.half_px_delay = 0;
   mode7_config.half_px_delay   = 0;
   default_config.divider       = 6;
   mode7_config.divider         = 8;
   default_config.full_px_delay = 5;   // Correct for the master
   mode7_config.full_px_delay   = 4;   // Correct for the master
   default_config.rate          = 0;
   mode7_config.rate            = 0;
   config = &default_config;
   for (int i = 0; i < 8; i++) {
      sum_metrics_default[i] = -1;
      sum_metrics_mode7[i] = -1;
      for (int j = 0; j < NUM_OFFSETS; j++) {
         raw_metrics_default[i][j] = -1;
         raw_metrics_mode7[i][j] = -1;
      }
   }
   errors_default = -1;
   errors_mode7 = -1;
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
   int *by_sample_metrics;

   int range;          // 0..5 in Modes 0..6, 0..7 in Mode 7
   int *sum_metrics;
   int *errors;

   int (*raw_metrics)[8][NUM_OFFSETS];

   if (mode7) {
      log_info("Calibrating mode: 7");
      raw_metrics = &raw_metrics_mode7;
      sum_metrics = &sum_metrics_mode7[0];
      errors      = &errors_mode7;
   } else {
      log_info("Calibrating mode: default");
      raw_metrics = &raw_metrics_default;
      sum_metrics = &sum_metrics_default[0];
      errors      = &errors_default;
   }
   range = config->divider;

   // Measure the error metrics at all possible offset values
   min_metric = INT_MAX;
   config->half_px_delay = 0;
   config->full_px_delay = 0;
   printf("INFO:                      ");
   for (int i = 0; i < NUM_OFFSETS; i++) {
      printf("%7c", 'A' + i);
   }
   printf("   total\r\n");
   for (int value = 0; value < range; value++) {
      for (int i = 0; i < NUM_OFFSETS; i++) {
         config->sp_offset[i] = value;
      }
      write_config(config);
      by_sample_metrics = diff_N_frames_by_sample(capinfo, NUM_CAL_FRAMES, mode7, elk);
      metric = 0;
      printf("INFO: value = %d: metrics = ", value);
      for (int i = 0; i < NUM_OFFSETS; i++) {
         (*raw_metrics)[value][i] = by_sample_metrics[i];
         metric += by_sample_metrics[i];
         printf("%7d", by_sample_metrics[i]);
      }
      printf("%8d\r\n", metric);
      sum_metrics[value] = metric;
      osd_sp(config, 1, metric);
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

   // If the min metric is at the limit, make use of the half pixel delay
   if (mode7 && min_metric > 0 && (min_i <= 1 || min_i >= 6)) {
      log_info("Enabling half pixel delay");
      config->half_px_delay = 1;
      min_i ^= 4;
      // Swap the metrics as well
      for (int i = 0; i < range; i++) {
         for (int j = 0; j < NUM_OFFSETS; j++)  {
            int tmp = (*raw_metrics)[i][j];
            (*raw_metrics)[i][j] = (*raw_metrics)[i ^ 4][j];
            (*raw_metrics)[i ^ 4][j] = tmp;
         }
      }
   }

   // In all modes, start with the min metric
   for (int i = 0; i < NUM_OFFSETS; i++) {
      config->sp_offset[i] = min_i;
   }
   log_sp(config);
   write_config(config);

   // If the metric is non zero, there is scope for further optimization in mode7
   if (mode7 && min_metric > 0) {
      log_info("Optimizing calibration");
      for (int i = 0; i < NUM_OFFSETS; i++) {
         // Start with current value of the sample point i
         int value = config->sp_offset[i];
         // Look up the current metric for this value
         int ref = (*raw_metrics)[value][i];
         // Loop up the metric if we decrease this by one
         int left = INT_MAX;
         if (value > 0) {
            left = (*raw_metrics)[value - 1][i];
         }
         // Look up the metric if we increase this by one
         int right = INT_MAX;
         if (value < 7) {
            right = (*raw_metrics)[value + 1][i];
         }
         // Make the actual decision
         if (left < right && left < ref) {
            config->sp_offset[i]--;
         } else if (right < left && right < ref) {
            config->sp_offset[i]++;
         }
      }
      write_config(config);
      *errors = diff_N_frames(capinfo, NUM_CAL_FRAMES, mode7, elk);
      osd_sp(config, 1, *errors);
      log_sp(config);
      log_info("Optimization complete, errors = %d", *errors);
   }

   // Determine mode 7 alignment
   if (supports_delay) {
      log_info("Aligning characters to word boundaries");
      if (mode7) {
         config->full_px_delay = analyze_mode7_alignment(capinfo);
      } else {
         config->full_px_delay = analyze_default_alignment(capinfo);
      }
      write_config(config);
   }

   // Perform a final test of errors
   log_info("Performing final test");
   *errors = diff_N_frames(capinfo, NUM_CAL_FRAMES, mode7, elk);
   osd_sp(config, 1, *errors);
   log_sp(config);
   log_info("Calibration complete, errors = %d", *errors);
}

static void update_param_range() {
   int max;
   // Set the range of the offset params based on cpld divider
   max = config->divider - 1;
   params[ALL_OFFSETS].max = max;
   params[A_OFFSET].max = max;
   params[B_OFFSET].max = max;
   params[C_OFFSET].max = max;
   params[D_OFFSET].max = max;
   params[E_OFFSET].max = max;
   params[F_OFFSET].max = max;
   for (int i = 0; i < NUM_OFFSETS; i++) {
      if (config->sp_offset[i] > max) {
         config->sp_offset[i] = max;
      }
   }
   // Finally, make surethe hardware is consistent with the current value of divider
   // Divider = 0 gives 6 clocks per pixel
   // Divider = 1 gives 8 clocks per pixel
   RPI_SetGpioValue(MODE7_PIN, config->divider == 8);
}

static void cpld_set_mode(capture_info_t *capinfo, int mode) {
   mode7 = mode;
   config = mode ? &mode7_config : &default_config;
   write_config(config);
   // Update the OSD param ranges based on the new config
   update_param_range();
   // Update the line capture code
   if (capinfo) {
      if (!mode) {
         if (capinfo->bpp == 8) {
             switch (capinfo->px_sampling) {
                    case PS_NORMAL:
                        capinfo->capture_line = capture_line_normal_8bpp_table;
                        break;
                    case PS_NORMAL_O:
                        capinfo->capture_line = capture_line_odd_8bpp_table;
                        break;
                    case PS_NORMAL_E:
                        capinfo->capture_line = capture_line_even_8bpp_table;
                        break;
                    case PS_HALF_O:
                        capinfo->capture_line = capture_line_half_odd_8bpp_table;
                        break;
                    case PS_HALF_E:
                        capinfo->capture_line = capture_line_half_even_8bpp_table;
                        break;
                    case PS_DOUBLE:
                        capinfo->capture_line = capture_line_double_8bpp_table;
                        break;
             }
         } else {
             switch (capinfo->px_sampling) {
                    case PS_NORMAL:
                        capinfo->capture_line = capture_line_normal_4bpp_table;
                        break;
                    case PS_NORMAL_O:
                        capinfo->capture_line = capture_line_odd_4bpp_table;
                        break;
                    case PS_NORMAL_E:
                        capinfo->capture_line = capture_line_even_4bpp_table;
                        break;
                    case PS_HALF_O:
                        capinfo->capture_line = capture_line_half_odd_4bpp_table;
                        break;
                    case PS_HALF_E:
                        capinfo->capture_line = capture_line_half_even_4bpp_table;
                        break;
                    case PS_DOUBLE:
                        capinfo->capture_line = capture_line_double_4bpp_table;
                        break;
             }
         }
      }
   }
}

static param_t *cpld_get_params() {
   return params;
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
   case DIVIDER:
      return config->divider;
   case DELAY:
      return config->full_px_delay;
   case SIXBIT:
      return config->rate;
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
   case DIVIDER:
      config->divider = value;
      update_param_range();
      break;
   case DELAY:
      config->full_px_delay = value;
      break;
   case SIXBIT:
      config->rate = value;
      break;
   }
   write_config(config);
}

static void cpld_show_cal_summary(int line) {
   osd_sp(config, line, mode7 ? errors_mode7 : errors_default);
}

static void cpld_show_cal_details(int line) {
   int *sum_metrics = mode7 ? sum_metrics_mode7 : sum_metrics_default;
   int range = (*sum_metrics < 0) ? 0 : config->divider;
   if (range == 0) {
      sprintf(message, "No calibration data for this mode");
      osd_set(line, 0, message);
   } else {
      for (int value = 0; value < range; value++) {
         sprintf(message, "Offset %d: Errors = %6d", value, sum_metrics[value]);
         osd_set(line + value, 0, message);
      }
   }
}

static void cpld_show_cal_raw(int line) {
   int (*raw_metrics)[8][NUM_OFFSETS] = mode7 ? &raw_metrics_mode7 : &raw_metrics_default;
   int range = ((*raw_metrics)[0][0] < 0) ? 0 : config->divider;
   if (range == 0) {
      sprintf(message, "No calibration data for this mode");
      osd_set(line, 0, message);
   } else {
      for (int value = 0; value < range; value++) {
         char *mp = message;
         mp += sprintf(mp, "%d:", value);
         for (int i = 0; i < NUM_OFFSETS; i++) {
            mp += sprintf(mp, "%6d", (*raw_metrics)[value][i]);
         }
         osd_set(line + value, 0, message);
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
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};
