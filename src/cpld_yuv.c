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
#include "rgb_to_hdmi.h"
#include "rgb_to_fb.h"
#include "rpi-gpio.h"

#define RANGE_SUBC_0  8
#define RANGE_SUBC_1 16

#define RANGE_MAX    16

// The number of frames to compute differences over
#define NUM_CAL_FRAMES 10

#define DAC_UPDATE 1
#define NO_DAC_UPDATE 0

typedef struct {
   int cpld_setup_mode;
   int all_offsets;
   int sp_offset[NUM_OFFSETS];
       int half_px_delay; // 0 = off, 1 = on, all modes
       int divider;       // cpld divider, 6 or 8
       int range;         // divider ranging
   int delay;
   int filter_l;
   int sub_c;
   int alt_r;
   int edge;
   int clamptype;
   int mux;
   int rate;
   int terminate;
   int coupling;
   int dac_a;
   int dac_b;
   int dac_c;
   int dac_d;
   int dac_e;
   int dac_f;
   int dac_g;
   int dac_h;
} config_t;

enum {
  YUV_RATE_UNUSED,
  YUV_RATE_6,
  YUV_RATE_6_LEVEL_4,
};

static const char *volt_names[] = {
   "000 (0.00v)",
   "001 (0.01v)",
   "002 (0.03v)",
   "003 (0.04v)",
   "004 (0.05v)",
   "005 (0.06v)",
   "006 (0.08v)",
   "007 (0.09v)",
   "008 (0.10v)",
   "009 (0.12v)",
   "010 (0.13v)",
   "011 (0.14v)",
   "012 (0.16v)",
   "013 (0.17v)",
   "014 (0.18v)",
   "015 (0.19v)",
   "016 (0.21v)",
   "017 (0.22v)",
   "018 (0.23v)",
   "019 (0.25v)",
   "020 (0.26v)",
   "021 (0.27v)",
   "022 (0.28v)",
   "023 (0.30v)",
   "024 (0.31v)",
   "025 (0.32v)",
   "026 (0.34v)",
   "027 (0.35v)",
   "028 (0.36v)",
   "029 (0.38v)",
   "030 (0.39v)",
   "031 (0.40v)",
   "032 (0.41v)",
   "033 (0.43v)",
   "034 (0.44v)",
   "035 (0.45v)",
   "036 (0.47v)",
   "037 (0.48v)",
   "038 (0.49v)",
   "039 (0.50v)",
   "040 (0.52v)",
   "041 (0.53v)",
   "042 (0.54v)",
   "043 (0.56v)",
   "044 (0.57v)",
   "045 (0.58v)",
   "046 (0.60v)",
   "047 (0.61v)",
   "048 (0.62v)",
   "049 (0.63v)",
   "050 (0.65v)",
   "051 (0.66v)",
   "052 (0.67v)",
   "053 (0.69v)",
   "054 (0.70v)",
   "055 (0.71v)",
   "056 (0.72v)",
   "057 (0.74v)",
   "058 (0.75v)",
   "059 (0.76v)",
   "060 (0.78v)",
   "061 (0.79v)",
   "062 (0.80v)",
   "063 (0.82v)",
   "064 (0.83v)",
   "065 (0.84v)",
   "066 (0.85v)",
   "067 (0.87v)",
   "068 (0.88v)",
   "069 (0.89v)",
   "070 (0.91v)",
   "071 (0.92v)",
   "072 (0.93v)",
   "073 (0.94v)",
   "074 (0.96v)",
   "075 (0.97v)",
   "076 (0.98v)",
   "077 (1.00v)",
   "078 (1.01v)",
   "079 (1.02v)",
   "080 (1.04v)",
   "081 (1.05v)",
   "082 (1.06v)",
   "083 (1.07v)",
   "084 (1.09v)",
   "085 (1.10v)",
   "086 (1.11v)",
   "087 (1.13v)",
   "088 (1.14v)",
   "089 (1.15v)",
   "090 (1.16v)",
   "091 (1.18v)",
   "092 (1.19v)",
   "093 (1.20v)",
   "094 (1.22v)",
   "095 (1.23v)",
   "096 (1.24v)",
   "097 (1.26v)",
   "098 (1.27v)",
   "099 (1.28v)",
   "100 (1.29v)",
   "101 (1.31v)",
   "102 (1.32v)",
   "103 (1.33v)",
   "104 (1.35v)",
   "105 (1.36v)",
   "106 (1.37v)",
   "107 (1.38v)",
   "108 (1.40v)",
   "109 (1.41v)",
   "110 (1.42v)",
   "111 (1.44v)",
   "112 (1.45v)",
   "113 (1.46v)",
   "114 (1.48v)",
   "115 (1.49v)",
   "116 (1.50v)",
   "117 (1.51v)",
   "118 (1.53v)",
   "119 (1.54v)",
   "120 (1.55v)",
   "121 (1.57v)",
   "122 (1.58v)",
   "123 (1.59v)",
   "124 (1.60v)",
   "125 (1.62v)",
   "126 (1.63v)",
   "127 (1.64v)",
   "128 (1.66v)",
   "129 (1.67v)",
   "130 (1.68v)",
   "131 (1.70v)",
   "132 (1.71v)",
   "133 (1.72v)",
   "134 (1.73v)",
   "135 (1.75v)",
   "136 (1.76v)",
   "137 (1.77v)",
   "138 (1.79v)",
   "139 (1.80v)",
   "140 (1.81v)",
   "141 (1.82v)",
   "142 (1.84v)",
   "143 (1.85v)",
   "144 (1.86v)",
   "145 (1.88v)",
   "146 (1.89v)",
   "147 (1.90v)",
   "148 (1.92v)",
   "149 (1.93v)",
   "150 (1.94v)",
   "151 (1.95v)",
   "152 (1.97v)",
   "153 (1.98v)",
   "154 (1.99v)",
   "155 (2.01v)",
   "156 (2.02v)",
   "157 (2.03v)",
   "158 (2.04v)",
   "159 (2.06v)",
   "160 (2.07v)",
   "161 (2.08v)",
   "162 (2.10v)",
   "163 (2.11v)",
   "164 (2.12v)",
   "165 (2.14v)",
   "166 (2.15v)",
   "167 (2.16v)",
   "168 (2.17v)",
   "169 (2.19v)",
   "170 (2.20v)",
   "171 (2.21v)",
   "172 (2.23v)",
   "173 (2.24v)",
   "174 (2.25v)",
   "175 (2.26v)",
   "176 (2.28v)",
   "177 (2.29v)",
   "178 (2.30v)",
   "179 (2.32v)",
   "180 (2.33v)",
   "181 (2.34v)",
   "182 (2.36v)",
   "183 (2.37v)",
   "184 (2.38v)",
   "185 (2.39v)",
   "186 (2.41v)",
   "187 (2.42v)",
   "188 (2.43v)",
   "189 (2.45v)",
   "190 (2.46v)",
   "191 (2.47v)",
   "192 (2.48v)",
   "193 (2.50v)",
   "194 (2.51v)",
   "195 (2.52v)",
   "196 (2.54v)",
   "197 (2.55v)",
   "198 (2.56v)",
   "199 (2.58v)",
   "200 (2.59v)",
   "201 (2.60v)",
   "202 (2.61v)",
   "203 (2.63v)",
   "204 (2.64v)",
   "205 (2.65v)",
   "206 (2.67v)",
   "207 (2.68v)",
   "208 (2.69v)",
   "209 (2.70v)",
   "210 (2.72v)",
   "211 (2.73v)",
   "212 (2.74v)",
   "213 (2.76v)",
   "214 (2.77v)",
   "215 (2.78v)",
   "216 (2.80v)",
   "217 (2.81v)",
   "218 (2.82v)",
   "219 (2.83v)",
   "220 (2.85v)",
   "221 (2.86v)",
   "222 (2.87v)",
   "223 (2.89v)",
   "224 (2.90v)",
   "225 (2.91v)",
   "226 (2.92v)",
   "227 (2.94v)",
   "228 (2.95v)",
   "229 (2.96v)",
   "230 (2.98v)",
   "231 (2.99v)",
   "232 (3.00v)",
   "233 (3.02v)",
   "234 (3.03v)",
   "235 (3.04v)",
   "236 (3.05v)",
   "237 (3.07v)",
   "238 (3.08v)",
   "239 (3.09v)",
   "240 (3.11v)",
   "241 (3.12v)",
   "242 (3.13v)",
   "243 (3.14v)",
   "244 (3.16v)",
   "245 (3.17v)",
   "246 (3.18v)",
   "247 (3.20v)",
   "248 (3.21v)",
   "249 (3.22v)",
   "250 (3.24v)",
   "251 (3.25v)",
   "252 (3.26v)",
   "253 (3.27v)",
   "254 (3.29v)",
   "255 (3.30v)",
   "Disabled"
};


