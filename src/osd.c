#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "defs.h"
#include "cpld.h"
#include "gitversion.h"
#include "info.h"
#include "logging.h"
#include "osd.h"
#include "rpi-gpio.h"
#include "rpi-mailbox.h"
#include "rpi-mailbox-interface.h"
#include "saa5050_font.h"

#define NLINES 12
#define LINELEN 40

static char buffer[LINELEN * NLINES];
static int attributes[NLINES];

// Mapping table for expanding 12-bit row to 24 bit pixel (3 words)
static uint32_t double_size_map[0x1000 * 3];

// Mapping table for expanding 12-bit row to 12 bit pixel (2 words + 2 words)
static uint32_t normal_size_map[0x1000 * 4];

static char message[80];

static int active = 0;

enum {
   IDLE,
   MANUAL,
   FEATURE,
};


static int osd_state = IDLE;
static int param_num = 0;
static int feature_num = 0;

enum {
   PALETTE_DEFAULT,
   PALETTE_INVERSE,
   PALETTE_MONO1,
   PALETTE_MONO2,
   PALETTE_RED,
   PALETTE_GREEN,
   PALETTE_BLUE,
   PALETTE_NOT_RED,
   PALETTE_NOT_GREEN,
   PALETTE_NOT_BLUE,
   NUM_PALETTES
};

static const char *palette_names[] = {
   "Default",
   "Inverse",
   "Mono 1",
   "Mono 2",
   "Just Red",
   "Just Green",
   "Just Blue",
   "Not Red",
   "Not Green",
   "Not Blue"
};

enum {
   INFO_VERSION,
   INFO_CAL_SUMMARY,
   INFO_CAL_DETAILS,
   INFO_CAL_RAW,
   NUM_INFOS
};

static const char *info_names[] = {
   "Firmware Version",
   "Calibration Summary",
   "Calibration Detail",
   "Calibration Raw"
};

static const char *machine_names[] = {
   "Beeb",
   "Elk"
};

static const char *pllh_names[] = {
   "Original",
   "623",
   "624",
   "625",
   "626",
   "627",
};

static const char *deinterlace_names[] = {
   "None",
   "Motion Adaptive 1",
   "Motion Adaptive 2",
   "Motion Adaptive 3",
   "Motion Adaptive 4",
   "Aligned Motion Adaptive 1",
   "Aligned Motion Adaptive 2",
   "Aligned Motion Adaptive 3",
   "Aligned Motion Adaptive 4"
};

#ifdef MULTI_BUFFER
static const char *nbuffer_names[] = {
   "Single buffered",
   "Double buffered",
   "Triple buffered",
   "Quadruple buffered",
};
#endif

// =============================================================
// Feature definitions for OSD
// =============================================================

enum {
   F_INFO,
   F_PALETTE,
   F_DEINTERLACE,
   F_H_OFFSET,
   F_V_OFFSET,
   F_SCANLINES,
   F_MUX,
   F_ELK,
   F_VSYNC,
   F_PLLH,
#ifdef MULTI_BUFFER
   F_NBUFFERS,
#endif
   F_DEBUG
};

static param_t features[] = {
   { "Info",          0, NUM_INFOS - 1 },
   { "Color Palette", 0, NUM_PALETTES - 1 },
   { "Deinterlace",   0, NUM_DEINTERLACES - 1 },
   { "Hor Offset",    0, 39 },
   { "Ver Offset",    0, 39 },
   { "Scanlines",     0, 1 },
   { "Input Mux",     0, 1 },
   { "Elk",           0, 1 },
   { "Vsync",         0, 1 },
   { "HDMI Clock",    0, 5 },
#ifdef MULTI_BUFFER
   { "Num Buffers",   0, 3 },
#endif
   { "Debug",         0, 1 },
   { NULL,            0, 0 },
};

static int info      = INFO_VERSION;
static int palette   = PALETTE_DEFAULT;
static int mux       = 0;

