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

typedef struct {
   int cpld_setup_mode;
   int sp_offset[NUM_OFFSETS];
   int half_px_delay; // 0 = off, 1 = on, all modes
   int divider;       // cpld divider, 6 or 8
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

// Indicates the Analog frontent interface supports 4 level RGB
static int supports_8bit;

// Indicates CPLD supports new style cpld with 6x2, 4 level and 12 bpp
static int supports_6x2_or_4_level_or_12;

// Indicates CPLD supports 6/8 divider bit
static int supports_divider;

// Indicates CPLD supports 6/8 divider workaround for 24Mhz CPLD
static int supports_divider_workaround;

// Indicates CPLD supports mux bit
static int supports_mux;

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
  RGB_RATE_3,               //00
  RGB_RATE_6,               //01
  RGB_RATE_6x2_OR_4_LEVEL,  //10 - 6x2 in digital mode and 4 level in analog mode
  RGB_RATE_9V,              //11
  RGB_RATE_9LO,             //11
  RGB_RATE_9HI,             //11
  RGB_RATE_12,              //11
  NUM_RGB_RATE
};

static const char *rate_names[] = {
   "3 Bits Per Pixel",
   "6 Bits Per Pixel",
   "Half-Odd (3BPP)",
   "Half-Even (3BPP)"
};

static const char *twelve_bit_rate_names_digital[] = {
   "3 Bits Per Pixel",
   "6 Bits Per Pixel",
   "6x2 Mux (12 BPP)",
   "9 Bits (V Sync)",
   "9 Bits Lo (2,1,0)",
   "9 Bits Hi (3,2,1)",
   "12 Bits Per Pixel"
};

static const char *twelve_bit_rate_names_analog[] = {
   "3 Bits Per Pixel",
   "6 Bits Per Pixel",
   "6 Bits (4 Level)",
   "9 Bits (V Sync)",
   "9 Bits (Bits 0-2)",
   "9 Bits (Bits 1-3)",
   "12 Bits Per Pixel"
};

static const char *cpld_setup_names[] = {
   "Normal",
   "Set Delay"
};

static const char *coupling_names[] = {
   "DC",
   "AC With Clamp"
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
   { ALL_OFFSETS, "All Offsets", "all_offsets", 0,   0, 1 },
   {    A_OFFSET,    "A Offset",    "a_offset", 0,   0, 1 },
   {    B_OFFSET,    "B Offset",    "b_offset", 0,   0, 1 },
   {    C_OFFSET,    "C Offset",    "c_offset", 0,   0, 1 },
   {    D_OFFSET,    "D Offset",    "d_offset", 0,   0, 1 },
   {    E_OFFSET,    "E Offset",    "e_offset", 0,   0, 1 },
   {    F_OFFSET,    "F Offset",    "f_offset", 0,   0, 1 },
   {        HALF,        "Half",        "half", 0,   1, 1 },
   {     DIVIDER,     "Divider",      "divider", 6,   8, 2 },
   {       DELAY,       "Delay",       "delay", 0,  15, 1 },
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
   {       DAC_A,  "DAC-A: G Hi",     "dac_a", 0, 255, 1 },
   {       DAC_B,  "DAC-B: G Lo",     "dac_b", 0, 255, 1 },
   {       DAC_C,  "DAC-C: RB Hi",    "dac_c", 0, 255, 1 },
   {       DAC_D,  "DAC-D: RB Lo",    "dac_d", 0, 255, 1 },
   {       DAC_E,  "DAC-E: Sync",     "dac_e", 0, 255, 1 },
   {       DAC_F,  "DAC-F: G/V Sync",  "dac_f", 0, 255, 1 },
   {       DAC_G,  "DAC-G: G Clamp",  "dac_g", 0, 255, 1 },
   {       DAC_H,  "DAC-H: Unused",   "dac_h", 0, 255, 1 },
   {          -1,          NULL,          NULL, 0,   0, 1 }
};

// =============================================================
// Private methods
// =============================================================

