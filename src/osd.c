#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "defs.h"
#include "cpld.h"
#include "geometry.h"
#include "gitversion.h"
#include "info.h"
#include "logging.h"
#include "osd.h"
#include "rpi-gpio.h"
#include "rpi-mailbox.h"
#include "rpi-mailbox-interface.h"
#include "saa5050_font.h"
#include "rgb_to_fb.h"

// =============================================================
// Definitions for the size of the OSD
// =============================================================

#define NLINES         16

#define LINELEN        40

#define MAX_MENU_DEPTH  4

// =============================================================
// Main states that the OSD can be in
// =============================================================

typedef enum {
   IDLE,       // No menu
   CLOCK_CAL0, // Intermediate state in clock calibration
   CLOCK_CAL1, // Intermediate state in clock calibration
   MENU,       // Browsing a menu
   PARAM,      // Changing the value of a menu item
   INFO        // Viewing an info panel
} osd_state_t;

// =============================================================
// Friently names for certain OSD feature values
// =============================================================

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
   "Not Blue",
   "Atom Colour Normal",
   "Atom Colour Extended",
   "Atom Colour Acorn",
   "Atom Mono"
};

static const char *pllh_names[] = {
   "Unlocked",
   "2000ppm Slow",
   "1000ppm Slow",
   "Locked (Exact)",
   "1000ppm Fast",
   "2000ppm Fast"
};

static const char *deinterlace_names[] = {
   "None",
   "Simple Bob (CRT-like)",
   "Simple Motion Adaptive 1",
   "Simple Motion Adaptive 2",
   "Simple Motion Adaptive 3",
   "Simple Motion Adaptive 4",
   "Advanced Motion Adaptive"
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
// Feature definitions
// =============================================================

enum {
   F_PALETTE,
   F_SCANLINES,
   F_ELK,
   F_DEINTERLACE,
   F_VSYNC,
   F_MUX,
   F_PLLH,
#ifdef MULTI_BUFFER
   F_NBUFFERS,
#endif
   F_M7DISABLE,
   F_DEBUG,
   F_VLOCK
};

static param_t features[] = {
   {     F_PALETTE,       "Palette", 0,     NUM_PALETTES - 1, 1 },
   {   F_SCANLINES,     "Scanlines", 0,                    1, 1 },
   {         F_ELK,           "Elk", 0,                    1, 1 },
   { F_DEINTERLACE,   "Deinterlace", 0, NUM_DEINTERLACES - 1, 1 },
   {       F_VSYNC,         "Vsync", 0,                    1, 1 },
   {         F_MUX,     "Input Mux", 0,                    1, 1 },
   {        F_PLLH,    "HDMI Clock", 0,                    5, 1 },
#ifdef MULTI_BUFFER
   {    F_NBUFFERS,   "Num Buffers", 0,                    3, 1 },
#endif
   {   F_M7DISABLE, "Mode7 Disable", 0,                    1, 1 },
   {       F_DEBUG,         "Debug", 0,                    1, 1 },
   {       F_VLOCK, "Vertical Lock", 1,                  100, 1 },
   {            -1,            NULL, 0,                    0, 0 },
};

// =============================================================
// Menu definitions
// =============================================================


typedef enum {
   I_MENU,     // Item points to a sub-menu
   I_FEATURE,  // Item is a "feature" (i.e. managed by the osd)
   I_GEOMETRY, // Item is a "geometry" (i.e. managed by the geometry)
   I_PARAM,    // Item is a "parameter" (i.e. managed by the cpld)
   I_INFO,     // Item is an info screen
   I_BACK      // Item is a link back to the previous menu
} item_type_t;

typedef struct {
   item_type_t       type;
} base_menu_item_t;

typedef struct menu {
   char *name;
   base_menu_item_t *items[];
} menu_t;

typedef struct {
   item_type_t       type;
   menu_t           *child;
   void            (*rebuild)(menu_t *menu);
} child_menu_item_t;

typedef struct {
   item_type_t       type;
   param_t          *param;
} param_menu_item_t;

typedef struct {
   item_type_t       type;
   char             *name;
   void            (*show_info)(int line);
} info_menu_item_t;

typedef struct {
   item_type_t       type;
   char             *name;
} back_menu_item_t;

static void info_firmware_version(int line);
static void info_cal_summary(int line);
static void info_cal_detail(int line);
static void info_cal_raw(int line);

static info_menu_item_t cal_summary_ref      = { I_INFO, "Calibration Summary", info_cal_summary};
static info_menu_item_t cal_detail_ref       = { I_INFO, "Calibration Detail",  info_cal_detail};
static info_menu_item_t cal_raw_ref          = { I_INFO, "Calibration Raw",     info_cal_raw};
static info_menu_item_t firmware_version_ref = { I_INFO, "Firmware Version",    info_firmware_version};
static back_menu_item_t back_ref             = { I_BACK, "Return"};

static menu_t info_menu = {
   "Info Menu",
   {
      (base_menu_item_t *) &back_ref,
      (base_menu_item_t *) &cal_summary_ref,
      (base_menu_item_t *) &cal_detail_ref,
      (base_menu_item_t *) &cal_raw_ref,
      (base_menu_item_t *) &firmware_version_ref,
      NULL
   }
};

static param_menu_item_t palette_ref     = { I_FEATURE, &features[F_PALETTE]     };
static param_menu_item_t scanlines_ref   = { I_FEATURE, &features[F_SCANLINES]   };
static param_menu_item_t elk_ref         = { I_FEATURE, &features[F_ELK]         };
static param_menu_item_t deinterlace_ref = { I_FEATURE, &features[F_DEINTERLACE] };
static param_menu_item_t vsync_ref       = { I_FEATURE, &features[F_VSYNC]       };
static param_menu_item_t mux_ref         = { I_FEATURE, &features[F_MUX]         };
static param_menu_item_t pllh_ref        = { I_FEATURE, &features[F_PLLH]        };
#ifdef MULTI_BUFFER
static param_menu_item_t nbuffers_ref    = { I_FEATURE, &features[F_NBUFFERS]    };
#endif
static param_menu_item_t m7disable_ref   = { I_FEATURE, &features[F_M7DISABLE]   };
static param_menu_item_t debug_ref       = { I_FEATURE, &features[F_DEBUG]       };
static param_menu_item_t vlock_ref       = { I_FEATURE, &features[F_VLOCK]       };

static menu_t processing_menu = {
   "Processing Menu",
   {
      (base_menu_item_t *) &back_ref,
      (base_menu_item_t *) &deinterlace_ref,
      (base_menu_item_t *) &palette_ref,
      (base_menu_item_t *) &scanlines_ref,
      NULL
   }
};

static menu_t settings_menu = {
   "Settings Menu",
   {
      (base_menu_item_t *) &back_ref,
      (base_menu_item_t *) &elk_ref,
      (base_menu_item_t *) &mux_ref,
      (base_menu_item_t *) &debug_ref,
      (base_menu_item_t *) &m7disable_ref,
      (base_menu_item_t *) &vsync_ref,
      (base_menu_item_t *) &pllh_ref,
      (base_menu_item_t *) &nbuffers_ref,
      (base_menu_item_t *) &vlock_ref,
      NULL
   }
};

static param_menu_item_t dynamic_item[] = {
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL },
   { I_PARAM, NULL }
};

