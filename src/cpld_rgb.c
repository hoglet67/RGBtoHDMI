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
   int cpld_setup_mode;
   int sp_offset[NUM_OFFSETS];
   int half_px_delay; // 0 = off, 1 = on, all modes
   int divider;       // cpld divider, 6 or 8
   int full_px_delay; // 0..15
   int rate;          // 0 = normal psync rate (3 bpp), 1 = double psync rate (6 bpp), 2 = sub-sample (even), 3=sub-sample(odd)
   int mux;           // 0 = direct, 1 = via the 74LS08 buffer
   int dac_a;
   int dac_b;
   int dac_c;
   int dac_d;
   int dac_e;
   int dac_f;
   int dac_g;
   int dac_h;
} config_t;

static const char *rate_names[] = {
   "Normal (3bpp)",
   "Double (6bpp)",
   "Half-Odd (3bpp)",
   "Half-Even (3bpp)"
};

// Current calibration state for mode 0..6
static config_t default_config;

// Current calibration state for mode 7
static config_t mode7_config;

// Current configuration
static config_t *config;
static int mode7;
static int frontend = 0;

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

// Indicates the CPLD supports the delay parameter
static int supports_delay;

// Indicates the CPLD supports the rate parameter
static int supports_rate;  // 0 = no, 1 = 1-bit rate param, 2 = 1-bit rate param

// Indicates the CPLD supports the invert parameter
static int supports_invert;

// Indicates the CPLD supports vsync on psync when version = 0
static int supports_vsync;

// Indicates the CPLD supports separate vsync & hsync
static int supports_separate;

// Indicates the Analog frontent interface is present
static int supports_analog;

// invert state (not part of config)
static int invert = 0;
// =============================================================
// Param definitions for OSD
// =============================================================

enum {
   // Sampling params
   CPLD_SETUP_MODE,
   ALL_OFFSETS,
   A_OFFSET,
   B_OFFSET,
   C_OFFSET,
   D_OFFSET,
   E_OFFSET,
   F_OFFSET,
   HALF,
   DIVIDER,
   DELAY,
   RATE,
   MUX,
   DAC_A,
   DAC_B,
   DAC_C,
   DAC_D,
   DAC_E,
   DAC_F,
   DAC_G,
   DAC_H
};


static const char *cpld_setup_names[] = {
   "Normal",
   "Set Delay"
};

enum {
   CPLD_SETUP_NORMAL,
   CPLD_SETUP_DELAY,
   NUM_CPLD_SETUP
};

static param_t params[] = {
   {  CPLD_SETUP_MODE,  "Setup Mode", "setup_mode", 0,NUM_CPLD_SETUP-1, 1 },
   { ALL_OFFSETS, "All Offsets", "all_offsets", 0,   0, 1 },
   {    A_OFFSET,    "A Offset",    "a_offset", 0,   0, 1 },
   {    B_OFFSET,    "B Offset",    "b_offset", 0,   0, 1 },
   {    C_OFFSET,    "C Offset",    "c_offset", 0,   0, 1 },
   {    D_OFFSET,    "D Offset",    "d_offset", 0,   0, 1 },
   {    E_OFFSET,    "E Offset",    "e_offset", 0,   0, 1 },
   {    F_OFFSET,    "F Offset",    "f_offset", 0,   0, 1 },
   {        HALF,        "Half",        "half", 0,   1, 1 },
   {     DIVIDER,     "Divider",     "divider", 6,   8, 2 },
   {       DELAY,       "Delay",       "delay", 0,  15, 1 },
   {        RATE, "Sample Mode", "sample_mode", 0,   3, 1 },
   {         MUX,   "Input Mux",   "input_mux", 0,   1, 1 },
   {       DAC_A,  "DAC-A (G/Y Hi)",   "dac_a", 0, 255, 1 },
   {       DAC_B,  "DAC-B (G/Y Lo)",   "dac_b", 0, 255, 1 },
   {       DAC_C,  "DAC-C (RB/UV Hi)", "dac_c", 0, 255, 1 },
   {       DAC_D,  "DAC-D (RB/UV Lo)", "dac_d", 0, 255, 1 },
   {       DAC_E,  "DAC-E (G/Y Sync)", "dac_f", 0, 255, 1 },
   {       DAC_F,  "DAC-F (Sync)",     "dac_g", 0, 255, 1 },
   {       DAC_G,  "DAC-G (Terminate)","dac_g", 0, 255, 1 },
   {       DAC_H,  "DAC-H (G/Y Clamp)","dac_h", 0, 255, 1 },
   {          -1,          NULL,          NULL, 0,   0, 1 }
};

