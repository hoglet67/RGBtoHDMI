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

// The number of frames to compute differences over
#define NUM_CAL_FRAMES 10

#define DAC_UPDATE 1
#define NO_DAC_UPDATE 0

typedef struct {
   int cpld_setup_mode;
   int all_offsets;
   int sp_offset[NUM_OFFSETS];
   int half_px_delay; // 0 = off, 1 = on, all modes
   int divider;       // cpld divider, 0-3 which means 6,8,3,4
   int range;
   int full_px_delay; // 0..15
       int filter_l;
       int sub_c;
       int alt_r;
       int edge;
       int clamptype;
   int mux;           // 0 = direct, 1 = via the 74LS08 buffer
   int rate;          // 0 = normal psync rate (3 bpp), 1 = double psync rate (6 bpp), 2 = sub-sample (odd), 3=sub-sample(even)
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

// phase text buffer
static char phase_text[256];

// Per-Offset calibration metrics (i.e. errors) for mode 0..6
static int raw_metrics_default[16][NUM_OFFSETS];

// Per-offset calibration metrics (i.e. errors) for mode 7
static int raw_metrics_mode7[16][NUM_OFFSETS];

// Aggregate calibration metrics (i.e. errors summed across all offsets) for mode 0..6
static int sum_metrics_default[16]; // Last two not used

// Aggregate calibration metrics (i.e. errors summver across all offsets) for mode 7
static int sum_metrics_mode7[16];

// Error count for final calibration values for mode 0..6
static int errors_default;

// Error count for final calibration values for mode 7
static int errors_mode7;

// The CPLD version number
static int cpld_version;

// Indicates the CPLD supports the delay parameter
static int supports_delay = 0;

// Indicates the CPLD supports the rate parameter
static int supports_rate = 0;  // 0 = no, 1 = 1-bit rate param, 2 = 1-bit rate param

// Indicates the CPLD supports the invert parameter
static int supports_invert = 0;

// Indicates the CPLD supports vsync on psync when version = 0
static int supports_vsync = 0;

// Indicates the CPLD supports separate vsync & hsync
static int supports_separate = 0;

// Indicates the Analog frontent interface is present
static int supports_analog = 0;

// Indicates 24 Mhz mode 7 sampling support
static int supports_odd_even = 0;

// Indicates the Analog frontent interface supports 4 level RGB
static int supports_8bit = 0;

// Indicates CPLD supports 9 & 12 bpp on BBC CPLD build
static int supports_bbc_9_12 = 0;

// Indicates CPLD supports 6/8 divider bit
static int supports_divider = 0;

// Indicates CPLD supports mux bit
static int supports_mux = 0;

// Indicates CPLD supports 4 bit offset
static int supports_single_offset = 0;

static int RGB_CPLD_detected = 0;

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

enum {
  RGB_RATE_3,               //000
  RGB_RATE_6,               //001
  RGB_RATE_9V,              //011
  RGB_RATE_9LO,             //011
  RGB_RATE_9HI,             //011
  RGB_RATE_12,              //011
  RGB_RATE_6x2,             //110
  RGB_RATE_4_LEVEL,         //010
  RGB_RATE_1,               //100
  NUM_RGB_RATE
};

static const char *rate_names[] = {
   "3 Bits Per Pixel",
   "6 Bits Per Pixel",
   "9 Bits (V Sync)",
   "9 Bits (Bits 0-2)",
   "9 Bits (Bits 1-3)",
   "12 Bits Per Pixel",
   "6x2 Mux (12 BPP)",
   "6 Bits (4 Level)",
   "1 Bit Per Pixel",
};

static const char *cpld_setup_names[] = {
   "Normal",
   "Set Pixel H Offset"
};

int divider_lookup[] = { 6, 8, 10, 12, 14, 16, 3, 4 };
int divider_register[] = { 2, 3, 4, 5, 6, 7, 0, 1 };

static const char *divider_names[] = {
   "x6",
   "x8",
   "x10",
   "x12",
   "x14",
   "x16",
   "x3",
   "x4"
};

static const char *coupling_names[] = {
   "DC",
   "AC With Clamp"
};

static const char *range_names[] = {
   "Auto",
   "Fixed"
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



enum {
   DIVIDER_6,
   DIVIDER_8,
   DIVIDER_10,
   DIVIDER_12,
   DIVIDER_14,
   DIVIDER_16,
   DIVIDER_3,
   DIVIDER_4
};

enum {
   RANGE_AUTO,
   RANGE_FIXED,
   NUM_RANGE
};

enum {
   RGB_INPUT_HI,
   RGB_INPUT_TERM,
   NUM_RGB_TERM
};

enum {
   RGB_INPUT_DC,
   RGB_INPUT_AC,
   NUM_RGB_COUPLING
};

enum {
   CPLD_SETUP_NORMAL,
   CPLD_SETUP_DELAY,
   NUM_CPLD_SETUP
};

static param_t params[] = {
   {  CPLD_SETUP_MODE,  "Setup Mode", "setup_mode", 0,NUM_CPLD_SETUP-1, 1 },
   { ALL_OFFSETS, "Sampling Phase", "all_offsets", 0,   0, 1 },
   {    A_OFFSET,    "A Phase",    "a_offset", 0,   0, 1 },
   {    B_OFFSET,    "B Phase",    "b_offset", 0,   0, 1 },
   {    C_OFFSET,    "C Phase",    "c_offset", 0,   0, 1 },
   {    D_OFFSET,    "D Phase",    "d_offset", 0,   0, 1 },
   {    E_OFFSET,    "E Phase",    "e_offset", 0,   0, 1 },
   {    F_OFFSET,    "F Phase",    "f_offset", 0,   0, 1 },
   {        HALF,    "Half Pixel Shift",        "half", 0,   1, 1 },
   {     DIVIDER,    "Clock Multiplier",    "multiplier", 0,   7, 1 },
   {       RANGE,    "Multiplier Range",    "range", 0,   1, 1 },
   {       DELAY,    "Pixel H Offset",       "delay", 0,  15, 1 },
//block of hidden YUV options for file compatibility
   {    FILTER_L,  "Filter Y",      "l_filter", 0,   1, 1 },
   {       SUB_C,  "Subsample UV",     "sub_c", 0,   1, 1 },
   {       ALT_R,  "PAL switch",       "alt_r", 0,   1, 1 },
   {        EDGE,  "Sync Edge",         "edge", 0,   1, 1 },
   {   CLAMPTYPE,  "Clamp Type",   "clamptype", 0,   4, 1 },
//end of hidden block
   {         MUX,  "Sync on G/V",   "input_mux", 0,   1, 1 },
   {        RATE,  "Sample Mode", "sample_mode", 0, NUM_RGB_RATE-1, 1 },
   {   TERMINATE,  "75R Termination", "termination", 0,   NUM_RGB_TERM-1, 1 },
   {    COUPLING,  "G Coupling", "coupling", 0,   NUM_RGB_COUPLING-1, 1 },
   {       DAC_A,  "DAC-A: G Hi",     "dac_a", 0, 256, 1 },
   {       DAC_B,  "DAC-B: G Lo",     "dac_b", 0, 256, 1 },
   {       DAC_C,  "DAC-C: RB Hi",    "dac_c", 0, 256, 1 },
   {       DAC_D,  "DAC-D: RB Lo",    "dac_d", 0, 256, 1 },
   {       DAC_E,  "DAC-E: Sync",     "dac_e", 0, 256, 1 },
   {       DAC_F,  "DAC-F: G/V Sync",  "dac_f", 0, 256, 1 },
   {       DAC_G,  "DAC-G: G Clamp",  "dac_g", 0, 256, 1 },
   {       DAC_H,  "DAC-H: Unused",   "dac_h", 0, 256, 1 },
   {          -1,          NULL,          NULL, 0,   0, 1 }
};

// =============================================================
// Private methods
// =============================================================

static int get_adjusted_divider_index() {
    clk_info_t clkinfo;
    geometry_get_clk_params(&clkinfo);
    int clock =  clkinfo.clock;
    if (config->rate == RGB_RATE_6x2) {  //special double rate 12bpp mode
        clock <<= 1;
    }
    int divider_index = config->divider;
    while (divider_lookup[divider_index] * clock > MAX_CPLD_FREQUENCY && divider_index != 6) {
        divider_index = (divider_index - 1 ) & 7;
    }
    if (supports_odd_even && config->rate != RGB_RATE_3 && config->rate != RGB_RATE_9LO && config->rate != RGB_RATE_9HI && config->rate != RGB_RATE_12 && divider_index > 1) {
        divider_index = 1;
    }
    if (config->rate >= RGB_RATE_9V && config->rate <= RGB_RATE_12 && divider_index >= 6) {
        divider_index = 0;
    }

    return divider_index;
}

static void sendDAC(int dac, int value)
{
    if (value > 255) {
        value = 255;
    }
    int old_dac = -1;
    int old_value = value;
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
            old_dac = 0;
        break;
        case 3:
            M62364_dac = 0x02;
            old_dac = 1;
        break;
        case 4:
            M62364_dac = 0x0a;
        break;
        case 5:
            M62364_dac = 0x06;
            old_dac = 2;
        break;
        case 6:
            M62364_dac = 0x0e;
        break;
        case 7:
            M62364_dac = 0x01;
            old_dac = 3;
            switch (config->terminate) {
                  default:
                  case RGB_INPUT_HI:
                    old_value = 0;  //high impedance
                  break;
                  case RGB_INPUT_TERM:
                    old_value = 255; //termination
                  break;
            }
        break;
        default:
        break;
    }
    if (frontend == FRONTEND_ANALOG_ISSUE2_5259) {
        switch (dac) {
            case 6:
                dac = 7;
            break;
            case 7:
            {
                dac = 6;
                value = old_value;
            }
            break;
            default:
            break;
            }
    }
    switch (frontend) {
        case FRONTEND_ANALOG_ISSUE5:
        case FRONTEND_ANALOG_ISSUE4:
        case FRONTEND_ANALOG_ISSUE3_5259:  // max5259
        case FRONTEND_ANALOG_ISSUE2_5259:
        {
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
        break;

        case FRONTEND_ANALOG_ISSUE1_UA1:  // tlc5260 or tlv5260
        {
            //log_info("Issue 1A dac:%d = %d", old_dac, old_value);
            if (old_dac >= 0) {
                int packet = old_dac << 9 | old_value;
                RPI_SetGpioValue(STROBE_PIN, 1);
                delay_in_arm_cycles_cpu_adjust(1000);
                for (int i = 0; i < 11; i++) {
                    RPI_SetGpioValue(SP_CLKEN_PIN, 1);
                    RPI_SetGpioValue(SP_DATA_PIN, (packet >> 10) & 1);
                    delay_in_arm_cycles_cpu_adjust(500);
                    RPI_SetGpioValue(SP_CLKEN_PIN, 0);
                    delay_in_arm_cycles_cpu_adjust(500);
                    packet <<= 1;
                }
                RPI_SetGpioValue(STROBE_PIN, 0);
                delay_in_arm_cycles_cpu_adjust(500);
                RPI_SetGpioValue(STROBE_PIN, 1);
                RPI_SetGpioValue(SP_DATA_PIN, 0);
            }
        }
        break;

        case FRONTEND_ANALOG_ISSUE1_UB1:  // dac084s085
        {
            if (old_dac >= 0) {
                //log_info("Issue 1B dac:%d = %d", old_dac, old_value);
                int packet = (old_dac << 14) | 0x1000 | (old_value << 4);
                RPI_SetGpioValue(STROBE_PIN, 1);
                delay_in_arm_cycles_cpu_adjust(500);
                RPI_SetGpioValue(STROBE_PIN, 0);
                for (int i = 0; i < 16; i++) {
                    RPI_SetGpioValue(SP_CLKEN_PIN, 1);
                    RPI_SetGpioValue(SP_DATA_PIN, (packet >> 15) & 1);
                    delay_in_arm_cycles_cpu_adjust(500);
                    RPI_SetGpioValue(SP_CLKEN_PIN, 0);
                    delay_in_arm_cycles_cpu_adjust(500);
                    packet <<= 1;
                }
                RPI_SetGpioValue(SP_DATA_PIN, 0);
            }
        }
        break;
    }

}

static void write_config(config_t *config, int dac_update) {
   int sp = 0;
   int scan_len;
   if (supports_single_offset) {
       scan_len = supports_single_offset;
       int temp_offsets = config->all_offsets;
       if (config->half_px_delay == 0 && supports_single_offset == 4) {
           temp_offsets = (temp_offsets + (divider_lookup[get_adjusted_divider_index()] >> 1)) % divider_lookup[get_adjusted_divider_index()];
       }
       sp |= temp_offsets;
   } else {
       scan_len = 18;
       int range = divider_lookup[get_adjusted_divider_index()];
       if (supports_odd_even && range > 8) {
          range >>= 1;
       }

       for (int i = 0; i < NUM_OFFSETS; i++) {
          // Cycle the sample points taking account the pixel delay value
          // (if the CPLD support this, and we are in mode 7
          // delay = 0 : use ABCDEF
          // delay = 1 : use BCDEFA
          // delay = 2 : use CDEFAB
          // etc
          int offset = (supports_delay && capinfo->video_type == VIDEO_TELETEXT) ? (i + config->full_px_delay) % NUM_OFFSETS : i;
          sp |= (config->sp_offset[i] % range) << (offset * 3);
       }
   }
   if (supports_single_offset != 4) {
      sp |= (config->half_px_delay << scan_len);
      scan_len += 1;
   }

   if (supports_delay) {
      // We usually make use of 2 bits of delay, even in CPLDs that use more
      if (config->rate == RGB_RATE_1 || config->rate == RGB_RATE_6x2) {
          sp |= ((config->full_px_delay & 7) << scan_len);  // only use 3rd bit if 1bpp or 6x2
      } else {
          sp |= ((config->full_px_delay & 3) << scan_len);  // maintain h-offset compatibility with other CPLD versions by not using 3rd bit unless necessary
      }
      scan_len += supports_delay; // 2, 3 or 4 depending on the CPLD version
   }
   if (supports_rate) {
      int temprate = 0;

      if (RGB_CPLD_detected) {
          switch (config->rate) {
            default:
            case RGB_RATE_1:
                temprate = 0x04;     // selects 1bpp and also signals 3 bits of delay used
                break;
            case RGB_RATE_3:
                temprate = 0x00;
                break;
            case RGB_RATE_6:
                temprate = 0x01;
                break;
            case RGB_RATE_6x2:
                temprate = 0x06;     // selects 6x2 and also signals 3 bits of delay used
                break;
            case RGB_RATE_4_LEVEL:
                temprate = 0x02;
                break;
            case RGB_RATE_9V:
            case RGB_RATE_9LO:
            case RGB_RATE_9HI:
            case RGB_RATE_12:
                temprate = 0x03;
                break;
          }
      } else {
          switch (config->rate) {
            default:
            case RGB_RATE_9V:
            case RGB_RATE_3:
                temprate = 0x00;
                break;
            case RGB_RATE_9LO:
            case RGB_RATE_9HI:
            case RGB_RATE_12:
            case RGB_RATE_6:
                temprate = 0x01;
                break;
          }
          int range = divider_lookup[get_adjusted_divider_index()];
          if (supports_odd_even && range > 8) {  // if divider > x6 or x8 then select odd/even mode
            if (config->all_offsets >= (range >> 1)) {
                temprate = 0x03;  //odd
            } else {
                temprate = 0x02;  //even
            }
          }
    //      log_info ("RATE = %d", temprate);
      }

      sp |= (temprate << scan_len);
      scan_len += supports_rate;  // 1 - 3 depending on the CPLD version
   }
   if (supports_invert) {
      sp |= (invert << scan_len);
      scan_len += 1;
   }

   if (supports_divider) {
      if (supports_divider == 3) {
          sp |= (divider_register[get_adjusted_divider_index()] << scan_len);
      } else {
          sp |= (((divider_lookup[get_adjusted_divider_index()] == 8 || divider_lookup[get_adjusted_divider_index()] == 16) & 1 ) << scan_len);
      }
      scan_len += supports_divider;
   }

   if (supports_mux) {
      sp |= (config->mux << scan_len);
      scan_len += 1;
   }

   //log_info("scan = %X, %d",sp, scan_len);

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

   if (supports_analog) {
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
              if (supports_8bit && config->rate == RGB_RATE_4_LEVEL) {
                 dac_g = dac_c;
              } else {
                 dac_g = 0;
              }
           }

           int dac_h = config->dac_h;
           if (dac_h == 256) dac_h = dac_c;

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
          case RGB_INPUT_HI:
            RPI_SetGpioValue(SP_CLKEN_PIN, 0);  //termination
          break;
          case RGB_INPUT_TERM:
            RPI_SetGpioValue(SP_CLKEN_PIN, 1);
          break;
       }

      int coupling = config->coupling;
      if (frontend == FRONTEND_ANALOG_ISSUE2_5259 || frontend == FRONTEND_ANALOG_ISSUE1_UA1 || frontend == FRONTEND_ANALOG_ISSUE1_UB1) {
          coupling = RGB_INPUT_AC;                  // always ac coupling with issue 1 or 2
      }
      if (RGB_CPLD_detected) {
          switch(config->rate) {
                      default:
              case RGB_RATE_1:
              case RGB_RATE_3:
              case RGB_RATE_6:
                    RPI_SetGpioValue(SP_DATA_PIN, coupling);      //ac-dc
                    break;
              case RGB_RATE_9V:
              case RGB_RATE_4_LEVEL:
                    RPI_SetGpioValue(SP_DATA_PIN, 0);    //dc only in 4 level mode or enable RGB_RATE_9V
                    break;
              case RGB_RATE_6x2:
              case RGB_RATE_9LO:
              case RGB_RATE_9HI:
              case RGB_RATE_12:
                    RPI_SetGpioValue(SP_DATA_PIN, 1);    //enable 12 bit mode
                    break;
          }
      } else {
          switch(config->rate) {
                      default:
              case RGB_RATE_3:
              case RGB_RATE_6:
                    if (supports_bbc_9_12) {
                        RPI_SetGpioValue(SP_DATA_PIN, 0);   //no ac coupling
                    } else {
                        RPI_SetGpioValue(SP_DATA_PIN, coupling);
                    }
                    break;
              case RGB_RATE_9V:
              case RGB_RATE_9LO:
              case RGB_RATE_9HI:
              case RGB_RATE_12:
                    RPI_SetGpioValue(SP_DATA_PIN, 1);        //enable 12 bit mode
                    break;
          }
      }
   } else {
      if (RGB_CPLD_detected) {
          switch(config->rate) {
                      default:
              case RGB_RATE_1:
              case RGB_RATE_3:
              case RGB_RATE_6:
              case RGB_RATE_9V:
              case RGB_RATE_4_LEVEL:
                    RPI_SetGpioValue(SP_DATA_PIN, 0);
                    break;
              case RGB_RATE_6x2:
              case RGB_RATE_9LO:
              case RGB_RATE_9HI:
              case RGB_RATE_12:
                    RPI_SetGpioValue(SP_DATA_PIN, 1);
                    break;
          }
      } else {
          switch(config->rate) {
                      default:
              case RGB_RATE_3:
              case RGB_RATE_6:
                    RPI_SetGpioValue(SP_DATA_PIN, 0);
                    break;
              case RGB_RATE_9V:
              case RGB_RATE_9LO:
              case RGB_RATE_9HI:
              case RGB_RATE_12:
                    RPI_SetGpioValue(SP_DATA_PIN, 1);        //enable 12 bit mode
                    break;
          }
      }
      RPI_SetGpioValue(SP_CLKEN_PIN, 0);
   }
   RPI_SetGpioValue(MUX_PIN, config->mux);    //only required for old CPLDs - has no effect on newer ones
}