static menu_t geometry_menu = {
   "Geometry Menu",
   {
      // Allow space for max 20 params
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL
   }
};

static menu_t sampling_menu = {
   "Sampling Menu",
   {
      // Allow space for max 20 params
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL
   }
};

static void rebuild_geometry_menu(menu_t *menu);
static void rebuild_sampling_menu(menu_t *menu);

static child_menu_item_t info_menu_ref        = { I_MENU, &info_menu       , NULL};
static child_menu_item_t processing_menu_ref  = { I_MENU, &processing_menu , NULL};
static child_menu_item_t settings_menu_ref    = { I_MENU, &settings_menu   , NULL};
static child_menu_item_t geometry_menu_ref    = { I_MENU, &geometry_menu   , rebuild_geometry_menu};
static child_menu_item_t sampling_menu_ref    = { I_MENU, &sampling_menu   , rebuild_sampling_menu};

static menu_t main_menu = {
   "Main Menu",
   {
      (base_menu_item_t *) &back_ref,
      (base_menu_item_t *) &info_menu_ref,
      (base_menu_item_t *) &processing_menu_ref,
      (base_menu_item_t *) &settings_menu_ref,
      (base_menu_item_t *) &geometry_menu_ref,
      (base_menu_item_t *) &sampling_menu_ref,
      NULL
   }
};

// =============================================================
// Static local variables
// =============================================================

static char buffer[LINELEN * NLINES];

static int attributes[NLINES];

// Mapping table for expanding 12-bit row to 24 bit pixel (3 words) with 4 bits/pixel
static uint32_t double_size_map_4bpp[0x1000 * 3];

// Mapping table for expanding 12-bit row to 12 bit pixel (2 words + 2 words) with 4 bits/pixel
static uint32_t normal_size_map_4bpp[0x1000 * 4];