// Current calibration state for mode 0..6
static config_t set1_config;
static config_t set2_config;

// Current configuration
static config_t *config = &set1_config;

// OSD message buffer
static char message[256];

// phase text buffer
static char phase_text[256];

// Aggregate calibration metrics (i.e. errors summed across all channels)
static int sum_metrics[RANGE_MAX];

// Error count for final calibration values for mode 0..6
static int errors;

// The CPLD version number
static int cpld_version;

static int frontend = 0;

static int supports_invert         = 1;
static int supports_separate       = 1;
static int supports_vsync          = 1;
static int supports_sub_c          = 1; /* Supports chroma subsampling */
static int supports_alt_r          = 1; /* Supports R channel inversion on alternate lines */
static int supports_edge           = 1; /* Selection of leading rather than trailing edge */
static int supports_clamptype      = 1; /* Selection of back porch or sync tip clamping */
static int supports_delay          = 1; /* A 0-3 pixel delay */
static int supports_extended_delay = 1; /* A 0-15 pixel delay */
static int supports_four_level     = 1;
static int supports_mux            = 1; /* mux moved from pin to register bit */

// invert state (not part of config)
static int invert = 0;
static int supports_analog = 0;

static int modeset = 0;

int yuv_divider_lookup[] = { 6, 8, 10, 12, 14, 16, 3, 4 };

static const char *yuv_divider_names[] = {
   "x6",
   "x8",
   "x10",
   "x12",
   "x14",
   "x16",
   "x3",
   "x4"
};

// =============================================================
// Param definitions for OSD
// =============================================================