static int osd_sp(config_t *config, int line, int metric) {
   int range = divider_lookup[get_adjusted_divider_index()];
   int offset_range = 0;
   if (supports_odd_even && range > 8) {
       range >>= 1;
       offset_range = config->all_offsets >= range ? range : 0;
   }
   // Line ------
   if (mode7) {
      osd_set(line, 0, "            Mode: 7");
   } else {
      osd_set(line, 0, "            Mode: default");
   }
   line++;
   // Line ------
   if (mode7 && !RGB_CPLD_detected) {
       sprintf(message, " Sampling Phases: %d %d %d %d %d %d",
                 config->sp_offset[0] % range + offset_range, config->sp_offset[1] % range + offset_range, config->sp_offset[2] % range + offset_range,
                 config->sp_offset[3] % range + offset_range, config->sp_offset[4] % range + offset_range, config->sp_offset[5] % range + offset_range);
   } else {
       sprintf(message, "  Sampling Phase: %d", config->all_offsets);
   }
   osd_set(line, 0, message);

   line++;
   // Line ------
   sprintf(message,    "Half Pixel Shift: %d", config->half_px_delay);
   osd_set(line, 0, message);
   line++;
   // Line ------
   sprintf(message,    "Clock Multiplier: %s", divider_names[get_adjusted_divider_index()]);
   osd_set(line, 0, message);
   line++;
   // Line ------
   if (supports_delay) {
      sprintf(message, "  Pixel H Offset: %d", config->full_px_delay);
      osd_set(line, 0, message);
      line++;
   }
   // Line ------
   if (supports_rate) {
      char *rate_string;
      rate_string = (char *) rate_names[config->rate];
      sprintf(message, "     Sample Mode: %s", rate_string);
      osd_set(line, 0, message);
      line++;
   }
   // Line ------
   if (metric < 0) {
      sprintf(message, "          Errors: unknown");
   } else {
      sprintf(message, "          Errors: %d", metric);
   }
   osd_set(line, 0, message);

   return(line);
}