// Mapping table for expanding 12-bit row to 24 bit pixel (6 words) with 8 bits/pixel
static uint32_t double_size_map_8bpp[0x1000 * 6];

// Mapping table for expanding 12-bit row to 12 bit pixel (3 words) with 8 bits/pixel
static uint32_t normal_size_map_8bpp[0x1000 * 3];

// Temporary buffer for assembling OSD lines
static char message[80];

// Is the OSD currently active
static int active = 0;

// Main state of the OSD
osd_state_t osd_state;

// Current menu depth
static int depth = 0;

// Currently active menu
static menu_t *current_menu[MAX_MENU_DEPTH];

// Index to the currently selected menu
static int current_item[MAX_MENU_DEPTH];

// Currently selected palette setting
static int palette   = PALETTE_DEFAULT;

// Currently selected input mux setting
static int mux       = 0;

// Keymap
static int key_enter     = OSD_SW1;
static int key_menu_up   = OSD_SW2;
static int key_menu_down = OSD_SW3;
static int key_value_dec = OSD_SW2;
static int key_value_inc = OSD_SW3;
static int key_auto_cal  = OSD_SW3;
static int key_clock_cal = OSD_SW2;

// Whether the menu back pointer is at the start (0) or end (1) of the menu
static int return_at_end = 1;

// =============================================================
// Private Methods
// =============================================================

static int get_feature(int num) {
   switch (num) {
   case F_PALETTE:
      return palette;
   case F_DEINTERLACE:
      return get_deinterlace();
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
   case F_M7DISABLE:
      return get_m7disable();
   case F_VLOCK:
      return get_vlockline();
   }
   return -1;
}

static void set_feature(int num, int value) {
   switch (num) {
   case F_PALETTE:
      palette = value;
      osd_update_palette();
      break;
   case F_DEINTERLACE:
      set_deinterlace(value);
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
      osd_update_palette();
      break;
   case F_M7DISABLE:
      set_m7disable(value);
      break;
   case F_VLOCK:
      set_vlockline(value);
      break;
   }
}

// Wrapper to extract the name of a menu item
static const char *item_name(base_menu_item_t *item) {
   switch (item->type) {
   case I_MENU:
      return ((child_menu_item_t *)item)->child->name;
   case I_FEATURE:
   case I_GEOMETRY:
   case I_PARAM:
      return ((param_menu_item_t *)item)->param->name;
   case I_INFO:
      return ((info_menu_item_t *)item)->name;
   case I_BACK:
      return ((back_menu_item_t *)item)->name;
   default:
      // Should never hit this case
      return NULL;
   }
}

// Test if a parameter is a simple boolean
static int is_boolean_param(param_menu_item_t *param_item) {
   return param_item->param->min == 0 && param_item->param->max == 1;
}

// Set wrapper to abstract different between I_FEATURE, I_GEOMETRY and I_PARAM
static void set_param(param_menu_item_t *param_item, int value) {
   item_type_t type = param_item->type;
   if (type == I_FEATURE) {
      set_feature(param_item->param->key, value);
   } else if (type == I_GEOMETRY) {
      geometry_set_value(param_item->param->key, value);
   } else {
      cpld->set_value(param_item->param->key, value);
   }
}

// Get wrapper to abstract different between I_FEATURE, I_GEOMETRY and I_PARAM
static int get_param(param_menu_item_t *param_item) {
   item_type_t type = param_item->type;
   if (type == I_FEATURE) {
      return get_feature(param_item->param->key);
   } else if (type == I_GEOMETRY) {
      return geometry_get_value(param_item->param->key);
   } else {
      return cpld->get_value(param_item->param->key);
   }
}

static void toggle_boolean_param(param_menu_item_t *param_item) {
   set_param(param_item, get_param(param_item) ^ 1);
}

static const char *get_param_string(param_menu_item_t *param_item) {
   static char number[16];
   item_type_t type = param_item->type;
   param_t   *param = param_item->param;
   // Read the current value of the specified feature
   int value = get_param(param_item);
   // Convert certain features to human readable strings
   if (type == I_FEATURE) {
      switch (param->key) {
      case F_PALETTE:
         return palette_names[value];
      case F_DEINTERLACE:
         return deinterlace_names[value];
      case F_PLLH:
         return pllh_names[value];
#ifdef MULTI_BUFFER
      case F_NBUFFERS:
         return nbuffer_names[value];
#endif
      }
   }
   if (is_boolean_param(param_item)) {
      return value ? "On" : "Off";
   }
   sprintf(number, "%d", value);
   return number;
}