enum {
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
       RANGE,
   DELAY,
   FILTER_L,
   SUB_C,
   ALT_R,
   EDGE,
   CLAMPTYPE,
   MUX,
   RATE,
   TERMINATE,
   COUPLING,
   DAC_A,
   DAC_B,
   DAC_C,
   DAC_D,
   DAC_E,
   DAC_F,
   DAC_G,
   DAC_H
};

static const char *edge_names[] = {
   "Trailing",
   "Leading"
};

static const char *cpld_setup_names[] = {
   "Normal",
   "Set Pixel H Offset"
};

static const char *clamptype_names[] = {
   "Sync Tip",
   "Back Porch Short",
   "Back Porch Medium",
   "Back Porch Long",
   "Back Porch Auto"
};

static const char *level_names[] = {
   "3 Level YUV",
   "3 Lvl Y, 4 Lvl UV",
   "4 Lvl Y, 3 Lvl UV",
   "4 Level YUV",
   "Auto 4 Level YUV"
};

static const char *rate_names[] = {
   "Unused",
   "6 Bits Per Pixel",
   "6 Bits (4 Level)",
};

static const char *coupling_names[] = {
   "DC",
   "AC With Clamp"
};

enum {
   YUV_INPUT_HI,
   YUV_INPUT_TERM,
   NUM_YUV_TERM
};

enum {
   YUV_INPUT_DC,
   YUV_INPUT_AC,
   NUM_YUV_COUPLING
};

enum {
   CPLD_SETUP_NORMAL,
   CPLD_SETUP_DELAY,
   NUM_CPLD_SETUP
};

enum {
   CLAMPTYPE_SYNC,
   CLAMPTYPE_SHORT,
   CLAMPTYPE_MEDIUM,
   CLAMPTYPE_LONG,
   CLAMPTYPE_AUTO,
   NUM_CLAMPTYPE
};

static param_t params[] = {
   {  CPLD_SETUP_MODE,  "Setup Mode", "setup_mode", 0, NUM_CPLD_SETUP-1, 1 },
   { ALL_OFFSETS,      "Sampling Phase",          "offset", 0,  15, 1 },
//block of hidden RGB options for file compatibility
   {    A_OFFSET,    "A Phase",    "a_offset", 0,   15, 1 },
   {    B_OFFSET,    "B Phase",    "b_offset", 0,   15, 1 },
   {    C_OFFSET,    "C Phase",    "c_offset", 0,   15, 1 },
   {    D_OFFSET,    "D Phase",    "d_offset", 0,   15, 1 },
   {    E_OFFSET,    "E Phase",    "e_offset", 0,   15, 1 },
   {    F_OFFSET,    "F Phase",    "f_offset", 0,   15, 1 },
   {        HALF,        "Half Pixel Shift",        "half", 0,   1, 1 },
   {     DIVIDER,    "Clock Multiplier",    "multiplier", 0,   7, 1 },
   {       RANGE,    "Multiplier Range",    "range", 0,   1, 1 },
//end of hidden block
   {       DELAY,  "Pixel H Offset",            "delay", 0,  15, 1 },
   {    FILTER_L,  "Filter Y",      "l_filter", 0,   1, 1 },
   {       SUB_C,  "Subsample UV",     "sub_c", 0,   1, 1 },
   {       ALT_R,  "PAL switch",       "alt_r", 0,   1, 1 },
   {        EDGE,  "Sync Edge",         "edge", 0,   1, 1 },
   {   CLAMPTYPE,  "Clamp Type",   "clamptype", 0,     NUM_CLAMPTYPE-1, 1 },
   {         MUX,  "Sync on Y/V",     "input_mux", 0,   1, 1 },
   {        RATE,  "Sample Mode", "sample_mode", 1,   2, 1 },
   {   TERMINATE,  "75R Termination", "termination", 0,   NUM_YUV_TERM-1, 1 },
   {    COUPLING,  "Y Coupling", "coupling", 0,   NUM_YUV_COUPLING-1, 1 },
   {       DAC_A,  "DAC-A: Y Hi",      "dac_a", 0, 256, 1 },
   {       DAC_B,  "DAC-B: Y Lo",      "dac_b", 0, 256, 1 },
   {       DAC_C,  "DAC-C: UV Hi",     "dac_c", 0, 256, 1 },
   {       DAC_D,  "DAC-D: UV Lo",     "dac_d", 0, 256, 1 },
   {       DAC_E,  "DAC-E: Sync",      "dac_e", 0, 256, 1 },
   {       DAC_F,  "DAC-F: Y/V Sync",   "dac_f", 0, 256, 1 },
   {       DAC_G,  "DAC-G: Y Clamp",   "dac_g", 0, 256, 1 },
   {       DAC_H,  "DAC-H: Unused",     "dac_h", 0, 256, 1 },
   {          -1,  NULL,                  NULL, 0,   0, 0 }
};

// =============================================================
// Private methods
// =============================================================

static int getRange() {
   if (supports_sub_c && !config->sub_c) {
      return RANGE_SUBC_0;
   } else {
      return RANGE_SUBC_1;
   }
}