static void log_sp(config_t *config) {
   int range = divider_lookup[get_adjusted_divider_index()];
   int offset_range = 0;
   if (supports_odd_even && range > 8) {
       range >>= 1;
       offset_range = config->all_offsets >= range ? range : 0;
   }
   char *mp = message;
   mp += sprintf(mp, "phases = %d %d %d %d %d %d; half = %d",
                 config->sp_offset[0] % range + offset_range, config->sp_offset[1] % range + offset_range, config->sp_offset[2] % range + offset_range,
                 config->sp_offset[3] % range + offset_range, config->sp_offset[4] % range + offset_range, config->sp_offset[5] % range + offset_range,
                 config->half_px_delay);
   if (supports_delay) {
      mp += sprintf(mp, "; pixel h offset = %d", config->full_px_delay);
   }

   if (supports_divider) {
      mp += sprintf(mp, "; mult = %d", get_adjusted_divider_index());
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
// hide YUV lines included for file compatibility
   params[FILTER_L].hidden = 1;
   params[SUB_C].hidden = 1;
   params[ALT_R].hidden = 1;
   params[EDGE].hidden = 1;
   params[CLAMPTYPE].hidden = 1;
   params[DIVIDER].max = 1;
   if (eight_bit_detected()) {
       supports_8bit = 1;
   }

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
      params[DELAY].hidden = 1;
   }

   // Optional rate parameter
   // CPLDv3 supports a 1-bit rate, CPLDv4 and beyond supports a 2-bit rate
   if (major >= 4) {
      supports_rate = 2;
      params[RATE].max = RGB_RATE_6;
      supports_odd_even = 1;
      params[DIVIDER].max = 7;
   } else if (major >= 3) {
      supports_rate = 1;
      params[RATE].max = RGB_RATE_6;
   } else {
      supports_rate = 0;
      params[RATE].hidden = 1;
   }
   // Optional invert parameter
   // CPLDv5 and beyond support an invertion of sync
   if (major >= 5) {
      supports_invert = 1;
   }
   if (major >= 6) {
      supports_vsync = 1;
   }

   //*******************************************************************************************************************************
   if ((major >= 6 && minor >= 4) || major >= 7) {
      supports_separate = 1;
   }

   if (major == 6 && minor >= 7) {
      supports_invert = 0;           //BBC cpld replaces invert with divider in chain
      supports_vsync = 0;
      supports_separate = 0;
      supports_divider = 1;
   }

   if (major == 7 && minor >= 6) {    //BBC cpld
      supports_invert = 0;            //BBC cpld replaces invert with divider in chain
      supports_vsync = 0;
      supports_separate = 0;
      supports_divider = 1;
      params[MUX].hidden = 1;
      if (minor >= 8) {
         if (supports_8bit && !supports_analog) {
            params[RATE].max = RGB_RATE_12;
         }
         supports_bbc_9_12 = 1;
      }
   }

   if (major >= 8) {           // V8 CPLD branch is deprecated
      RGB_CPLD_detected = 1;
      params[DIVIDER].max = 1;
      params[RATE].max = RGB_RATE_4_LEVEL;
      supports_odd_even = 0;
      supports_divider = 1;
      supports_mux = 1;
   }

   if (major == 9) {
      RGB_CPLD_detected = 1;
      params[RATE].max = RGB_RATE_1;
      params[DIVIDER].max = 1;
      supports_odd_even = 0;
      supports_mux = 1;
      supports_single_offset = 3;
      supports_rate = 3;
      supports_delay = 3;
      supports_divider = 2;
   }

   if (major == 9 && minor >= 2) {
      params[DIVIDER].max = 7;
      supports_divider = 3;
      supports_single_offset = 4;
   }

   //*******************************************************************************************************************************

   // Hide analog frontend parameters.
   if (!supports_analog) {
       params[TERMINATE].hidden = 1;
       params[COUPLING].hidden = 1;
       params[DAC_A].hidden = 1;
       params[DAC_B].hidden = 1;
       params[DAC_C].hidden = 1;
       params[DAC_D].hidden = 1;
       params[DAC_E].hidden = 1;
       params[DAC_F].hidden = 1;
       params[DAC_G].hidden = 1;
   }

   params[DAC_H].hidden = 1;

   if (major > 2) {
       geometry_hide_pixel_sampling();
   }

   for (int i = 0; i < NUM_OFFSETS; i++) {
      default_config.sp_offset[i] = 2;
      mode7_config.sp_offset[i] = 5;
   }
   default_config.half_px_delay = 0;
   mode7_config.half_px_delay   = 0;
   default_config.divider       = 0;
   mode7_config.divider         = 1;
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
   config->cpld_setup_mode = 0;
}