static void sendDAC(int dac, int value)
{
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
        case FRONTEND_ANALOG_ISSUE4:
        case FRONTEND_ANALOG_ISSUE3_5259:  // max5259
        case FRONTEND_ANALOG_ISSUE2_5259:
        {
            if (new_M62364_DAC_detected()) {
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
            }
            RPI_SetGpioValue(STROBE_PIN, 1);
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

int divider_workaround(int rate) {
    if (supports_divider_workaround) {
      if (config->divider != 8) {
          rate = RGB_RATE_6;
      }
      if (config->divider == 8 && rate == RGB_RATE_6) {
          rate = RGB_RATE_3;
      }
    }
    return rate;
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
      int offset = (supports_delay && capinfo->video_type == VIDEO_TELETEXT) ? (i + config->full_px_delay) % NUM_OFFSETS : i;
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
      int temprate = 0;
      switch (divider_workaround(config->rate)) {
        case RGB_RATE_3:               //00
            temprate = 0x00;
            break;
        case RGB_RATE_6:               //01
            temprate = 0x01;
            break;
        case RGB_RATE_6x2_OR_4_LEVEL:  //10 - 6x2 in digital mode and 4 level in analog mode
            temprate = 0x02;
            break;
        case RGB_RATE_9V:              //11
        case RGB_RATE_9LO:             //11
        case RGB_RATE_9HI:             //11
        case RGB_RATE_12:              //11
            temprate = 0x03;
            break;
      }
      sp |= (temprate << scan_len);
      scan_len += supports_rate;  // 1 or 2 depending on the CPLD version
   }
   if (supports_invert) {
      sp |= (invert << scan_len);
      scan_len += 1;
   }
   if (supports_divider) {
      sp |= ((config->divider == 8) << scan_len);
      scan_len += 1;
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

   if (supports_analog) {

       int dac_a = config->dac_a;
       if (dac_a == 255) dac_a = 75; //approx 1v (255 = 3.3v) so a grounded input will be detected as 0 without exceeding 1.4v difference
       int dac_b = config->dac_b;
       if (dac_b == 255) dac_b = dac_a;

       int dac_c = config->dac_c;
       if (dac_c == 255) dac_c = 75; //approx 1v (255 = 3.3v) so a grounded input will be detected as 0 without exceeding 1.4v difference
       int dac_d = config->dac_d;
       if (dac_d == 255) dac_d = dac_c;

       int dac_e = config->dac_e;
       if (dac_e == 255 && config->mux != 0) dac_e = dac_a;    //if sync level is disabled, track dac_a unless sync source is sync (stops spurious sync detection)

       int dac_f = config->dac_f;
       if (dac_f == 255 && config->mux == 0) dac_f = dac_a;   //if vsync level is disabled, track dac_a unless sync source is vsync (stops spurious sync detection)

       int dac_g = config->dac_g;
       if (dac_g == 255) {
          if (supports_8bit && config->rate == RGB_RATE_6x2_OR_4_LEVEL) {
             dac_g = dac_c;
          } else {
             dac_g = 0;
          }
       }

       int dac_h = config->dac_h;
       if (dac_h == 255) dac_h = dac_c;

       sendDAC(0, dac_a);
       sendDAC(1, dac_b);
       sendDAC(2, dac_c);
       sendDAC(3, dac_d);
       sendDAC(5, dac_e);   // DACs E and F positions swapped in menu compared to hardware
       sendDAC(4, dac_f);
       sendDAC(6, dac_g);
       sendDAC(7, dac_h);

       switch (config->terminate) {
          default:
          case RGB_INPUT_HI:
            RPI_SetGpioValue(SP_CLKEN_PIN, 0);  //termination
          break;
          case RGB_INPUT_TERM:
            RPI_SetGpioValue(SP_CLKEN_PIN, 1);
          break;
       }

      switch(config->rate) {
          case RGB_RATE_3:
          case RGB_RATE_6:
                RPI_SetGpioValue(SP_DATA_PIN, config->coupling);      //ac-dc
                break;
          case RGB_RATE_9V:
          case RGB_RATE_6x2_OR_4_LEVEL:
                if (supports_6x2_or_4_level_or_12) {
                    RPI_SetGpioValue(SP_DATA_PIN, 0);    //dc only in 4 level mode
                } else {
                    RPI_SetGpioValue(SP_DATA_PIN, config->coupling);   //ac-dc
                }
                break;
          case RGB_RATE_9LO:
          case RGB_RATE_9HI:
          case RGB_RATE_12:
                if (supports_6x2_or_4_level_or_12) {
                    RPI_SetGpioValue(SP_DATA_PIN, 1);    //enable 12 bit mode
                } else {
                    RPI_SetGpioValue(SP_DATA_PIN, config->coupling);   //ac-dc
                }
                break;
      }

   } else {
      if ((config->rate == RGB_RATE_6x2_OR_4_LEVEL || config->rate >= RGB_RATE_9LO) && supports_6x2_or_4_level_or_12) {
          RPI_SetGpioValue(SP_DATA_PIN, 1);    //enables multiplex signal in 6x2 mode and 12 bit mode
      } else {
          RPI_SetGpioValue(SP_DATA_PIN, 0);
      }
      RPI_SetGpioValue(SP_CLKEN_PIN, 0);
   }

   RPI_SetGpioValue(MUX_PIN, config->mux);    //only required for old CPLDs - has no effect on newer ones

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
// hide YUV lines included for file compatibility
   params[FILTER_L].hidden = 1;
   params[SUB_C].hidden = 1;
   params[ALT_R].hidden = 1;
   params[EDGE].hidden = 1;
   params[CLAMPTYPE].hidden = 1;

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
      params[RATE].max = 3;
   } else if (major >= 3) {
      supports_rate = 1;
      params[RATE].max = 1;
   } else {
      supports_rate = 0;
      params[RATE].hidden = 1;
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
   if (major == 7 && minor >=5) {
      supports_divider_workaround = 1;
   } else {
      supports_divider_workaround = 0;
   }

   if (major >= 8) {
      supports_6x2_or_4_level_or_12 = 1;
      supports_divider = 1;
      supports_mux = 1;
      if (eight_bit_detected()) {
        supports_8bit = 1;
        params[RATE].max = NUM_RGB_RATE - 1;
      } else {
        supports_8bit = 0;
        params[RATE].max = NUM_RGB_RATE - 5;      // running on a 6 bit board so hide the 12 bit options
        params[DAC_H].hidden = 1;
      }
   } else {
      supports_6x2_or_4_level_or_12 = 0;
      supports_divider = 0;
      supports_mux = 0;
      supports_8bit = 0;
      params[DAC_H].hidden = 1;  // hide spare DAC as will only be useful with new 8 bit CPLDs with new drivers (hiding maintains compatible save format)
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
       params[DAC_H].hidden = 1;
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
   config->cpld_setup_mode = 0;
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
   if (capinfo->video_type == VIDEO_TELETEXT && min_metric > 0 && (min_i <= 1 || min_i >= 6)) {
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
   if (capinfo->video_type == VIDEO_TELETEXT && min_metric > 0) {
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
      if (capinfo->video_type == VIDEO_TELETEXT) {
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

static void cpld_set_vsync_psync(int state) {  //state = 1 is psync (version = 1), state = 0 is vsync (version = 0, mux = 1)
   int temp_mux = config->mux;
   if (state == 0) {
       config->mux = 1;
   }
   write_config(config);
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
         write_config(config);
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

      switch(divider_workaround(config->rate)) {
          case RGB_RATE_3:
                capinfo->sample_width = SAMPLE_WIDTH_3;
                break;
          case RGB_RATE_6:
                capinfo->sample_width = SAMPLE_WIDTH_6;
                break;
          case RGB_RATE_6x2_OR_4_LEVEL:
                if (supports_6x2_or_4_level_or_12) {
                    if (supports_analog) {
                        capinfo->sample_width = SAMPLE_WIDTH_6;
                    } else {
                        capinfo->sample_width = SAMPLE_WIDTH_6x2;
                    }
                } else {
                    capinfo->sample_width = SAMPLE_WIDTH_3;
                }
                break;
          case RGB_RATE_9V:
          case RGB_RATE_9HI:
                 if (supports_6x2_or_4_level_or_12) {
                    capinfo->sample_width = SAMPLE_WIDTH_9HI;
                } else {
                    capinfo->sample_width = SAMPLE_WIDTH_3;
                }
                break;
          case RGB_RATE_9LO:
                if (supports_6x2_or_4_level_or_12) {
                    capinfo->sample_width = SAMPLE_WIDTH_9LO;
                } else {
                    capinfo->sample_width = SAMPLE_WIDTH_3;
                }
                break;
          case RGB_RATE_12:
                if (supports_6x2_or_4_level_or_12) {
                    capinfo->sample_width = SAMPLE_WIDTH_12;
                } else {
                    capinfo->sample_width = SAMPLE_WIDTH_3;
                }
                break;
      }

      // Update the line capture function
        switch (capinfo->sample_width) {
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
            case SAMPLE_WIDTH_6x2 :
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
}

static param_t *cpld_get_params() {
    params[A_OFFSET].hidden = !mode7;
    params[B_OFFSET].hidden = !mode7;
    params[C_OFFSET].hidden = !mode7;
    params[D_OFFSET].hidden = !mode7;
    params[E_OFFSET].hidden = !mode7;
    params[F_OFFSET].hidden = !mode7;
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
      if (supports_analog) {
          if (supports_6x2_or_4_level_or_12) {
              return twelve_bit_rate_names_analog[config->rate];
          } else {
              return rate_names[config->rate];
          }
      } else {
          if (supports_6x2_or_4_level_or_12) {
              return twelve_bit_rate_names_digital[config->rate];
          } else {
              return rate_names[config->rate];
          }
      }
   }
   if (num == CPLD_SETUP_MODE) {
      return cpld_setup_names[config->cpld_setup_mode];
   }
   if (num == COUPLING) {
      return coupling_names[config->coupling];
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
      if (supports_analog) {
          if (supports_8bit) {
              if (config->rate == RGB_RATE_6x2_OR_4_LEVEL) {
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
    //if (cpld_version & 1) {
    //    return cpld_get_value(DIVIDER) / 2;
    //} else {
        return cpld_get_value(DIVIDER);
    //}
}

static int cpld_get_delay() {
    return cpld_get_value(DELAY);
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
   .name = "3_BIT_RGB",
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

cpld_t cpld_rgb_ttl_24mhz = {
   .name = "6-12_BIT_RGB_24Mhz",
   .default_profile = "Master_128_24MHz_Even",
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
    if (new_M62364_DAC_detected()) {
        return FRONTEND_ANALOG_ISSUE4 | FRONTEND_ANALOG_ISSUE4 << 16;
    } else {
        return FRONTEND_ANALOG_ISSUE3_5259 | FRONTEND_ANALOG_ISSUE1_UB1 << 16;
    }
}

static void cpld_set_frontend_rgb_analog(int value) {
   frontend = value;
   write_config(config);
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
   .name = "6-12_BIT_RGB_24Mhz",
   .default_profile = "Master_128_24MHz_Even",
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