// =============================================================
// Private methods
// =============================================================

static void sendDAC(int dac, int value)
{
    int old_dac = -1;
    switch (dac) {
        case 2:
            old_dac = 0;
        break;
        case 3:
            old_dac = 1;
        break;
        case 5:
            old_dac = 2;
        break;
        case 6:
            old_dac = 3;
            if (value >=1) value = 255;
        break;
        default:
        break;
    }

    switch (frontend) {
        case FRONTEND_ANALOG_5259:  // max5259
        {
            int packet = (dac << 11) | 0x600 | value;
            RPI_SetGpioValue(STROBE_PIN, 0);
            for (int i = 0; i < 16; i++) {
                RPI_SetGpioValue(SP_CLKEN_PIN, 0);
                RPI_SetGpioValue(SP_DATA_PIN, (packet >> 15) & 1);
                delay_in_arm_cycles(500);
                RPI_SetGpioValue(SP_CLKEN_PIN, 1);
                delay_in_arm_cycles(500);
                packet <<= 1;
            }
            RPI_SetGpioValue(STROBE_PIN, 1);
            RPI_SetGpioValue(SP_DATA_PIN, 0);
        }
        break;

        case FRONTEND_ANALOG_UA1:  // tlc5260 or tlv5260
        {
            if (old_dac >= 0) {
                int packet = old_dac << 9 | value;
                RPI_SetGpioValue(STROBE_PIN, 1);
                delay_in_arm_cycles(1000);
                for (int i = 0; i < 11; i++) {
                    RPI_SetGpioValue(SP_CLKEN_PIN, 1);
                    RPI_SetGpioValue(SP_DATA_PIN, (packet >> 10) & 1);
                    delay_in_arm_cycles(500);
                    RPI_SetGpioValue(SP_CLKEN_PIN, 0);
                    delay_in_arm_cycles(500);
                    packet <<= 1;
                }
                RPI_SetGpioValue(STROBE_PIN, 0);
                delay_in_arm_cycles(500);
                RPI_SetGpioValue(STROBE_PIN, 1);
                RPI_SetGpioValue(SP_DATA_PIN, 0);
            }
        }
        break;

        case FRONTEND_ANALOG_UB1:  // dac084s085
        {
            if (old_dac >= 0) {
                int packet = (old_dac << 14) | 0x1000 | (value << 4);
                RPI_SetGpioValue(STROBE_PIN, 1);
                delay_in_arm_cycles(500);
                RPI_SetGpioValue(STROBE_PIN, 0);
                for (int i = 0; i < 16; i++) {
                    RPI_SetGpioValue(SP_CLKEN_PIN, 1);
                    RPI_SetGpioValue(SP_DATA_PIN, (packet >> 15) & 1);
                    delay_in_arm_cycles(500);
                    RPI_SetGpioValue(SP_CLKEN_PIN, 0);
                    delay_in_arm_cycles(500);
                    packet <<= 1;
                }
                RPI_SetGpioValue(SP_DATA_PIN, 0);
            }
        }
        break;
    }

}



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
      // We only ever make use of 2 bits of delay, even in CPLDs that use more
      sp |= ((config->full_px_delay & 3) << scan_len);
      scan_len += supports_delay; // 2 or 4 depending on the CPLD version
   }
   if (supports_rate) {
      sp |= (config->rate << scan_len);
      scan_len += supports_rate;  // 1 or 2 depending on the CPLD version
   }
   if (supports_invert) {
      sp |= (invert << scan_len);
      scan_len += 1;
   }
   for (int i = 0; i < scan_len; i++) {
      RPI_SetGpioValue(SP_DATA_PIN, sp & 1);
      delay_in_arm_cycles(100);
      RPI_SetGpioValue(SP_CLKEN_PIN, 1);
      delay_in_arm_cycles(100);
      RPI_SetGpioValue(SP_CLK_PIN, 0);
      delay_in_arm_cycles(100);
      RPI_SetGpioValue(SP_CLK_PIN, 1);
      delay_in_arm_cycles(100);
      RPI_SetGpioValue(SP_CLKEN_PIN, 0);
      delay_in_arm_cycles(100);
      sp >>= 1;
   }

   if (supports_analog) {

      sendDAC(0, config->dac_a);
      sendDAC(1, config->dac_b);
      sendDAC(2, config->dac_c);
      sendDAC(3, config->dac_d);
      sendDAC(4, config->dac_e);
      sendDAC(5, config->dac_f);
      sendDAC(6, config->dac_g);
      sendDAC(7, config->dac_h);

      RPI_SetGpioValue(SP_CLKEN_PIN, config->dac_g > 0 ? 1 : 0);
   }

   RPI_SetGpioValue(SP_DATA_PIN, 0);
   RPI_SetGpioValue(MUX_PIN, config->mux);

}

