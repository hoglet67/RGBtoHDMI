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
  RGB_RATE_6x2_OR_4_LEVEL,  //010 - 6x2 in digital mode and 4 level in analog mode
  RGB_RATE_1,               //100
  NUM_RGB_RATE
};

static const char *twelve_bit_rate_names_digital[] = {
   "3 Bits Per Pixel",
   "6 Bits Per Pixel",
   "9 Bits (V Sync)",
   "9 Bits (Bits 0-2)",
   "9 Bits (Bits 1-3)",
   "12 Bits Per Pixel",
   "6x2 Mux (12 BPP)",
   "1 Bit Per Pixel",
};

static const char *twelve_bit_rate_names_analog[] = {
   "3 Bits Per Pixel",
   "6 Bits Per Pixel",
   "9 Bits (V Sync)",
   "9 Bits (Bits 0-2)",
   "9 Bits (Bits 1-3)",
   "12 Bits Per Pixel",
   "6 Bits (4 Level)",
   "1 Bit Per Pixel",
};

static const char *cpld_setup_names[] = {
   "Normal",
   "Set Pixel H Offset"
};

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


int divider_lookup[] = { 6, 8, 10, 12, 14, 16, 3, 4 };
int divider_register[] = { 2, 3, 4, 5, 6, 7, 0, 1 };

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
   {     DIVIDER,    "Clock Multiplier",    "divider", 0,   7, 1 },
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