uint32_t *osd_get_palette() {
   int m;
   static uint32_t palette_data[16];
   for (int i = 0; i < 16; i++) {
      int r = (i & 1) ? 255 : 0;
      int g = (i & 2) ? 255 : 0;
      int b = (i & 4) ? 255 : 0;
      switch (palette) {
      case PALETTE_INVERSE:
         r = 255 - r;
         g = 255 - g;
         b = 255 - b;
         break;
      case PALETTE_MONO1:
         m = 0.299 * r + 0.587 * g + 0.114 * b;
         r = m; g = m; b = m;
         break;
      case PALETTE_MONO2:
         m = (i & 7) * 255 / 7;
         r = m; g = m; b = m;
         break;
      case PALETTE_RED:
         m = (i & 7) * 255 / 7;
         r = m; g = 0; b = 0;
         break;
      case PALETTE_GREEN:
         m = (i & 7) * 255 / 7;
         r = 0; g = m; b = 0;
         break;
      case PALETTE_BLUE:
         m = (i & 7) * 255 / 7;
         r = 0; g = 0; b = m;
         break;
      case PALETTE_NOT_RED:
         r = 0;
         g = (i & 3) * 255 / 3;
         b = ((i >> 2) & 1) * 255;
         break;
      case PALETTE_NOT_GREEN:
         r = (i & 3) * 255 / 3;
         g = 0;
         b = ((i >> 2) & 1) * 255;
         break;
      case PALETTE_NOT_BLUE:
         r = ((i >> 2) & 1) * 255;
         g = (i & 3) * 255 / 3;
         b = 0;
         break;
      }
      if (active) {
         if (i >= 8) {
            palette_data[i] = 0xFFFFFFFF;
         } else {
            r >>= 1; g >>= 1; b >>= 1;
            palette_data[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
         }
      } else {
         palette_data[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
      }
      if (get_debug()) {
         palette_data[i] |= 0x00101010;
      }
   }
   return palette_data;
}

static void update_palette() {
   // Flush the previous swapBuffer() response from the GPU->ARM mailbox
   RPI_Mailbox0Flush( MB0_TAGS_ARM_TO_VC  );
   RPI_PropertyInit();
   RPI_PropertyAddTag( TAG_SET_PALETTE, osd_get_palette());
   RPI_PropertyProcess();
}

void osd_clear() {
   if (active) {
      memset(buffer, 0, sizeof(buffer));
      osd_update((uint32_t *)capinfo->fb, capinfo->pitch);
      active = 0;
      RPI_SetGpioValue(LED1_PIN, active);
      update_palette();
   }
}

void osd_set(int line, int attr, char *text) {
   if (!active) {
      active = 1;
      RPI_SetGpioValue(LED1_PIN, active);
      update_palette();
   }
   attributes[line] = attr;
   memset(buffer + line * LINELEN, 0, LINELEN);
   int len = strlen(text);
   if (len > LINELEN) {
      len = LINELEN;
   }
   strncpy(buffer + line * LINELEN, text, len);
   osd_update((uint32_t *)capinfo->fb, capinfo->pitch);
}


int osd_active() {
   return active;
}

static void delay() {
   for (volatile int i = 0; i < 100000000; i++);
}

static void show_param(int num) {
   param_t *params = cpld->get_params();
   sprintf(message, "%s = %d", params[num].name, cpld->get_value(num));
   osd_set(1, 0, message);
}

static int get_feature(int num) {
   switch (num) {
   case F_INFO:
      return info;
   case F_PALETTE:
      return palette;
   case F_DEINTERLACE:
      return get_deinterlace();
   case F_H_OFFSET:
      return get_h_offset();
   case F_V_OFFSET:
      return get_v_offset();
   case F_SCANLINES:
      return get_scanlines();
   case F_MUX:
      return mux;
   case F_ELK:
      return get_elk();
   case F_VSYNC:
      return get_vsync();
   case F_PLLH:
      return get_pllh();
#ifdef MULTI_BUFFER
   case F_NBUFFERS:
      return get_nbuffers();
#endif
   case F_DEBUG:
      return get_debug();
   }
   return -1;
}

static void set_feature(int num, int value) {
   switch (num) {
   case F_INFO:
      info = value;
      break;
   case F_PALETTE:
      palette = value;
      update_palette();
      break;
   case F_DEINTERLACE:
      set_deinterlace(value);
      break;
   case F_H_OFFSET:
      set_h_offset(value);
      break;
   case F_V_OFFSET:
      set_v_offset(value);
      break;
   case F_SCANLINES:
      set_scanlines(value);
      break;
   case F_MUX:
      mux = value;
      RPI_SetGpioValue(MUX_PIN, mux);
      break;
   case F_ELK:
      set_elk(value);
      break;
   case F_VSYNC:
      set_vsync(value);
      break;
   case F_PLLH:
      set_pllh(value);
      break;
#ifdef MULTI_BUFFER
   case F_NBUFFERS:
      set_nbuffers(value);
      break;
#endif
   case F_DEBUG:
      set_debug(value);
      update_palette();
      break;
   }
}

static const char *get_value_string(int num) {
   static char buffer[100];
   // Read the current value of the specified feature
   int value = get_feature(num);
   // Convert that to a human readable string
   switch (num) {
   case F_INFO:
      return  info_names[value];
   case F_PALETTE:
      return  palette_names[value];
   case F_DEINTERLACE:
      return  deinterlace_names[value];
   case F_PLLH:
      return  pllh_names[value];
#ifdef MULTI_BUFFER
   case F_NBUFFERS:
      return  nbuffer_names[value];
#endif
   case F_H_OFFSET:
   case F_V_OFFSET:
      sprintf(buffer, "%d", value);
      return buffer;
   default:
      return value ? "On" : "Off";
   }
}

static void show_feature(int num) {
   const char *machine;
   const char *valstr = get_value_string(num);
   // Clear lines 2 onwards
   memset(buffer + 2 * LINELEN, 0, (NLINES - 2) * LINELEN);
   // Dispay the feature name and value in line 1
   sprintf(message, "%s = %s", features[num].name, valstr);
   osd_set(1, 0, message);
   // If the info screen, provide further info on lines 3 onwards
   if (num == F_INFO) {
      switch (info) {
      case INFO_VERSION:
         sprintf(message, "Pi Firmware: %s", GITVERSION);
         osd_set(3, 0, message);
         sprintf(message, "%s CPLD: v%x.%x",
                 cpld->name,
                 (cpld->get_version() >> VERSION_MAJOR_BIT) & 0xF,
                 (cpld->get_version() >> VERSION_MINOR_BIT) & 0xF);
         osd_set(4, 0, message);
         break;
      case INFO_CAL_SUMMARY:
         machine = machine_names[get_elk()];
         if (clock_error_ppm > 0) {
            sprintf(message, "Clk Err: %d ppm (%s slower than Pi)", clock_error_ppm, machine);
         } else if (clock_error_ppm < 0) {
            sprintf(message, "Clk Err: %d ppm (%s faster than Pi)", -clock_error_ppm, machine);
         } else {
            sprintf(message, "Clk Err: %d ppm (exact match)", clock_error_ppm);
         }
         osd_set(3, 0, message);
         if (cpld->show_cal_summary) {
            cpld->show_cal_summary(5);
         } else {
            sprintf(message, "show_cal_summary() not implemented");
            osd_set(5, 0, message);
         }
         break;
      case INFO_CAL_DETAILS:
         if (cpld->show_cal_details) {
            cpld->show_cal_details(3);
         } else {
            sprintf(message, "show_cal_details() not implemented");
            osd_set(3, 0, message);
         }
         break;
      case INFO_CAL_RAW:
         if (cpld->show_cal_raw) {
            cpld->show_cal_raw(3);
         } else {
            sprintf(message, "show_cal_raw() not implemented");
            osd_set(3, 0, message);
         }
         break;
      }
   }
}


void osd_refresh() {
   switch (osd_state) {

   case IDLE:
      osd_clear();
      break;

   case MANUAL:
      osd_set(0, ATTR_DOUBLE_SIZE, "Manual Calibration");
      show_param(param_num);
      break;

   case FEATURE:
      osd_set(0, ATTR_DOUBLE_SIZE, "Feature Selection");
      show_feature(feature_num);
      break;
   }
}

void osd_key(int key) {
   int value;
   param_t *params = cpld->get_params();

   switch (osd_state) {

   case IDLE:
      switch (key) {
      case OSD_SW1:
         // Feature Selection
         osd_state = FEATURE;
         osd_refresh();
         break;
      case OSD_SW2:
         // Manual Calibration
         osd_state = MANUAL;
         osd_refresh();
         break;
      case OSD_SW3:
         // Auto Calibration
         osd_set(0, ATTR_DOUBLE_SIZE, "Auto Calibration");
         action_calibrate();
         delay();
         osd_clear();
         break;
      }
      break;

   case MANUAL:
      switch (key) {
      case OSD_SW1:
         // exit manual configuration
         osd_state = IDLE;
         osd_refresh();
         break;
      case OSD_SW2:
         // next param
         param_num++;
         if (params[param_num].name == NULL) {
            param_num = 0;
         }
         show_param(param_num);
         break;
      case OSD_SW3:
         // next value
         value = cpld->get_value(param_num);
         if (value < params[param_num].max) {
            value++;
         } else {
            value = params[param_num].min;
         }
         cpld->set_value(param_num, value);
         show_param(param_num);
         break;
      }
      break;

   case FEATURE:
      switch (key) {
      case OSD_SW1:
         // exit feature selection
         osd_state = IDLE;
         osd_refresh();
         break;
      case OSD_SW2:
         // next feature
         feature_num++;
         if (features[feature_num].name == NULL) {
            feature_num = 0;
         }
         show_feature(feature_num);
         break;
      case OSD_SW3:
         // next value
         value = get_feature(feature_num);
         if (value < features[feature_num].max) {
            value++;
         } else {
            value = features[feature_num].min;
         }
         set_feature(feature_num, value);
         show_feature(feature_num);
         break;
      }
      break;
   }
}

void osd_init() {
   char *prop;
   // Precalculate character->screen mapping table
   //
   // Normal size mapping, odd numbered characters
   //
   // char bit  0 -> double_size_map + 2 bit 31
   // ...
   // char bit  7 -> double_size_map + 2 bit  3
   // char bit  8 -> double_size_map + 1 bit 31
   // ...
   // char bit 11 -> double_size_map + 1 bit 19
   //
   // Normal size mapping, even numbered characters
   //
   // char bit  0 -> double_size_map + 1 bit 15
   // ...
   // char bit  3 -> double_size_map + 1 bit  3
   // char bit  4 -> double_size_map + 0 bit 31
   // ...
   // char bit 11 -> double_size_map + 0 bit  3
   //
   // Double size mapping
   //
   // char bit  0 -> double_size_map + 2 bits 31, 27
   // ...
   // char bit  3 -> double_size_map + 2 bits  7,  3
   // char bit  4 -> double_size_map + 1 bits 31, 27
   // ...
   // char bit  7 -> double_size_map + 1 bits  7,  3
   // char bit  8 -> double_size_map + 0 bits 31, 27
   // ...
   // char bit 11 -> double_size_map + 0 bits  7,  3
   memset(normal_size_map, 0, sizeof(normal_size_map));
   memset(double_size_map, 0, sizeof(double_size_map));
   for (int i = 0; i < NLINES; i++) {
      attributes[i] = 0;
   }
   for (int i = 0; i <= 0xFFF; i++) {
      for (int j = 0; j < 12; j++) {
         // j is the pixel font data bit, with bit 11 being left most
         if (i & (1 << j)) {
            // Normal size, odd characters
            // cccc.... dddddddd
            if (j < 8) {
               normal_size_map[i * 4 + 3] |= 0x8 << (4 * (7 - (j ^ 1)));   // dddddddd
            } else {
                  normal_size_map[i * 4 + 2] |= 0x8 << (4 * (15 - (j ^ 1)));  // cccc....
            }
            // Normal size, even characters
            // aaaaaaaa ....bbbb
            if (j < 4) {
               normal_size_map[i * 4 + 1] |= 0x8 << (4 * (3 - (j ^ 1)));   // ....bbbb
            } else {
               normal_size_map[i * 4    ] |= 0x8 << (4 * (11 - (j ^ 1)));  // aaaaaaaa
            }
            // Double size
            // aaaaaaaa bbbbbbbb cccccccc
            if (j < 4) {
               double_size_map[i * 3 + 2] |= 0x88 << (8 * (3 - j));  // cccccccc
            } else if (j < 8) {
               double_size_map[i * 3 + 1] |= 0x88 << (8 * (7 - j));  // bbbbbbbb
            } else {
               double_size_map[i * 3    ] |= 0x88 << (8 * (11 - j)); // aaaaaaaa
            }
         }
      }
   }
   // Initialize the OSD features
   prop = get_cmdline_prop("info");
   if (prop) {
      int val = atoi(prop);
      set_feature(F_INFO, val);
      log_info("config.txt:     info = %d", val);
   }
   prop = get_cmdline_prop("palette");
   if (prop) {
      int val = atoi(prop);
      set_feature(F_PALETTE, val);
      log_info("config.txt:     palette = %d", val);
   }
   prop = get_cmdline_prop("deinterlace");
   if (prop) {
      int val = atoi(prop);
      set_feature(F_DEINTERLACE, val);
      log_info("config.txt: deinterlace = %d", val);
   }
   prop = get_cmdline_prop("h_offset");
   if (prop) {
      int val = atoi(prop);
      set_feature(F_H_OFFSET, val);
      log_info("config.txt: h_offset = %d", val);
   }
   prop = get_cmdline_prop("v_offset");
   if (prop) {
      int val = atoi(prop);
      set_feature(F_V_OFFSET, val);
      log_info("config.txt: v_offset = %d", val);
   }
   prop = get_cmdline_prop("scanlines");
   if (prop) {
      int val = atoi(prop);
      set_feature(F_SCANLINES, val);
      log_info("config.txt:   scanlines = %d", val);
   }
   prop = get_cmdline_prop("mux");
   if (prop) {
      int val = atoi(prop);
      set_feature(F_MUX, val);
      log_info("config.txt:         mux = %d", val);
   }
   prop = get_cmdline_prop("elk");
   if (prop) {
      int val = atoi(prop);
      set_feature(F_ELK, val);
      log_info("config.txt:         elk = %d", val);
   }
   prop = get_cmdline_prop("pllh");
   if (prop) {
      int val = atoi(prop);
      set_feature(F_PLLH, val);
      log_info("config.txt:        pllh = %d", val);
   }
#ifdef MULTI_BUFFER
   prop = get_cmdline_prop("nbuffers");
   if (prop) {
      int val = atoi(prop);
      set_feature(F_NBUFFERS, val);
      log_info("config.txt:    nbuffers = %d", val);
   }
#endif
   prop = get_cmdline_prop("vsync");
   if (prop) {
      int val = atoi(prop);
      set_feature(F_VSYNC, val);
      log_info("config.txt:       vsync = %d", val);
   }
   prop = get_cmdline_prop("debug");
   if (prop) {
      int val = atoi(prop);
      set_feature(F_DEBUG, val);
      log_info("config.txt:       debug = %d", val);
   }
   // Initialize the CPLD sampling points
   for (int m7 = 0; m7 <= 1; m7++) {
      char *propname = m7 ? "sampling7" : "sampling06";
      prop = get_cmdline_prop(propname);
      if (prop) {
         cpld->set_mode(m7);
         log_info("config.txt:  %s = %s", propname, prop);
         int i = 0;
         char *prop2 = strtok(prop, ",");
         while (prop2) {
            const char *name = cpld->get_params()[i].name;
            if (!name) {
               log_warn("Too many sampling sub-params, ignoring the rest");
               break;
            }
            int val = atoi(prop2);
            log_info("cpld: %s = %d", name, val);
            cpld->set_value(i, val);
            prop2 = strtok(NULL, ",");
            i++;
         }
      }
   }
   // Disable CPLDv2 specific features for CPLDv1
   if (((cpld->get_version() >> VERSION_MAJOR_BIT) & 0x0F) < 2) {
      features[F_DEINTERLACE].max = DEINTERLACE_MA4;
   }
}

void osd_update(uint32_t *osd_base, int bytes_per_line) {
   if (!active) {
      return;
   }
   // SAA5050 character data is 12x20
   uint32_t *line_ptr = osd_base;
   int words_per_line = bytes_per_line >> 2;
   for (int line = 0; line < NLINES; line++) {
      int attr = attributes[line];
      int len = (attr & ATTR_DOUBLE_SIZE) ? (LINELEN >> 1) : LINELEN;
      for (int y = 0; y < 20; y++) {
         uint32_t *word_ptr = line_ptr;
         for (int i = 0; i < len; i++) {
            int c = buffer[line * LINELEN + i];
            // Deal with unprintable characters
            if (c < 32 || c > 127) {
               c = 32;
            }
            // Character row is 12 pixels
            int data = fontdata[32 * c + y] & 0x3ff;
            // Map to the screen pixel format
            if (attr & ATTR_DOUBLE_SIZE) {
               // Map to three 32-bit words in frame buffer format
               uint32_t *map_ptr = double_size_map + data * 3;
               *word_ptr &= 0x77777777;
               *word_ptr |= *map_ptr;
               *(word_ptr + words_per_line) &= 0x77777777;;
               *(word_ptr + words_per_line) |= *map_ptr;
               word_ptr++;
               map_ptr++;
               *word_ptr &= 0x77777777;
               *word_ptr |= *map_ptr;
               *(word_ptr + words_per_line) &= 0x77777777;;
               *(word_ptr + words_per_line) |= *map_ptr;
               word_ptr++;
               map_ptr++;
               *word_ptr &= 0x77777777;
               *word_ptr |= *map_ptr;
               *(word_ptr + words_per_line) &= 0x77777777;;
               *(word_ptr + words_per_line) |= *map_ptr;
               word_ptr++;
            } else {
               // Map to two 32-bit words in frame buffer format
               if (i & 1) {
                  // odd character
                  uint32_t *map_ptr = normal_size_map + (data << 2) + 2;
                  *word_ptr &= 0x7777FFFF;
                  *word_ptr |= *map_ptr;
                  word_ptr++;
                  map_ptr++;
                  *word_ptr &= 0x77777777;
                  *word_ptr |= *map_ptr;
                  word_ptr++;
               } else {
                  // even character
                  uint32_t *map_ptr = normal_size_map + (data << 2);
                  *word_ptr &= 0x77777777;
                  *word_ptr |= *map_ptr;
                  word_ptr++;
                  map_ptr++;
                  *word_ptr &= 0xFFFF7777;
                  *word_ptr |= *map_ptr;
               }
            }
         }
         if (attr & ATTR_DOUBLE_SIZE) {
            line_ptr += 2 * words_per_line;
         } else {
            line_ptr += words_per_line;
         }
      }
   }
}

// This is a stripped down version of the above that is significantly
// faster, but assumes all the osd pixel bits are initially zero.
//
// This is used in mode 0..6, and is called by the rgb_to_fb code
// after the RGB data has been written into the frame buffer.
//
// It's a shame we have had to duplicate code here, but speed matters!

void osd_update_fast(uint32_t *osd_base, int bytes_per_line) {
   if (!active) {
      return;
   }
   // SAA5050 character data is 12x20
   uint32_t *line_ptr = osd_base;
   int words_per_line = bytes_per_line >> 2;
   for (int line = 0; line < NLINES; line++) {
      int attr = attributes[line];
      int len = (attr & ATTR_DOUBLE_SIZE) ? (LINELEN >> 1) : LINELEN;
      for (int y = 0; y < 20; y++) {
         uint32_t *word_ptr = line_ptr;
         for (int i = 0; i < len; i++) {
            int c = buffer[line * LINELEN + i];
            // Bail at the first zero character
            if (c == 0) {
               break;
            }
            // Deal with unprintable characters
            if (c < 32 || c > 127) {
               c = 32;
            }
            // Character row is 12 pixels
            int data = fontdata[32 * c + y] & 0x3ff;
            // Map to the screen pixel format
            if (attr & ATTR_DOUBLE_SIZE) {
               // Map to three 32-bit words in frame buffer format
               uint32_t *map_ptr = double_size_map + data * 3;
               *word_ptr |= *map_ptr;
               *(word_ptr + words_per_line) |= *map_ptr;
               word_ptr++;
               map_ptr++;
               *word_ptr |= *map_ptr;
               *(word_ptr + words_per_line) |= *map_ptr;
               word_ptr++;
               map_ptr++;
               *word_ptr |= *map_ptr;
               *(word_ptr + words_per_line) |= *map_ptr;
               word_ptr++;
            } else {
               // Map to two 32-bit words in frame buffer format
               if (i & 1) {
                  // odd character
                  uint32_t *map_ptr = normal_size_map + (data << 2) + 2;
                  *word_ptr |= *map_ptr;
                  word_ptr++;
                  map_ptr++;
                  *word_ptr |= *map_ptr;
                  word_ptr++;
               } else {
                  // even character
                  uint32_t *map_ptr = normal_size_map + (data << 2);
                  *word_ptr |= *map_ptr;
                  word_ptr++;
                  map_ptr++;
                  *word_ptr |= *map_ptr;
               }
            }
         }
         if (attr & ATTR_DOUBLE_SIZE) {
            line_ptr += 2 * words_per_line;
         } else {
            line_ptr += words_per_line;
         }
      }
   }
}
