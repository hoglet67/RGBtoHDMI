#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include "defs.h"
#include "cpld.h"
#include "osd.h"
#include "logging.h"
#include "rpi-gpio.h"

#define RANGE 16

// The number of frames to compute differences over
#define NUM_CAL_FRAMES 10

typedef struct {
   int sp_offset;
   int h_offset;      // horizontal offset (in psync clocks)
   int v_offset;      // vertical offset (in lines)
   int h_width;       // active horizontal width (in 8-bit characters)
   int v_height;      // active vertical height (in lines)
   int fb_width;      // framebuffer width in pixels
   int fb_height;     // framebuffer height in pixels
   int clock;         // cpld clock (in Hz)
   int line_len;      // number of clocks per horizontal line
   int n_lines;       // number of horizontal lines per frame (two fields)
   int divider;       // cpld divider: 0=divide-by-6; 1 = divide-by-8
} config_t;

// Current calibration state for mode 0..6
static config_t default_config;

// Current configuration
static config_t *config;

// OSD message buffer
static char message[80];

// Per-Offset calibration metrics (i.e. errors) for mode 0..6
static int raw_metrics_default[RANGE][NUM_OFFSETS];

// Aggregate calibration metrics (i.e. errors summed across all offsets) for mode 0..6
static int sum_metrics_default[RANGE]; // Last two not used

// Error count for final calibration values for mode 0..6
static int errors_default;

// The CPLD version number
static int cpld_version;

// =============================================================
// Param definitions for OSD
// =============================================================

enum {
   // Sampling params
   OFFSET,
   // Geometry params
   H_OFFSET,
   V_OFFSET,
   H_WIDTH,
   V_HEIGHT,
   FB_WIDTH,
   FB_HEIGHT,
   CLOCK,
   LINE_LEN,
   N_LINES,
   DIVIDER,
};

static param_t default_sampling_params[] = {
   {      OFFSET,      "Offset", 0,  15 },
   {          -1,          NULL, 0,   0 }
};


static param_t default_geometry_params[] = {
   {  H_OFFSET,        "H offset",         0,        59 },
   {  V_OFFSET,        "V offset",         0,        39 },
   {   H_WIDTH,         "H width",         1,       100 },
   {  V_HEIGHT,        "V height",         1,       300 },
   {  FB_WIDTH,        "FB width",       400,       800 },
   { FB_HEIGHT,       "FB height",       320,       600 },
   {     CLOCK,      "Clock freq",  75000000, 100000000 },
   {  LINE_LEN,     "Line length",      1000,      9999 },
   {   N_LINES, "Lines per frame",       500,       699 },
   {   DIVIDER,         "Divider",         0,         1 },
   {        -1,              NULL,         0,         0 }
};

// =============================================================
// Private methods
// =============================================================

static void write_config(config_t *config) {
   int sp = 0;
   int scan_len = 4;
   sp |= config->sp_offset & 15;
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
   sprintf(message, "Offset: %d", config->sp_offset);
   osd_set(line, 0, message);
   line++;
   // Line ------
   if (metric < 0) {
      sprintf(message, " Errors: unknown");
   } else {
      sprintf(message, " Errors: %d", metric);
   }
   osd_set(line, 0, message);
   line++;
}

static void log_sp(config_t *config) {
   log_info("sp_offset = %d", config->sp_offset);
}

// =============================================================
// Public methods
// =============================================================

static void cpld_init(int version) {
   cpld_version = version;
   default_config.h_offset  =       19;
   default_config.v_offset  =       11;
   default_config.h_width   =       76;
   default_config.v_height  =      240;
   default_config.fb_width  =      608;
   default_config.fb_height =      480;
   default_config.clock     = 57272720;
   default_config.line_len  =     3648;
   default_config.n_lines   =      524;
   default_config.divider   =        0;
   default_config.sp_offset =        2;
   config = &default_config;
   for (int i = 0; i < RANGE; i++) {
      sum_metrics_default[i] = -1;
      for (int j = 0; j < NUM_OFFSETS; j++) {
         raw_metrics_default[i][j] = -1;
      }
   }
   errors_default = -1;
}

static int cpld_get_version() {
   return cpld_version;
}

static int sum_channels(int *rgb) {
   return rgb[CHAN_RED] + rgb[CHAN_GREEN] + rgb[CHAN_BLUE];
}