static int get_adjusted_divider_index() {
    clk_info_t clkinfo;
    geometry_get_clk_params(&clkinfo);
    int divider_index = config->divider;
    while (divider_lookup[divider_index] * clkinfo.clock > MAX_CPLD_FREQUENCY && divider_index != 6) {
        divider_index = (divider_index - 1 ) & 7;
    }
    if (supports_odd_even && (config->rate == RGB_RATE_6 || config->rate == RGB_RATE_9V) && divider_index > 1) {
        divider_index = 1;
    }
    return divider_index;
}

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
      if (RGB_CPLD_detected && config->rate == RGB_RATE_1) {
             sp |= ((config->full_px_delay & 7) << scan_len);  // only use 3rd bit if 1bpp
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
                temprate = 0x04;
                break;
            case RGB_RATE_3:
                temprate = 0x00;
                break;
            case RGB_RATE_6:
                temprate = 0x01;
                break;
            case RGB_RATE_6x2_OR_4_LEVEL:
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
            if (config->sp_offset[0] >= (range >> 1)) {
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
              case RGB_RATE_6x2_OR_4_LEVEL:
                    RPI_SetGpioValue(SP_DATA_PIN, 0);    //dc only in 4 level mode or enable RGB_RATE_9V
                    break;
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
                    RPI_SetGpioValue(SP_DATA_PIN, 0);
                    break;
              case RGB_RATE_6x2_OR_4_LEVEL:
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

static int osd_sp(config_t *config, int range, int offset_range, int line, int metric) {
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
       sprintf(message, "  Sampling Phase: %d", config->all_offsets % range + offset_range);
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
      if (supports_analog) {
          rate_string = (char *) twelve_bit_rate_names_analog[config->rate];
      } else {
          rate_string = (char *) twelve_bit_rate_names_digital[config->rate];
      }
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

static void log_sp(config_t *config, int range, int offset_range) {
   char *mp = message;
   mp += sprintf(mp, "phases = %d %d %d %d %d %d; half = %d",
                 config->sp_offset[0] % range + offset_range, config->sp_offset[1] % range + offset_range, config->sp_offset[2] % range + offset_range,
                 config->sp_offset[3] % range + offset_range, config->sp_offset[4] % range + offset_range, config->sp_offset[5] % range + offset_range,
                 config->half_px_delay);
   if (supports_delay) {
      mp += sprintf(mp, "; pixel h offset = %d", config->full_px_delay);
   }

   if (supports_divider) {
      mp += sprintf(mp, "; clock multiplier = %d", get_adjusted_divider_index());
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
      params[RATE].max = 1;
      supports_odd_even = 1;
      params[DIVIDER].max = 7;
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
         params[RATE].max = NUM_RGB_RATE - 3;
         supports_bbc_9_12 = 1;
      }
   }

   if (major >= 8) {           // V8 CPLD branch is deprecated
      RGB_CPLD_detected = 1;
      params[DIVIDER].max = 1;
      if (eight_bit_detected()) {
         supports_8bit = 1;
         params[RATE].max = NUM_RGB_RATE - 2;
      } else {
         params[RATE].max = 2;
      }
      supports_odd_even = 0;
      supports_divider = 1;
      supports_mux = 1;
   }

   if (major == 9) {
      if (eight_bit_detected()) {
         params[RATE].max = NUM_RGB_RATE - 1;
      } else {
         params[RATE].max = 2;
      }
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

static int cpld_calibrate_sub(capture_info_t *capinfo, int elk, int finalise) {
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
   int odd_even = 0;
   int offset_range = 0;

   int (*raw_metrics)[16][NUM_OFFSETS];

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
   range = divider_lookup[get_adjusted_divider_index()];
   if (supports_odd_even && range > 8) {
       range >>= 1;
       odd_even = 1;
       offset_range = config->sp_offset[0] >= range ? range : 0;
   }

   // Measure the error metrics at all possible offset values
   old_full_px_delay = config->full_px_delay;
   min_metric = INT_MAX;
   if (!odd_even) {  // if mode 7 cpld overclocking let caller set config->half_px_delay
      config->half_px_delay = 0;
   }
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
      config->all_offsets = config->sp_offset[0];
      config->sp_offset[0] += offset_range;
      write_config(config, DAC_UPDATE);
      config->sp_offset[0] -= offset_range;
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
      osd_sp(config, range, offset_range, 2, metric);
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
      win_metric = sum_metrics[left] + sum_metrics[i] + sum_metrics[right];
      if (sum_metrics[i] == min_metric) {
         if (win_metric < min_win_metric) {
            min_win_metric = win_metric;
            min_i = i;
         }
      }
   }

   // If the min metric is at the limit, make use of the half pixel delay
   if (!RGB_CPLD_detected && capinfo->video_type == VIDEO_TELETEXT && range >= 6 && min_metric > 0 && (min_i <= 1 || min_i >= (range - 2))) {
      if (!odd_even) {  //do not select half if 24 Mhz rate (mode 7 overclocking) as CPLD behaves strangely due to being very overclocked
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
      }
   }

   // In all modes, start with the min metric
   for (int i = 0; i < NUM_OFFSETS; i++) {
      config->sp_offset[i] = min_i;
   }
   config->all_offsets = config->sp_offset[0];
   log_sp(config, range, offset_range);
   config->sp_offset[0] += offset_range;
   write_config(config, DAC_UPDATE);
   config->sp_offset[0] -= offset_range;

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
      config->sp_offset[0] += offset_range;
      write_config(config, DAC_UPDATE);
      config->sp_offset[0] -= offset_range;
      *errors = diff_N_frames(capinfo, NUM_CAL_FRAMES, mode7, elk);
      osd_sp(config, range, offset_range, 2, *errors);
      log_sp(config, range, offset_range);
      log_info("Optimization complete, errors = %d", *errors);
   } else {
      *errors = diff_N_frames(capinfo, NUM_CAL_FRAMES, mode7, elk);
   }

   if (finalise) {
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
          config->sp_offset[0] += offset_range;
          write_config(config, DAC_UPDATE);
          config->sp_offset[0] -= offset_range;
       }

       // Perform a final test of errors
       log_info("Performing final test");
       *errors = diff_N_frames(capinfo, NUM_CAL_FRAMES, mode7, elk);
       osd_sp(config, range, offset_range, 2, *errors);
       if (capinfo->video_type == VIDEO_INTERLACED && capinfo->detected_sync_type & SYNC_BIT_INTERLACED) {
             unsigned int flags = extra_flags() | mode7 | BIT_CALIBRATE | (2 << OFFSET_NBUFFERS);
             rgb_to_fb(capinfo, flags);  //restore OSD
             delay_in_arm_cycles_cpu_adjust(100000000);
          }
       log_sp(config, range, offset_range);
       log_info("Calibration complete, errors = %d", *errors);
   }

   for (int i = 0; i < NUM_OFFSETS; i++) {
      config->sp_offset[i] = config->sp_offset[i] + offset_range;
   }
   config->all_offsets = config->sp_offset[0];

   return *errors;

}

static void cpld_calibrate(capture_info_t *capinfo, int elk) {
    int range = divider_lookup[get_adjusted_divider_index()];
    if (supports_odd_even && range > 8) { // 24 Mhz mode 7
        range >>= 1;
        int min_errors = 0x70000000;
        int min_range = 0;
        int min_half = 0;
        for (int i = 0; i < 4; i++) {
            switch (i){
                case 0:
                config->sp_offset[0] = 0;
                config->half_px_delay = 0;
                break;
                case 1:
                config->sp_offset[0] = range;
                config->half_px_delay = 0;
                break;
                case 2:
                config->sp_offset[0] = 0;
                config->half_px_delay = 1;
                break;
                case 3:
                config->sp_offset[0] = range;
                config->half_px_delay = 1;
                break;
            }
            int current_errors = cpld_calibrate_sub(capinfo, elk, 0);
            if (current_errors < min_errors) {
                min_errors = current_errors;
                min_range = config->sp_offset[0] >= range ? range : 0;
                min_half = config->half_px_delay;
                log_info("Current minimum: Phase = %d, Range = %d,  Half = %d", config->all_offsets, min_range, min_half);
            }
        }
        config->sp_offset[0] = min_range;
        config->half_px_delay = min_half;
        // re-run calibration one last time with best choice of above values
    }

    cpld_calibrate_sub(capinfo, elk, 1);
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
   RPI_SetGpioValue(MODE7_PIN, (divider_lookup[get_adjusted_divider_index()] & 1) != 0);

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
                capinfo->sample_width = SAMPLE_WIDTH_6;
                break;
          case RGB_RATE_6x2_OR_4_LEVEL:
                if (supports_analog) {
                    capinfo->sample_width = SAMPLE_WIDTH_6;
                } else {
                    capinfo->sample_width = SAMPLE_WIDTH_6x2;
                }
          case RGB_RATE_9V:
          case RGB_RATE_9HI:
                capinfo->sample_width = SAMPLE_WIDTH_9HI;
                break;
          case RGB_RATE_9LO:
                capinfo->sample_width = SAMPLE_WIDTH_9LO;
                break;
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
         return twelve_bit_rate_names_analog[config->rate];
      } else {
         return twelve_bit_rate_names_digital[config->rate];
      }
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
   return NULL;
}

