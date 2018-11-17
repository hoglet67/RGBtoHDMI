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

// The number of frames to compute differences over
#define NUM_CAL_FRAMES 10

typedef struct {
   int sp_offset[NUM_OFFSETS];
   int half_px_delay; // 0 = off, 1 = on, all modes
   int full_px_delay; // 0..15, mode 7 only
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

// Current calibration state for mode 7
static config_t mode7_config;

// Current configuration
static config_t *config;
static int mode7;

// OSD message buffer
static char message[80];

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

// Indicated the CPLD supports the mode 7 delay parameter
static int supports_delay;

// =============================================================
// Param definitions for OSD
// =============================================================

enum {
   // Sampling params
   ALL_OFFSETS,
   A_OFFSET,
   B_OFFSET,
   C_OFFSET,
   D_OFFSET,
   E_OFFSET,
   F_OFFSET,
   HALF,
   DELAY,
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
   { ALL_OFFSETS, "All offsets", 0,   5 },
   {    A_OFFSET,    "A offset", 0,   5 },
   {    B_OFFSET,    "B offset", 0,   5 },
   {    C_OFFSET,    "C offset", 0,   5 },
   {    D_OFFSET,    "D offset", 0,   5 },
   {    E_OFFSET,    "E offset", 0,   5 },
   {    F_OFFSET,    "F offset", 0,   5 },
   {        HALF,        "Half", 0,   1 },
   {       DELAY,       "Delay", 0,  15 },
   {          -1,          NULL, 0,   0 }
};

static param_t mode7_sampling_params[] = {
   { ALL_OFFSETS, "All offsets", 0,   7 },
   {    A_OFFSET,    "A offset", 0,   7 },
   {    B_OFFSET,    "B offset", 0,   7 },
   {    C_OFFSET,    "C offset", 0,   7 },
   {    D_OFFSET,    "D offset", 0,   7 },
   {    E_OFFSET,    "E offset", 0,   7 },
   {    F_OFFSET,    "F offset", 0,   7 },
   {        HALF,        "Half", 0,   1 },
   {       DELAY,       "Delay", 0,  15 },
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

static param_t mode7_geometry_params[] = {
   {  H_OFFSET,        "H offset",         0,        39 },
   {  V_OFFSET,        "V offset",         0,        39 },
   {   H_WIDTH,         "H width",         1,       100 },
   {  V_HEIGHT,        "V height",         1,       300 },
   {  FB_WIDTH,        "FB width",       400,       800 },
   { FB_HEIGHT,       "FB height",       480,       600 },
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
   // Line ------
   if (mode7) {
      osd_set(line, 0, "   Mode: 7");
   } else {
      osd_set(line, 0, "   Mode: 0..6");
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
   if (mode7 && supports_delay) {
      sprintf(message, "  Delay: %d", config->full_px_delay);
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
   line++;
}

static void log_sp(config_t *config) {
   if (supports_delay) {
      log_info("sp_offset = %d %d %d %d %d %d; half = %d; delay = %d",
               config->sp_offset[0], config->sp_offset[1], config->sp_offset[2],
               config->sp_offset[3], config->sp_offset[4], config->sp_offset[5],
               config->half_px_delay, config->full_px_delay);
   } else {
      log_info("sp_offset = %d %d %d %d %d %d; half = %d",
               config->sp_offset[0], config->sp_offset[1], config->sp_offset[2],
               config->sp_offset[3], config->sp_offset[4], config->sp_offset[5],
               config->half_px_delay);
   }
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
   mode7_config.h_offset    =  24;
   mode7_config.v_offset    =  21;
   mode7_config.h_width     =  63;
   mode7_config.v_height    = 270;
   mode7_config.fb_width    = 504;
   mode7_config.fb_height   = 540;
   mode7_config.clock       =   96000000;
   mode7_config.line_len    =   96 * 64;
   mode7_config.n_lines     = 625;
   mode7_config.divider     =   1;
   default_config.h_offset  =  32;
   default_config.v_offset  =  21;
   default_config.h_width   =  83;
   default_config.v_height  = 270;
   default_config.fb_width  = 672;
   default_config.fb_height = 540;
   default_config.clock     =   96000000;
   default_config.line_len  =   96 * 64;
   default_config.n_lines   = 625;
   default_config.divider   =   0;
   // Version 2 CPLD supports the delay parameter, and starts sampling earlier
   supports_delay = ((cpld_version >> VERSION_MAJOR_BIT) & 0x0F) >= 2;
   if (!supports_delay) {
      mode7_sampling_params[DELAY].name = NULL;
      default_sampling_params[DELAY].name = NULL;
      mode7_config.h_offset = 0;
      default_config.h_offset = 0;
   }
   for (int i = 0; i < NUM_OFFSETS; i++) {
      default_config.sp_offset[i] = 0;
      mode7_config.sp_offset[i] = 0;
   }
   default_config.half_px_delay = 0;
   mode7_config.half_px_delay = 0;
   default_config.full_px_delay = 0;
   mode7_config.full_px_delay = 4;   // Correct for the master
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

   int (*raw_metrics)[8][NUM_OFFSETS];

   if (mode7) {
      log_info("Calibrating mode 7");
      range       = 8;
      raw_metrics = &raw_metrics_mode7;
      sum_metrics = &sum_metrics_mode7[0];
      errors      = &errors_mode7;
   } else {
      log_info("Calibrating modes 0..6");
      range       = 6;
      raw_metrics = &raw_metrics_default;
      sum_metrics = &sum_metrics_default[0];
      errors      = &errors_default;
   }

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
      rgb_metric = diff_N_frames_by_sample(capinfo, NUM_CAL_FRAMES, mode7, elk);
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
      rgb_metric = diff_N_frames(capinfo, NUM_CAL_FRAMES, mode7, elk);
      *errors = sum_channels(rgb_metric);
      osd_sp(config, 1, *errors);
      log_sp(config);
      log_info("Optimization complete, errors = %d", *errors);
   }

   // Determine mode 7 alignment
   if (mode7 && supports_delay) {
      log_info("Aligning characters to word boundaries");
      config->full_px_delay = analyze_mode7_alignment(capinfo);
      write_config(config);
   }

   // Perform a final test of errors
   log_info("Performing final test");
   rgb_metric = diff_N_frames(capinfo, NUM_CAL_FRAMES, mode7, elk);
   *errors = sum_channels(rgb_metric);
   osd_sp(config, 1, *errors);
   log_sp(config);
   log_info("Calibration complete, errors = %d", *errors);
}

static void cpld_set_mode(int mode) {
   mode7 = mode;
   config = mode ? &mode7_config : &default_config;
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
   if (mode7) {
      return mode7_sampling_params;
   } else {
      return default_sampling_params;
   }
}

static param_t *cpld_get_geometry_params() {
   if (mode7) {
      return mode7_geometry_params;
   } else {
      return default_geometry_params;
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
   case DELAY:
      return config->full_px_delay;
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
   case DELAY:
      config->full_px_delay = value;
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
   osd_sp(config, line, mode7 ? errors_mode7 : errors_default);
}

static void cpld_show_cal_details(int line) {
   int *sum_metrics = mode7 ? sum_metrics_mode7 : sum_metrics_default;
   int range = (*sum_metrics < 0) ? 0 : mode7 ? 8 : 6;
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
   int range = ((*raw_metrics)[0][0] < 0) ? 0 : mode7 ? 8 : 6;
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