static void sendDAC(int dac, int value)
{
    if (value > 255) {
        value = 255;
    }
    int M62364_dac = 0;

    switch (dac) {
        case 0:
            M62364_dac = 0x08;
        break;
        case 1:
            M62364_dac = 0x04;
        break;
        case 2:
            M62364_dac = 0x0c;
        break;
        case 3:
            M62364_dac = 0x02;
        break;
        case 4:
            M62364_dac = 0x0a;
        break;
        case 5:
            M62364_dac = 0x06;
        break;
        case 6:
            M62364_dac = 0x0e;
            if (frontend == FRONTEND_YUV_ISSUE2_5259) {
                dac = 7;
            }
        break;
        case 7:
            M62364_dac = 0x01;
            if (frontend == FRONTEND_YUV_ISSUE2_5259) {
                dac = 6;
                switch (config->terminate) {
                      default:
                      case YUV_INPUT_HI:
                        value = 0;  //high impedance
                      break;
                      case YUV_INPUT_TERM:
                        value = 255; //termination
                      break;
                }
            }
        break;
        default:
        break;
    }

    if (new_DAC_detected() == 1) {
        int packet = (M62364_dac << 8) | value;
        //log_info("M62364 dac:%d = %02X, %03X", dac, value, packet);
        RPI_SetGpioValue(STROBE_PIN, 0);
        for (int i = 0; i < 12; i++) {
            RPI_SetGpioValue(SP_CLKEN_PIN, 0);
            RPI_SetGpioValue(SP_DATA_PIN, (packet >> 11) & 1);
            delay_in_arm_cycles_cpu_adjust(500);
            RPI_SetGpioValue(SP_CLKEN_PIN, 1);
            delay_in_arm_cycles_cpu_adjust(500);
            packet <<= 1;
        }
        RPI_SetGpioValue(STROBE_PIN, 1);
    } else if (new_DAC_detected() == 2) {
        int packet = (dac + 1) | (value << 6);
        //log_info("bu2506 dac:%d = %02X, %03X", dac, value, packet);
        RPI_SetGpioValue(STROBE_PIN, 0);

        for (int i = 0; i < 14; i++) {
            RPI_SetGpioValue(SP_CLKEN_PIN, 0);
            RPI_SetGpioValue(SP_DATA_PIN, packet & 1);
            delay_in_arm_cycles_cpu_adjust(500);
            RPI_SetGpioValue(SP_CLKEN_PIN, 1);
            delay_in_arm_cycles_cpu_adjust(500);
            packet >>= 1;
        }
        RPI_SetGpioValue(STROBE_PIN, 1);
        delay_in_arm_cycles_cpu_adjust(500);
        RPI_SetGpioValue(STROBE_PIN, 0);
    } else {
        //log_info("Issue2/3 dac:%d = %d", dac, value);
        int packet = (dac << 11) | 0x600 | value;
        RPI_SetGpioValue(STROBE_PIN, 0);
        for (int i = 0; i < 16; i++) {
            RPI_SetGpioValue(SP_CLKEN_PIN, 0);
            RPI_SetGpioValue(SP_DATA_PIN, (packet >> 15) & 1);
            delay_in_arm_cycles_cpu_adjust(500);
            RPI_SetGpioValue(SP_CLKEN_PIN, 1);
            delay_in_arm_cycles_cpu_adjust(500);
            packet <<= 1;
        }
        RPI_SetGpioValue(STROBE_PIN, 1);
    }

    RPI_SetGpioValue(SP_DATA_PIN, 0);
}