static int cpld_get_version() {
   return cpld_version;
}

static void update_param_range() {
   int max;
   // Set the range of the offset params based on cpld divider
   max = divider_lookup[get_adjusted_divider_index()] - 1;
   params[ALL_OFFSETS].max = max;
   if (config->all_offsets > max) {
      config->all_offsets = max;
   }
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

   if (supports_divider) {
       RPI_SetGpioValue(MODE7_PIN, mode7);
   } else {
       RPI_SetGpioValue(MODE7_PIN, (max == 7 || max == 15));
   }
}

static void cpld_set_mode(int mode) {
   mode7 = mode;
   config = mode ? &mode7_config : &default_config;
   write_config(config, DAC_UPDATE);
   // Update the OSD param ranges based on the new config
   update_param_range();
}

static void cpld_set_vsync_psync(int state) {  //state = 1 is psync (version = 1), state = 0 is vsync (version = 0, mux = 1)
   if (supports_analog) {
       int temp_mux = config->mux;
       if (state == 0) {
           config->mux = 1;
       }
       write_config(config, NO_DAC_UPDATE);
       config->mux = temp_mux;
   }
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

      switch(config->rate) {
          default:
          case RGB_RATE_1:
                capinfo->sample_width = SAMPLE_WIDTH_1;
                break;
          case RGB_RATE_3:
                capinfo->sample_width = SAMPLE_WIDTH_3;
            break;
          case RGB_RATE_6:
          case RGB_RATE_4_LEVEL:
                capinfo->sample_width = SAMPLE_WIDTH_6;
                break;
          case RGB_RATE_9V:
          case RGB_RATE_9HI:
                capinfo->sample_width = SAMPLE_WIDTH_9HI;
                break;
          case RGB_RATE_9LO:
                capinfo->sample_width = SAMPLE_WIDTH_9LO;
                break;
          case RGB_RATE_6x2:
          case RGB_RATE_12:
                capinfo->sample_width = SAMPLE_WIDTH_12;
                break;
      }

      // Update the line capture function
        switch (capinfo->sample_width) {
            case SAMPLE_WIDTH_1 :
                    capinfo->capture_line = capture_line_normal_1bpp_table;
            break;
            case SAMPLE_WIDTH_3:
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
            break;
            case SAMPLE_WIDTH_6 :
                    capinfo->capture_line = capture_line_normal_6bpp_table;
            break;
            case SAMPLE_WIDTH_9LO :
                    capinfo->capture_line = capture_line_normal_9bpplo_table;
            break;
            case SAMPLE_WIDTH_9HI :
                    capinfo->capture_line = capture_line_normal_9bpphi_table;
            break;
            case SAMPLE_WIDTH_12 :
                    capinfo->capture_line = capture_line_normal_12bpp_table;
            break;
        }
    }
    write_config(config, DAC_UPDATE);
}