static void info_firmware_version(int line) {
   sprintf(message, "Pi Firmware: %s", GITVERSION);
   osd_set(line, 0, message);
   sprintf(message, "%s CPLD: v%x.%x",
           cpld->name,
           (cpld->get_version() >> VERSION_MAJOR_BIT) & 0xF,
           (cpld->get_version() >> VERSION_MINOR_BIT) & 0xF);
   osd_set(line + 1, 0, message);
}

static void info_cal_summary(int line) {
   const char *machine = get_elk() ? "Elk" : "Beeb";
   if (clock_error_ppm > 0) {
      sprintf(message, "Clk Err: %d ppm (%s slower than Pi)", clock_error_ppm, machine);
   } else if (clock_error_ppm < 0) {
      sprintf(message, "Clk Err: %d ppm (%s faster than Pi)", -clock_error_ppm, machine);
   } else {
      sprintf(message, "Clk Err: %d ppm (exact match)", clock_error_ppm);
   }
   osd_set(line, 0, message);
   if (cpld->show_cal_summary) {
      cpld->show_cal_summary(line + 2);
   } else {
      sprintf(message, "show_cal_summary() not implemented");
      osd_set(line + 2, 0, message);
   }
}

static void info_cal_detail(int line) {
   if (cpld->show_cal_details) {
      cpld->show_cal_details(line);
   } else {
      sprintf(message, "show_cal_details() not implemented");
      osd_set(line, 0, message);
   }
}

static void info_cal_raw(int line) {
   if (cpld->show_cal_raw) {
      cpld->show_cal_raw(line);
   } else {
      sprintf(message, "show_cal_raw() not implemented");
      osd_set(line, 0, message);
   }
}

static void rebuild_menu(menu_t *menu, item_type_t type, param_t *param_ptr) {
   int i = 0;
   if (!return_at_end) {
      menu->items[i++] = (base_menu_item_t *)&back_ref;
   }
   while (param_ptr->key >= 0) {
      dynamic_item[i].type = type;
      dynamic_item[i].param = param_ptr;
      menu->items[i] = (base_menu_item_t *)&dynamic_item[i];
      param_ptr++;
      i++;
   }
   if (return_at_end) {
      menu->items[i++] = (base_menu_item_t *)&back_ref;
   }
}

static void rebuild_geometry_menu(menu_t *menu) {
   rebuild_menu(menu, I_GEOMETRY, geometry_get_params());
}

static void rebuild_sampling_menu(menu_t *menu) {
   rebuild_menu(menu, I_PARAM, cpld->get_params());
}

static void redraw_menu() {
   menu_t *menu = current_menu[depth];
   int current = current_item[depth];
   int line = 0;
   base_menu_item_t **item_ptr;
   base_menu_item_t *item;
   if (osd_state == INFO) {
      item = menu->items[current];
      // We should always be on an INFO item...
      if (item->type == I_INFO) {
         info_menu_item_t *info_item = (info_menu_item_t *)item;
         osd_set(line, ATTR_DOUBLE_SIZE, info_item->name);
         line += 2;
         info_item->show_info(line);
      }
   } else if (osd_state == MENU || osd_state == PARAM) {
      osd_set(line, ATTR_DOUBLE_SIZE, menu->name);
      line += 2;
      // Work out the longest item name
      int max = 0;
      item_ptr = menu->items;
      while ((item = *item_ptr++)) {
         int len = strlen(item_name(item));
         if (len > max) {
            max = len;
         }
      }
      item_ptr = menu->items;
      int i = 0;
      while ((item = *item_ptr++)) {
         char *mp         = message;
         char sel_none    = ' ';
         char sel_open    = (i == current) ? ']' : sel_none;
         char sel_close   = (i == current) ? '[' : sel_none;
         const char *name = item_name(item);
         *mp++ = (osd_state != PARAM) ? sel_open : sel_none;
         strcpy(mp, name);
         mp += strlen(mp);
         if ((item)->type == I_FEATURE || (item)->type == I_GEOMETRY || (item)->type == I_PARAM) {
            int len = strlen(name);
            while (len < max) {
               *mp++ = ' ';
               len++;
            }
            *mp++ = ' ';
            *mp++ = '=';
            *mp++ = (osd_state == PARAM) ? sel_open : sel_none;
            strcpy(mp, get_param_string((param_menu_item_t *)item));
            mp += strlen(mp);
         }
         *mp++ = sel_close;
         *mp++ = '\0';
         osd_set(line++, 0, message);
         i++;
      }
   }
}