static void cpld_calibrate(capture_info_t *capinfo, int elk) {
   int min_i = 0;
   int metric;         // this is a point value (at one sample offset)
   int min_metric;
   int win_metric;     // this is a windowed value (over three sample offsets)
   int min_win_metric;
   int *rgb_metric;

   int range;          // 0..5 in Modes 0..6, 0..7 in Mode 7
   int *sum_metrics;
   int *errors;

   int (*raw_metrics)[RANGE][NUM_OFFSETS];

   log_info("Calibrating...");
   range       = RANGE;
   raw_metrics = &raw_metrics_default;
   sum_metrics = &sum_metrics_default[0];
   errors      = &errors_default;

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
      rgb_metric = diff_N_frames_by_sample(capinfo, NUM_CAL_FRAMES, 0, elk);
      metric = 0;
      printf("INFO: value = %d: metrics = ", value);
      for (int i = 0; i < NUM_OFFSETS; i++) {
         int offset_metric = sum_channels(rgb_metric + i * NUM_CHANNELS);
         (*raw_metrics)[value][i] = offset_metric;
         metric += offset_metric;
         printf("%7d", offset_metric);
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

   // In all modes, start with the min metric
   config->sp_offset = min_i;
   log_sp(config);
   write_config(config);

   // Perform a final test of errors
   log_info("Performing final test");
   rgb_metric = diff_N_frames(capinfo, NUM_CAL_FRAMES, 0, elk);
   *errors = sum_channels(rgb_metric);
   osd_sp(config, 1, *errors);
   log_sp(config);
   log_info("Calibration complete, errors = %d", *errors);
}

static void cpld_set_mode(int mode) {
   config = &default_config;
   write_config(config);
   // The "mode7" pin on the CPLD switches between 6 clocks per
   // pixel (modes 0..6) and 8 clocks per pixel (mode 7)
   RPI_SetGpioValue(MODE7_PIN, config->divider);
}

static void cpld_get_fb_params(capture_info_t *capinfo) {
   capinfo->h_offset       = config->h_offset;
   capinfo->v_offset       = config->v_offset;
   capinfo->chars_per_line = config->h_width;
   capinfo->nlines         = config->v_height;
   capinfo->width          = config->fb_width;
   capinfo->height         = config->fb_height;
}

static void cpld_get_clk_params(clk_info_t *clkinfo) {
   clkinfo->clock        = config->clock;
   clkinfo->line_len     = config->line_len;
   clkinfo->n_lines      = config->n_lines;
}

static param_t *cpld_get_sampling_params() {
   return default_sampling_params;
}

static param_t *cpld_get_geometry_params() {
   return default_geometry_params;
}

static int cpld_get_value(int num) {
   switch (num) {
   case OFFSET:
      return config->sp_offset;
   case H_OFFSET:
      return config->h_offset;
   case V_OFFSET:
      return config->v_offset;
   case H_WIDTH:
      return config->h_width;
   case V_HEIGHT:
      return config->v_height;
   case FB_WIDTH:
      return config->fb_width;
   case FB_HEIGHT:
      return config->fb_height;
   case CLOCK:
      return config->clock;
   case LINE_LEN:
      return config->line_len;
   case N_LINES:
      return config->n_lines;
   case DIVIDER:
      return config->divider;
   }
   return 0;
}

static void cpld_set_value(int num, int value) {
   switch (num) {
   case OFFSET:
      config->sp_offset = value;
      break;
   case H_OFFSET:
      config->h_offset = value;
      break;
   case V_OFFSET:
      config->v_offset = value;
      break;
   case H_WIDTH:
      config->h_width = value;
      break;
   case V_HEIGHT:
      config->v_height = value;
      break;
   case FB_WIDTH:
      config->fb_width = value;
      break;
   case FB_HEIGHT:
      config->fb_height = value;
      break;
   case CLOCK:
      config->clock = value;
      break;
   case LINE_LEN:
      config->line_len = value;
      break;
   case N_LINES:
      config->n_lines = value;
      break;
   case DIVIDER:
      config->divider = value;
      break;
   }
   write_config(config);
}

static void cpld_show_cal_summary(int line) {
   osd_sp(config, line, errors_default);
}

static void cpld_show_cal_details(int line) {
   int *sum_metrics = sum_metrics_default;
   int range = (*sum_metrics < 0) ? 0 : RANGE;
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
   int (*raw_metrics)[RANGE][NUM_OFFSETS] = &raw_metrics_default;
   int range = ((*raw_metrics)[0][0] < 0) ? 0 : RANGE;
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

cpld_t cpld_atom = {
   .name = "Atom",
   .init = cpld_init,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .get_fb_params = cpld_get_fb_params,
   .get_clk_params = cpld_get_clk_params,
   .get_sampling_params = cpld_get_sampling_params,
   .get_geometry_params = cpld_get_geometry_params,
   .get_value = cpld_get_value,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};