static int offset_fixup(int value) {
    int range = divider_lookup[get_adjusted_divider_index()];
    if (supports_odd_even && range > 8) {
        range >>= 1;
        int odd_even_offset = config->sp_offset[0] - (config->sp_offset[0] % range);
        return (value % range) + odd_even_offset;
    } else {
        return value;
    }
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
      config->all_offsets = value;
      config->sp_offset[0] = value;
      config->sp_offset[1] = offset_fixup(value);
      config->sp_offset[2] = offset_fixup(value);
      config->sp_offset[3] = offset_fixup(value);
      config->sp_offset[4] = offset_fixup(value);
      config->sp_offset[5] = offset_fixup(value);
      break;
   case A_OFFSET:
      config->sp_offset[0] = value;
      config->sp_offset[1] = offset_fixup(config->sp_offset[1]);
      config->sp_offset[2] = offset_fixup(config->sp_offset[2]);
      config->sp_offset[3] = offset_fixup(config->sp_offset[3]);
      config->sp_offset[4] = offset_fixup(config->sp_offset[4]);
      config->sp_offset[5] = offset_fixup(config->sp_offset[5]);
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
             if (value == 2) {
                 value = 3;
             } else if (value == 4) {
                 value = 5;
             } else if (value == 6) {
                 value = 0;
             } else if (value == 7) {
                 value = 5;
             }
         } else {
             if (value == 2) {
                 value = 1;
             } else if (value == 4) {
                 value = 3;
             } else if (value == 6) {
                 value = 5;
             } else if (value == 7) {
                 value = 0;
             }
         }
      }
      config->divider = value;
      update_param_range();
      int actual_value = get_adjusted_divider_index();
      if (actual_value != value) {
          char msg[64];
          if (config->rate == RGB_RATE_6 || config->rate == RGB_RATE_9V){
              sprintf(msg, "6 Bit & 9 Bit(V) Limited to %s", divider_names[actual_value]);
          } else {
              sprintf(msg, "Clock>192MHz: Limited to %s", divider_names[actual_value]);
          }
          set_status_message(msg);
      }
      break;
   case DELAY:
      config->full_px_delay = value;
      break;
   case RATE:
      config->rate = value;
      if (supports_analog) {
          if (supports_8bit) {
              if (RGB_CPLD_detected && config->rate == RGB_RATE_6x2_OR_4_LEVEL) {
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

static int cpld_show_cal_summary(int line) {
   int range = divider_lookup[get_adjusted_divider_index()];
   int offset_range = 0;
   if (supports_odd_even && range > 8) {
       range >>= 1;
       offset_range = config->sp_offset[0] >= range ? range : 0;
   }
   return osd_sp(config, range, offset_range, line, mode7 ? errors_mode7 : errors_default);
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
          offset_range = config->sp_offset[0] >= range ? range : 0;
      }
      for (int value = 0; value < range; value++) {
         sprintf(message, "Phase %d: Errors = %6d", value + offset_range, sum_metrics[value]);
         osd_set(line + value, 0, message);
      }
   }
}

static void cpld_show_cal_raw(int line) {
   int (*raw_metrics)[16][NUM_OFFSETS] = mode7 ? &raw_metrics_mode7 : &raw_metrics_default;
   int range = ((*raw_metrics)[0][0] < 0) ? 0 : divider_lookup[get_adjusted_divider_index()];
   if (range == 0) {
      sprintf(message, "No calibration data for this mode");
      osd_set(line, 0, message);
   } else {
   if (supports_odd_even && range > 8) {
       range >>= 1;
   }
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
    return divider_lookup[get_adjusted_divider_index()];
}

static int cpld_get_delay() {
    if (RGB_CPLD_detected && config->rate == RGB_RATE_1) {
        return cpld_get_value(DELAY) & 0x08;         // mask out lower 3 bits of delay
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