static int osd_sp(config_t *config, int line, int metric) {
   // Line ------
   if (mode7) {
      osd_set(line, 0, "           Mode: 7");
   } else {
      osd_set(line, 0, "           Mode: default");
   }
   line++;
   // Line ------
   sprintf(message, "        Offsets: %d %d %d %d %d %d",
           config->sp_offset[0], config->sp_offset[1], config->sp_offset[2],
           config->sp_offset[3], config->sp_offset[4], config->sp_offset[5]);
   osd_set(line, 0, message);
   line++;
   // Line ------
   sprintf(message, "           Half: %d", config->half_px_delay);
   osd_set(line, 0, message);
   line++;
   // Line ------
   if (supports_delay) {
      sprintf(message, "          Delay: %d", config->full_px_delay);
      osd_set(line, 0, message);
      line++;
   }
   // Line ------
   if (supports_rate) {
      sprintf(message, "           Rate: %d", config->rate);
      osd_set(line, 0, message);
      line++;
   }
   // Line ------
   if (supports_invert) {
      sprintf(message, "         Invert: %d", invert);
      osd_set(line, 0, message);
      line++;
   }
   // Line ------
   if (metric < 0) {
      sprintf(message, "         Errors: unknown");
   } else {
      sprintf(message, "         Errors: %d", metric);
   }
   osd_set(line, 0, message);

   return(line);
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
   if (supports_invert) {
      mp += sprintf(mp, "; invert = %d", invert);
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

   // Extract just the major version number
   int major = (cpld_version >> VERSION_MAJOR_BIT) & 0x0F;
   int minor = (cpld_version >> VERSION_MINOR_BIT) & 0x0F;
   // Optional delay parameter
   // CPLDv2 and beyond supports the delay parameter, and starts sampling earlier
   if (major >= 6) {
      supports_delay = 2; // 2 bits of delay
   } else if (major >= 2) {
      supports_delay = 4; // 4 bits of delay
   } else {
      supports_delay = 0;
   }

   // Optional rate parameter
   // CPLDv3 supports a 1-bit rate, CPLDv4 and beyond supports a 2-bit rate
   if (major >= 4) {
      supports_rate = 2;
      params[RATE].max = 3;
   } else if (major >= 3) {
      supports_rate = 1;
      params[RATE].max = 1;
   } else {
      supports_rate = 0;
   }
   // Optional invert parameter
   // CPLDv5 and beyond support an invertion of sync
   if (major >= 5) {
      supports_invert = 1;
   } else {
      supports_invert = 0;
   }
   if (major >= 6) {
      supports_vsync = 1;
   } else {
      supports_vsync = 0;
   }
   //*******************************************************************************************************************************
   if ((major >= 6 && minor >=4) || major >=7) {
      supports_separate = 1;
   } else {
      supports_separate = 0;
   }
   //*******************************************************************************************************************************

   // Hide analog frontend parameters
   if (!supports_analog) {
      params[DAC_A].key = -1;
   }

   if (major > 2) {
       geometry_hide_pixel_sampling();
   }

   for (int i = 0; i < NUM_OFFSETS; i++) {
      default_config.sp_offset[i] = 2;
      mode7_config.sp_offset[i] = 5;
   }
   default_config.half_px_delay = 0;
   mode7_config.half_px_delay   = 0;
   default_config.divider       = 6;
   mode7_config.divider         = 8;
   default_config.mux           = 0;
   mode7_config.mux             = 0;
   default_config.full_px_delay = 5;   // Correct for the master
   mode7_config.full_px_delay   = 8;   // Correct for the master
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
   int old_full_px_delay;

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
   old_full_px_delay = config->full_px_delay;
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
      osd_sp(config, 2, metric);
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
      for (int i = 0; i < 4; i++) {
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
      osd_sp(config, 2, *errors);
      log_sp(config);
      log_info("Optimization complete, errors = %d", *errors);
   }

   // Determine mode 7 alignment
   if (supports_delay) {
      signed int new_full_px_delay;
      if (mode7) {
         new_full_px_delay = analyze_mode7_alignment(capinfo);
      } else {
         new_full_px_delay = analyze_default_alignment(capinfo);
      }
      if (new_full_px_delay >= 0) {                               // if negative then not in a bbc autoswitch profile so don't auto update delay
          log_info("Characters aligned to word boundaries");
          config->full_px_delay = (int) new_full_px_delay;
      } else {
          log_info("Not a BBC display: Delay not auto adjusted");
          config->full_px_delay = old_full_px_delay;
      }
      write_config(config);
   }

   // Perform a final test of errors
   log_info("Performing final test");
   *errors = diff_N_frames(capinfo, NUM_CAL_FRAMES, mode7, elk);
   osd_sp(config, 2, *errors);
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

static void cpld_set_mode(int mode) {
   mode7 = mode;
   config = mode ? &mode7_config : &default_config;
   write_config(config);
   // Update the OSD param ranges based on the new config
   update_param_range();
}

static int cpld_analyse(int manual_setting) {
   if (supports_invert) {
      int polarity;
      if (manual_setting < 0) {
         polarity = analyse_sync(); // returns lsb set to 1 if sync polarity incorrect
      } else {
         polarity = manual_setting ^ invert;
      }
      if (polarity & SYNC_BIT_HSYNC_INVERTED) {
         invert ^= (polarity & SYNC_BIT_HSYNC_INVERTED);
         if (invert) {
            log_info("Analyze Csync: polarity changed to inverted");
         } else {
            log_info("Analyze Csync: polarity changed to non-inverted");
         }
         write_config(config);
      } else {
         if (invert) {
            log_info("Analyze Csync: polarity unchanged (inverted)");
         } else {
            log_info("Analyze Csync: polarity unchanged (non-inverted)");
         }
      }
      polarity &= ~SYNC_BIT_HSYNC_INVERTED;
      polarity |= (invert ? SYNC_BIT_HSYNC_INVERTED : 0);
      if (supports_separate == 0) {
          polarity ^= ((polarity & SYNC_BIT_VSYNC_INVERTED) ? SYNC_BIT_HSYNC_INVERTED : 0);
          polarity |= SYNC_BIT_MIXED_SYNC;
      }
     // if (supports_analog) {
     //   polarity = (polarity & SYNC_BIT_HSYNC_INVERTED) | SYNC_BIT_COMPOSITE_SYNC; // inhibit vsync detection in analog mode as vsync used for other things
     // }
      if (supports_vsync) {
         return (polarity);
      } else {
         return ((polarity & SYNC_BIT_HSYNC_INVERTED) | SYNC_BIT_COMPOSITE_SYNC | SYNC_BIT_MIXED_SYNC);
      }
   } else {
       return (SYNC_BIT_COMPOSITE_SYNC | SYNC_BIT_MIXED_SYNC);
   }

}

static void cpld_update_capture_info(capture_info_t *capinfo) {
   // Update the capture info stucture, if one was passed in
   if (capinfo) {
      // Update the sample width
      capinfo->sample_width = (config->rate == 1);  // 1 = 6bpp, everything else 3bpp
      // Update the line capture function
      if (!mode7) {
         if (capinfo->sample_width) {
             switch (capinfo->px_sampling) {
                case PS_NORMAL:
                    capinfo->capture_line = capture_line_normal_6bpp_table;
                    break;
                case PS_NORMAL_O:
                    capinfo->capture_line = capture_line_odd_6bpp_table;
                    break;
                case PS_NORMAL_E:
                    capinfo->capture_line = capture_line_even_6bpp_table;
                    break;
                case PS_HALF_O:
                    capinfo->capture_line = capture_line_half_odd_6bpp_table;
                    break;
                case PS_HALF_E:
                    capinfo->capture_line = capture_line_half_even_6bpp_table;
                    break;
             }
         } else {
             switch (capinfo->px_sampling) {
                case PS_NORMAL:
                    capinfo->capture_line = capture_line_normal_3bpp_table;
                    break;
                case PS_NORMAL_O:
                    capinfo->capture_line = capture_line_odd_3bpp_table;
                    break;
                case PS_NORMAL_E:
                    capinfo->capture_line = capture_line_even_3bpp_table;
                    break;
                case PS_HALF_O:
                    capinfo->capture_line = capture_line_half_odd_3bpp_table;
                    break;
                case PS_HALF_E:
                    capinfo->capture_line = capture_line_half_even_3bpp_table;
                    break;
             }
         }
      } else {
          capinfo->capture_line = capture_line_mode7_3bpp_table;
      }
   }
}

static param_t *cpld_get_params() {
   return params;
}

static int cpld_get_value(int num) {
   switch (num) {

   case CPLD_SETUP_MODE:
      return config->cpld_setup_mode;
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
   case RATE:
      return config->rate;
   case MUX:
      return config->mux;
   case DAC_A:
      return config->dac_a;
   case DAC_B:
      return config->dac_b;
   case DAC_C:
      return config->dac_c;
   case DAC_D:
      return config->dac_d;
   case DAC_E:
      return config->dac_e;
   case DAC_F:
      return config->dac_f;
   case DAC_G:
      return config->dac_g;
   case DAC_H:
      return config->dac_h;
   }
   return 0;
}

static const char *cpld_get_value_string(int num) {
   if (num == RATE) {
      return rate_names[config->rate];
   }
   if (num == CPLD_SETUP_MODE) {
      return cpld_setup_names[config->cpld_setup_mode];
   }
   return NULL;
}

static void cpld_set_value(int num, int value) {
   if (value < params[num].min) {
      value = params[num].min;
   }
   if (value > params[num].max) {
      value = params[num].max;
   }
   switch (num) {
   case CPLD_SETUP_MODE:
      config->cpld_setup_mode = value;
      set_setup_mode(value);
      break;
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
   case RATE:
      config->rate = value;
      break;
   case MUX:
      config->mux = value;
      break;
   case DAC_A:
      config->dac_a = value;
      break;
   case DAC_B:
      config->dac_b = value;
      break;
   case DAC_C:
      config->dac_c = value;
      break;
   case DAC_D:
      config->dac_d = value;
      break;
   case DAC_E:
      config->dac_e = value;
      break;
   case DAC_F:
      config->dac_f = value;
      break;
   case DAC_G:
      config->dac_g = value;
      break;
   case DAC_H:
      config->dac_h = value;
      break;
   }
   write_config(config);
}

static int cpld_show_cal_summary(int line) {
   return osd_sp(config, line, mode7 ? errors_mode7 : errors_default);
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

static int cpld_old_firmware_support() {
    int firmware = 0;
    if (((cpld_version >> VERSION_MAJOR_BIT ) & 0x0F) <= 1) {
        firmware |= BIT_NORMAL_FIRMWARE_V1;
    }
    if (((cpld_version >> VERSION_MAJOR_BIT ) & 0x0F) == 2) {
        firmware |= BIT_NORMAL_FIRMWARE_V2;
    }
    return firmware;
}


static int cpld_get_divider() {
    return cpld_get_value(DIVIDER);
}

static int cpld_get_delay() {
    return cpld_get_value(DELAY);
}

static void cpld_set_frontend(int value) {
}

// =============================================================
//  BBC Driver Specific
// =============================================================

static void cpld_init_bbc(int version) {
   supports_analog = 0;
   cpld_init(version);
}

static int cpld_frontend_info_bbc() {
    return FRONTEND_TTL_3BIT | FRONTEND_TTL_3BIT << 16;
}

cpld_t cpld_bbc = {
   .name = "BBC",
   .default_profile = "BBC_Micro",
   .init = cpld_init_bbc,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_bbc,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};

cpld_t cpld_bbcv1v2 = {
   .name = "OBBC",
   .default_profile = "BBC_Micro_v10-v20",
   .init = cpld_init_bbc,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_bbc,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};


// =============================================================
// RGB_TTL Driver Specific
// =============================================================

static void cpld_init_rgb_ttl(int version) {
   supports_analog = 0;
   cpld_init(version);
}

static int cpld_frontend_info_rgb_ttl() {
    return FRONTEND_TTL_6BIT | FRONTEND_TTL_6BIT << 16;
}

cpld_t cpld_rgb_ttl = {
   .name = "RGB(TTL)",
   .default_profile = "BBC_Micro",
   .init = cpld_init_rgb_ttl,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_rgb_ttl,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};

// =============================================================
// RGB_Analog Driver Specific
// =============================================================

static void cpld_init_rgb_analog(int version) {
   supports_analog = 1;
   cpld_init(version);
}

static int cpld_frontend_info_rgb_analog() {
    return FRONTEND_ANALOG_5259 | FRONTEND_ANALOG_UB1 << 16;
}

static void cpld_set_frontend_rgb_analog(int value) {
   frontend = value;
   write_config(config);
}

cpld_t cpld_rgb_analog = {
   .name = "RGB(Analog)",
   .default_profile = "Amstrad_CPC",
   .init = cpld_init_rgb_analog,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_rgb_analog,
   .set_frontend = cpld_set_frontend_rgb_analog,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};