static param_t *cpld_get_params() {
    int hide_offsets = RGB_CPLD_detected | !mode7;
    params[A_OFFSET].hidden = hide_offsets;
    params[B_OFFSET].hidden = hide_offsets;
    params[C_OFFSET].hidden = hide_offsets;
    params[D_OFFSET].hidden = hide_offsets;
    params[E_OFFSET].hidden = hide_offsets;
    params[F_OFFSET].hidden = hide_offsets;
    params[RANGE].hidden = hide_offsets;
   return params;
}

static int cpld_get_value(int num) {
   switch (num) {

   case CPLD_SETUP_MODE:
      return config->cpld_setup_mode;
   case ALL_OFFSETS:
      return config->all_offsets;
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
   case RANGE:
      return config->range;
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
   case TERMINATE:
      return config->terminate;
   case COUPLING:
      return config->coupling;

   case FILTER_L:
      return config->filter_l;
   case SUB_C:
      return config->sub_c;
   case ALT_R:
      return config->alt_r;
   case EDGE:
      return config->edge;
   case CLAMPTYPE:
      return config->clamptype;

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
   if (num == DIVIDER) {
      return divider_names[config->divider];
   }
   if (num == COUPLING) {
      return coupling_names[config->coupling];
   }
   if (num == RANGE) {
      return range_names[config->range];
   }
   if (num >= DAC_A && num <= DAC_H) {
      return volt_names[cpld_get_value(num)];
   }
   if (num >= ALL_OFFSETS && num <= F_OFFSET) {
      sprintf(phase_text, "%d (%d Degrees)", cpld_get_value(num), cpld_get_value(num) * 360 / divider_lookup[config->divider]);
      return phase_text;
   }

   return NULL;
}

static int offset_fixup(int value) {
    int range = divider_lookup[get_adjusted_divider_index()];
    if (supports_odd_even && range > 8) {
        range >>= 1;
        int odd_even_offset = config->all_offsets - (config->all_offsets % range);
        return (value % range) + odd_even_offset;
    } else {
        return value;
    }
}

static void cpld_set_value(int num, int value) {
   if (value < params[num].min) {
      value = params[num].min;
   }
   if (value > params[num].max && (num < ALL_OFFSETS || num > F_OFFSET)) { //don't clip offsets because the max value could change after the values are written when loading a new profile if the divider is different
      value = params[num].max;
   }
   switch (num) {
   case CPLD_SETUP_MODE:
      config->cpld_setup_mode = value;
      set_setup_mode(value);
      break;
   case ALL_OFFSETS:
      config->all_offsets = value;
      config->sp_offset[0] = offset_fixup(value);
      config->sp_offset[1] = offset_fixup(value);
      config->sp_offset[2] = offset_fixup(value);
      config->sp_offset[3] = offset_fixup(value);
      config->sp_offset[4] = offset_fixup(value);
      config->sp_offset[5] = offset_fixup(value);
      break;
   case A_OFFSET:
      config->sp_offset[0] = offset_fixup(value);
      break;
   case B_OFFSET:
      config->sp_offset[1] = offset_fixup(value);
      break;
   case C_OFFSET:
      config->sp_offset[2] = offset_fixup(value);
      break;
   case D_OFFSET:
      config->sp_offset[3] = offset_fixup(value);
      break;
   case E_OFFSET:
      config->sp_offset[4] = offset_fixup(value);
      break;
   case F_OFFSET:
      config->sp_offset[5] = offset_fixup(value);
      break;
   case HALF:
      config->half_px_delay = value;
      break;
   case DIVIDER:
      if (supports_odd_even) {
         if (value > config->divider) {
             if (value == DIVIDER_10) {
                 value = DIVIDER_12;
             } else if (value == DIVIDER_14) {
                 value = DIVIDER_16;
             } else if (value == DIVIDER_3) {
                 value = DIVIDER_6;
             } else if (value == DIVIDER_4) {
                 value = DIVIDER_16;
             }
         } else {
             if (value == DIVIDER_10) {
                 value = DIVIDER_8;
             } else if (value == DIVIDER_14) {
                 value = DIVIDER_12;
             } else if (value == DIVIDER_3) {
                 value = DIVIDER_16;
             } else if (value == DIVIDER_4) {
                 value = DIVIDER_6;
             }
         }
      }
      config->divider = value;
      update_param_range();
      int actual_value = get_adjusted_divider_index();
      if (actual_value != value) {
          char msg[64];
          if (supports_odd_even && config->rate != RGB_RATE_3 && config->rate != RGB_RATE_9LO && config->rate != RGB_RATE_9HI && config->rate != RGB_RATE_12){
              sprintf(msg, "1 Bit, 6 Bit & 9 Bit(V) Limited to %s", divider_names[actual_value]);
          } else if (config->rate >= RGB_RATE_9V && config->rate <= RGB_RATE_12 && value >= 6) {
              sprintf(msg, "Can't use %s with 9 or 12 BPP, using %s", divider_names[value], divider_names[actual_value]);
          } else {
              sprintf(msg, "Clock > 192MHz: Limited to %s", divider_names[actual_value]);
          }
          set_status_message(msg);
      }
      break;
   case RANGE:
      config->range = value;
      break;
   case DELAY:
      config->full_px_delay = value;
      break;
   case RATE:
      if (!supports_8bit || supports_analog) {
          if (value > config->rate) {
             if (value == RGB_RATE_9V) {
                 value = RGB_RATE_6x2;
             }
          } else {
             if (value == RGB_RATE_12) {
                 value = RGB_RATE_6;
             }
          }
      }
      config->rate = value;
      if (supports_analog) {
          if (supports_8bit) {
              if (RGB_CPLD_detected && config->rate == RGB_RATE_4_LEVEL) {
                 params[DAC_H].hidden = 0;
                 params[COUPLING].hidden = 1;
                 params[DAC_F].label = "DAC-F: G Mid";
                 params[DAC_G].label = "DAC-G: R Mid";
                 params[DAC_H].label = "DAC-H: B Mid";
              } else {
                 params[DAC_H].hidden = 1;
                 params[COUPLING].hidden = 0;
                 params[DAC_F].label = "DAC-F: G V Sync";
                 params[DAC_G].label = "DAC-G: G Clamp";
                 params[DAC_H].label = "DAC-H: Unused";
              }
          }
          if (!supports_bbc_9_12 && (config->rate == RGB_RATE_1 || config->rate == RGB_RATE_3 || config->rate == RGB_RATE_6)) {
              params[COUPLING].hidden = 0;
          } else {
              params[COUPLING].hidden = 1;
          }
      }
      osd_refresh();
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
   case TERMINATE:
      config->terminate = value;
      break;
   case COUPLING:
      config->coupling = value;
      break;

   case FILTER_L:
      config->filter_l = value;
      break;
   case SUB_C:
      config->sub_c = value;
      break;
   case ALT_R:
      config->alt_r = value;
      break;
   case EDGE:
      config->edge = value;
      break;
   case CLAMPTYPE:
      config->clamptype = value;
      break;

   }
   write_config(config, DAC_UPDATE);
}


static void cpld_calibrate_sub(capture_info_t *capinfo, int elk, int (*raw_metrics)[16][NUM_OFFSETS], int (*sum_metrics)[16], int *errors) {
   int min_i = 0;
   int metric;         // this is a point value (at one sample offset)
   int min_metric;
   int win_metric;     // this is a windowed value (over three sample offsets)
   int min_win_metric;
   int *by_sample_metrics;
   int range;          // 0..5 in Modes 0..6, 0..7 in Mode 7
   int oddeven = 0;
   int offset_range = 0;
   char msg[256];
   int msgptr = 0;

   range = divider_lookup[get_adjusted_divider_index()];
   if (supports_odd_even && range > 8) {
       oddeven = 1;
       range >>= 1;
       offset_range = config->all_offsets >= range ? range : 0;
   }

   // Measure the error metrics at all possible offset values
   min_metric = INT_MAX;
   if (!oddeven) {  // if mode 7 cpld using odd/even then let caller set config->half_px_delay as it is actually a quarter pixel delay
      config->half_px_delay = 0;
   }
   config->full_px_delay = 0;
   msgptr = 0;
   msgptr += sprintf(msg, "INFO:                      ");
   for (int i = 0; i < NUM_OFFSETS; i++) {
      msgptr += sprintf(msg + msgptr ,"%7c", 'A' + i);
   }
   sprintf(msg + msgptr, "   total");
   log_info(msg);
   for (int value = 0; value < range; value++) {
      for (int i = 0; i < NUM_OFFSETS; i++) {
         config->sp_offset[i] = value;
      }
      config->all_offsets = config->sp_offset[0] + offset_range;
      write_config(config, DAC_UPDATE);
      by_sample_metrics = diff_N_frames_by_sample(capinfo, NUM_CAL_FRAMES, mode7, elk);
      metric = 0;
      msgptr = 0;
      msgptr += sprintf(msg + msgptr, "INFO: value = %d: metrics = ", value + offset_range);
      for (int i = 0; i < NUM_OFFSETS; i++) {
         (*raw_metrics)[value][i] = by_sample_metrics[i];
         metric += by_sample_metrics[i];
         msgptr += sprintf(msg + msgptr, "%7d", by_sample_metrics[i]);
      }
      msgptr += sprintf(msg + msgptr, "%8d", metric);
      log_info(msg);
      (*sum_metrics)[value] = metric;
      osd_sp(config, 2, metric);
      if (capinfo->bpp == 16) {
         unsigned int flags = extra_flags() | mode7 | BIT_CALIBRATE | (2 << OFFSET_NBUFFERS);
         rgb_to_fb(capinfo, flags);  //restore OSD
         delay_in_arm_cycles_cpu_adjust(1000000000);
      }
      if (metric < min_metric) {
         min_metric = metric;
      }
   }

   // Use a 3 sample window to find the minimum and maximum
   min_win_metric = INT_MAX;
   for (int i = 0; i < range; i++) {
      int left  = (i - 1 + range) % range;
      int right = (i + 1 + range) % range;
      win_metric = (*sum_metrics)[left] + (*sum_metrics)[i] + (*sum_metrics)[right];
      if ((*sum_metrics)[i] == min_metric) {
         if (win_metric < min_win_metric) {
            min_win_metric = win_metric;
            min_i = i;
         }
      }
   }

   // If the min metric is at the limit, make use of the half pixel delay
   if (!RGB_CPLD_detected && capinfo->video_type == VIDEO_TELETEXT && range >= 6 && min_metric > 0 && (min_i <= 1 || min_i >= (range - 2))) {
      if (!oddeven && range > 4) {  //do not select half if odd/even rate as it is actually a quarter pixel shift
          log_info("Enabling half pixel delay for metric %d, range = %d", min_i, range);
          config->half_px_delay = 1;
          min_i = (min_i + (range >> 1)) % range;
          log_info("Adjusted metric = %d", min_i);
          // Swap the metrics as well
          for (int i = 0; i < (range >> 1); i++) {
             for (int j = 0; j < NUM_OFFSETS; j++)  {
                int tmp = (*raw_metrics)[i][j];
                (*raw_metrics)[i][j] = (*raw_metrics)[(i + (range >> 1)) % range][j];
                (*raw_metrics)[(i + (range >> 1)) % range][j] = tmp;
             }
          }
          for (int i = 0; i < (range >> 1); i++) {
             int tmp = (*sum_metrics)[i];
             (*sum_metrics)[i] = (*sum_metrics)[(i + (range >> 1)) % range];
             (*sum_metrics)[(i + (range >> 1)) % range] = tmp;
          }
      }
   }

   // In all modes, start with the min metric
   for (int i = 0; i < NUM_OFFSETS; i++) {
      config->sp_offset[i] = min_i;
   }

   // If the metric is non zero, there is scope for further optimization in mode7
   if (!RGB_CPLD_detected && mode7 && min_metric > 0) {
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
         if (value < (range - 1)) {
            right = (*raw_metrics)[value + 1][i];
         }
         // Make the actual decision
         if (left < right && left < ref) {
            config->sp_offset[i]--;
         } else if (right < left && right < ref) {
            config->sp_offset[i]++;
         }
      }
   }

   for (int i = 0; i < NUM_OFFSETS; i++) {
      config->sp_offset[i] = config->sp_offset[i] + offset_range;
   }
   config->all_offsets = config->sp_offset[0];
   write_config(config, DAC_UPDATE);
   *errors = diff_N_frames(capinfo, NUM_CAL_FRAMES, mode7, elk);
   osd_sp(config, 2, *errors);
   log_sp(config);
   log_info("Calibration pass complete, retested errors = %d", *errors);
}