static void cycle_menu(menu_t *menu) {
   base_menu_item_t **mp = menu->items;
   base_menu_item_t *first = *mp;
   while (*(mp + 1)) {
      *mp = *(mp + 1);
      mp++;
   }
   *mp = first;
}

static int get_key_down_duration(int key) {
   switch (key) {
   case OSD_SW1:
      return sw1counter;
   case OSD_SW2:
      return sw2counter;
   case OSD_SW3:
      return sw3counter;
   }
   return 0;
}


// =============================================================
// Public Methods
// =============================================================

static uint32_t palette_data[256];

void osd_update_palette() {
   int m;
   int num_colours = (capinfo->bpp == 8) ? 256 : 16;
   for (int i = 0; i < num_colours; i++) {
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
      case PALETTE_ATOM_COLOUR_NORMAL:
         // In the Atom CPLD, colour bit 3 indicates additional colours
         //  8 = 1000 = normal orange
         //  9 = 1001 = bright orange
         // 10 = 1010 = dark green text background
         // 11 = 1011 = dark orange text background
         if (i & 8) {
            if ((i & 7) == 0) {
               // orange
               r = 160; g = 80; b = 0;
            } else if ((i & 7) == 1) {
               // bright orange
               r = 255; g = 127; b = 0;
            } else {
               // otherwise show as black
               r = g = b = 0;
            }
         }
         break;
      case PALETTE_ATOM_COLOUR_EXTENDED:
         // In the Atom CPLD, colour bit 3 indicates additional colours
         if (i & 8) {
            if ((i & 7) == 0) {
               // orange
               r = 160; g = 80; b = 0;
            } else if ((i & 7) == 1) {
               // bright orange
               r = 255; g = 127; b = 0;
            } else if ((i & 7) == 2) {
               // dark green
               r = 0; g = 31; b = 0;
            } else if ((i & 7) == 3) {
               // dark orange
               r = 31; g = 15; b = 0;
            } else {
               // otherwise show as black
               r = g = b = 0;
            }
         }
         break;
      case PALETTE_ATOM_COLOUR_ACORN:
         // In the Atom CPLD, colour bit 3 indicates additional colours
         if (i & 8) {
            if ((i & 6) == 0) {
               // orange => red
               r = 255; g = 0; b = 0;
            } else {
               // otherwise show as black
               r = g = b = 0;
            }
         }
         break;
      case PALETTE_ATOM_MONO:
         m = 0;
         switch (i) {
         case 3: // yellow
         case 7: // white (buff)
         case 9: // bright orange
            // Y = WH (0.42V)
            m = 255;
            break;
         case 2: // green
         case 5: // magenta
         case 6: // cyan
         case 8: // normal orange
            // Y = WM (0.54V)
            m = 255 * (72 - 54) / (72 - 42);
            break;
         case 1: // red
         case 4: // blue
            // Y = WL (0.65V)
            m = 255 * (72 - 65) / (72 - 42);
            break;
         default:
            // Y = BL (0.72V)
            m = 0;
         }
         r = g = b = m;
         break;
      }
      if (active) {
         if (i >= (num_colours >> 1)) {
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

   // Flush the previous swapBuffer() response from the GPU->ARM mailbox
   RPI_Mailbox0Flush( MB0_TAGS_ARM_TO_VC  );
   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_SET_PALETTE, num_colours, palette_data);
   RPI_PropertyProcess();
}

void osd_clear() {
   if (active) {
      memset(buffer, 0, sizeof(buffer));
      osd_update((uint32_t *)capinfo->fb, capinfo->pitch);
      active = 0;
      RPI_SetGpioValue(LED1_PIN, active);
      osd_update_palette();
   }
}

void osd_set(int line, int attr, char *text) {
   if (!active) {
      active = 1;
      RPI_SetGpioValue(LED1_PIN, active);
      osd_update_palette();
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

void osd_refresh() {
   osd_clear();
   if (osd_state == MENU || osd_state == PARAM || osd_state == INFO) {
      redraw_menu();
   }
}

int osd_key(int key) {
   item_type_t type;
   base_menu_item_t *item = current_menu[depth]->items[current_item[depth]];
   child_menu_item_t *child_item = (child_menu_item_t *)item;
   param_menu_item_t *param_item = (param_menu_item_t *)item;
   int val;
   int ret = -1;
   static int last_vsync;

   switch (osd_state) {

   case IDLE:
      if (key == key_enter) {
         // Enter
         osd_state = MENU;
         current_menu[depth] = &main_menu;
         current_item[depth] = 0;
         redraw_menu();
      } else if (key == key_clock_cal) {
         // Clock Calibration
         osd_set(0, ATTR_DOUBLE_SIZE, "Clock Calibration");
         // Record the starting value of vsync
         last_vsync = get_vsync();
         // Enable vsync
         set_vsync(1);
         // Fire OSD_EXPIRED in 50 frames time
         ret = 50;
         // come back to CLOCK_CAL0
         osd_state = CLOCK_CAL0;
      } else if (key == key_auto_cal) {
         // Auto Calibration
         osd_set(0, ATTR_DOUBLE_SIZE, "Auto Calibration");
         action_calibrate_auto();
         // Fire OSD_EXPIRED in 50 frames time
         ret = 50;
      } else if (key == OSD_EXPIRED) {
         osd_clear();
      }
      break;

   case CLOCK_CAL0:
      // Do the actual clock calibration
      action_calibrate_clocks();
      // Fire OSD_EXPIRED in 50 frames time
      ret = 50;
      // come back to CLOCK_CAL1
      osd_state = CLOCK_CAL1;
      break;

   case CLOCK_CAL1:
      // Restore the original setting of vsync
      set_vsync(last_vsync);
      osd_clear();
      // back to CLOCK_IDLE
      osd_state = IDLE;
      break;

   case MENU:
      type = item->type;
      if (key == key_enter) {
         // ENTER
         switch (type) {
         case I_MENU:
            depth++;
            current_menu[depth] = child_item->child;
            current_item[depth] = 0;
            // Rebuild dynamically populated menus, e.g. the sampling and geometry menus that are mode specific
            if (child_item->rebuild) {
               child_item->rebuild(child_item->child);
            }
            osd_clear();
            redraw_menu();
            break;
         case I_FEATURE:
         case I_GEOMETRY:
         case I_PARAM:
            if (is_boolean_param(param_item)) {
               // If it's a boolean item, then just toggle it
               toggle_boolean_param(param_item);
            } else {
               // If not then move to the parameter editing state
               osd_state = PARAM;
            }
            redraw_menu();
            break;
         case I_INFO:
            osd_state = INFO;
            osd_clear();
            redraw_menu();
            break;
         case I_BACK:
            osd_clear();
            if (depth == 0) {
               osd_state = IDLE;
            } else {
               depth--;
               redraw_menu();
            }
            break;
         }
      } else if (key == key_menu_up) {
         // PREVIOUS
         if (current_item[depth] == 0) {
            while (current_menu[depth]->items[current_item[depth]] != NULL)
               current_item[depth]++;
         }
         current_item[depth]--;
         redraw_menu();
      } else if (key == key_menu_down) {
         // NEXT
         current_item[depth]++;
         if (current_menu[depth]->items[current_item[depth]] == NULL) {
            current_item[depth] = 0;
         }
         redraw_menu();
      }
      break;

   case PARAM:
      type = item->type;
      if (key == key_enter) {
         // ENTER
         osd_state = MENU;
      } else {
         val = get_param(param_item);
         int delta = param_item->param->step;
         int range = param_item->param->max - param_item->param->min;
         // Get the key pressed duration, in units of ~20ms
         int duration = get_key_down_duration(key);
         // Implement an exponential function for the increment
         while (range > delta * 100 && duration > 100) {
            delta *= 10;
            duration -= 100;
         }
         if (key == key_value_dec) {
            // PREVIOUS
            val -= delta;
         } else if (key == key_value_inc) {
            // NEXT
            val += delta;
         }
         if (val < param_item->param->min) {
            val = param_item->param->max;
         }
         if (val > param_item->param->max) {
            val = param_item->param->min;
         }
         set_param(param_item, val);
      }
      redraw_menu();
      break;

   case INFO:
      if (key == key_enter) {
         // ENTER
         osd_state = MENU;
         osd_clear();
         redraw_menu();
      }
      break;
   }
   return ret;
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
   memset(normal_size_map_4bpp, 0, sizeof(normal_size_map_4bpp));
   memset(double_size_map_4bpp, 0, sizeof(double_size_map_4bpp));
   memset(normal_size_map_8bpp, 0, sizeof(normal_size_map_8bpp));
   memset(double_size_map_8bpp, 0, sizeof(double_size_map_8bpp));
   for (int i = 0; i < NLINES; i++) {
      attributes[i] = 0;
   }
   for (int i = 0; i <= 0xFFF; i++) {
      for (int j = 0; j < 12; j++) {
         // j is the pixel font data bit, with bit 11 being left most
         if (i & (1 << j)) {

            // ======= 4 bits/pixel tables ======

            // Normal size, odd characters
            // cccc.... dddddddd
            if (j < 8) {
               normal_size_map_4bpp[i * 4 + 3] |= 0x8 << (4 * (7 - (j ^ 1)));   // dddddddd
            } else {
                  normal_size_map_4bpp[i * 4 + 2] |= 0x8 << (4 * (15 - (j ^ 1)));  // cccc....
            }
            // Normal size, even characters
            // aaaaaaaa ....bbbb
            if (j < 4) {
               normal_size_map_4bpp[i * 4 + 1] |= 0x8 << (4 * (3 - (j ^ 1)));   // ....bbbb
            } else {
               normal_size_map_4bpp[i * 4    ] |= 0x8 << (4 * (11 - (j ^ 1)));  // aaaaaaaa
            }
            // Double size
            // aaaaaaaa bbbbbbbb cccccccc
            if (j < 4) {
               double_size_map_4bpp[i * 3 + 2] |= 0x88 << (8 * (3 - j));  // cccccccc
            } else if (j < 8) {
               double_size_map_4bpp[i * 3 + 1] |= 0x88 << (8 * (7 - j));  // bbbbbbbb
            } else {
               double_size_map_4bpp[i * 3    ] |= 0x88 << (8 * (11 - j)); // aaaaaaaa
            }

            // ======= 8 bits/pixel tables ======

            // Normal size
            // aaaaaaaa bbbbbbbb cccccccc
            if (j < 4) {
               normal_size_map_8bpp[i * 3 + 2] |= 0x80 << (8 * (3 - j));  // cccccccc
            } else if (j < 8) {
               normal_size_map_8bpp[i * 3 + 1] |= 0x80 << (8 * (7 - j));  // bbbbbbbb
            } else {
               normal_size_map_8bpp[i * 3    ] |= 0x80 << (8 * (11 - j)); // aaaaaaaa
            }

            // Double size
            // aaaaaaaa bbbbbbbb cccccccc dddddddd eeeeeeee ffffffff
            if (j < 2) {
               double_size_map_8bpp[i * 6 + 5] |= 0x8080 << (16 * (1 - j));  // ffffffff
            } else if (j < 4) {
               double_size_map_8bpp[i * 6 + 4] |= 0x8080 << (16 * (3 - j));  // eeeeeeee
            } else if (j < 6) {
               double_size_map_8bpp[i * 6 + 3] |= 0x8080 << (16 * (5 - j));  // dddddddd
            } else if (j < 8) {
               double_size_map_8bpp[i * 6 + 2] |= 0x8080 << (16 * (7 - j));  // cccccccc
            } else if (j < 10) {
               double_size_map_8bpp[i * 6 + 1] |= 0x8080 << (16 * (9 - j));  // bbbbbbbb
            } else {
               double_size_map_8bpp[i * 6    ] |= 0x8080 << (16 * (11 - j)); // aaaaaaaa
            }
         }
      }
   }
   // Initialize the OSD features
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
   prop = get_cmdline_prop("m7disable");
   if (prop) {
      int val = atoi(prop);
      set_feature(F_M7DISABLE, val);
      log_info("config.txt:   m7disable = %d", val);
   }
   // Initialize the CPLD sampling points
   for (int p = 0; p < 2; p++) {
      for (int m7 = 0; m7 <= 1; m7++) {
         char *propname;
         if (p == 0) {
            propname = m7 ? "sampling7" : "sampling06";
         } else {
            propname = m7 ? "geometry7" : "geometry06";
         }
         prop = get_cmdline_prop(propname);
         if (prop) {
            cpld->set_mode(NULL, m7);
            geometry_set_mode(m7);
            log_info("config.txt:  %s = %s", propname, prop);
            char *prop2 = strtok(prop, ",");
            int i = 0;
            while (prop2) {
               param_t *param;
               if (p == 0) {
                  param = cpld->get_params() + i;
               } else {
                  param = geometry_get_params() + i;
               }
               if (param->key < 0) {
                  log_warn("Too many sampling sub-params, ignoring the rest");
                  break;
               }
               int val = atoi(prop2);
               if (p == 0) {
                  log_info("cpld: %s = %d", param->name, val);
                  cpld->set_value(param->key, val);
               } else {
                  log_info("geometry: %s = %d", param->name, val);
                  geometry_set_value(param->key, val);
               }
               prop2 = strtok(NULL, ",");
               i++;
            }
         }
      }
   }
   // Properties below this point are not updateable in the UI
   prop = get_cmdline_prop("keymap");
   if (prop) {
      int i = 0;
      while (*prop) {
         int val = (*prop++) - '1' + OSD_SW1;
         if (val >= OSD_SW1 && val <= OSD_SW3) {
            switch (i) {
            case 0:
               key_enter = val;
               break;
            case 1:
               key_menu_up = val;
               break;
            case 2:
               key_menu_down = val;
               break;
            case 3:
               key_value_dec = val;
               break;
            case 4:
               key_value_inc = val;
               break;
            case 5:
               key_auto_cal = val;
               break;
            case 6:
               key_clock_cal = val;
               break;
            }
         }
         i++;
      }
   }
   // Implement a return property that selects whether the menu
   // return links is placed at the start (0) or end (1).
   prop = get_cmdline_prop("return");
   if (prop) {
      return_at_end = atoi(prop);
   }
   if (return_at_end) {
      // The menu's are constructed with the back link in at the start
      cycle_menu(&main_menu);
      cycle_menu(&info_menu);
      cycle_menu(&processing_menu);
      cycle_menu(&settings_menu);
   }
   // Disable CPLDv2 specific features for CPLDv1
   if (((cpld->get_version() >> VERSION_MAJOR_BIT) & 0x0F) < 2) {
      features[F_DEINTERLACE].max = DEINTERLACE_MA4;
      if (get_feature(F_DEINTERLACE) > features[F_DEINTERLACE].max) {
         set_feature(F_DEINTERLACE, DEINTERLACE_MA1); // TODO: Decide whether this is the right fallback
      }
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
            if (capinfo->bpp == 8) {
               if (attr & ATTR_DOUBLE_SIZE) {
                  uint32_t *map_ptr = double_size_map_8bpp + data * 6;
                  for (int k = 0; k < 6; k++) {
                     *word_ptr &= 0x7f7f7f7f;
                     *word_ptr |= *map_ptr;
                     *(word_ptr + words_per_line) &= 0x7f7f7f7f;
                     *(word_ptr + words_per_line) |= *map_ptr;
                     word_ptr++;
                     map_ptr++;
                  }
               } else {
                  uint32_t *map_ptr = normal_size_map_8bpp + data * 3;
                  for (int k = 0; k < 3; k++) {
                     *word_ptr &= 0x7f7f7f7f;
                     *word_ptr |= *map_ptr;
                     word_ptr++;
                     map_ptr++;
                  }
               }
            } else {
               if (attr & ATTR_DOUBLE_SIZE) {
                  // Map to three 32-bit words in frame buffer format
                  uint32_t *map_ptr = double_size_map_4bpp + data * 3;
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
                     uint32_t *map_ptr = normal_size_map_4bpp + (data << 2) + 2;
                     *word_ptr &= 0x7777FFFF;
                     *word_ptr |= *map_ptr;
                     word_ptr++;
                     map_ptr++;
                     *word_ptr &= 0x77777777;
                     *word_ptr |= *map_ptr;
                     word_ptr++;
                  } else {
                     // even character
                     uint32_t *map_ptr = normal_size_map_4bpp + (data << 2);
                     *word_ptr &= 0x77777777;
                     *word_ptr |= *map_ptr;
                     word_ptr++;
                     map_ptr++;
                     *word_ptr &= 0xFFFF7777;
                     *word_ptr |= *map_ptr;
                  }
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
            if (capinfo->bpp == 8) {
               if (attr & ATTR_DOUBLE_SIZE) {
                  uint32_t *map_ptr = double_size_map_8bpp + data * 6;
                  for (int k = 0; k < 6; k++) {
                     *word_ptr |= *map_ptr;
                     *(word_ptr + words_per_line) |= *map_ptr;
                     word_ptr++;
                     map_ptr++;
                  }
               } else {
                  uint32_t *map_ptr = normal_size_map_8bpp + data * 3;
                  for (int k = 0; k < 3; k++) {
                     *word_ptr |= *map_ptr;
                     word_ptr++;
                     map_ptr++;
                  }
               }
            } else {
               if (attr & ATTR_DOUBLE_SIZE) {
                  // Map to three 32-bit words in frame buffer format
                  uint32_t *map_ptr = double_size_map_4bpp + data * 3;
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
                     uint32_t *map_ptr = normal_size_map_4bpp + (data << 2) + 2;
                     *word_ptr |= *map_ptr;
                     word_ptr++;
                     map_ptr++;
                     *word_ptr |= *map_ptr;
                     word_ptr++;
                  } else {
                     // even character
                     uint32_t *map_ptr = normal_size_map_4bpp + (data << 2);
                     *word_ptr |= *map_ptr;
                     word_ptr++;
                     map_ptr++;
                     *word_ptr |= *map_ptr;
                  }
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