static void write_config(config_t *config, int dac_update) {
   int sp = 0;
   int scan_len = 0;
   if (supports_delay) {
      // In the hardware we effectively have a total of 5 bits of actual offset
      // and we need to derive this from the offset and delay values
      if (config->sub_c) {
         // Use 4 bits of offset and 1 bit of delay
         sp |= (config->all_offsets & 15) << scan_len;
         scan_len += 4;
         sp |= ((~config->delay >> 1) & 1) << scan_len;
         scan_len += 1;
      } else {
         // Use 3 bits of offset and 2 bit of delay
         sp |= (config->all_offsets & 7) << scan_len;
         scan_len += 3;
         sp |= (~config->delay & 3) << scan_len;
         scan_len += 2;
      }
   } else {
      sp |= (config->all_offsets & 15) << scan_len;
      scan_len += 4;
   }

   if (supports_extended_delay) {
      sp |= ((~config->delay >> 2) & 3) << scan_len;
      scan_len += 2;
   }

   sp |= ((config->rate - 1) & 1) << scan_len;
   scan_len++;

   sp |= config->filter_l << scan_len;
   scan_len++;

   if (supports_invert) {
      sp |= invert << scan_len;
      scan_len++;
   }
   if (supports_sub_c) {
      sp |= config->sub_c << scan_len;
      scan_len++;
   }
   if (supports_alt_r) {
      sp |= config->alt_r << scan_len;
      scan_len++;
   }
   if (supports_edge) {
      sp |= config->edge << scan_len;
      scan_len++;
   }
   if (supports_clamptype) {
      int clamptype = config->clamptype;
      if (clamptype == CLAMPTYPE_AUTO) {
          if (config->rate == YUV_RATE_6) {
              clamptype = CLAMPTYPE_SHORT;
              if (geometry_get_value(CLOCK) >= 6750000) {
                  clamptype = CLAMPTYPE_MEDIUM;
              }
              if (geometry_get_value(CLOCK) >= 9750000) {
                  clamptype = CLAMPTYPE_LONG;
              }
          } else {
              clamptype = CLAMPTYPE_LONG; // force 4 level YUV in four level mode
          }
      }
      sp |= clamptype << scan_len;
      scan_len += 2;
   }

   if (supports_mux) {
       sp |= config->mux << scan_len;
       scan_len += 1;
   }

   for (int i = 0; i < scan_len; i++) {
      RPI_SetGpioValue(SP_DATA_PIN, sp & 1);
      delay_in_arm_cycles_cpu_adjust(250);
      RPI_SetGpioValue(SP_CLKEN_PIN, 1);
      delay_in_arm_cycles_cpu_adjust(250);
      RPI_SetGpioValue(SP_CLK_PIN, 0);
      delay_in_arm_cycles_cpu_adjust(250);
      RPI_SetGpioValue(SP_CLK_PIN, 1);
      delay_in_arm_cycles_cpu_adjust(250);
      RPI_SetGpioValue(SP_CLKEN_PIN, 0);
      delay_in_arm_cycles_cpu_adjust(250);
      sp >>= 1;
   }

   if (dac_update) {
       int dac_a = config->dac_a;
       if (dac_a == 256) dac_a = 75; //approx 1v (255 = 3.3v) so a grounded input will be detected as 0 without exceeding 1.4v difference
       int dac_b = config->dac_b;
       if (dac_b == 256) dac_b = dac_a;

       int dac_c = config->dac_c;
       if (dac_c == 256) dac_c = 75; //approx 1v (255 = 3.3v) so a grounded input will be detected as 0 without exceeding 1.4v difference
       int dac_d = config->dac_d;
       if (dac_d == 256) dac_d = dac_c;

       int dac_e = config->dac_e;
       if (dac_e == 256 && config->mux != 0) dac_e = dac_a;    //if sync level is disabled, track dac_a unless sync source is sync (stops spurious sync detection)

       int dac_f = config->dac_f;
       if (dac_f == 256 && config->mux == 0) dac_f = dac_a;   //if vsync level is disabled, track dac_a unless sync source is vsync (stops spurious sync detection)

       int dac_g = config->dac_g;
       if (dac_g == 256) {
          if (supports_four_level && config->rate >= YUV_RATE_6_LEVEL_4) {
             dac_g = dac_c;
          } else {
             dac_g = 0;
          }
       }

       int dac_h = config->dac_h;
       if (dac_h == 256) dac_h = dac_c;

       if (params[DAC_G].hidden == 1) {
           dac_g = dac_c;
       }
       if (params[DAC_H].hidden == 1) {
           dac_h = dac_c;
       }

       sendDAC(0, dac_a);
       sendDAC(1, dac_b);
       sendDAC(2, dac_c);
       sendDAC(3, dac_d);
       sendDAC(5, dac_e);   // DACs E and F positions swapped in menu compared to hardware
       sendDAC(4, dac_f);
       sendDAC(6, dac_g);
       sendDAC(7, dac_h);
   }

   switch (config->terminate) {
      default:
      case YUV_INPUT_HI:
        RPI_SetGpioValue(SP_CLKEN_PIN, 0);  //termination
      break;
      case YUV_INPUT_TERM:
        RPI_SetGpioValue(SP_CLKEN_PIN, 1);
      break;
   }

   int coupling = config->coupling;
   if (frontend == FRONTEND_ANALOG_ISSUE2_5259 || frontend == FRONTEND_ANALOG_ISSUE1_UA1 || frontend == FRONTEND_ANALOG_ISSUE1_UB1) {
       coupling = YUV_INPUT_AC;                  // always ac coupling with issue 1 or 2
   }

   switch (coupling) {
      default:
      case YUV_INPUT_DC:
        RPI_SetGpioValue(SP_DATA_PIN, 0);   //ac-dc
      break;
      case YUV_INPUT_AC:
        RPI_SetGpioValue(SP_DATA_PIN, (config->rate < YUV_RATE_6_LEVEL_4) || !supports_four_level);  // only allow clamping in 3 level mode or old cpld or 6 bit board
      break;
   }

   RPI_SetGpioValue(MUX_PIN, config->mux);
}

static int osd_sp(config_t *config, int line, int metric) {
   // Line ------
   sprintf(message,    " Sampling Phase: %d", config->all_offsets);
   osd_set(line++, 0, message);
   // Line ------
   if (metric < 0) {
      sprintf(message, "         Errors: unknown");
   } else {
      sprintf(message, "         Errors: %d", metric);
   }
   osd_set(line++, 0, message);
   return(line);
}

static void log_sp(config_t *config) {
   log_info("phase = %d", config->all_offsets);
}

// =============================================================
// Public methods
// =============================================================