static void cpld_calibrate(capture_info_t *capinfo, int elk) {
   int (*raw_metrics)[16][NUM_OFFSETS];
   int (*sum_metrics)[16];
   int *errors;
   config_t min_config;
   int min_raw_metrics[16][NUM_OFFSETS];
   int min_sum_metrics[16];
   int min_errors = 0x70000000;
   int old_full_px_delay;

   if (mode7) {
      log_info("Calibrating mode: 7");
      raw_metrics = &raw_metrics_mode7;
      sum_metrics = &sum_metrics_mode7;
      errors      = &errors_mode7;
   } else {
      log_info("Calibrating mode: default");
      raw_metrics = &raw_metrics_default;
      sum_metrics = &sum_metrics_default;
      errors      = &errors_default;
   }
    old_full_px_delay = config->full_px_delay;
    int multiplier = divider_lookup[get_adjusted_divider_index()];
    if (supports_odd_even) { // odd even modes in BBC CPLD only
        if (mode7 && config->range == RANGE_AUTO && multiplier != 16) {     //don't auto range if multiplier set to x16
            log_info("Auto range: First setting to x8 multiplier in mode 7");
            cpld_set_value(DIVIDER, DIVIDER_8);
            calibrate_sampling_clock(0);
            multiplier = divider_lookup[get_adjusted_divider_index()];
        }
        if (multiplier <= 8) {
            cpld_calibrate_sub(capinfo, elk, raw_metrics, sum_metrics, errors);
            if (mode7 && config->range == RANGE_AUTO && *errors != 0) {
                log_info("Auto range: trying to increase to x12 multiplier in mode 7");
                cpld_set_value(DIVIDER, DIVIDER_12);
                calibrate_sampling_clock(0);
                multiplier = divider_lookup[get_adjusted_divider_index()];
            }
        }
        if (multiplier > 8) {
            int range = multiplier >> 1;
            for (int i = 0; i < 4; i++) {
                switch (i){
                    case 0:
                    config->all_offsets = 0;
                    config->half_px_delay = 0;
                    break;
                    case 1:
                    config->all_offsets = range;
                    config->half_px_delay = 0;
                    break;
                    case 2:
                    config->all_offsets = 0;
                    config->half_px_delay = 1;
                    break;
                    case 3:
                    config->all_offsets = range;
                    config->half_px_delay = 1;
                    break;
                }
                cpld_calibrate_sub(capinfo, elk, raw_metrics, sum_metrics, errors);
                int all_offsets = config->all_offsets % range;
                if (*errors < min_errors || (*errors == min_errors && all_offsets > 0 && all_offsets < (range - 1))) {
                    min_errors = *errors;
                    memcpy(&min_config, config, sizeof min_config);
                    memcpy(&min_raw_metrics, raw_metrics, sizeof min_raw_metrics);
                    memcpy(&min_sum_metrics, sum_metrics, sizeof min_sum_metrics);
                    log_info("Current minimum: Phase = %d, Half = %d", config->all_offsets, config->half_px_delay);
                }
            }
            *errors = min_errors;
            memcpy(config, &min_config, sizeof min_config);
            memcpy(raw_metrics, &min_raw_metrics, sizeof min_raw_metrics);
            memcpy(sum_metrics, &min_sum_metrics, sizeof min_sum_metrics);
            write_config(config, DAC_UPDATE);
        }
    } else {
        cpld_calibrate_sub(capinfo, elk, raw_metrics, sum_metrics, errors);
    }
    unsigned int flags = extra_flags() | mode7 | BIT_CALIBRATE | (2 << OFFSET_NBUFFERS);
    rgb_to_fb(capinfo, flags);  //restore OSD
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
      write_config(config, DAC_UPDATE);
    }
    osd_sp(config, 2, *errors);
    log_sp(config);
    rgb_to_fb(capinfo, flags);  //restore OSD
    log_info("Calibration complete, errors = %d", *errors);
    delay_in_arm_cycles_cpu_adjust(1000000000);
}