static void cpld_init(int version) {
   params[A_OFFSET].hidden = 1;
   params[B_OFFSET].hidden = 1;
   params[C_OFFSET].hidden = 1;
   params[D_OFFSET].hidden = 1;
   params[E_OFFSET].hidden = 1;
   params[F_OFFSET].hidden = 1;
   params[HALF].hidden = 1;
   params[DIVIDER].hidden = 1;
   params[RANGE].hidden = 1;
   cpld_version = version;
   config->all_offsets = 0;
   config->sp_offset[0] = 0;
   config->sp_offset[1] = 0;
   config->sp_offset[2] = 0;
   config->sp_offset[3] = 0;
   config->sp_offset[4] = 0;
   config->sp_offset[5] = 0;
   config->rate  = YUV_RATE_6;
   config->filter_l  = 1;
   config->divider = 1;
   for (int i = 0; i < RANGE_MAX; i++) {
      sum_metrics[i] = -1;
   }
   errors = -1;

   int major = (cpld_version >> VERSION_MAJOR_BIT) & 0x0F;

   // CPLDv3 adds support for inversion of video
   if (major < 3) {
      supports_invert = 0;
      supports_separate = 0;
      supports_vsync = 0;
   }
   // CPLDv4 adds support for chroma subsampling
   if (major < 4) {
      supports_sub_c = 0;
      params[SUB_C].hidden = 1;
   }
   // CPLDv5 adds support for inversion of R on alternative lines (for Spectrum)
   if (major < 5) {
      supports_alt_r = 0;
      params[ALT_R].hidden = 1;
   }
   // CPLDv6 adds support for sync edge selection and 2-bit pixel delay
   if (major < 6) {
      supports_edge = 0;
      supports_delay = 0;
      params[EDGE].hidden = 1;
      params[DELAY].hidden = 1;
   }
   // CPLDv7 adds support for programmable clamp length
   if (major < 7) {
      supports_clamptype = 0;
      params[CLAMPTYPE].hidden = 1;
   }
   // CPLDv8 addes support for 4-bit pixel delay
   if (major < 8) {
      supports_extended_delay = 0;
   }
   // CPLDv82 adds support for 4 level YUV, replacing the chroma filter
   if ((cpld_version & 0xff) < 0x82) {
       // no support for four levels and Chroma filtering available
       params[RATE].label = "Filter UV";
       params[DAC_H].hidden = 1;
       supports_four_level = 0;
   } else {
       if (!eight_bit_detected()) {
           params[RATE].max = YUV_RATE_6;   // four level YUV won't work on 6 bit boards
           params[DAC_H].hidden = 1;
           config->rate  = YUV_RATE_6;
           supports_four_level = 0;
       }
   }
   if (major < 9) {
      supports_mux = 0;
   }
   geometry_hide_pixel_sampling();
   config->cpld_setup_mode = 0;

   // Hide analog frontend parameters.
   if (!supports_analog) {
       params[CLAMPTYPE].hidden = 1;
       params[TERMINATE].hidden = 1;
       params[COUPLING].hidden = 1;
       params[DAC_A].hidden = 1;
       params[DAC_B].hidden = 1;
       params[DAC_C].hidden = 1;
       params[DAC_D].hidden = 1;
       params[DAC_E].hidden = 1;
       params[DAC_F].hidden = 1;
       params[DAC_G].hidden = 1;
       params[DAC_H].hidden = 1;
       supports_four_level = 0;
       params[RATE].max = YUV_RATE_6;
       config->rate  = YUV_RATE_6;
   }
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

   int range = getRange();

   log_info("Calibrating...");

   // Measure the error metrics at all possible offset values
   min_metric = INT_MAX;
   for (int value = 0; value < range; value++) {
      config->all_offsets = value;
      config->sp_offset[0] = value;
      config->sp_offset[1] = value;
      config->sp_offset[2] = value;
      config->sp_offset[3] = value;
      config->sp_offset[4] = value;
      config->sp_offset[5] = value;
      write_config(config, DAC_UPDATE);
      metric = diff_N_frames(capinfo, NUM_CAL_FRAMES, elk);
      log_info("INFO: value = %d: metric = %d", value, metric);
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

   // In all modes, start with the min metric

   config->all_offsets = min_i;
   config->sp_offset[0] = min_i;
   config->sp_offset[1] = min_i;
   config->sp_offset[2] = min_i;
   config->sp_offset[3] = min_i;
   config->sp_offset[4] = min_i;
   config->sp_offset[5] = min_i;
   log_sp(config);
   write_config(config, DAC_UPDATE);

   // Perform a final test of errors
   log_info("Performing final test");
   errors = diff_N_frames(capinfo, NUM_CAL_FRAMES, elk);
   osd_sp(config, 2, errors);
   log_sp(config);
   log_info("Calibration complete, errors = %d", errors);
}

static void cpld_set_mode(int mode) {
   modeset = mode;
   if (modeset == MODE_SET1) {
       config = &set1_config;
   } else {
       config = &set2_config;
   }
   write_config(config, DAC_UPDATE);
}

static void cpld_set_vsync_psync(int state) {  //state = 1 is psync (version = 1), state = 0 is vsync (version = 0, mux = 1)
   int temp_mux = config->mux;
   if (state == 0) {
       config->mux = 1;
   }
   write_config(config, NO_DAC_UPDATE);
   config->mux = temp_mux;
   RPI_SetGpioValue(VERSION_PIN, state);
}

static int cpld_analyse(int selected_sync_state, int analyse) {
   if (supports_invert) {
      if (invert ^ (selected_sync_state & SYNC_BIT_HSYNC_INVERTED)) {
         invert ^= SYNC_BIT_HSYNC_INVERTED;
         if (invert) {
            log_info("Analyze Csync: polarity changed to inverted");
         } else {
            log_info("Analyze Csync: polarity changed to non-inverted");
         }
         write_config(config, DAC_UPDATE);
      } else {
         if (invert) {
            log_info("Analyze Csync: polarity unchanged (inverted)");
         } else {
            log_info("Analyze Csync: polarity unchanged (non-inverted)");
         }
      }
      int polarity = selected_sync_state;
      if (analyse) {
          polarity = analyse_sync();
          //log_info("Raw polarity = %x %d %d %d %d", polarity, hsync_comparison_lo, hsync_comparison_hi, vsync_comparison_lo, vsync_comparison_hi);
          if (selected_sync_state & SYNC_BIT_COMPOSITE_SYNC) {
              polarity &= SYNC_BIT_HSYNC_INVERTED;
              polarity |= SYNC_BIT_COMPOSITE_SYNC;
          }
      }
      polarity ^= invert;
      if (supports_separate == 0) {
          polarity ^= ((polarity & SYNC_BIT_VSYNC_INVERTED) ? SYNC_BIT_HSYNC_INVERTED : 0);
          polarity |= SYNC_BIT_MIXED_SYNC;
      }
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
      capinfo->sample_width = SAMPLE_WIDTH_6;
      // Update the line capture function
      capinfo->capture_line = capture_line_normal_6bpp_table;
   }
   write_config(config, DAC_UPDATE);
}

static param_t *cpld_get_params() {
   return params;
}

static int cpld_get_value(int num) {
   switch (num) {
   case CPLD_SETUP_MODE:
      return config->cpld_setup_mode;
   case ALL_OFFSETS:
      return config->all_offsets;
   case RATE:
      return config->rate;
   case FILTER_L:
      return config->filter_l;
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
   case SUB_C:
      return config->sub_c;
   case ALT_R:
      return config->alt_r;
   case EDGE:
      return config->edge;
   case DELAY:
      return config->delay;
   case CLAMPTYPE:
      return config->clamptype;
   case TERMINATE:
      return config->terminate;
   case COUPLING:
      return config->coupling;

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
      config->divider = 1; //fixed at x8
      return config->divider;
   }
   return 0;
}

static const char *cpld_get_value_string(int num) {
   if (num == EDGE) {
      return edge_names[config->edge];
   }
   if (num == DIVIDER) {
      return yuv_divider_names[config->divider];
   }
   if (num == RATE) {
      if ((cpld_version & 0xff) >= 0x82) {
          return rate_names[config->rate];
      }
   }
   if (num == CLAMPTYPE) {
      if ((cpld_version & 0xff) >= 0x82 && config->rate == YUV_RATE_6_LEVEL_4) {
          return level_names[config->clamptype];
      } else {
          return clamptype_names[config->clamptype];
      }
   }
   if (num == CPLD_SETUP_MODE) {
      return cpld_setup_names[config->cpld_setup_mode];
   }
   if (num == COUPLING) {
      return coupling_names[config->coupling];
   }
   if (num >= DAC_A && num <= DAC_H) {
      return volt_names[cpld_get_value(num)];
   }
   if (num >= ALL_OFFSETS && num <= F_OFFSET) {
      if (config->sub_c != 0) {
          sprintf(phase_text, "%d (%d Degrees)", cpld_get_value(num), cpld_get_value(num) * 180 / yuv_divider_lookup[config->divider]);
      } else {
          sprintf(phase_text, "%d (%d Degrees)", cpld_get_value(num), cpld_get_value(num) * 360 / yuv_divider_lookup[config->divider]);
      }
      return phase_text;
   }

   return NULL;
}

static void cpld_set_value(int num, int value) {
   if (value < params[num].min) {
      value = params[num].min;
   }
   if (value > params[num].max  && (num < ALL_OFFSETS || num > F_OFFSET)) { //don't clip offsets because the max value could change after the values are written when loading a new profile if the divider is different
      value = params[num].max;
   }
   switch (num) {

   case CPLD_SETUP_MODE:
      config->cpld_setup_mode = value;
      set_setup_mode(value);
      break;
   case ALL_OFFSETS:
      config->all_offsets = value;
      config->sp_offset[0] = config->all_offsets;
      config->sp_offset[1] = config->all_offsets;
      config->sp_offset[2] = config->all_offsets;
      config->sp_offset[3] = config->all_offsets;
      config->sp_offset[4] = config->all_offsets;
      config->sp_offset[5] = config->all_offsets;
      break;
   case RATE:
      config->rate = value;
      if (supports_four_level && supports_analog) {
          params[DAC_B].label = "DAC-B: Y Lo";
          params[DAC_D].label = "DAC-D: UV Lo";
          params[DAC_F].label = "DAC-F: Y V Sync";
          params[DAC_G].label = "DAC-G: Y Clamp";
          params[DAC_H].label = "DAC-H: Unused";
          params[DAC_G].hidden = 0;
          params[DAC_H].hidden = 1;
          if (config->rate == YUV_RATE_6) {
             params[CLAMPTYPE].label = "Clamp Type";
             params[DAC_H].hidden = 1;
             params[COUPLING].hidden = 0;
          } else {
             params[CLAMPTYPE].label = "Level Type";
             params[COUPLING].hidden = 1;
             int levels = config->clamptype;
             if (levels > 3) {
                 levels = 3;
             }
             params[DAC_G].hidden = 1;
             if (levels & 2) {
                 params[DAC_B].label = "DAC-B: Y Mid";
                 params[DAC_F].label = "DAC-F: Y Lo";
             }
             if (levels & 1) {
                 params[DAC_G].hidden = 0;
                 params[DAC_H].hidden = 0;
                 params[DAC_D].label = "DAC-D: UV Mid";
                 params[DAC_G].label = "DAC-G: V Lo";
                 params[DAC_H].label = "DAC-H: U Lo";
             }
          }
          osd_refresh();
      }
      break;
   case FILTER_L:
      config->filter_l = value;
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
   case SUB_C:
      config->sub_c = value;
      // Keep offset in the legal range (which depends on config->sub_c)
      params[ALL_OFFSETS].max = getRange() - 1;
      config->all_offsets &= getRange() - 1;
      config->sp_offset[0] = config->all_offsets;
      config->sp_offset[1] = config->all_offsets;
      config->sp_offset[2] = config->all_offsets;
      config->sp_offset[3] = config->all_offsets;
      config->sp_offset[4] = config->all_offsets;
      config->sp_offset[5] = config->all_offsets;
      break;
   case ALT_R:
      config->alt_r = value;
      break;
   case EDGE:
      config->edge = value;
      break;
   case DELAY:
      config->delay = value;
      break;
   case CLAMPTYPE:
      config->clamptype = value;
      cpld_set_value(RATE, config->rate);
      break;
   case TERMINATE:
      config->terminate = value;
      break;
   case COUPLING:
      config->coupling = value;
      break;

   case A_OFFSET:
      break;
   case B_OFFSET:
      break;
   case C_OFFSET:
      break;
   case D_OFFSET:
      break;
   case E_OFFSET:
      break;
   case F_OFFSET:
      break;
   case HALF:
      config->half_px_delay = value;
      break;
   case DIVIDER:
      config->divider = 1;       // fixed at x8
      break;

   }
   write_config(config, DAC_UPDATE);
}

static int cpld_show_cal_summary(int line) {
   return osd_sp(config, line, errors);
}

static void cpld_show_cal_details(int line) {
   int range = (*sum_metrics < 0) ? 0 : getRange();
   if (range == 0) {
      sprintf(message, "No calibration data for this mode");
      osd_set(line, 0, message);
   } else {
      if (range > 8) {
         // Two column display
         int num = range >> 1;
         for (int value = 0; value < num; value++) {
            sprintf(message, "Phase %d: %6d; Offset %2d: %6d", value, sum_metrics[value], value + num, sum_metrics[value + num]);
            osd_set(line + value, 0, message);
         }
      } else {
         // One column display
         for (int value = 0; value < range; value++) {
            sprintf(message, "Phase %d: %6d", value, sum_metrics[value]);
            osd_set(line + value, 0, message);
         }
      }
   }
}

static int cpld_old_firmware_support() {
    return 0;
}

static int cpld_get_divider() {
    return yuv_divider_lookup[config->divider];
}

static int cpld_get_delay() {
   int delay;
   if (supports_extended_delay) {
      delay = 0;
   } else {
      delay = cpld_get_value(DELAY) & 0x0c;         //mask out lower 2 bits of delay
   }
   // Compensate for change of delay with YUV CPLD v8.x
   int major = (cpld_version >> VERSION_MAJOR_BIT) & 0x0F;
   if (major < 8) {
      delay += 16;
   }
   return delay;
}

static int cpld_get_sync_edge() {
    return config->edge;
}

static int cpld_frontend_info_analog() {
    if (new_DAC_detected() == 1) {
        return FRONTEND_YUV_ISSUE4 | FRONTEND_YUV_ISSUE4 << 16;
    } else if (new_DAC_detected() == 2) {
        return FRONTEND_YUV_ISSUE5 | FRONTEND_YUV_ISSUE5 << 16;
    } else{
        return FRONTEND_YUV_ISSUE3_5259 | FRONTEND_YUV_ISSUE2_5259 << 16;
    }
}

static int cpld_frontend_info_ttl() {
    return FRONTEND_YUV_TTL_6_8BIT | FRONTEND_YUV_TTL_6_8BIT << 16;
}

static void cpld_set_frontend_ttl(int value) {
}

static void cpld_set_frontend_analog(int value) {
   frontend = value;
   write_config(config, DAC_UPDATE);
}

static void cpld_init_analog(int value) {
    supports_analog = 1;
    cpld_init(value);
}

static void cpld_init_ttl(int value) {
    supports_analog = 0;
    cpld_init(value);
}

cpld_t cpld_yuv_analog = {
   .name = "6-12_BIT_YUV_Analog",
   .default_profile = "Apple_II",
   .init = cpld_init_analog,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .set_vsync_psync = cpld_set_vsync_psync,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_analog,
   .set_frontend = cpld_set_frontend_analog,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .get_sync_edge = cpld_get_sync_edge,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details
};

cpld_t cpld_yuv_ttl = {
   .name = "6-12_BIT_YUV",
   .default_profile = "Apple_IIc_TTL",
   .init = cpld_init_ttl,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .set_vsync_psync = cpld_set_vsync_psync,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_ttl,
   .set_frontend = cpld_set_frontend_ttl,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .get_sync_edge = cpld_get_sync_edge,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details
};