static int cpld_show_cal_summary(int line) {
   return osd_sp(config, line, mode7 ? errors_mode7 : errors_default);
}

static void cpld_show_cal_details(int line) {
   int *sum_metrics = mode7 ? sum_metrics_mode7 : sum_metrics_default;
   int range = (*sum_metrics < 0) ? 0 : divider_lookup[get_adjusted_divider_index()];
   int offset_range = 0;
   if (range == 0) {
      sprintf(message, "No calibration data for this mode");
      osd_set(line, 0, message);
   } else {
      if (supports_odd_even && range > 8) {
          range >>= 1;
          offset_range = config->all_offsets >= range ? range : 0;
      }
      for (int value = 0; value < range; value++) {
         sprintf(message, "Phase %2d: Errors = %6d", value + offset_range, sum_metrics[value]);
         osd_set(line + value, 0, message);
      }
   }
}

static void cpld_show_cal_raw(int line) {
   int (*raw_metrics)[16][NUM_OFFSETS] = mode7 ? &raw_metrics_mode7 : &raw_metrics_default;
   int range = ((*raw_metrics)[0][0] < 0) ? 0 : divider_lookup[get_adjusted_divider_index()];
   int offset_range = 0;
   if (range == 0) {
      sprintf(message, "No calibration data for this mode");
      osd_set(line, 0, message);
   } else {
   if (supports_odd_even && range > 8) {
       range >>= 1;
       offset_range = config->all_offsets >= range ? range : 0;
   }
      for (int value = 0; value < range; value++) {
         char *mp = message;
         mp += sprintf(mp, "%2d:", value + offset_range);
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
    if (config->rate == RGB_RATE_6x2) {  //special double rate 12bpp mode
        return divider_lookup[get_adjusted_divider_index()] << 1;
    } else {
        return divider_lookup[get_adjusted_divider_index()];
    }
}

static int cpld_get_delay() {
    if (config->rate == RGB_RATE_1) {
        return cpld_get_value(DELAY) & 0x08;         // mask out lower 3 bits of delay
    } else if (config->rate == RGB_RATE_6x2) {  //special double rate 12bpp mode
        return (cpld_get_value(DELAY) & 0x08) >> 1;  // mask out lower 3 bits of delay
    } else {
        return cpld_get_value(DELAY) & 0x0c;         // mask out lower 2 bits of delay
    }
}

static int cpld_get_sync_edge() {
    return 0;                      // always trailing edge in rgb cpld
}

static void cpld_set_frontend(int value) {
}

// =============================================================
//  BBC Driver Specific
// =============================================================

static void cpld_init_bbc(int version) {
   supports_analog = 0;
   cpld_init(version);
   params[MUX].label = "Input Mux";
}

static int cpld_frontend_info_bbc() {
    return FRONTEND_TTL_3BIT | FRONTEND_TTL_3BIT << 16;
}

cpld_t cpld_bbc = {
   .name = "3-12_BIT_BBC",
   .default_profile = "BBC_Micro",
   .init = cpld_init_bbc,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .set_vsync_psync = cpld_set_vsync_psync,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_bbc,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .get_sync_edge = cpld_get_sync_edge,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};

cpld_t cpld_bbcv10v20 = {
   .name = "Legacy_3_BIT",
   .default_profile = "BBC_Micro_v10-v20",
   .init = cpld_init_bbc,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .set_vsync_psync = cpld_set_vsync_psync,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_bbc,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .get_sync_edge = cpld_get_sync_edge,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};

cpld_t cpld_bbcv21v23 = {
   .name = "Legacy_3_BIT",
   .default_profile = "BBC_Micro_v21-v23",
   .init = cpld_init_bbc,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .set_vsync_psync = cpld_set_vsync_psync,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_bbc,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .get_sync_edge = cpld_get_sync_edge,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};

cpld_t cpld_bbcv24 = {
   .name = "Legacy_3_BIT",
   .default_profile = "BBC_Micro_v24",
   .init = cpld_init_bbc,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .set_vsync_psync = cpld_set_vsync_psync,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_bbc,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .get_sync_edge = cpld_get_sync_edge,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};

cpld_t cpld_bbcv30v62 = {
   .name = "Legacy_3_BIT",
   .default_profile = "BBC_Micro_v30-v62",
   .init = cpld_init_bbc,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .set_vsync_psync = cpld_set_vsync_psync,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_bbc,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .get_sync_edge = cpld_get_sync_edge,
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
    return FRONTEND_TTL_6_8BIT | FRONTEND_TTL_6_8BIT << 16;
}

cpld_t cpld_rgb_ttl = {
   .name = "6-12_BIT_RGB",
   .default_profile = "Acorn_Electron",
   .init = cpld_init_rgb_ttl,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .set_vsync_psync = cpld_set_vsync_psync,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_rgb_ttl,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .get_sync_edge = cpld_get_sync_edge,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};

cpld_t cpld_rgb_ttl_24mhz = {
   .name = "3-12_BIT_BBC",
   .default_profile = "BBC_Micro",
   .init = cpld_init_rgb_ttl,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .set_vsync_psync = cpld_set_vsync_psync,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_rgb_ttl,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .get_sync_edge = cpld_get_sync_edge,
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
    if (new_DAC_detected() == 1) {
        return FRONTEND_ANALOG_ISSUE4 | FRONTEND_ANALOG_ISSUE4 << 16;
    } else if (new_DAC_detected() == 2) {
        return FRONTEND_ANALOG_ISSUE5 | FRONTEND_ANALOG_ISSUE5 << 16;
    } else {
        return FRONTEND_ANALOG_ISSUE3_5259 | FRONTEND_ANALOG_ISSUE1_UB1 << 16;
    }
}

static void cpld_set_frontend_rgb_analog(int value) {
   frontend = value;
   write_config(config, DAC_UPDATE);
}

cpld_t cpld_rgb_analog = {
   .name = "6-12_BIT_RGB_Analog",
   .default_profile = "Amstrad_CPC",
   .init = cpld_init_rgb_analog,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .set_vsync_psync = cpld_set_vsync_psync,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_rgb_analog,
   .set_frontend = cpld_set_frontend_rgb_analog,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .get_sync_edge = cpld_get_sync_edge,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};


cpld_t cpld_rgb_analog_24mhz = {
   .name = "3-12_BIT_BBC_Analog",
   .default_profile = "BBC_Micro",
   .init = cpld_init_rgb_analog,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .set_vsync_psync = cpld_set_vsync_psync,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info_rgb_analog,
   .set_frontend = cpld_set_frontend_rgb_analog,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .get_sync_edge = cpld_get_sync_edge,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};