#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
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
#include "8x8_font.h"
#include "rgb_to_fb.h"
#include "rgb_to_hdmi.h"
#include "filesystem.h"
#include "fatfs/ff.h"
#include "jtag/update_cpld.h"
#include <math.h>

// =============================================================
// Definitions for the size of the OSD
// =============================================================

#define NLINES         30

#define FONT_THRESHOLD    25

#define LINELEN        42

#define MAX_MENU_DEPTH  4

#define DEFAULT_CPLD_FIRMWARE_DIR "/cpld_firmware/recovery"

#define PI 3.14159265f
// =============================================================
// Definitions for the key press interface
// =============================================================

#define LONG_KEY_PRESS_DURATION 25

// =============================================================
// Main states that the OSD can be in
// =============================================================

typedef enum {
   IDLE,          // Wait for button to be pressed
   DURATION,      // Determine whether short or long button press

   MIN_ACTION,    // Marker state, never actually used

   // The next N states are the pre-defined action states
   // At most 6 of these can be bound to button presses (3 buttons, long/short)
   A0_LAUNCH,     // Action 0: Launch menu system
   A1_CAPTURE,    // Action 1: Screen capture
   A2_CLOCK_CAL,  // Action 2: HDMI clock calibration
   A3_AUTO_CAL,   // Action 3: Auto calibration
   A4_SCANLINES,  // Action 4: Toggle scanlines
   A5_NTSCCOLOUR, // Action 5: Toggle ntsc
   A6_SPARE,      // Action 6: Spare
   A7_SPARE,      // Action 7: Spare

   MAX_ACTION,    // Marker state, never actually used
   A1_CAPTURE_SUB,    // Action 1: Screen capture
   CLOCK_CAL0,    // Intermediate state in clock calibration
   CLOCK_CAL1,    // Intermediate state in clock calibration

   NTSC_MESSAGE,
   MENU,          // Browsing a menu
   PARAM,         // Changing the value of a menu item
   INFO           // Viewing an info panel
} osd_state_t;

// =============================================================
// Friently names for certain OSD feature values
// =============================================================


static char *default_palette_names[] = {
   "RGB",
   "RGBI",
   "RGBI_(CGA)",
   "RGBI_(Spectrum)",
   "RGBrgb_(Spectrum)",
   "RGBrgb_(Amstrad)",
   "RrGgBb_(EGA)",
   "MDA-Hercules",
   "Dragon-CoCo",
   "Dragon-CoCo_Full",
   "Dragon-CoCo_Emu",
   "Atom_MKII",
   "Atom_MKII_Plus",
   "Atom_MKII_Full",
   "Mono_(2_level)",
   "Mono_(3_level)",
   "Mono_(4_level)",
   "Mono_(6_level)",
   "TI-99-4a",
   "Spectrum_48K_9Col",
   "Colour_Genie_S24",
   "Colour_Genie_S25",
   "Colour_Genie_N25",
   "Commodore_64",
   "Commodore_64_Rev1",
   "Test_4_Lvl_G_or_Y",
   "Test_4_Lvl_B_or_U",
   "Test_4_Lvl_R_or_V",
};

static const char *palette_control_names[] = {
   "Off",
   "In Band Commands",
   "CGA NTSC Artifact",
   "Mono NTSC Artifact",
   "Auto NTSC Artifact",
   "C64 PAL Artifact",
   "Atari GTIA"
};

static const char *return_names[] = {
   "Start",
   "End"
};

static const char *vlockmode_names[] = {
   "Genlocked (Exact)",
   "1000ppm Fast",
   "2000ppm Fast",
   "Unlocked",
   "2000ppm Slow",
   "1000ppm Slow"
};

static const char *deinterlace_names[] = {
   "None (Weave)",
   "Simple Bob",
   "Simple Motion 1",
   "Simple Motion 2",
   "Simple Motion 3",
   "Simple Motion 4",
   "Advanced Motion"
};

#ifdef MULTI_BUFFER
static const char *nbuffer_names[] = {
   "Single buffered",
   "Double buffered",
   "Triple buffered",
   "Quadruple buffered"
};
#endif

static const char *autoswitch_names[] = {
   "Off",
   "Sub-Profile",
   "BBC Mode 7"
};

static const char *overscan_names[] = {
   "0%",
   "10%",
   "20%",
   "30%",
   "40%",
   "50%",
   "60%",
   "70%",
   "80%",
   "90%",
   "100%"
};

static const char *colour_names[] = {
   "Normal",
   "Monochrome",
   "Green",
   "Amber"
};

static const char *invert_names[] = {
   "Normal",
   "Invert RGB/YUV",
   "Invert G/Y"
};

static const char *scaling_names[] = {
   "Auto",
   "Integer / Sharp",
   "Integer / Soft",
   "Integer / Softer",
   "Interpolate 4:3 / Soft",
   "Interpolate 4:3 / Softer",
   "Interpolate Full / Soft",
   "Interpolate Full / Softer"
};

static const char *frontend_names_6[] = {
   "3 BIT RGB",
   "12 BIT Simple",
   "Atom",
   "6 BIT RGB",
   "6 BIT RGB Analog Issue 3",
   "6 BIT RGB Analog Issue 2",
   "6 BIT RGB Analog Issue 1A",
   "6 BIT RGB Analog Issue 1B",
   "6 BIT YUV Analog Issue 3",
   "6 BIT YUV Analog Issue 2"
};

static const char *frontend_names_8[] = {
   "3 BIT RGB",
   "12 BIT Simple",
   "Atom",
   "8/12 BIT RGB",
   "8 BIT RGB Analog Issue 3",
   "8 BIT RGB Analog Issue 2",
   "8 BIT RGB Analog Issue 1A",
   "8 BIT RGB Analog Issue 1B",
   "8 BIT YUV Analog Issue 3",
   "8 BIT YUV Analog Issue 2"

};

static const char *vlockspeed_names[] = {
   "Slow (333PPM)",
   "Medium (1000PPM)",
   "Fast (2000PPM)"
};

static const char *vlockadj_names[] = {
   "-5% to +5%",
   "Full Range",
   "Up to 260Mhz"
};

static const char *fontsize_names[] = {
   "Always 8x8",
   "Auto 12x20 4bpp",
   "Auto 12x20 8bpp"
};

static const char *even_scaling_names[] = {
   "Even",
   "Uneven (3:2>>4:3)"
};

static const char *screencap_names[] = {
   "Normal",
   "Full Screen",
   "4:3 Crop",
   "Full 4:3 Crop"
};

// =============================================================
// Feature definitions
// =============================================================

enum {
   F_AUTOSWITCH,
   F_RESOLUTION,
   F_SCALING,
   F_FRONTEND,
   F_PROFILE,
   F_SUBPROFILE,
   F_PALETTE,
   F_PALETTECONTROL,
   F_NTSCCOLOUR,
   F_NTSCPHASE,
   F_TINT,
   F_SAT,
   F_CONT,
   F_BRIGHT,
   F_GAMMA,
   F_M7DEINTERLACE,
   F_DEINTERLACE,
   F_M7SCALING,
   F_NORMALSCALING,
   F_COLOUR,
   F_INVERT,
   F_SCANLINES,
   F_SCANLINESINT,
   F_OVERSCAN,
   F_CAPSCALE,
   F_FONTSIZE,
   F_BORDER,
   F_VSYNC,
   F_VLOCKMODE,
   F_VLOCKLINE,
   F_VLOCKSPEED,
   F_VLOCKADJ,
#ifdef MULTI_BUFFER
   F_NBUFFERS,
#endif
   F_RETURN,
   F_DEBUG
};

static param_t features[] = {
   {      F_AUTOSWITCH,       "Auto Switch",       "auto_switch", 0, NUM_AUTOSWITCHES - 1, 1 },
   {      F_RESOLUTION,        "Resolution",        "resolution", 0,                    0, 1 },
   {         F_SCALING,           "Scaling",           "scaling", 0,      NUM_SCALING - 1, 1 },
   {        F_FRONTEND,         "Interface",         "interface", 0,    NUM_FRONTENDS - 1, 1 },
   {         F_PROFILE,           "Profile",           "profile", 0,                    0, 1 },
   {      F_SUBPROFILE,       "Sub-Profile",        "subprofile", 0,                    0, 1 },
   {         F_PALETTE,           "Palette",           "palette", 0,                    0, 1 },
   {  F_PALETTECONTROL,   "Palette Control",   "palette_control", 0,     NUM_CONTROLS - 1, 1 },
   {      F_NTSCCOLOUR,"NTSC Artifact Colour",     "ntsc_colour", 0,                    1, 1 },
   {       F_NTSCPHASE,"NTSC Artifact Phase",       "ntsc_phase", 0,                    3, 1 },
   {           F_TINT,               "Tint",             "tint",-60,                   60, 1 },
   {            F_SAT,         "Saturation",        "saturation", 0,                  200, 1 },
   {           F_CONT,           "Contrast",         "contrast",  0,                  200, 1 },
   {         F_BRIGHT,         "Brightness",        "brightness", 0,                  200, 1 },
   {          F_GAMMA,              "Gamma",             "gamma", 10,                  300, 1 },
   {  F_M7DEINTERLACE,"Teletext Deinterlace","teletext_deinterlace", 0, NUM_M7DEINTERLACES - 1, 1 },
   {    F_DEINTERLACE, "Normal Deinterlace",  "normal_deinterlace", 0, NUM_DEINTERLACES - 1, 1 },
   {       F_M7SCALING,  "Teletext Scaling",     "teletext_scaling", 0,   NUM_ESCALINGS - 1, 1 },
   {   F_NORMALSCALING,    "Normal Scaling",    "normal_scaling", 0, NUM_ESCALINGS - 1, 1 },
   {          F_COLOUR,     "Output Colour",     "output_colour", 0,      NUM_COLOURS - 1, 1 },
   {          F_INVERT,     "Output Invert",     "output_invert", 0,       NUM_INVERT - 1, 1 },
   {       F_SCANLINES,         "Scanlines",         "scanlines", 0,                    1, 1 },
   {    F_SCANLINESINT,    "Scanline Level",    "scanline_level", 0,                   15, 1 },
   {        F_OVERSCAN,          "Overscan",          "overscan", 0,     NUM_OVERSCAN - 1, 1 },
   {        F_CAPSCALE,    "ScreenCap Size",    "screencap_size", 0,    NUM_SCREENCAP - 1, 1 },
   {        F_FONTSIZE,         "Font Size",         "font_size", 0,     NUM_FONTSIZE - 1, 1 },
   {          F_BORDER,     "Border Colour",     "border_colour", 0,                  255, 1 },
   {           F_VSYNC,  "V Sync Indicator",   "vsync_indicator", 0,                    1, 1 },
   {       F_VLOCKMODE,       "V Lock Mode",        "vlock_mode", 0,         NUM_HDMI - 1, 1 },
   {       F_VLOCKLINE,      "Genlock Line",      "genlock_line",35,                  140, 1 },
   {      F_VLOCKSPEED,     "Genlock Speed",     "genlock_speed", 0,   NUM_VLOCKSPEED - 1, 1 },
   {        F_VLOCKADJ,    "Genlock Adjust",    "genlock_adjust", 0,     NUM_VLOCKADJ - 2, 1 },  //-2 so disables 260 mhz for now
#ifdef MULTI_BUFFER
   {        F_NBUFFERS,       "Num Buffers",       "num_buffers", 0,                    3, 1 },
#endif
   {          F_RETURN,   "Return Position",            "return", 0,                    1, 1 },
   {           F_DEBUG,             "Debug",             "debug", 0,                    1, 1 },
   {                -1,                NULL,                NULL, 0,                    0, 0 }
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
   I_BACK,     // Item is a link back to the previous menu
   I_SAVE,     // Item is a saving profile option
   I_RESTORE,  // Item is a restoring a profile option
   I_UPDATE,   // Item is a cpld update
   I_CALIBRATE // Item is a calibration update
} item_type_t;

typedef struct {
   item_type_t       type;
} base_menu_item_t;

typedef struct menu {
   char *name;
   void            (*rebuild)(struct menu *this);
   base_menu_item_t *items[];
} menu_t;

typedef struct {
   item_type_t       type;
   menu_t           *child;
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

typedef struct {
   item_type_t       type;
   char             *name;
} action_menu_item_t;

static void info_source_summary(int line);
static void info_system_summary(int line);
static void info_cal_summary(int line);
static void info_cal_detail(int line);
static void info_cal_raw(int line);
static void info_credits(int line);
static void info_reboot(int line);

static void rebuild_geometry_menu(menu_t *menu);
static void rebuild_sampling_menu(menu_t *menu);
static void rebuild_update_cpld_menu(menu_t *menu);

static info_menu_item_t source_summary_ref   = { I_INFO, "Source Summary",      info_source_summary};
static info_menu_item_t system_summary_ref   = { I_INFO, "System Summary",      info_system_summary};
static info_menu_item_t cal_summary_ref      = { I_INFO, "Calibration Summary", info_cal_summary};
static info_menu_item_t cal_detail_ref       = { I_INFO, "Calibration Detail",  info_cal_detail};
static info_menu_item_t cal_raw_ref          = { I_INFO, "Calibration Raw",     info_cal_raw};
static info_menu_item_t credits_ref          = { I_INFO, "Credits",             info_credits};
static info_menu_item_t reboot_ref           = { I_INFO, "Reboot",              info_reboot};

static back_menu_item_t back_ref             = { I_BACK, "Return"};
static action_menu_item_t save_ref           = { I_SAVE, "Save Configuration"};
static action_menu_item_t restore_ref        = { I_RESTORE, "Restore Default Configuration"};
static action_menu_item_t cal_sampling_ref   = { I_CALIBRATE, "Auto Calibrate Video Sampling"};


static menu_t update_cpld_menu = {
   "Update CPLD Menu",
   rebuild_update_cpld_menu,
   {
      // Allow space for max 30 params
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

static child_menu_item_t update_cpld_menu_ref = { I_MENU, &update_cpld_menu};

static menu_t info_menu = {
   "Info Menu",
   NULL,
   {
      (base_menu_item_t *) &back_ref,
      (base_menu_item_t *) &source_summary_ref,
      (base_menu_item_t *) &system_summary_ref,
      (base_menu_item_t *) &cal_summary_ref,
      (base_menu_item_t *) &cal_detail_ref,
      (base_menu_item_t *) &cal_raw_ref,
      (base_menu_item_t *) &credits_ref,
      (base_menu_item_t *) &reboot_ref,
      (base_menu_item_t *) &update_cpld_menu_ref,
      NULL
   }
};

static param_menu_item_t profile_ref         = { I_FEATURE, &features[F_PROFILE]        };
static param_menu_item_t subprofile_ref      = { I_FEATURE, &features[F_SUBPROFILE]     };
static param_menu_item_t resolution_ref      = { I_FEATURE, &features[F_RESOLUTION]     };
static param_menu_item_t scaling_ref         = { I_FEATURE, &features[F_SCALING]        };
static param_menu_item_t frontend_ref        = { I_FEATURE, &features[F_FRONTEND]       };
static param_menu_item_t overscan_ref        = { I_FEATURE, &features[F_OVERSCAN]       };
static param_menu_item_t capscale_ref        = { I_FEATURE, &features[F_CAPSCALE]       };
static param_menu_item_t border_ref          = { I_FEATURE, &features[F_BORDER]         };
static param_menu_item_t palettecontrol_ref  = { I_FEATURE, &features[F_PALETTECONTROL] };
static param_menu_item_t ntsccolour_ref      = { I_FEATURE, &features[F_NTSCCOLOUR]     };
static param_menu_item_t ntscphase_ref       = { I_FEATURE, &features[F_NTSCPHASE]      };
static param_menu_item_t tint_ref            = { I_FEATURE, &features[F_TINT]           };
static param_menu_item_t sat_ref             = { I_FEATURE, &features[F_SAT]            };
static param_menu_item_t cont_ref            = { I_FEATURE, &features[F_CONT]           };
static param_menu_item_t bright_ref          = { I_FEATURE, &features[F_BRIGHT]         };
static param_menu_item_t gamma_ref           = { I_FEATURE, &features[F_GAMMA]          };
static param_menu_item_t palette_ref         = { I_FEATURE, &features[F_PALETTE]        };
static param_menu_item_t m7deinterlace_ref   = { I_FEATURE, &features[F_M7DEINTERLACE]  };
static param_menu_item_t deinterlace_ref     = { I_FEATURE, &features[F_DEINTERLACE]    };
static param_menu_item_t m7scaling_ref       = { I_FEATURE, &features[F_M7SCALING]      };
static param_menu_item_t normalscaling_ref   = { I_FEATURE, &features[F_NORMALSCALING]  };
static param_menu_item_t scanlines_ref       = { I_FEATURE, &features[F_SCANLINES]      };
static param_menu_item_t scanlinesint_ref    = { I_FEATURE, &features[F_SCANLINESINT]   };
static param_menu_item_t colour_ref          = { I_FEATURE, &features[F_COLOUR]         };
static param_menu_item_t invert_ref          = { I_FEATURE, &features[F_INVERT]         };
static param_menu_item_t fontsize_ref        = { I_FEATURE, &features[F_FONTSIZE]       };
static param_menu_item_t vsync_ref           = { I_FEATURE, &features[F_VSYNC]          };
static param_menu_item_t vlockmode_ref       = { I_FEATURE, &features[F_VLOCKMODE]      };
static param_menu_item_t vlockline_ref       = { I_FEATURE, &features[F_VLOCKLINE]      };
static param_menu_item_t vlockspeed_ref      = { I_FEATURE, &features[F_VLOCKSPEED]     };
static param_menu_item_t vlockadj_ref        = { I_FEATURE, &features[F_VLOCKADJ]       };
#ifdef MULTI_BUFFER
static param_menu_item_t nbuffers_ref        = { I_FEATURE, &features[F_NBUFFERS]       };
#endif
static param_menu_item_t autoswitch_ref      = { I_FEATURE, &features[F_AUTOSWITCH]     };
static param_menu_item_t return_ref          = { I_FEATURE, &features[F_RETURN]         };
static param_menu_item_t debug_ref           = { I_FEATURE, &features[F_DEBUG]          };

static menu_t palette_menu = {
   "Palette Menu",
   NULL,
   {
      (base_menu_item_t *) &back_ref,
      (base_menu_item_t *) &palette_ref,
      (base_menu_item_t *) &colour_ref,
      (base_menu_item_t *) &invert_ref,
      (base_menu_item_t *) &border_ref,
      (base_menu_item_t *) &bright_ref,
      (base_menu_item_t *) &cont_ref,
      (base_menu_item_t *) &gamma_ref,
      (base_menu_item_t *) &sat_ref,
      (base_menu_item_t *) &tint_ref,
      (base_menu_item_t *) &palettecontrol_ref,
      (base_menu_item_t *) &ntsccolour_ref,
      (base_menu_item_t *) &ntscphase_ref,
      NULL
   }
};
static menu_t preferences_menu = {
   "Preferences Menu",
   NULL,
   {
      (base_menu_item_t *) &back_ref,
      (base_menu_item_t *) &scanlines_ref,
      (base_menu_item_t *) &scanlinesint_ref,
      (base_menu_item_t *) &m7deinterlace_ref,
      (base_menu_item_t *) &deinterlace_ref,
      (base_menu_item_t *) &m7scaling_ref,
      (base_menu_item_t *) &normalscaling_ref,
      (base_menu_item_t *) &overscan_ref,
      (base_menu_item_t *) &capscale_ref,
      NULL
   }
};

static menu_t settings_menu = {
   "Settings Menu",
   NULL,
   {
      (base_menu_item_t *) &back_ref,
      (base_menu_item_t *) &fontsize_ref,
      (base_menu_item_t *) &vsync_ref,
      (base_menu_item_t *) &vlockmode_ref,
      (base_menu_item_t *) &vlockline_ref,
      (base_menu_item_t *) &vlockspeed_ref,
      (base_menu_item_t *) &vlockadj_ref,
      (base_menu_item_t *) &nbuffers_ref,
      (base_menu_item_t *) &return_ref,
      (base_menu_item_t *) &debug_ref,
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
   rebuild_geometry_menu,
   {
      // Allow space for max 30 params
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
   rebuild_sampling_menu,
   {
      // Allow space for max 30 params
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

static child_menu_item_t info_menu_ref        = { I_MENU, &info_menu        };
static child_menu_item_t palette_menu_ref     = { I_MENU, &palette_menu     };
static child_menu_item_t preferences_menu_ref = { I_MENU, &preferences_menu };
static child_menu_item_t settings_menu_ref    = { I_MENU, &settings_menu    };
static child_menu_item_t geometry_menu_ref    = { I_MENU, &geometry_menu    };
static child_menu_item_t sampling_menu_ref    = { I_MENU, &sampling_menu    };

static menu_t main_menu = {
   "Main Menu",
   NULL,
   {
      (base_menu_item_t *) &back_ref,
      (base_menu_item_t *) &info_menu_ref,
      (base_menu_item_t *) &palette_menu_ref,
      (base_menu_item_t *) &preferences_menu_ref,
      (base_menu_item_t *) &settings_menu_ref,
      (base_menu_item_t *) &geometry_menu_ref,
      (base_menu_item_t *) &sampling_menu_ref,
      (base_menu_item_t *) &save_ref,
      (base_menu_item_t *) &restore_ref,
      (base_menu_item_t *) &cal_sampling_ref,
      (base_menu_item_t *) &resolution_ref,
      (base_menu_item_t *) &scaling_ref,
      (base_menu_item_t *) &frontend_ref,
      (base_menu_item_t *) &profile_ref,
      (base_menu_item_t *) &autoswitch_ref,
      (base_menu_item_t *) &subprofile_ref,
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

// Mapping table for expanding 12-bit row to 24 bit pixel (12 words) with 16 bits/pixel
static uint32_t double_size_map_16bpp[0x1000 * 12];

// Mapping table for expanding 12-bit row to 12 bit pixel (6 words) with 16 bits/pixel
static uint32_t normal_size_map_16bpp[0x1000 * 6];



// Mapping table for expanding 8-bit row to 16 bit pixel (2 words) with 4 bits/pixel
static uint32_t double_size_map8_4bpp[0x1000 * 2];

// Mapping table for expanding 8-bit row to 8 bit pixel (1 word) with 4 bits/pixel
static uint32_t normal_size_map8_4bpp[0x1000 * 1];

// Mapping table for expanding 8-bit row to 16 bit pixel (4 words) with 8 bits/pixel
static uint32_t double_size_map8_8bpp[0x1000 * 4];

// Mapping table for expanding 8-bit row to 8 bit pixel (2 words) with 8 bits/pixel
static uint32_t normal_size_map8_8bpp[0x1000 * 2];

// Mapping table for expanding 8-bit row to 16 bit pixel (8 words) with 16 bits/pixel
static uint32_t double_size_map8_16bpp[0x1000 * 8];

// Mapping table for expanding 8-bit row to 8 bit pixel (4 words) with 16 bits/pixel
static uint32_t normal_size_map8_16bpp[0x1000 * 4];

// Temporary buffer for assembling OSD lines
static char message[80];

// Temporary filename for assembling OSD lines
static char filename[80];

// Is the OSD currently active
static int active = 0;

// Main state of the OSD
osd_state_t osd_state = IDLE;

// Current menu depth
static int depth = 0;

// Currently active menu
static menu_t *current_menu[MAX_MENU_DEPTH];

// Index to the currently selected menu
static int current_item[MAX_MENU_DEPTH];

// Currently selected palette setting
static int palette   = PALETTE_RGB;

//osd high water mark (highest line number used)
static int osd_hwm = 0;

static int all_frontends[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// Default action map, maps from physical key press to action
static osd_state_t action_map[] = {
   A0_LAUNCH,    //   0 - SW1 short press
   A1_CAPTURE,   //   1 - SW2 short press
   A2_CLOCK_CAL, //   2 - SW3 short press
   A4_SCANLINES, //   3 - SW1 long press
   A5_NTSCCOLOUR,//   4 - SW2 long press
   A3_AUTO_CAL,  //   5 - SW3 long press
};

#define NUM_ACTIONS (sizeof(action_map) / sizeof(osd_state_t))

// Default keymap, used for menu navigation
static int key_enter     = OSD_SW1;
static int key_menu_up   = OSD_SW3;
static int key_menu_down = OSD_SW2;
static int key_value_dec = OSD_SW2;
static int key_value_inc = OSD_SW3;

// Whether the menu back pointer is at the start (0) or end (1) of the menu
static int return_at_end = 0;
static char config_buffer[MAX_CONFIG_BUFFER_SIZE];
static char save_buffer[MAX_BUFFER_SIZE];
static char default_buffer[MAX_BUFFER_SIZE];
static char main_buffer[MAX_BUFFER_SIZE];
static char sub_default_buffer[MAX_BUFFER_SIZE];
static char sub_profile_buffers[MAX_SUB_PROFILES][MAX_BUFFER_SIZE];
static int has_sub_profiles[MAX_PROFILES];
static char profile_names[MAX_PROFILES][MAX_PROFILE_WIDTH];
static char sub_profile_names[MAX_SUB_PROFILES][MAX_PROFILE_WIDTH];
static char resolution_names[MAX_NAMES][MAX_NAMES_WIDTH];

static uint32_t palette_data[256];
static unsigned char equivalence[256];

static char palette_names[MAX_NAMES][MAX_NAMES_WIDTH];
static uint32_t palette_array[MAX_NAMES][256];

static double tint = 0;
static double saturation = 100;
static double contrast = 100;
static double brightness = 100;
static double Pgamma = 100;
static int inhibit_palette_dimming = 0;

typedef struct {
   int clock;
   int line_len;
   int clock_ppm;
   int lines_per_frame;
   int sync_type;
   int lower_limit;
   int upper_limit;
} autoswitch_info_t;

static autoswitch_info_t autoswitch_info[MAX_SUB_PROFILES];

static char cpld_firmware_dir[256] = DEFAULT_CPLD_FIRMWARE_DIR;

// =============================================================
// Private Methods
// =============================================================

static void cycle_menu(menu_t *menu) {
   // count the number of items in the menu, excluding the back reference
   int nitems = 0;
   base_menu_item_t **mp = menu->items;
   while (*mp++) {
      nitems++;
   }
   nitems--;
   base_menu_item_t *first = *(menu->items);
   base_menu_item_t *last = *(menu->items + nitems);
   base_menu_item_t *back = (base_menu_item_t *)&back_ref;
   if (return_at_end) {
      if (first == back) {
         for (int i = 0; i < nitems; i++) {
            *(menu->items + i) = *(menu->items + i + 1);
         }
         *(menu->items + nitems) = back;
      }
   } else {
      if (last == back) {
         for (int i = nitems; i > 0; i--) {
            *(menu->items + i) = *(menu->items + i - 1);
         }
         *(menu->items) = back;
      }
   }
}

static void cycle_menus() {
   cycle_menu(&main_menu);
   cycle_menu(&info_menu);
   cycle_menu(&preferences_menu);
   cycle_menu(&settings_menu);
}

static int get_feature(int num) {
   switch (num) {
   case F_PROFILE:
      return get_profile();
   case F_SUBPROFILE:
      return get_subprofile();
   case F_RESOLUTION:
      return get_resolution();
   case F_SCALING:
      return get_scaling();
   case F_FRONTEND:
      return get_frontend();
   case F_OVERSCAN:
      return get_overscan();
   case F_CAPSCALE:
      return get_capscale();
   case F_BORDER:
      return get_border();
   case F_FONTSIZE:
      return get_fontsize();
   case F_M7DEINTERLACE:
      return get_m7deinterlace();
   case F_M7SCALING:
      return get_m7scaling();
   case F_DEINTERLACE:
      return get_deinterlace();
   case F_NORMALSCALING:
      return get_normalscaling();
   case F_PALETTE:
      return palette;
   case F_PALETTECONTROL:
      return get_paletteControl();
   case F_NTSCCOLOUR:
      return get_ntsccolour();
   case F_NTSCPHASE:
      return get_ntscphase();
   case F_TINT:
      return tint;
   case F_SAT:
      return saturation;
   case F_CONT:
      return contrast;
   case F_BRIGHT:
      return brightness;
   case F_GAMMA:
      return Pgamma;
   case F_SCANLINES:
      return get_scanlines();
   case F_SCANLINESINT:
      return get_scanlines_intensity();
   case F_COLOUR:
      return get_colour();
   case F_INVERT:
      return get_invert();
   case F_VSYNC:
      return get_vsync();
   case F_VLOCKMODE:
      return get_vlockmode();
   case F_VLOCKLINE:
      return get_vlockline();
   case F_VLOCKSPEED:
      return get_vlockspeed();
   case F_VLOCKADJ:
      return get_vlockadj();
#ifdef MULTI_BUFFER
   case F_NBUFFERS:
      return get_nbuffers();
#endif
   case F_AUTOSWITCH:
      return get_autoswitch();
   case F_RETURN:
      return return_at_end;
   case F_DEBUG:
      return get_debug();
   }
   return -1;
}

static void set_feature(int num, int value) {
   if (value < features[num].min) {
      value = features[num].min;
   }
   if (value > features[num].max) {
      value = features[num].max;
   }
   switch (num) {
   case F_PROFILE:
      set_profile(value);
      load_profiles(value, 1);
      process_profile(value);
      set_feature(F_SUBPROFILE, 0);
      break;
   case F_SUBPROFILE:
      set_subprofile(value);
      process_sub_profile(get_profile(), value);
      break;
   case F_RESOLUTION:
      set_resolution(value, resolution_names[value], 1);
      break;
   case F_SCALING:
      set_scaling(value, 1);
      break;
   case F_FRONTEND:
      set_frontend(value, 1);
      break;
   case F_M7DEINTERLACE:
      set_m7deinterlace(value);
      break;
   case F_M7SCALING:
      set_m7scaling(value);
      break;
   case F_DEINTERLACE:
      set_deinterlace(value);
      break;
   case F_NORMALSCALING:
      set_normalscaling(value);
      break;
   case F_OVERSCAN:
      set_overscan(value);
      break;
   case F_CAPSCALE:
      set_capscale(value);
      break;
   case F_BORDER:
      set_border(value);
      break;
   case F_FONTSIZE:
      if(active) {
         osd_clear();
         set_fontsize(value);
         osd_refresh();
      } else {
         set_fontsize(value);
      }
      break;
   case F_PALETTE:
      palette = value;
      osd_update_palette();
      break;
   case F_PALETTECONTROL:
      set_paletteControl(value);
      int hidden = (value < PALETTECONTROL_NTSCARTIFACT_CGA);
      features[F_NTSCCOLOUR].hidden = hidden;
      features[F_NTSCPHASE].hidden = hidden;
      osd_update_palette();
      break;
   case F_NTSCCOLOUR:
      set_ntsccolour(value);
      osd_update_palette();
      break;
   case F_NTSCPHASE:
      set_ntscphase(value);
      break;

   case F_TINT:
      tint = value;
      osd_update_palette();
      break;
   case F_SAT:
      saturation = value;
      osd_update_palette();
      break;
   case F_CONT:
      contrast = value;
      osd_update_palette();
      break;
   case F_BRIGHT:
      brightness = value;
      osd_update_palette();
      break;
   case F_GAMMA:
      Pgamma = value;
      osd_update_palette();
      break;

   case F_SCANLINES:
      set_scanlines(value);
      break;
   case F_SCANLINESINT:
      set_scanlines_intensity(value);
      break;
   case F_COLOUR:
      set_colour(value);
      osd_update_palette();
      break;
   case F_INVERT:
      set_invert(value);
      osd_update_palette();
      break;
   case F_VSYNC:
      set_vsync(value);
      break;
   case F_VLOCKMODE:
      set_vlockmode(value);
      break;
   case F_VLOCKLINE:
      set_vlockline(value);
      break;
   case F_VLOCKSPEED:
      set_vlockspeed(value);
      break;
   case F_VLOCKADJ:
      set_vlockadj(value);
      break;
#ifdef MULTI_BUFFER
   case F_NBUFFERS:
      set_nbuffers(value);
      break;
#endif
   case F_RETURN:
      return_at_end = value;
      cycle_menus();
      break;
   case F_DEBUG:
      set_debug(value);
      osd_update_palette();
      break;
   case F_AUTOSWITCH:
      set_autoswitch(value);
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
   case I_UPDATE:
   case I_PARAM:
      return ((param_menu_item_t *)item)->param->label;
   case I_INFO:
      return ((info_menu_item_t *)item)->name;
   case I_BACK:
      return ((back_menu_item_t *)item)->name;
   default:
      // Otherwise, default to a single action menu item type
      return ((action_menu_item_t *)item)->name;
   }
}

// Test if a parameter is a simple boolean
static int is_boolean_param(param_menu_item_t *param_item) {
   return param_item->param->min == 0 && param_item->param->max == 1;
}

// Test if a paramater is toggleable (i.e. just two valid values)
static int is_toggleable_param(param_menu_item_t *param_item) {
   return param_item->param->min + param_item->param->step == param_item->param->max;
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

// Toggle a parameter that only has two legal values (it's min and max)
static void toggle_param(param_menu_item_t *param_item) {
   if (get_param(param_item) == param_item->param->max) {
      set_param(param_item, param_item->param->min);
   } else {
      set_param(param_item, param_item->param->max);
   }
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
      case F_PROFILE:
         return profile_names[value];
      case F_SUBPROFILE:
         return sub_profile_names[value];
      case F_RESOLUTION:
         return resolution_names[value];
      case F_SCALING:
         return scaling_names[value];
      case F_FRONTEND:
         if (eight_bit_detected()) {
            return frontend_names_8[value];
         } else {
            return frontend_names_6[value];
         }
      case F_OVERSCAN:
         return overscan_names[value];
      case F_COLOUR:
         return colour_names[value];
      case F_INVERT:
         return invert_names[value];
      case F_FONTSIZE:
         return fontsize_names[value];
      case F_PALETTE:
         return palette_names[value];
      case F_PALETTECONTROL:
         return palette_control_names[value];
      case F_AUTOSWITCH:
         return autoswitch_names[value];
      case F_M7DEINTERLACE:
      case F_DEINTERLACE:
         return deinterlace_names[value];
      case F_M7SCALING:
         return even_scaling_names[value];
      case F_NORMALSCALING:
         return even_scaling_names[value];
      case F_CAPSCALE:
         return screencap_names[value];
      case F_VLOCKMODE:
         return vlockmode_names[value];
      case F_VLOCKSPEED:
         return vlockspeed_names[value];
      case F_VLOCKADJ:
         return vlockadj_names[value];
#ifdef MULTI_BUFFER
      case F_NBUFFERS:
         return nbuffer_names[value];
#endif
      case F_RETURN:
         return return_names[value];
      }
   } else if (type == I_GEOMETRY) {
      const char *value_str = geometry_get_value_string(param->key);
      if (value_str) {
         return value_str;
      }
   } else {
      const char *value_str = cpld->get_value_string(param->key);
      if (value_str) {
         return value_str;
      }
   }
   if (is_boolean_param(param_item)) {
      return value ? "On" : "Off";
   }
   sprintf(number, "%d", value);
   return number;
}

static volatile uint32_t *gpioreg = (volatile uint32_t *)(PERIPHERAL_BASE + 0x101000UL);

static void info_system_summary(int line) {
   sprintf(message, "RGBtoHDMI Version: %s", GITVERSION);
   osd_set(line++, 0, message);
   sprintf(message, "     CPLD Version: %s v%x.%x",
           cpld->name,
           (cpld->get_version() >> VERSION_MAJOR_BIT) & 0xF,
           (cpld->get_version() >> VERSION_MINOR_BIT) & 0xF);
   osd_set(line++, 0, message);
   int ANA1_PREDIV = (gpioreg[PLLA_ANA1] >> 14) & 1;
   int NDIV = (gpioreg[PLLA_CTRL] & 0x3ff) << ANA1_PREDIV;
   int FRAC = gpioreg[PLLA_FRAC] << ANA1_PREDIV;
   int clockA = (double) (CRYSTAL * ((double)NDIV + ((double)FRAC) / ((double)(1 << 20))) + 0.5);
   ANA1_PREDIV = (gpioreg[PLLB_ANA1] >> 14) & 1;
   NDIV = (gpioreg[PLLB_CTRL] & 0x3ff) << ANA1_PREDIV;
   FRAC = gpioreg[PLLB_FRAC] << ANA1_PREDIV;
   int clockB = (double) (CRYSTAL * ((double)NDIV + ((double)FRAC) / ((double)(1 << 20))) + 0.5);
   ANA1_PREDIV = (gpioreg[PLLC_ANA1] >> 14) & 1;
   NDIV = (gpioreg[PLLC_CTRL] & 0x3ff) << ANA1_PREDIV;
   FRAC = gpioreg[PLLC_FRAC] << ANA1_PREDIV;
   int clockC = (double) (CRYSTAL * ((double)NDIV + ((double)FRAC) / ((double)(1 << 20))) + 0.5);
   ANA1_PREDIV = (gpioreg[PLLD_ANA1] >> 14) & 1;
   NDIV = (gpioreg[PLLD_CTRL] & 0x3ff) << ANA1_PREDIV;
   FRAC = gpioreg[PLLD_FRAC] << ANA1_PREDIV;
   int clockD = (double) (CRYSTAL * ((double)NDIV + ((double)FRAC) / ((double)(1 << 20))) + 0.5);
   sprintf(message, "             PLLA: %4d Mhz", clockA);
   osd_set(line++, 0, message);
   sprintf(message, "             PLLB: %4d Mhz", clockB);
   osd_set(line++, 0, message);
   sprintf(message, "             PLLC: %4d Mhz", clockC);
   osd_set(line++, 0, message);
   sprintf(message, "             PLLD: %4d Mhz", clockD);
   osd_set(line++, 0, message);
   sprintf(message, "        CPU Clock: %4d Mhz", get_clock_rate(ARM_CLK_ID)/1000000);
   osd_set(line++, 0, message);
   sprintf(message, "       CORE Clock: %4d Mhz", get_clock_rate(CORE_CLK_ID)/1000000);
   osd_set(line++, 0, message);
   sprintf(message, "      SDRAM Clock: %4d Mhz", get_clock_rate(SDRAM_CLK_ID)/1000000);
   osd_set(line++, 0, message);
   sprintf(message, "        Core Temp: %6.2f C", get_temp());
   osd_set(line++, 0, message);
   sprintf(message, "     Core Voltage: %6.2f V", get_voltage(COMPONENT_CORE));
   osd_set(line++, 0, message);
   sprintf(message, "  SDRAM_C Voltage: %6.2f V", get_voltage(COMPONENT_SDRAM_C));
   osd_set(line++, 0, message);
   sprintf(message, "  SDRAM_P Voltage: %6.2f V", get_voltage(COMPONENT_SDRAM_P));
   osd_set(line++, 0, message);
   sprintf(message, "  SDRAM_I Voltage: %6.2f V", get_voltage(COMPONENT_SDRAM_I));
   osd_set(line++, 0, message);
}

static void info_credits(int line) {
   osd_set(line++, 0, "Many thanks to our main developers:");
   osd_set(line++, 0, "- David Banks (hoglet)");
   osd_set(line++, 0, "- Ian Bradbury (IanB)");
   osd_set(line++, 0, "- Dominic Plunkett (dp11)");
   osd_set(line++, 0, "- Ed Spittles (BigEd)");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "Thanks also to the members of stardot");
   osd_set(line++, 0, "who have provided encouragement, ideas");
   osd_set(line++, 0, "and helped with beta testing.");
}
static void info_reboot(int line) {
   reboot();
}

static void info_source_summary(int line) {
   line = show_detected_status(line);
}

static void info_cal_summary(int line) {
   if (cpld->show_cal_summary) {
      line = cpld->show_cal_summary(line);
   } else {
      sprintf(message, "show_cal_summary() not implemented");
      osd_set(line++, 0, message);
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
      if (!param_ptr->hidden) {
         dynamic_item[i].type = type;
         dynamic_item[i].param = param_ptr;
         menu->items[i] = (base_menu_item_t *)&dynamic_item[i];
         i++;
      }
      param_ptr++;
   }
   if (return_at_end) {
      menu->items[i++] = (base_menu_item_t *)&back_ref;
   }
   // Add a terminator, in case the menu has had items removed
   menu->items[i++] = NULL;
}

static void rebuild_geometry_menu(menu_t *menu) {
   rebuild_menu(menu, I_GEOMETRY, geometry_get_params());
}

static void rebuild_sampling_menu(menu_t *menu) {
   rebuild_menu(menu, I_PARAM, cpld->get_params());
}

static char cpld_filenames[MAX_CPLD_FILENAMES][MAX_FILENAME_WIDTH];

static param_t cpld_filename_params[MAX_CPLD_FILENAMES];

static void rebuild_update_cpld_menu(menu_t *menu) {
   int i;
   int count;
   char cpld_dir[256];
   strncpy(cpld_dir, cpld_firmware_dir, 256);
   if (get_debug()) {
       strncat(cpld_dir, "/old", 256);
   }
   scan_cpld_filenames(cpld_filenames, cpld_dir, &count);
   for (i = 0; i < count; i++) {
      cpld_filename_params[i].key = i;
      cpld_filename_params[i].label = cpld_filenames[i];
   }
   cpld_filename_params[i].key = -1;
   rebuild_menu(menu, I_UPDATE, cpld_filename_params);
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
         osd_set_noupdate(line, ATTR_DOUBLE_SIZE, info_item->name);
         line += 2;
         info_item->show_info(line);
      }
   } else if (osd_state == MENU || osd_state == PARAM) {
      osd_set_noupdate(line, ATTR_DOUBLE_SIZE, menu->name);
      line += 2;
      // Work out the longest item name
      int max = 0;
      item_ptr = menu->items;
      while ((item = *item_ptr++)) {
         int len = strlen(item_name(item));
         if (((item)->type == I_FEATURE || (item)->type == I_GEOMETRY || (item)->type == I_PARAM || (item)->type == I_UPDATE) && (len > max)){
            max = len;
         }
      }
      item_ptr = menu->items;
      int i = 0;
      while ((item = *item_ptr++)) {
         char *mp         = message;
         char sel_none    = ' ';
         char sel_open    = (i == current) ? '>' : sel_none;
         char sel_close   = (i == current) ? '<' : sel_none;
         const char *name = item_name(item);
         *mp++ = (osd_state != PARAM) ? sel_open : sel_none;
         strcpy(mp, name);
         mp += strlen(mp);
         if ((item)->type == I_FEATURE || (item)->type == I_GEOMETRY || (item)->type == I_PARAM || (item)->type == I_PARAM) {
            int len = strlen(name);
            while (len < max) {
               *mp++ = ' ';
               len++;
            }
            *mp++ = ' ';
            *mp++ = '=';
            *mp++ = (osd_state == PARAM) ? sel_open : sel_none;
            strcpy(mp, get_param_string((param_menu_item_t *)item));
            int param_len = strlen(mp);
            for (int j=0; j < param_len; j++) {
               if (*mp == '_') {
                  *mp = ' ';
               }
               mp++;
            }
         }
         *mp++ = sel_close;
         *mp++ = '\0';
         osd_set_noupdate(line++, 0, message);
         i++;
      }
   }
   if (capinfo->bpp != 16) {
       osd_update((uint32_t *) (capinfo->fb + capinfo->pitch * capinfo->height * get_current_display_buffer() + capinfo->pitch * capinfo->v_adjust + capinfo->h_adjust), capinfo->pitch);
   }
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

static void set_key_down_duration(int key, int value) {
   switch (key) {
   case OSD_SW1:
      sw1counter = value;
      break;
   case OSD_SW2:
      sw2counter = value;
      break;
   case OSD_SW3:
      sw3counter = value;
      break;
   }
}

void set_auto_name(char* name) {
    scaling_names[0] = name;
}

void yuv2rgb(int maxdesat, int mindesat, int luma_scale, int black_ref, int y1_millivolts, int u1_millivolts, int v1_millivolts, int *r, int *g, int *b, int *m) {

   int desat = maxdesat;
   if (y1_millivolts >= 720) {
       desat = mindesat;
   }
   *m = 255 * (black_ref - y1_millivolts) / (black_ref - 420);
   for(int chroma_scale = 100; chroma_scale > desat; chroma_scale--) {
      int y = (luma_scale * 255 * (black_ref - y1_millivolts) / (black_ref - 420));
      int u = (chroma_scale * ((u1_millivolts - 2000) / 500) * 127);
      int v = (chroma_scale * ((v1_millivolts - 2000) / 500) * 127);

      int r1 = (((10000 * y) - ( 0001 * u) + (11398 * v)) / 1000000);
      int g1 = (((10000 * y) - ( 3946 * u) - ( 5805 * v)) / 1000000);
      int b1 = (((10000 * y) + (20320 * u) - ( 0005 * v)) / 1000000);


      *r = r1 < 1 ? 1 : r1;
      *r = r1 > 254 ? 254 : *r;
      *g = g1 < 1 ? 1 : g1;
      *g = g1 > 254 ? 254 : *g;
      *b = b1 < 1 ? 1 : b1;
      *b = b1 > 254 ? 254 : *b;

      if (*r == r1 && *g == g1 && *b == b1) {
         break;
      }
   }

   //int new_y = ((299* *r + 587* *g + 114* *b) );
   //new_y = new_y > 255000 ? 255000 : new_y;
   //if (colour == 0) {
   //    log_info("");
   //}
   //log_info("Col=%2x,  R=%4d,G=%4d,B=%4d, Y=%3d Y=%6f (%3d/256 sat)",colour,*r,*g,*b, (int) (new_y + 500)/1000, (double) new_y/1000, chroma_scale);

}


// =============================================================
// Public Methods
// =============================================================



uint32_t osd_get_equivalence(uint32_t value) {
   if (capinfo->bpp == 8) {
        return equivalence[value & 0xff] | (equivalence[(value >> 8) & 0xff] << 8) | (equivalence[(value >> 16) & 0xff] << 16) | (equivalence[value >> 24] << 24);
   } else {
        return equivalence[value & 0xf] | (equivalence[(value >> 4) & 0xf] << 4) | (equivalence[(value >> 8) & 0xf] << 8) | (equivalence[(value >> 12) & 0xf] << 12)
        | (equivalence[(value >> 16) & 0xf] << 16) | (equivalence[(value >> 20) & 0xf] << 20) | (equivalence[(value >> 24) & 0xf] << 24) | (equivalence[(value >> 28) & 0xf] << 28);
   }
}

uint32_t osd_get_palette(int index) {
   return palette_data[index];
}

double gamma_correct(double value, double normalised_gamma) {
    value = value < 0 ? 0 : value;
    value = pow(value, normalised_gamma) * 255;
    value = round(value);
    value = value < 0 ? 0 : value;
    value = value > 255 ? 255 : value;
    return value;
}

int adjust_palette(int palette) {
    if (tint !=0 || saturation != 100 || contrast != 100 || brightness != 100 || Pgamma != 100) {
        double R = (double)(palette & 0xff) / 255;
        double G = (double)((palette >> 8) & 0xff) / 255;
        double B = (double)((palette >> 16) & 0xff) / 255;
        double M = (double)((palette >> 24) & 0xff) / 255;

        double normalised_contrast = (double)contrast / 100;
        double normalised_brightness = (double)brightness / 200 - 0.5f;
        double normalised_saturation = (double)saturation / 100;
        double normalised_gamma = 1 / ((double)Pgamma / 100);

        double Y = 0.299 * R + 0.587 * G + 0.114 * B;
        double U = -0.14713 * R - 0.28886 * G + 0.436 * B;
        double V = 0.615 * R - 0.51499 * G - 0.10001 * B;

        Y = (Y + normalised_brightness) * normalised_contrast;
        double hue = tint * PI / 180.0f;
        double U2 = (U * cos(hue) + V * sin(hue)) * normalised_saturation * normalised_contrast;
        double V2 = (V * cos(hue) - U * sin(hue)) * normalised_saturation * normalised_contrast;

        M = (M + normalised_brightness) * normalised_contrast;

        R = (Y + 1.140 * V2);
        G = (Y - 0.396 * U2 - 0.581 * V2);
        B = (Y + 2.029 * U2);

        R = gamma_correct(R, normalised_gamma);
        G = gamma_correct(G, normalised_gamma);
        B = gamma_correct(B, normalised_gamma);
        M = gamma_correct(M, normalised_gamma);

        return (int)R | ((int)G << 8) | ((int)B << 16) | ((int)M << 24);
    } else {
        return (palette);
    }
}

int colodore_gamma_correct(double value) {
    static double source = 2.8;
    static double target = 2.2;
    value = value < 0 ? 0 : value;
    value = value > 255 ? 255 : value;
    value = pow(255, 1 - source ) * pow(value, source);
    value = pow(255, 1 - 1 / target) * pow(value, 1 / target);
    return (int) round(value);
}

 void create_colodore_colours(int index, int revision, double brightness, double contrast, double saturation, int *red, int *green, int *blue, int *mono) {
    static int mc[] = { 0, 32, 10, 20, 12, 16, 8, 24, 12, 8, 16, 10, 15, 24, 15, 20};
    static int fr[] = { 0, 32, 8, 24, 16, 16, 8, 24, 16, 8, 16, 8, 16, 24, 16, 24};
    static int angles[] = { 0, 0, 4, 12, 2, 10, 15, 7, 5, 6, 4, 0, 0, 10, 15, 0 };

    double sector = 360 / 16;
    double origin = sector / 2;
    double radian = M_PI / 180;
    double screen = 1 / 5;

    brightness -=50;
    contrast /=100;
    saturation *= 1 - screen;

    double y = 0;
    double u = 0;
    double v = 0;

    if (angles[index]) {
        double angle = (origin + angles[index] * sector ) * radian;
        u = cos(angle) * saturation;
        v = sin(angle) * saturation;
    }
    if (revision) {
        y = 8 * mc[ index ] + brightness;
    } else {
        y = 8 * fr[ index ] + brightness;
    }

    y = y * contrast + screen;
    u = u * contrast + screen;
    v = v * contrast + screen;

    double r = y + 1.140 * v;
    double g = y - 0.396 * u - 0.581 * v;
    double b = y + 2.029 * u;

    *red =  colodore_gamma_correct(r);
    *green = colodore_gamma_correct(g);
    *blue = colodore_gamma_correct(b);
    *mono = colodore_gamma_correct(y);
}

int create_NTSC_artifact_colours(int index, int filtered_bitcount) {
    int colour = index & 0x0f;
    int bitcount = 0;
    if (colour & 1) bitcount++;
    if (colour & 2) bitcount++;
    if (colour & 4) bitcount++;
    if (colour & 8) bitcount++;
    double Y=0;
    double U=0;
    double V=0;
        switch (colour) {
           case 0x00:
              Y=0     ; U=0     ; V=0     ; break; //Black
           case 0x08:
              Y=0.25  ; U=0     ; V=0.5   ; break; //Deep Red
           case 0x01:
              Y=0.25  ; U=0.5   ; V=0     ; break; //Dark Blue
           case 0x09:
              Y=0.5   ; U=1     ; V=1     ; break; //Purple
           case 0x02:
              Y=0.25  ; U=0     ; V=-0.5  ; break; //Dark Green
           case 0x0a:
              Y=0.5   ; U=0     ; V=0     ; break; //lower Gray
           case 0x03:
              Y=0.5   ; U=1     ; V=-1    ; break; //Medium Blue
           case 0x0b:
              Y=0.75  ; U=0.5   ; V=0     ; break; //Light Blue
           case 0x04:
              Y=0.25  ; U=-0.5  ; V=0     ; break; //Brown
           case 0x0c:
              Y=0.5   ; U=-1    ; V=1     ; break; //Orange
           case 0x05:
              Y=0.5   ; U=0     ; V=0     ; break; //upper Gray
           case 0x0d:
              Y=0.75  ; U=0     ; V=0.5   ; break; //Pink
           case 0x06:
              Y=0.5   ; U=-1    ; V=-1    ; break; //Light Green
           case 0x0e:
              Y=0.75  ; U=-0.5  ; V=0     ; break; //Yellow
           case 0x07:
              Y=0.75  ; U=0     ; V=-0.5  ; break; //Aquamarine
           case 0x0f:
              Y=1     ; U=0     ; V=0     ; break; //White
        }

        double R = (Y + 1.140 * V);
        double G = (Y - 0.395 * U - 0.581 * V);
        double B = (Y + 2.032 * U);

        if (filtered_bitcount == 1) {

            switch(bitcount) {
                case 1:
                    R = R * 100 / 100;
                    G = G * 100 / 100;
                    B = B * 100 / 100;
                    break;
                case 2:
                    R = R * 50 / 100;
                    G = G * 50 / 100;
                    B = B * 50 / 100;
                    break;
                case 3:
                    R = R * 33 / 100;
                    G = G * 33 / 100;
                    B = B * 33 / 100;
                    break;
                case 4:
                    R = R * 33 /100;
                    G = G * 33 /100;
                    B = B * 33 /100;
                    break;
                default:
                    break;
            }

        }
        if (filtered_bitcount == 2) {
            switch(bitcount) {
                case 1:
                    R = R * 125 / 100;
                    G = G * 125 / 100;
                    B = B * 125 / 100;
                    break;
                case 2:
                    R = R * 100 / 100;
                    G = G * 100 / 100;
                    B = B * 100 / 100;
                    break;
                case 3:
                    R = R * 66 / 100;
                    G = G * 66 / 100;
                    B = B * 66 / 100;
                    break;
                case 4:
                    R = R * 66 /100;
                    G = G * 66 /100;
                    B = B * 66 /100;
                    break;
                default:
                    break;
            }
        }

        if (filtered_bitcount == 3) {
            switch(bitcount) {
                case 1:
                    R = R * 150 / 100;
                    G = G * 150 / 100;
                    B = B * 150 / 100;
                    break;
                case 2:
                    R = R * 125 / 100;
                    G = G * 125 / 100;
                    B = B * 125 / 100;
                    break;
                case 3:
                    R = R * 100 / 100;
                    G = G * 100 / 100;
                    B = B * 100 / 100;
                    break;
                case 4:
                    R = R * 100 /100;
                    G = G * 100 /100;
                    B = B * 100 /100;
                    break;
                default:
                    break;
            }
        }

        R = gamma_correct(R, 1);
        G = gamma_correct(G, 1);
        B = gamma_correct(B, 1);
        Y = gamma_correct(Y, 1);

    return (int)R | ((int)G<<8) | ((int)B<<16) | ((int)Y<<24);
}

int create_NTSC_artifact_colours_palette_320(int index) {
    int colour = index & 0x0f;
    int R = 0;
    int G = 0;
    int B = 0;

    if (index < 0x40) {
        switch (colour) {
           case 0x00:
              R=0     ; G=0     ; B=0     ; break; //Black
           case 0x01:
              R=0     ; G=117   ; B=108   ; break;
           case 0x02:
              R=0     ; G=49    ; B=111   ; break;
           case 0x03:
              R=0     ; G=83    ; B=63    ; break;
           case 0x04:
              R=123   ; G=52    ; B=0     ; break;
           case 0x05:
              R=57    ; G=190   ; B=66    ; break;
           case 0x06:
              R=131   ; G=118   ; B=73    ; break;
           case 0x07:
              R=83    ; G=155   ; B=14    ; break;
           case 0x08:
              R=235   ; G=50    ; B=7     ; break;
           case 0x09:
              R=210   ; G=196   ; B=153   ; break;
           case 0x0a:
              R=248   ; G=122   ; B=155   ; break;
           case 0x0b:
              R=217   ; G=160   ; B=107   ; break;
           case 0x0c:
              R=180   ; G=69    ; B=0     ; break;
           case 0x0d:
              R=139   ; G=208   ; B=74    ; break;
           case 0x0e:
              R=190   ; G=133   ; B=80    ; break;
           case 0x0f:
              R=152   ; G=173   ; B=20    ; break;
        }
    } else {
        switch (colour) {
           case 0x00:
              R=0     ; G=0     ; B=0     ; break; //Black
           case 0x01:
              R=0     ; G=139   ; B=172   ; break;
           case 0x02:
              R=0     ; G=73    ; B=174   ; break;
           case 0x03:
              R=0     ; G=158   ; B=232   ; break;
           case 0x04:
              R=89    ; G=28    ; B=0     ; break;
           case 0x05:
              R=0     ; G=188   ; B=155   ; break;
           case 0x06:
              R=99    ; G=116   ; B=158   ; break;
           case 0x07:
              R=0     ; G=206   ; B=217   ; break;
           case 0x08:
              R=237   ; G=28    ; B=39    ; break;
           case 0x09:
              R=180   ; G=196   ; B=238   ; break;
           case 0x0a:
              R=221   ; G=125   ; B=239   ; break;
           case 0x0b:
              R=188   ; G=210   ; B=255   ; break;
           case 0x0c:
              R=255   ; G=73    ; B=0     ; break;
           case 0x0d:
              R=247   ; G=237   ; B=193   ; break;
           case 0x0e:
              R=255   ; G=165   ; B=196   ; break;
           case 0x0f:
              R=255   ; G=255   ; B=255   ; break;
        }
    }

    double Y = 0.299 * (double)R + 0.587 * (double)G + 0.114 * (double)B;
    Y = gamma_correct(Y, 1);

    return R | (G << 8) | (B << 16) | ((int)Y << 24);
}

void generate_palettes() {

#define bp  0x24    // b-y plus
#define bz  0x20    // b-y zero
#define bz2 0x04    // b-y zero2
#define bm  0x00    // b-y minus
#define rp  0x09    // r-y plus
#define rz  0x08    // r-y zero
#define rz2 0x01    // r-y zero2
#define rm  0x00    // r-y minus

    for(int palette = 0; palette < NUM_PALETTES; palette++) {
        for (int i = 0; i < 256; i++) {
            int r = 0;
            int g = 0;
            int b = 0;
            int m = -1;

            int luma = i & 0x12;
            int maxdesat = 99;
            int mindesat = 20;
            int luma_scale = 81;
            int black_ref = 770;

            switch (palette) {
                 case PALETTE_RGB:
                    r = (i & 1) ? 255 : 0;
                    g = (i & 2) ? 255 : 0;
                    b = (i & 4) ? 255 : 0;
                    break;

                 case PALETTE_RGBI:
                    r = (i & 1) ? 0xaa : 0x00;
                    g = (i & 2) ? 0xaa : 0x00;
                    b = (i & 4) ? 0xaa : 0x00;
                    if (i & 0x10) {                           // intensity is actually on lsb green pin on 9 way D
                       r += 0x55;
                       g += 0x55;
                       b += 0x55;
                    }
                    break;

                 case PALETTE_RGBICGA:
                    r = (i & 1) ? 0xaa : 0x00;
                    g = (i & 2) ? 0xaa : 0x00;
                    b = (i & 4) ? 0xaa : 0x00;
                    if (i & 0x10) {                           // intensity is actually on lsb green pin on 9 way D
                        r += 0x55;
                        g += 0x55;
                        b += 0x55;
                    } else {
                        if ((i & 0x07) == 3 ) {
                            g = 0x55;                          // exception for colour 6 which is brown instead of dark yellow
                        }
                    }
                    break;

                 case PALETTE_RGBISPECTRUM:
                    m = (i & 0x10) ? 0xff : 0xd7;
                    r = (i & 1) ? m : 0x00;
                    g = (i & 2) ? m : 0x00;
                    b = (i & 4) ? m : 0x00;
                    break;

                 case PALETTE_SPECTRUM:
                    switch (i & 0x09) {
                        case 0x00:
                            r = 0x00; break;
                        case 0x09:
                            r = 0xff; break;
                        default:
                            r = 0xd7; break;
                    }
                     switch (i & 0x12) {
                        case 0x00:
                            g = 0x00; break;
                        case 0x12:
                            g = 0xff; break;
                        default:
                            g = 0xd7; break;
                    }
                    switch (i & 0x24) {
                        case 0x00:
                            b = 0x00; break;
                        case 0x24:
                            b = 0xff; break;
                        default:
                            b = 0xd7; break;
                    }
                    break;

                 case PALETTE_AMSTRAD:
                    switch (i & 0x09) {
                        case 0x00:
                            r = 0x00; break;
                        case 0x09:
                            r = 0xff; break;
                        default:
                            r = 0x7f; break;
                    }
                     switch (i & 0x12) {
                        case 0x00:
                            g = 0x00; break;
                        case 0x12:
                            g = 0xff; break;
                        default:
                            g = 0x7f; break;
                    }
                    switch (i & 0x24) {
                        case 0x00:
                            b = 0x00; break;
                        case 0x24:
                            b = 0xff; break;
                        default:
                            b = 0x7f; break;
                    }
                    break;

                 case PALETTE_RrGgBb:
                    r = (i & 1) ? 0xaa : 0x00;
                    g = (i & 2) ? 0xaa : 0x00;
                    b = (i & 4) ? 0xaa : 0x00;
                    r = (i & 0x08) ? (r + 0x55) : r;
                    g = (i & 0x10) ? (g + 0x55) : g;
                    b = (i & 0x20) ? (b + 0x55) : b;
                    break;

                 case PALETTE_MDA:
                    r = (i & 0x20) ? 0xaa : 0x00;
                    r = (i & 0x10) ? (r + 0x55) : r;
                    g = r; b = r;
                    if (i & 1) {
                         r ^= 0xff;
                    }
                    break;

                 case PALETTE_ATOM_MKI: {
                    switch (i & 0x2d) {  //these five are luma independent
                        case (bz + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 650, 2000, 2500, &r, &g, &b, &m); break; // red
                        case (bp + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 650, 2500, 2000, &r, &g, &b, &m); break; // blue
                        case (bp + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 2500, 2500, &r, &g, &b, &m); break; // magenta
                        case (bz + rm):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 2000, 1500, &r, &g, &b, &m); break; // cyan
                        case (bm + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 1500, 2000, &r, &g, &b, &m); break; // yellow
                        case (bz + rz): {
                            switch (luma) {
                                case 0x00:
                                case 0x10: //alt
                                case 0x02: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, black_ref, 2000, 2000, &r, &g, &b, &m); break; // black
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 2000, 2000, &r, &g, &b, &m); break; // white (buff)
                            }
                        }
                        break;
                        case (bm + rm): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, black_ref, 2000, 2000, &r, &g, &b, &m); break; // dark green
                                case 0x10:
                                case 0x02:
                                case 0x12: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 1500, 1500, &r, &g, &b, &m); break; // green
                            }
                        }
                        break;
                        case (bm + rp): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, black_ref, 2000, 2000, &r, &g, &b, &m); break; // dark orange
                                case 0x10:
                                case 0x02:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 1500, 2500, &r, &g, &b, &m); break; // normal orange
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 1500, 2500, &r, &g, &b, &m); break; // bright orange
                            }
                        }
                        break;
                    }
                 }
                 break;

                 case PALETTE_ATOM_MKI_FULL: {
                    switch (i & 0x2d) {  //these five are luma independent
                        case (bz + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 650, 2000, 2500, &r, &g, &b, &m); break; // red
                        case (bp + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 650, 2500, 2000, &r, &g, &b, &m); break; // blue
                        case (bp + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 2500, 2500, &r, &g, &b, &m); break; // magenta
                        case (bz + rm):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 2000, 1500, &r, &g, &b, &m); break; // cyan
                        case (bm + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 1500, 2000, &r, &g, &b, &m); break; // yellow
                        case (bz + rz): {
                            switch (luma) {
                                case 0x00:
                                case 0x10: //alt
                                case 0x02: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 720, 2000, 2000, &r, &g, &b, &m); break; // black
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 2000, 2000, &r, &g, &b, &m); break; // white (buff)
                            }
                        }
                        break;
                        case (bm + rm): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 720, 1500, 1500, &r, &g, &b, &m); break; // dark green
                                case 0x10:
                                case 0x02:
                                case 0x12: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 1500, 1500, &r, &g, &b, &m); break; // green
                            }
                        }
                        break;
                        case (bm + rp): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 720, 1500, 2500, &r, &g, &b, &m); break; // dark orange
                                case 0x10:
                                case 0x02:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 1500, 2500, &r, &g, &b, &m); break; // normal orange
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 1500, 2500, &r, &g, &b, &m); break; // bright orange
                            }
                        }
                        break;
                    }
                 }
                 break;

                 case PALETTE_ATOM_6847_EMULATORS: {
                    switch (i & 0x2d) {  //these five are luma independent
                        case (bz + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 650, 2000, 2500, &r, &g, &b, &m); r = 181; g =   5; b =  34; break; // red
                        case (bp + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 650, 2500, 2000, &r, &g, &b, &m); r =  34; g =  19; b = 181; break; // blue
                        case (bp + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 2500, 2500, &r, &g, &b, &m); r = 255; g =  28; b = 255; break; // magenta
                        case (bz + rm):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 2000, 1500, &r, &g, &b, &m); r =  10; g = 212; b = 112; break; // cyan
                        case (bm + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 1500, 2000, &r, &g, &b, &m); r = 255; g = 255; b =  67; break; // yellow
                        case (bz + rz): {
                            switch (luma) {
                                case 0x00:
                                case 0x10: //alt
                                case 0x02: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 720, 2000, 2000, &r, &g, &b, &m); r =   9; g =   9; b =   9; break; // black
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 2000, 2000, &r, &g, &b, &m); r = 255; g = 255; b = 255; break; // white (buff)
                            }
                        }
                        break;
                        case (bm + rm): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 720, 1500, 1500, &r, &g, &b, &m); r =   0; g =  65; b =   0; break; // dark green
                                case 0x10:
                                case 0x02:
                                case 0x12: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 1500, 1500, &r, &g, &b, &m); r =  10; g = 255; b =  10; break; // green
                            }
                        }
                        break;
                        case (bm + rp): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 720, 1500, 2500, &r, &g, &b, &m); r = 107; g =   0; b =   0; break; // dark orange
                                case 0x10:
                                case 0x02:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 1500, 2500, &r, &g, &b, &m); r = 255; g =  67; b =  10; break; // normal orange
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 1500, 2500, &r, &g, &b, &m); r = 255; g = 181; b =  67; break; // bright orange
                            }
                        }
                        break;
                    }
                 }
                 break;

                 case PALETTE_ATOM_MKII: {
                    switch (i & 0x2d) {  //these five are luma independent
                        case (bz + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 650, 2000, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0x00; break; // red
                        case (bp + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 650, 2500, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0xff; break; // blue
                        case (bp + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 2500, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0xff; break; // magenta
                        case (bz + rm):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 2000, 1500, &r, &g, &b, &m); r=0x00; g=0xff; b=0xff; break; // cyan
                        case (bm + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 1500, 2000, &r, &g, &b, &m); r=0xff; g=0xff; b=0x00; break; // yellow
                        case (bz + rz): {
                            switch (luma) {
                                case 0x00:
                                case 0x10: //alt
                                case 0x02: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, black_ref, 2000, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0x00; break; // black
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 2000, 2000, &r, &g, &b, &m); r=0xff; g=0xff; b=0xff; break; // white (buff)
                            }
                        }
                        break;
                        case (bm + rm): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, black_ref, 2000, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0x00; break; // dark green (force black)
                                case 0x10:
                                case 0x02:
                                case 0x12: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 1500, 1500, &r, &g, &b, &m); r=0x00; g=0xff; b=0x00; break; // green
                            }
                        }
                        break;
                        case (bm + rp): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, black_ref, 2000, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0x00; break; // dark orange (force black)
                                case 0x10:
                                case 0x02:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 1500, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0x00; break; // normal orange (force red)
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 1500, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0x00; break; // bright orange (force red)
                            }
                        }
                        break;
                    }
                 }
                 break;

                 case PALETTE_ATOM_MKII_PLUS: {
                    switch (i & 0x2d) {  //these five are luma independent
                        case (bz + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 650, 2000, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0x00; break; // red
                        case (bp + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 650, 2500, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0xff; break; // blue
                        case (bp + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 2500, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0xff; break; // magenta
                        case (bz + rm):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 2000, 1500, &r, &g, &b, &m); r=0x00; g=0xff; b=0xff; break; // cyan
                        case (bm + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 1500, 2000, &r, &g, &b, &m); r=0xff; g=0xff; b=0x00; break; // yellow
                        case (bz + rz): {
                            switch (luma) {
                                case 0x00:
                                case 0x10: //alt
                                case 0x02: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, black_ref, 2000, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0x00; break; // black
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 2000, 2000, &r, &g, &b, &m); r=0xff; g=0xff; b=0xff; break; // white (buff)
                            }
                        }
                        break;
                        case (bm + rm): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, black_ref, 2000, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0x00; break; // dark green (force black)
                                case 0x10:
                                case 0x02:
                                case 0x12: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 1500, 1500, &r, &g, &b, &m); r=0x00; g=0xff; b=0x00; break; // green
                            }
                        }
                        break;
                        case (bm + rp): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, black_ref, 2000, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0x00; break; // dark orange (force black)
                                case 0x10:
                                case 0x02:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 1500, 2500, &r, &g, &b, &m); break; // normal orange was r = 160; g = 80; b = 0;
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 1500, 2500, &r, &g, &b, &m); break; // bright orange was r = 255; g = 127; b = 0;
                            }
                        }
                        break;
                    }
                 }
                 break;

                 case PALETTE_ATOM_MKII_FULL: {
                    switch (i & 0x2d) {  //these five are luma independent
                        case (bz + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 650, 2000, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0x00; break; // red
                        case (bp + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 650, 2500, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0xff; break; // blue
                        case (bp + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 2500, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0xff; break; // magenta
                        case (bz + rm):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 2000, 1500, &r, &g, &b, &m); r=0x00; g=0xff; b=0xff; break; // cyan
                        case (bm + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 1500, 2000, &r, &g, &b, &m); r=0xff; g=0xff; b=0x00; break; // yellow
                        case (bz + rz): {
                            switch (luma) {
                                case 0x00:
                                case 0x10: //alt
                                case 0x02: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 720, 2000, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0x00; break; // black
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 2000, 2000, &r, &g, &b, &m); r=0xff; g=0xff; b=0xff; break; // white (buff)
                            }
                        }
                        break;
                        case (bm + rm): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 720, 1500, 1500, &r, &g, &b, &m); break; // dark green was r = 0; g = 31; b = 0;
                                case 0x10:
                                case 0x02:
                                case 0x12: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 1500, 1500, &r, &g, &b, &m); r=0x00; g=0xff; b=0x00; break; // green
                            }
                        }
                        break;
                        case (bm + rp): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 720, 1500, 2500, &r, &g, &b, &m); break; // dark orange was r = 31; g = 15; b = 0;
                                case 0x10:
                                case 0x02:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 540, 1500, 2500, &r, &g, &b, &m); break; // normal orange was r = 160; g = 80; b = 0;
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, black_ref, 420, 1500, 2500, &r, &g, &b, &m); break; // bright orange was r = 255; g = 127; b = 0;
                            }
                        }
                        break;
                    }
                 }
                 break;

                 case PALETTE_MONO2:
                    switch (i & 0x02) {
                        case 0x00:
                            r = 0x00; break ;
                        case 0x02:
                            r = 0xff; break ;
                    }
                    g = r; b = r;
                    if (i & 1) {
                        r ^= 0xff;
                    }
                    break;
                 case PALETTE_MONO3:
                    switch (i & 0x12) {
                        case 0x00:
                            r = 0x00; break ;
                        case 0x10:
                        case 0x02:
                            r = 0x7f; break ;
                        case 0x12:
                            r = 0xff; break ;
                    }
                    g = r; b = r;
                    if (i & 1) {
                        r ^= 0xff;
                    }
                    break;
                 case PALETTE_MONO4:
                    switch (i & 0x12) {
                        case 0x00:
                            r = 0x00; break ;
                        case 0x10:
                            r = 0x55; break ;
                        case 0x02:
                            r = 0xaa; break ;
                        case 0x12:
                            r = 0xff; break ;
                    }
                    g = r; b = r;
                    if (i & 1) {
                        r ^= 0xff;
                    }
                    break;
                 case PALETTE_MONO6:
                    switch (i & 0x24) {
                        case 0x00:
                            r = 0x00; break ;
                        case 0x20:
                            r = 0x33; break ;
                        case 0x24:
                            r = 0x66; break ;
                    }
                    switch (i & 0x12) {
                        case 0x10:
                            r = 0x99; break ;
                        case 0x02:
                            r = 0xcc; break ;
                        case 0x12:
                            r = 0xff; break ;
                    }
                    g = r; b = r;
                    if (i & 1) {
                        r ^= 0xff;
                    }
                    break;

                    case PALETTE_YG_4:
                    switch (i & 0x12) {
                        case 0x00:
                            r = 0x00;g=0x00;b=0x00; break;
                        case 0x10:
                            r = 0x00;g=0xff;b=0x00; break;
                        case 0x02:
                            r = 0xff;g=0xff;b=0x00; break;
                        case 0x12:
                            r = 0xff;g=0xff;b=0xff; break;
                    }
                    break;
                    case PALETTE_VR_4:
                    switch (i & 0x09) {
                        case 0x00:
                            r = 0x00;g=0x00;b=0x00; break;
                        case 0x08:
                            r = 0xff;g=0x00;b=0x00; break;
                        case 0x01:
                            r = 0xff;g=0x00;b=0xff; break;
                        case 0x09:
                            r = 0xff;g=0xff;b=0xff; break;
                    }
                    break;
                    case PALETTE_UB_4:
                    switch (i & 0x24) {
                        case 0x00:
                            r = 0x00;g=0x00;b=0x00; break;
                        case 0x20:
                            r = 0x00;g=0x00;b=0xff; break;
                        case 0x04:
                            r = 0x00;g=0xff;b=0xff; break;
                        case 0x24:
                            r = 0xff;g=0xff;b=0xff; break;
                    }
                    break;
//**********************************************************************************

#define b3 0x24    // b-y 3
#define b2 0x04    // b-y 2
#define b1 0x20    // b-y 1
#define b0 0x00    // b-y 0
#define r3 0x09    // r-y 3
#define r2 0x01    // r-y 2
#define r1 0x08    // r-y 1
#define r0 0x00    // r-y 0
#define g3 0x12    // y 3
#define g2 0x02    // y 2
#define g1 0x10    // y 1
#define g0 0x00    // y 0
                 case PALETTE_TI: {
                    r=g=b=0;

                    switch (i & 0x12) {   //4 luminance levels
                        case 0x00:        // if here then black
                        {
                        switch (i & 0x2d) {
                                case (b2+r2):
                                r = 0x00;g=0x00;b=0x00; //black
                                break;
                                case (b3+r0): //ALT
                                case (b3+r1): //ALT
                                case (b3+r3): //ALT
                                case (b3+r2):
                                r = 0x5b;g=0x56;b=0xd7; //dk blue
                                break;
                                case (b2+r3): //ALT
                                case (b1+r3): //ALT
                                case (b1+r2):
                                r = 0xb5;g=0x60;b=0x54; //dk red
                                break;
                                case (b1+r0): //ALT
                                case (b1+r1):
                                r = 0x3f;g=0x9f;b=0x45; //dk green
                                break;
                            }
                        }
                        break;
                        case 0x10:        // if here then either md green/lt blue/md red/magenta
                        {
                        switch (i & 0x2d) {
                                case (b1+r2):
                                r = 0xb5;g=0x60;b=0x54; //dk red
                                break;
                                case (b1+r1):
                                r = 0x3f;g=0x9f;b=0x45; //dk green
                                break;
                                case (b1+r0):
                                r = 0x44;g=0xb5;b=0x4e; //md green
                                break;
                                case (b3+r2):
                                case (b3+r0): //ALT
                                case (b3+r1): //ALT
                                r = 0x81;g=0x78;b=0xea; //lt blue
                                break;
                                case (b1+r3):
                                r = 0xd5;g=0x68;b=0x5d; //md red
                                break;
                                case (b2+r2):
                                case (b2+r3): //ALT
                                case (b3+r3): //ALT
                                r = 0xb4;g=0x69;b=0xb2; //magenta
                                break;
                            }
                        }
                        break ;
                        case 0x02:        // if here then either lt green/lt red/cyan/dk yellow
                        {
                        switch (i & 0x2d) {
                                case (b1+r1):
                                case (b1+r0):  //ALT
                                r = 0x79;g=0xce;b=0x70; //lt green
                                break;
                                case (b1+r3):
                                r = 0xf9;g=0x8c;b=0x81; //lt red
                                break;
                                case (b2+r0):
                                case (b3+r0):  //ALT
                                r = 0x6c;g=0xda;b=0xec; //cyan
                                break;
                                case (b0+r2):
                                r = 0xcc;g=0xc3;b=0x66; //dk yellow
                                break;
                                case (b1+r2):
                                r = 0xde;g=0xd1;b=0x8d; //lt yellow
                                break;
                                case (b2+r2):
                                r = 0xcc;g=0xcc;b=0xcc; //grey
                                break;


                            }
                        }
                        break;
                        case 0x12:        //if here then white
                        {
                                r = 0xff;g=0xff;b=0xff; //white
                        }
                        break ;
                    }
                 }
                 break;

                 case PALETTE_SPECTRUM48K:
                    r=g=b=0;

                    switch (i & 0x12) {   //3 luminance levels

                        case 0x00:        // if here then either black/BLACK blue/BLUE red/RED magenta/MAGENTA green/GREEN
                        {

                            switch (i & 0x2d) {
                                case (bz + rz):
                                case (bz + rz2):
                                case (bz2 + rz):
                                case (bz2 + rz2):
                                r = 0x00;g=0x00;b=0x00;     //black
                                break;
                                case (bm + rz):
                                case (bm + rz2):
                                case (bm + rp): //alt
                                r = 0x00;g=0x00;b=0xd7;     //blue
                                break;
                                case (bp + rm):
                                r = 0xd7;g=0x00;b=0x00;     //red
                                break;
                                case (bz + rm):
                                case (bz2 + rm):
                                case (bm + rm): //alt
                                r = 0xd7;g=0x00;b=0xd7;     //magenta
                                break;
                                case (bp + rp):
                                r = 0x00;g=0xd7;b=0x00;     //green
                                break;
                            }
                        }
                        break;
                        case 0x02:
                        case 0x10:        // if here then either magenta/MAGENTA green/GREEN cyan/CYAN yellow/YELLOW white/WHITE
                        {
                            switch (i & 0x2d) {
                                case (bz + rm):
                                case (bz2 + rm):
                                case (bm + rm): //alt
                                r = 0xd7;g=0x00;b=0xd7;     //magenta
                                break;
                                case (bp + rp):
                                r = 0x00;g=0xd7;b=0x00;     //green
                                break;
                                case (bz + rp):
                                case (bz2 + rp):
                                case (bm + rp): //alt
                                r = 0x00;g=0xd7;b=0xd7;     //cyan
                                break;
                                case (bp + rz):
                                case (bp + rz2):
                                case (bp + rm): //alt
                                r = 0xd7;g=0xd7;b=0x00;     //yellow
                                break;
                                case (bz + rz):
                                case (bz + rz2):
                                case (bz2 + rz):
                                case (bz2 + rz2):
                                r = 0xd7;g=0xd7;b=0xd7;     //white
                                break;
                            }
                        }
                        break;

                        case 0x12:        //if here then YELLOW or WHITE
                        {
                        switch (i & 0x2d) {
                                case (bp + rz):
                                case (bp + rz2):
                                case (bp + rm): //alt
                                r = 0xd7;g=0xd7;b=0x00;     //yellow
                                break;
                                default:
                                r = 0xff;g=0xff;b=0xff;     //bright white
                                break;
                            }
                        }
                        break;

                    }
                    break;


                 case PALETTE_CGS24:
                    if ((i & 0x30) == 0x30) {

                    switch (i & 0x0f) {
                        case 12 :
                            r=234;g=234;b=234;
                        break;
                        case 10 :
                            r=31;g=157;b=0;
                        break;
                        case 13 :
                            r=188;g=64;b=0;
                        break;
                        case 11 :
                            r=238;g=195;b=14;
                        break;
                        case 9 :
                            r=235;g=111;b=43;
                        break;
                        case 7 :
                            r=94;g=129;b=255;
                        break;
                        case 14 :
                            r=125;g=255;b=251;
                        break;
                        case 1 :
                            r=255;g=172;b=255;
                        break;
                        case 6 :
                            r=171;g=255;b=74;
                        break;
                        case 15 :
                            r=94;g=94;b=94;
                        break;
                        case 8 :
                            r=234;g=234;b=234;
                        break;
                        case 4 :
                            r=78;g=239;b=204;
                        break;
                        case 3 :
                            r=152;g=32;b=255;
                        break;
                        case 2 :
                            r=47;g=83;b=255;
                        break;
                        case 5 :
                            r=255;g=242;b=61;
                        break;
                        case 0 :
                            r=37;g=37;b=37;
                        break;
                    }

                    } else {
                    r=0;g=0;b=0;
                    }
                 break;
                 case PALETTE_CGS25:
                 if ((i & 0x30) == 0x30) {
                    switch (i & 0x0f) {
                        case 12 :
                            r=234;g=234;b=234;
                        break;
                        case 10 :
                            r=31;g=157;b=0;
                        break;
                        case 13 :
                            r=188;g=64;b=0;
                        break;
                        case 11 :
                            r=238;g=195;b=14;
                        break;
                        case 9 :
                            r=235;g=111;b=43;
                        break;
                        case 7 :
                            r=94;g=129;b=255;
                        break;
                        case 14 :
                            r=125;g=255;b=251;
                        break;
                        case 1 :
                            r=255;g=172;b=255;
                        break;
                        case 6 :
                            r=47;g=83;b=255;
                        break;
                        case 15 :
                            r=234;g=234;b=234;
                        break;
                        case 8 :
                            r=255;g=242;b=61;
                        break;
                        case 4 :
                            r=78;g=239;b=204;
                        break;
                        case 3 :
                            r=171;g=255;b=74;
                        break;
                        case 2 :
                            r=94;g=94;b=94;
                        break;
                        case 5 :
                            r=152;g=32;b=255;
                        break;
                        case 0 :
                            r=37;g=37;b=37;
                        break;
                    }
                    } else {
                    r=0;g=0;b=0;
                    }
                 break;
                 case PALETTE_CGN25:
                 if ((i & 0x30) == 0x30) {
                    switch (i & 0x0f) {
                        case 12 :
                            r=234;g=234;b=234;
                        break;
                        case 10 :
                            r=171;g=255;b=74;
                        break;
                        case 13 :
                            r=203;g=38;b=94;
                        break;
                        case 11 :
                            r=255;g=242;b=61;
                        break;
                        case 9 :
                            r=235;g=111;b=43;
                        break;
                        case 7 :
                            r=47;g=83;b=255;
                        break;
                        case 14 :
                            r=124;g=255;b=234;
                        break;
                        case 1 :
                            r=152;g=32;b=255;
                        break;
                        case 6 :
                            r=188;g=223;b=255;
                        break;
                        case 15 :
                            r=94;g=94;b=94;
                        break;
                        case 8 :
                            r=234;g=255;b=39;
                        break;
                        case 4 :
                            r=138;g=103;b=255;
                        break;
                        case 3 :
                            r=140;g=140;b=140;
                        break;
                        case 2 :
                            r=31;g=196;b=140;
                        break;
                        case 5 :
                            r=199;g=78;b=255;
                        break;
                        case 0 :
                            r=255;g=255;b=255;
                        break;
                    }

                    } else {
                    r=0;g=0;b=0;
                    }
                 break;

                case PALETTE_C64_REV1:
                case PALETTE_C64: {
                    int revision = palette == PALETTE_C64_REV1 ? 0 : 1;
                    double brightness = 50;
                    double contrast = 100;
                    double saturation = 50;
                    r=g=b=0;

                    switch (i & 0x3f) {
                        case (g0+b1+r1):
                        create_colodore_colours(0, revision, brightness, contrast, saturation, &r, &g, &b, &m); //black
                        break;
                        case (g3+b1+r1):
                        create_colodore_colours(1, revision, brightness, contrast, saturation, &r, &g, &b, &m); //white
                        break;
                        case (g0+b1+r3):
                        create_colodore_colours(2, revision, brightness, contrast, saturation, &r, &g, &b, &m); //red
                        break;

                        case (g3+b3+r0):
                        create_colodore_colours(3, revision, brightness, contrast, saturation, &r, &g, &b, &m); //cyan
                        break;
                        case (g1+b3+r3):
                        create_colodore_colours(4, revision, brightness, contrast, saturation, &r, &g, &b, &m); //violet
                        break;
                        case (g1+b0+r0):
                        create_colodore_colours(5, revision, brightness, contrast, saturation, &r, &g, &b, &m); //green
                        break;

                        case (g0+b3+r1):
                        create_colodore_colours(6, revision, brightness, contrast, saturation, &r, &g, &b, &m); //blue
                        break;
                        case (g3+b0+r1):
                        create_colodore_colours(7, revision, brightness, contrast, saturation, &r, &g, &b, &m); //yellow
                        break;
                        case (g1+b0+r3):
                        create_colodore_colours(8, revision, brightness, contrast, saturation, &r, &g, &b, &m); //orange
                        break;

                        case (g0+b0+r1):
                        create_colodore_colours(9, revision, brightness, contrast, saturation, &r, &g, &b, &m); //brown
                        break;
                        case (g1+b1+r3):
                        create_colodore_colours(10, revision, brightness, contrast, saturation, &r, &g, &b, &m); //light red
                        break;
                        case (g0+b1+r0):
                        create_colodore_colours(11, revision, brightness, contrast, saturation, &r, &g, &b, &m); //dark grey
                        break;

                        case (g1+b1+r1):
                        create_colodore_colours(12, revision, brightness, contrast, saturation, &r, &g, &b, &m); //grey2
                        break;
                        case (g3+b0+r0):
                        create_colodore_colours(13, revision, brightness, contrast, saturation, &r, &g, &b, &m); //light green
                        break;
                        case (g1+b3+r1):
                        create_colodore_colours(14, revision, brightness, contrast, saturation, &r, &g, &b, &m); //light blue
                        break;

                        case (g3+b1+r0):
                        create_colodore_colours(15, revision, brightness, contrast, saturation, &r, &g, &b, &m); //light grey
                        break;

                    }
                 }
                 break;
            }

            if (m == -1) {  // calculate mono if not already set
                m = ((299 * r + 587 * g + 114 * b + 500) / 1000);
                if (m > 255) {
                   m = 255;
                }
            }
            palette_array[palette][i] = (m << 24) | (b << 16) | (g << 8) | r;
        }
        strncpy(palette_names[palette], default_palette_names[palette], MAX_NAMES_WIDTH);
    }
}




void osd_update_palette() {
    if (capinfo->bpp < 16) {
        int r = 0;
        int g = 0;
        int b = 0;
        int m = 0;
        int num_colours = (capinfo->bpp == 8) ? 256 : 16;
        int design_type = (cpld->get_version() >> VERSION_DESIGN_BIT) & 0x0F;

        //copy selected palette to current palette, translating for Atom cpld and inverted Y setting (required for 6847 direct Y connection)
        for (int i = 0; i < num_colours; i++) {
            int i_adj = i;
            if(design_type == DESIGN_ATOM) {
                switch (i) {
                    case 0x00:
                        i_adj = 0x00 | (bz + rz); break;  //black
                    case 0x01:
                        i_adj = 0x10 | (bz + rp); break;  //red
                    case 0x02:
                        i_adj = 0x10 | (bm + rm); break;  //green
                    case 0x03:
                        i_adj = 0x12 | (bm + rz); break;  //yellow
                    case 0x04:
                        i_adj = 0x10 | (bp + rz); break;  //blue
                    case 0x05:
                        i_adj = 0x10 | (bp + rp); break;  //magenta
                    case 0x06:
                        i_adj = 0x10 | (bz + rm); break;  //cyan
                    case 0x07:
                        i_adj = 0x12 | (bz + rz); break;  //white
                    case 0x0b:
                        i_adj = 0x10 | (bm + rp); break;  //orange
                    case 0x13:
                        i_adj = 0x12 | (bm + rp); break;  //bright orange
                    case 0x08:
                        i_adj = 0x00 | (bm + rm); break;  //dark green
                    case 0x10:
                        i_adj = 0x00 | (bm + rp); break;  //dark orange
                    default:
                        i_adj = 0x00 | (bz + rz); break;  //black
                }
            }

            if (get_feature(F_INVERT) == INVERT_Y) {
                i_adj ^= 0x12;
            }

            if (capinfo->palette_control < PALETTECONTROL_NTSCARTIFACT_CGA || (capinfo->palette_control == PALETTECONTROL_NTSCARTIFACT_CGA && get_feature(F_NTSCCOLOUR) == 0) || capinfo->bpp != 8 || (capinfo->sizex2 & 2) != 0) {
                palette_data[i] = palette_array[palette][i_adj];
            } else {
                //if (get_paletteControl() == PALETTECONTROL_NTSCARTIFACT_CGA) {
                    if ((i & 0x7f) < 0x30) {
                        int filtered_bitcount = ((i % 0x30) >> 4) + 1;
                        palette_data[i] = create_NTSC_artifact_colours(i & 0x7f, filtered_bitcount);
                    } else if ((i & 0x7f) < 0x50) {
                        palette_data[i] = create_NTSC_artifact_colours_palette_320(i & 0x7f);
                    }
            }
            palette_data[i] = adjust_palette(palette_data[i]);
        }

        //scan translated palette for equivalences
        for (int i = 0; i < num_colours; i++) {
            for(int j = i; j < num_colours; j++) {
                if (palette_data[i] == palette_data[j]) {
                    equivalence[i] = (char) j;
                }
            }
        }

        // modify translated palette for remaining settings
        for (int i = 0; i < num_colours; i++) {
            if (paletteFlags & BIT_MODE2_PALETTE) {
                r = customPalette[i & 0x7f] & 0xff;
                g = (customPalette[i & 0x7f]>>8) & 0xff;
                b = (customPalette[i & 0x7f]>>16) & 0xff;
                m = ((299 * r + 587 * g + 114 * b + 500) / 1000);
                if (m > 255) {
                    m = 255;
                }
            } else {
                r = palette_data[i] & 0xff;
                g = (palette_data[i] >> 8) & 0xff;
                b = (palette_data[i] >>16) & 0xff;
                m = (palette_data[i] >>24);
            }
            if (get_feature(F_INVERT) == INVERT_RGB) {
                r = 255 - r;
                g = 255 - g;
                b = 255 - b;
                m = 255 - m;
            }
            if (get_feature(F_COLOUR) != COLOUR_NORMAL) {
                 switch (get_feature(F_COLOUR)) {
                 case COLOUR_MONO:
                    r = m;
                    g = m;
                    b = m;
                    break;
                 case COLOUR_GREEN:
                    r = 0;
                    g = m;
                    b = 0;
                    break;
                 case COLOUR_AMBER:
                    r = m*0xdf/0xff;
                    g = m;
                    b = 0;
                    break;
                 }
            }
            if (active) {
                if (i >= (num_colours >> 1)) {
                palette_data[i] = 0xFFFFFFFF;
                } else {
                if (!inhibit_palette_dimming) {
                    r >>= 1; g >>= 1; b >>= 1;
                }
                palette_data[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            } else {
                if ((i >= (num_colours >> 1)) && get_feature(F_SCANLINES)) {
                    int scanline_intensity = get_feature(F_SCANLINESINT) ;
                    r = (r * scanline_intensity)>>4;
                    g = (g * scanline_intensity)>>4;
                    b = (b * scanline_intensity)>>4;
                    palette_data[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
                } else {
                    palette_data[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            if (get_debug()) {
                palette_data[i] |= 0x00101010;
            }
       }
       RPI_PropertyInit();
       RPI_PropertyAddTag(TAG_SET_PALETTE, num_colours, palette_data);
       RPI_PropertyProcess();
    }
}

void osd_clear() {
   if (active) {
      memset(buffer, 0, sizeof(buffer));
      osd_update((uint32_t *) (capinfo->fb + capinfo->pitch * capinfo->height * get_current_display_buffer() + capinfo->pitch * capinfo->v_adjust + capinfo->h_adjust), capinfo->pitch);
      active = 0;
      osd_update_palette();
   }
   osd_hwm = 0;
}

void osd_clear_no_palette() {
   if (active) {
      memset(buffer, 0, sizeof(buffer));
      osd_update((uint32_t *) (capinfo->fb + capinfo->pitch * capinfo->height * get_current_display_buffer() + capinfo->pitch * capinfo->v_adjust + capinfo->h_adjust), capinfo->pitch);
      active = 0;
   }
   osd_hwm = 0;
}

int save_profile(char *path, char *name, char *buffer, char *default_buffer, char *sub_default_buffer)
{
   char *pointer = buffer;
   char param_string[80];
   param_t *param;
   int current_mode7 = geometry_get_mode();
   int i;
   int cpld_ver = (cpld->get_version() >> VERSION_DESIGN_BIT) & 0x0F;
   int index = 1;
   if (cpld_ver == DESIGN_ATOM ) {
       index = 0;
   }
   if (default_buffer != NULL) {
      if (get_feature(F_AUTOSWITCH) == AUTOSWITCH_MODE7) {

         geometry_set_mode(1);
         cpld->set_mode(1);
         pointer += sprintf(pointer, "sampling7=");
         i = index;
         param = cpld->get_params() + i;
         while(param->key >= 0) {
            pointer += sprintf(pointer, "%d,", cpld->get_value(param->key));
            i++;
            param = cpld->get_params() + i;
         }
         pointer += sprintf(pointer - 1, "\r\n") - 1;
         pointer += sprintf(pointer, "geometry7=");
         i = 1;
         param = geometry_get_params() + i;
         while(param->key >= 0) {
            pointer += sprintf(pointer, "%d,", geometry_get_value(param->key));
            i++;
            param = geometry_get_params() + i;
         }
         pointer += sprintf(pointer - 1, "\r\n") - 1;
      }

      geometry_set_mode(0);
      cpld->set_mode(0);

      pointer += sprintf(pointer, "sampling=");
      i = index;
      param = cpld->get_params() + i;
      while(param->key >= 0) {
         pointer += sprintf(pointer, "%d,", cpld->get_value(param->key));
         i++;
         param = cpld->get_params() + i;
      }
      pointer += sprintf(pointer - 1, "\r\n") - 1;
      pointer += sprintf(pointer, "geometry=");
      i = 1;
      param = geometry_get_params() + i;
      while(param->key >= 0) {
         pointer += sprintf(pointer, "%d,", geometry_get_value(param->key));
         i++;
         param = geometry_get_params() + i;
      }
      pointer += sprintf(pointer - 1, "\r\n") - 1;
      geometry_set_mode(current_mode7);
      cpld->set_mode(current_mode7);
   }

   i = 0;
   while (features[i].key >= 0) {
      if ((default_buffer != NULL && i != F_RESOLUTION && i != F_SCALING && i != F_FRONTEND && i != F_PROFILE && i != F_SUBPROFILE && (i != F_AUTOSWITCH || sub_default_buffer == NULL))
          || (default_buffer == NULL && i == F_AUTOSWITCH)) {
         strcpy(param_string, features[i].property_name);
         if (i == F_PALETTE) {
            sprintf(pointer, "%s=%s", param_string, palette_names[get_feature(i)]);
         } else {
            sprintf(pointer, "%s=%d", param_string, get_feature(i));
         }
         if (strstr(default_buffer, pointer) == NULL) {
            if (sub_default_buffer) {
               if (strstr(sub_default_buffer, pointer) == NULL) {
                  log_info("Writing sub profile entry: %s", pointer);
                  pointer += strlen(pointer);
                  pointer += sprintf(pointer, "\r\n");
               }
            } else {
               log_info("Writing profile entry: %s", pointer);
               pointer += strlen(pointer);
               pointer += sprintf(pointer, "\r\n");
            }
         }
      }
      i++;
   }
   *pointer = 0;
   return file_save(path, name, buffer, pointer - buffer);
}

void process_single_profile(char *buffer) {
   char param_string[80];
   char *prop;
   int current_mode7 = geometry_get_mode();
   int i;
   if (buffer[0] == 0) {
      return;
   }
   int cpld_ver = (cpld->get_version() >> VERSION_DESIGN_BIT) & 0x0F;
   int index = 1;
   if (cpld_ver == DESIGN_ATOM) {
       index = 0;
   }
   for (int m7 = 0; m7 < 2; m7++) {
      geometry_set_mode(m7);
      cpld->set_mode(m7);

      prop = get_prop(buffer, m7 ? "sampling7" : "sampling");
      if (prop) {
         char *prop2 = strtok(prop, ",");
         int i = index;
         while (prop2) {
            param_t *param;
            param = cpld->get_params() + i;
            if (param->key < 0) {
               log_warn("Too many sampling sub-params, ignoring the rest");
               break;
            }
            int val = atoi(prop2);
            log_debug("cpld: %s = %d", param->label, val);
            cpld->set_value(param->key, val);
            prop2 = strtok(NULL, ",");
            i++;
         }
      }

      prop = get_prop(buffer, m7 ? "geometry7" : "geometry");
      if (prop) {
         char *prop2 = strtok(prop, ",");
         int i = 1;
         while (prop2) {
            param_t *param;
            param = geometry_get_params() + i;
            if (param->key < 0) {
               log_warn("Too many sampling sub-params, ignoring the rest");
               break;
            }
            int val = atoi(prop2);
            log_debug("geometry: %s = %d", param->label, val);
            geometry_set_value(param->key, val);
            prop2 = strtok(NULL, ",");
            i++;
         }
      }

   }

   geometry_set_mode(current_mode7);
   cpld->set_mode(current_mode7);

   i = 0;
   while(features[i].key >= 0) {
      if (i != F_RESOLUTION && i != F_SCALING && i != F_FRONTEND && i != F_PROFILE && i != F_SUBPROFILE) {
         strcpy(param_string, features[i].property_name);
         prop = get_prop(buffer, param_string);
         if (prop) {
            if (i == F_PALETTE) {
                for (int j = 0; j <= features[F_PALETTE].max; j++) {
                    if (strcmp(palette_names[j], prop) == 0) {
                        set_feature(i, j);
                        break;
                    }
                }
                log_debug("profile: %s = %s",param_string, prop);
            } else {
                int val = atoi(prop);
                set_feature(i, val);
                log_debug("profile: %s = %d",param_string, val);
            }
         }
      }
      i++;
   }


   // Properties below this point are not updateable in the UI
   prop = get_prop(buffer, "keymap");
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
            }
         }
         i++;
      }
   }

   prop = get_prop(buffer, "actionmap");
   if (prop) {
      int i = 0;
      while (*prop && i < NUM_ACTIONS) {
         osd_state_t val = MIN_ACTION + 1 + ((*prop++) - '0');
         if (val > MIN_ACTION && val < MAX_ACTION) {
            action_map[i] = val;
         }
         i++;
      }
   }

   prop = get_prop(buffer, "cpld_firmware_dir");
   if (prop) {
      strcpy(cpld_firmware_dir, prop);
   }

   // Disable CPLDv2 specific features for CPLDv1
   if (cpld->old_firmware_support() & BIT_NORMAL_FIRMWARE_V1) {
      features[F_M7DEINTERLACE].max = M7DEINTERLACE_MA4;
      if (get_feature(F_M7DEINTERLACE) > features[F_M7DEINTERLACE].max) {
         set_feature(F_M7DEINTERLACE, M7DEINTERLACE_MA1); // TODO: Decide whether this is the right fallback
      }
   }
}

void get_autoswitch_geometry(char *buffer, int index)
{
   char *prop;
   // Default properties
   prop = get_prop(buffer, "geometry");
   if (prop) {
      char *prop2 = strtok(prop, ",");
      int i = 1;
      while (prop2) {
         param_t *param;
         param = geometry_get_params() + i;
         if (param->key < 0) {
            log_warn("Too many sampling sub-params, ignoring the rest");
            break;
         }
         int val = atoi(prop2);
         if (i == CLOCK) {
            autoswitch_info[index].clock = val;
            log_debug("autoswitch: %s = %d", param->label, val);
         }
         if (i == LINE_LEN) {
            autoswitch_info[index].line_len = val;
            log_debug("autoswitch: %s = %d", param->label, val);
         }
         if (i == CLOCK_PPM) {
            autoswitch_info[index].clock_ppm = val;
            log_debug("autoswitch: %s = %d", param->label, val);
         }
         if (i == LINES_FRAME) {
            autoswitch_info[index].lines_per_frame = val;
            log_debug("autoswitch: %s = %d", param->label, val);
         }
         if (i == SYNC_TYPE) {
            autoswitch_info[index].sync_type = val;
            log_debug("autoswitch: %s = %d", param->label, val);
         }
         prop2 = strtok(NULL, ",");
         i++;
      }
   }
   double line_time = (double) autoswitch_info[index].line_len * 1000000000 / autoswitch_info[index].clock;
   double window = (double) autoswitch_info[index].clock_ppm * line_time / 1000000;
   autoswitch_info[index].lower_limit = (int) line_time - window;
   autoswitch_info[index].upper_limit = (int) line_time + window;
   log_info("Autoswitch timings %d (%s) = %d, %d, %d, %d, %d", index, sub_profile_names[index], autoswitch_info[index].lower_limit, (int) line_time,
            autoswitch_info[index].upper_limit, autoswitch_info[index].lines_per_frame, autoswitch_info[index].sync_type);
}

void process_profile(int profile_number) {
   process_single_profile(default_buffer);
   if (has_sub_profiles[profile_number]) {
      process_single_profile(sub_default_buffer);
   } else {
      process_single_profile(main_buffer);
   }
   cycle_menus();
}

void process_sub_profile(int profile_number, int sub_profile_number) {
   if (has_sub_profiles[profile_number]) {
      int saved_autoswitch = get_feature(F_AUTOSWITCH);                   // save autoswitch so it can be disabled to manually switch sub profiles
      process_single_profile(default_buffer);
      process_single_profile(sub_default_buffer);
      set_feature(F_AUTOSWITCH, saved_autoswitch);
      process_single_profile(sub_profile_buffers[sub_profile_number]);
      cycle_menus();
   }
}

void load_profiles(int profile_number, int save_selected) {
   unsigned int bytes ;
   main_buffer[0] = 0;
   features[F_SUBPROFILE].max = 0;
   strcpy(sub_profile_names[0], NOT_FOUND_STRING);
   sub_profile_buffers[0][0] = 0;
   if (has_sub_profiles[profile_number]) {
      bytes = file_read_profile(profile_names[profile_number], DEFAULT_STRING, save_selected, sub_default_buffer, MAX_BUFFER_SIZE - 4);
      if (bytes) {
         size_t count = 0;
         scan_sub_profiles(sub_profile_names, profile_names[profile_number], &count);
         if (count) {
            features[F_SUBPROFILE].max = count - 1;
            for (int i = 0; i < count; i++) {
               file_read_profile(profile_names[profile_number], sub_profile_names[i], 0, sub_profile_buffers[i], MAX_BUFFER_SIZE - 4);
               get_autoswitch_geometry(sub_profile_buffers[i], i);
            }
         }
      }
   } else {
      features[F_SUBPROFILE].max = 0;
      strcpy(sub_profile_names[0], NONE_STRING);
      sub_profile_buffers[0][0] = 0;
      if (strcmp(profile_names[profile_number], NOT_FOUND_STRING) != 0) {
         file_read_profile(profile_names[profile_number], NULL, save_selected, main_buffer, MAX_BUFFER_SIZE - 4);
      }
   }
}

int sub_profiles_available(int profile_number) {
   return has_sub_profiles[profile_number];
}

int autoswitch_detect(int one_line_time_ns, int lines_per_frame, int sync_type) {
   if (has_sub_profiles[get_feature(F_PROFILE)]) {
      log_info("Looking for autoswitch match = %d, %d, %d", one_line_time_ns, lines_per_frame, sync_type);
      for (int i=0; i <= features[F_SUBPROFILE].max; i++) {
         //log_info("Autoswitch test: %s (%d) = %d, %d, %d, %d", sub_profile_names[i], i, autoswitch_info[i].lower_limit,
         //          autoswitch_info[i].upper_limit, autoswitch_info[i].lines_per_frame, autoswitch_info[i].sync_type );
         if (   one_line_time_ns > autoswitch_info[i].lower_limit
                && one_line_time_ns < autoswitch_info[i].upper_limit
                && lines_per_frame == autoswitch_info[i].lines_per_frame
                && sync_type == autoswitch_info[i].sync_type ) {
            log_info("Autoswitch match: %s (%d) = %d, %d, %d, %d", sub_profile_names[i], i, autoswitch_info[i].lower_limit,
                     autoswitch_info[i].upper_limit, autoswitch_info[i].lines_per_frame, autoswitch_info[i].sync_type );
            return (i);
         }
      }
   }
   return -1;
}
void osd_set_noupdate(int line, int attr, char *text) {
   if (line > osd_hwm) {
       osd_hwm = line;
   }
   if (!active) {
      active = 1;
      osd_update_palette();
   }
   attributes[line] = attr;
   memset(buffer + line * LINELEN, 0, LINELEN);
   int len = strlen(text);
   if (len > LINELEN) {
      len = LINELEN;
   }
   strncpy(buffer + line * LINELEN, text, len);
}

void osd_set(int line, int attr, char *text) {
   osd_set_noupdate(line, attr, text);
   osd_update((uint32_t *) (capinfo->fb + capinfo->pitch * capinfo->height * get_current_display_buffer() + capinfo->pitch * capinfo->v_adjust + capinfo->h_adjust), capinfo->pitch);
}

void osd_set_clear(int line, int attr, char *text) {
   osd_set_noupdate(line, attr, text);
   if (capinfo->bpp >= 16) {
       clear_screen();
   }
   osd_update((uint32_t *) (capinfo->fb + capinfo->pitch * capinfo->height * get_current_display_buffer() + capinfo->pitch * capinfo->v_adjust + capinfo->h_adjust), capinfo->pitch);
}

int osd_active() {
   return active;
}

void osd_show_cpld_recovery_menu() {
   static char name[] = "CPLD Recovery Menu";
   update_cpld_menu.name = name;
   current_menu[0] = &main_menu;
   current_item[0] = 0;
   current_menu[1] = &info_menu;
   current_item[1] = 0;
   current_menu[2] = &update_cpld_menu;
   current_item[2] = 0;
   depth = 2;
   osd_state = MENU;
   // Change the font size to the large font (no profile will be loaded)
   set_feature(F_FONTSIZE, FONTSIZE_12X20_8);
   // Bring up the menu
   osd_refresh();
   // Add some warnings
   int line = 6;
   osd_set(line++, ATTR_DOUBLE_SIZE,  "IMPORTANT:");
   line++;
   osd_set(line++, 0, "The CPLD type (3_BIT/6-8_BIT) must match");
   osd_set(line++, 0, "the RGBtoHDMI board type you have:");
   line++;
   osd_set(line++, 0, "Use 3_BIT_CPLD_vxx for Hoglet's");
   osd_set(line++, 0, "   original RGBtoHD (c) 2018 board");
   osd_set(line++, 0, "Use 6-8_BIT_CPLD_vxx for IanB's");
   osd_set(line++, 0, "   6-bit Issue 2 or 6/8-bit Issue 3 boards");
   line++;
   osd_set(line++, 0, "See Wiki for Atom board CPLD programming");
   line++;
   osd_set(line++, 0, "Programming the wrong CPLD type may");
   osd_set(line++, 0, "cause damage to your RGBtoHDMI board.");
   osd_set(line++, 0, "Please ask for help if you are not sure.");
   line++;
   osd_set(line++, 0, "Hold 3 buttons during reset for this menu.");
}

void osd_refresh() {
   // osd_refresh is called very infrequently, for example when the mode had changed,
   // or when the OSD font size has changes. To allow menus to change depending on
   // mode, we do a full rebuild of the current menu. We also try to keep the cursor
   // on the same logic item (by matching parameter key if there is one).
   menu_t *menu = current_menu[depth];
   base_menu_item_t *item;
   if (menu && menu->rebuild) {
      // record the current item
      item = menu->items[current_item[depth]];
      // record the param associates with the current item
      int key = -1;
      if (item->type == I_FEATURE || item->type == I_GEOMETRY || item->type == I_PARAM) {
         key = ((param_menu_item_t *) item)->param->key;
      }
      // rebuild the menu
      menu->rebuild(menu);
      // try to set the cursor at the same item
      int i = 0;
      while (menu->items[i]) {
         item = menu->items[i];
         if (item->type == I_FEATURE || item->type == I_GEOMETRY || item->type == I_PARAM) {
            if (((param_menu_item_t *) item)->param->key == key) {
               current_item[depth] = i;
               // don't break here, as we need the length of the list
            }
         }
         i++;
      }
      // Make really sure we are still pointing to a valid item
      if (current_item[depth] >= i) {
         current_item[depth] = 0;
      }
   }
   if (osd_state == MENU || osd_state == PARAM || osd_state == INFO) {
      osd_clear_no_palette();
      redraw_menu();
   } else {
      osd_clear();
   }
}

int osd_key(int key) {
   item_type_t type;
   base_menu_item_t *item = current_menu[depth]->items[current_item[depth]];
   child_menu_item_t *child_item = (child_menu_item_t *)item;
   param_menu_item_t *param_item = (param_menu_item_t *)item;
   int val;
   int ret = -1;
   static int cal_count;
   static int last_vsync;
   static int last_key;
   static int first_time_press = 0;
   static int last_up_down_key = 0;
   switch (osd_state) {

   case IDLE:
      if (key == OSD_EXPIRED) {
         osd_clear();
         // Remain in the idle state
      } else {
         // Remember the original key pressed
         last_key = key;
         // Fire OSD_EXPIRED in 1 frames time
         ret = 1;
         // come back to DURATION
         osd_state = DURATION;
      }
      break;

   case DURATION:
      // Fire OSD_EXPIRED in 1 frames time
      ret = 1;

      // Use duration to determine action
      val = get_key_down_duration(last_key);

      // Descriminate between short and long button press as early as possible
      if (val == 0 || val > LONG_KEY_PRESS_DURATION) {
         // Calculate the logical key pressed (0..5) based on the physical key and the duration
         int key_pressed = (last_key - OSD_SW1);
         if (val) {
            // long press
            key_pressed += 3;
            // avoid key immediately auto-repeating
            set_key_down_duration(last_key, 0);
         }
         if (key_pressed < 0 || key_pressed >= NUM_ACTIONS) {
            log_warn("Key pressed (%d) out of range 0..%d ", key_pressed, NUM_ACTIONS - 1);
            osd_state = IDLE;
         } else {
            log_debug("Key pressed = %d", key_pressed);
            int action = action_map[key_pressed];
            log_debug("Action      = %d", action);
            // Transition to action state
            osd_state = action;
         }
      }
      break;

   case A0_LAUNCH:
      osd_state = MENU;
      current_menu[depth] = &main_menu;
      current_item[depth] = 0;
      if(active == 0) {
         clear_menu_bits();
      }
      redraw_menu();
      break;

   case A1_CAPTURE:
      // Capture screen shot
      ret = 4;
      osd_state = A1_CAPTURE_SUB;
      break;

   case A1_CAPTURE_SUB:
      // Capture screen shot
      osd_clear();
      capture_screenshot(capinfo, profile_names[get_feature(F_PROFILE)]);
      // Fire OSD_EXPIRED in 50 frames time
      ret = 50;
      // come back to IDLE
      osd_state = IDLE;
      break;

   case A2_CLOCK_CAL:
      // HDMI Calibration
      clear_menu_bits();
      osd_set(0, ATTR_DOUBLE_SIZE, "Enable Genlock");
      // Record the starting value of vsync
      last_vsync = get_vsync();
      // Enable vsync
      set_vsync(1);
      // Do the actual clock calibration
      action_calibrate_clocks();
      // Initialize the counter used to limit the calibration time
      cal_count = 0;
      // Fire OSD_EXPIRED in 50 frames time
      ret = 50;
      // come back to CLOCK_CAL0
      osd_state = CLOCK_CAL0;
      break;

   case A3_AUTO_CAL:
      clear_menu_bits();
      osd_set(0, ATTR_DOUBLE_SIZE, "Auto Calibration");
      osd_set(1, 0, "Video must be static during calibration");
      action_calibrate_auto();
      // Fire OSD_EXPIRED in 50 frames time
      ret = 50;
      // come back to IDLE
      osd_state = IDLE;
      break;

   case A4_SCANLINES:
      clear_menu_bits();
      set_feature(F_SCANLINES, 1 - get_feature(F_SCANLINES));
      if (get_feature(F_SCANLINES)) {
         osd_set(0, ATTR_DOUBLE_SIZE, "Scanlines on");
      } else {
         osd_set(0, ATTR_DOUBLE_SIZE, "Scanlines off");
      }
      // Fire OSD_EXPIRED in 50 frames time
      ret = 50;
      // come back to IDLE
      osd_state = IDLE;
      break;

   case A5_NTSCCOLOUR:
      set_feature(F_NTSCCOLOUR, 1 - get_feature(F_NTSCCOLOUR));
      ret = 1;
      osd_state = NTSC_MESSAGE;
      break;

   case NTSC_MESSAGE:
      clear_menu_bits();
      if (get_paletteControl() >= PALETTECONTROL_NTSCARTIFACT_CGA) {
          if (get_feature(F_NTSCCOLOUR)) {
             osd_set(0, ATTR_DOUBLE_SIZE, "NTSC Colour on");
          } else {
             osd_set(0, ATTR_DOUBLE_SIZE, "NTSC Colour off");
          }
      } else {
          set_feature(F_NTSCCOLOUR, 0);
          osd_set(0, ATTR_DOUBLE_SIZE, "Not NTSC Artifacting");
      }
      // Fire OSD_EXPIRED in 50 frames time
      ret = 50;
      // come back to IDLE
      osd_state = IDLE;
      break;

   case A6_SPARE:
   case A7_SPARE:
      clear_menu_bits();
      sprintf(message, "Action %d (spare)", osd_state - (MIN_ACTION + 1));
      osd_set(0, ATTR_DOUBLE_SIZE, message);
      // Fire OSD_EXPIRED in 50 frames time
      ret = 50;
      // come back to IDLE
      osd_state = IDLE;
      break;

   case CLOCK_CAL0:
      // Fire OSD_EXPIRED in 50 frames time
      ret = 50;
      if (is_genlocked()) {
         // move on when locked
         osd_set(0, ATTR_DOUBLE_SIZE, "Genlock Succeeded");
         osd_state = CLOCK_CAL1;
      } else if (cal_count == 10) {
         // give up after 10 seconds
         osd_set(0, ATTR_DOUBLE_SIZE, "Genlock Failed");
         osd_state = CLOCK_CAL1;
         // restore the original HDMI clock
         set_vlockmode(HDMI_ORIGINAL);
      } else {
         cal_count++;
      }
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
         last_up_down_key = 0;
         // ENTER
         switch (type) {
         case I_MENU:
            depth++;
            current_menu[depth] = child_item->child;
            current_item[depth] = 0;
            // Rebuild dynamically populated menus, e.g. the sampling and geometry menus that are mode specific
            if (child_item->child && child_item->child->rebuild) {
               child_item->child->rebuild(child_item->child);
            }
            osd_clear_no_palette();
            redraw_menu();
            break;
         case I_FEATURE:
         case I_GEOMETRY:
         case I_PARAM:
            // Test if a toggleable param (i.e. just two legal values)
            // (this is a generalized of boolean parameters)
            if (is_toggleable_param(param_item)) {
               // If so, then just toggle it
               toggle_param(param_item);
               // Special case the return at end parameter, to keep the cursor in the same position
               if (type == I_FEATURE && param_item->param->key == F_RETURN) {
                  if (return_at_end) {
                     current_item[depth]--;
                  } else {
                     current_item[depth]++;
                  }

               }
            } else {
               // If not then move to the parameter editing state
               osd_state = PARAM;
              //  if (!inhibit_palette_dimming && type == I_FEATURE
              //      && (param_item->param->key == F_TINT || param_item->param->key == F_SAT || param_item->param->key == F_CONT || param_item->param->key == F_BRIGHT || param_item->param->key == F_GAMMA)
                if (!inhibit_palette_dimming && current_menu[depth] == &palette_menu) {
                    inhibit_palette_dimming = 1;
                    osd_update_palette();
                }

            }
            redraw_menu();
            break;
         case I_INFO:
            osd_state = INFO;
            osd_clear_no_palette();
            redraw_menu();
            break;
         case I_BACK:
            set_setup_mode(SETUP_NORMAL);
            int cpld_ver = (cpld->get_version() >> VERSION_DESIGN_BIT) & 0x0F;
            if (cpld_ver != DESIGN_ATOM) {
                cpld->set_value(0, 0);
            }
            if (depth == 0) {
               osd_clear();
               osd_state = IDLE;
            } else {
               depth--;
               if (return_at_end == 0)
                  current_item[depth] = 0;
               osd_clear_no_palette();
               redraw_menu();
            }
            break;
         case I_SAVE: {
            int result = 0;
            int asresult = -1;
            char msg[256];
            char path[256];
            if (has_sub_profiles[get_feature(F_PROFILE)]) {
               asresult = save_profile(profile_names[get_feature(F_PROFILE)], "Default", save_buffer, NULL, NULL);
               result = save_profile(profile_names[get_feature(F_PROFILE)], sub_profile_names[get_feature(F_SUBPROFILE)], save_buffer, default_buffer, sub_default_buffer);
               sprintf(path, "%s/%s.txt", profile_names[get_feature(F_PROFILE)], sub_profile_names[get_feature(F_SUBPROFILE)]);
            } else {
               result = save_profile(NULL, profile_names[get_feature(F_PROFILE)], save_buffer, default_buffer, NULL);
               sprintf(path, "%s.txt", profile_names[get_feature(F_PROFILE)]);
            }
            if (result == 0) {
               sprintf(msg, "Saved: %s", path);
            } else {
               if (result == -1) {
                  if (asresult == 0) {
                     sprintf(msg, "Auto Switching state saved");
                  } else {
                     sprintf(msg, "Not saved (same as default)");
                  }
               } else {
                  sprintf(msg, "Error %d saving file", result);
               }

            }
            set_status_message(msg);
            // Don't re-write profile.txt here - it was
            // already written if the profile was changed
            load_profiles(get_feature(F_PROFILE), 0);
            break;
         }
         case I_RESTORE:
            if (first_time_press == 0) {
                set_status_message("Press again to confirm restore");
                first_time_press = 1;
            } else {
                first_time_press = 0;
                if (has_sub_profiles[get_feature(F_PROFILE)]) {
                   file_restore(profile_names[get_feature(F_PROFILE)], "Default");
                   file_restore(profile_names[get_feature(F_PROFILE)], sub_profile_names[get_feature(F_SUBPROFILE)]);
                } else {
                   file_restore(NULL, profile_names[get_feature(F_PROFILE)]);
                }
                set_feature(F_PROFILE, get_feature(F_PROFILE));
                force_reinit();
            }
            break;
         case I_UPDATE:
            if (first_time_press == 0) {
                char msg[256];
                int major = (cpld->get_version() >> VERSION_MAJOR_BIT) & 0xF;
                int minor = (cpld->get_version() >> VERSION_MINOR_BIT) & 0xF;
                if (major == 0x0f && minor == 0x0f) {
                    sprintf(msg, "Current=BLANK: Confirm?");
                } else {
                    sprintf(msg, "Current=%s v%x.%x: Confirm?", cpld->name, major, minor);
                }
                set_status_message(msg);
                first_time_press = 1;
            } else {
                first_time_press = 0;
                // Generate the CPLD filename from the menu item
                if (get_debug()) {
                    sprintf(filename, "%s/old/%s.xsvf", cpld_firmware_dir, param_item->param->label);
                } else {
                    sprintf(filename, "%s/%s.xsvf", cpld_firmware_dir, param_item->param->label);
                }
                // Reprograme the CPLD
                update_cpld(filename);
            }
            break;
         case I_CALIBRATE:
            if (first_time_press == 0) {
                set_status_message("Press again to confirm calibration");
                first_time_press = 1;
            } else {
                first_time_press = 0;
                osd_clear();
                osd_set(0, ATTR_DOUBLE_SIZE, "Auto Calibration");
                osd_set(1, 0, "Video must be static during calibration");
                action_calibrate_auto();
                delay_in_arm_cycles_cpu_adjust(1500000000);
                osd_clear();
                redraw_menu();
            }
            break;
         }
      } else if (key == key_menu_up) {
         first_time_press = 0;
         // PREVIOUS
         if (current_item[depth] == 0) {
            while (current_menu[depth]->items[current_item[depth]] != NULL)
               current_item[depth]++;
         }
         current_item[depth]--;
         if (last_up_down_key == key_menu_down && get_key_down_duration(last_up_down_key) != 0) {
            redraw_menu();
            capture_screenshot(capinfo, profile_names[get_feature(F_PROFILE)]);
            delay_in_arm_cycles_cpu_adjust(1500000000);
         }
         redraw_menu();
         last_up_down_key = key;
         //osd_state = CAPTURE;
      } else if (key == key_menu_down) {
         first_time_press = 0;
         // NEXT
         current_item[depth]++;
         if (current_menu[depth]->items[current_item[depth]] == NULL) {
            current_item[depth] = 0;
         }
         if (last_up_down_key == key_menu_up && get_key_down_duration(last_up_down_key) != 0) {
            redraw_menu();
            capture_screenshot(capinfo, profile_names[get_feature(F_PROFILE)]);
            delay_in_arm_cycles_cpu_adjust(1500000000);
         }
         redraw_menu();
         last_up_down_key = key;
      }
      break;

   case PARAM:
      type = item->type;
      if (key == key_enter) {
         // ENTER
         osd_state = MENU;
         if  (inhibit_palette_dimming) {
            inhibit_palette_dimming = 0;
            osd_update_palette();
         }
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
         if (type == I_GEOMETRY) {
            switch(param_item->param->key) {
                case CLOCK:
                case LINE_LEN:
                  set_helper_flag();
                  if (geometry_get_value(SETUP_MODE) != SETUP_CLOCK && geometry_get_value(SETUP_MODE) != SETUP_FINE) {
                       geometry_set_value(SETUP_MODE, SETUP_CLOCK);
                  }
                  break;
                case H_ASPECT:
                case V_ASPECT:
                case CLOCK_PPM:
                case LINES_FRAME:
                case SYNC_TYPE:
                  set_helper_flag();
                  break;
                default:
                  break;
            }
         }

      }
      redraw_menu();
      break;

   case INFO:
      if (key == key_enter) {
         // ENTER
         osd_state = MENU;
         osd_clear_no_palette();
         redraw_menu();
      } else if (key == key_menu_up) {
         if (last_up_down_key == key_menu_down && get_key_down_duration(last_up_down_key) != 0) {
            capture_screenshot(capinfo, profile_names[get_feature(F_PROFILE)]);
            delay_in_arm_cycles_cpu_adjust(1500000000);
            redraw_menu();
         }

         last_up_down_key = key;
      } else if (key == key_menu_down) {
         if (last_up_down_key == key_menu_up && get_key_down_duration(last_up_down_key) != 0) {
            capture_screenshot(capinfo, profile_names[get_feature(F_PROFILE)]);
            delay_in_arm_cycles_cpu_adjust(1500000000);
            redraw_menu();
         }
         last_up_down_key = key;
      }
      break;

   default:
      log_warn("Illegal osd state %d reached", osd_state);
      osd_state = IDLE;
   }
   return ret;
}

int get_existing_frontend(int frontend) {
    return all_frontends[frontend];
}

void osd_init() {
   const char *prop = NULL;
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

   for (int i = 0; i <= 0xff; i++)
   {
      unsigned char r;
      unsigned char g;
      unsigned char b;
      unsigned char lum = (i & 0x0f);
      lum |= lum << 4;
      if (i >15) {
         r = (i & 0x10)? lum : 0;
         g = (i & 0x20)? lum : 0;
         b = (i & 0x40)? lum : 0;
      }
      else
      {
         r = (i & 0x1)? lum : 0;
         g = (i & 0x2)? lum : 0;
         b = (i & 0x4)? lum : 0;
      }
      customPalette[i] = ((int)b<<16) | ((int)g<<8) | (int)r;
      paletteHighNibble[i] = i >> 5;
   }

   memset(normal_size_map_4bpp, 0, sizeof(normal_size_map_4bpp));
   memset(double_size_map_4bpp, 0, sizeof(double_size_map_4bpp));
   memset(normal_size_map_8bpp, 0, sizeof(normal_size_map_8bpp));
   memset(double_size_map_8bpp, 0, sizeof(double_size_map_8bpp));
   memset(normal_size_map_16bpp, 0, sizeof(normal_size_map_16bpp));
   memset(double_size_map_16bpp, 0, sizeof(double_size_map_16bpp));

   memset(normal_size_map8_4bpp, 0, sizeof(normal_size_map8_4bpp));
   memset(double_size_map8_4bpp, 0, sizeof(double_size_map8_4bpp));
   memset(normal_size_map8_8bpp, 0, sizeof(normal_size_map8_8bpp));
   memset(double_size_map8_8bpp, 0, sizeof(double_size_map8_8bpp));
   memset(normal_size_map8_16bpp, 0, sizeof(normal_size_map8_16bpp));
   memset(double_size_map8_16bpp, 0, sizeof(double_size_map8_16bpp));

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


            // ======= 4 bits/pixel tables for 8x8 font======

            // Normal size,
            // aaaaaaaa
            if (j < 8) {
               normal_size_map8_4bpp[i       ] |= 0x8 << (4 * (7 - (j ^ 1)));   // aaaaaaaa
            }

            // Double size
            // aaaaaaaa bbbbbbbb
            if (j < 4) {
               double_size_map8_4bpp[i * 2 + 1] |= 0x88 << (8 * (3 - j));  // bbbbbbbb
            } else if (j < 8) {
               double_size_map8_4bpp[i * 2    ] |= 0x88 << (8 * (7 - j));  // aaaaaaaa
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


            // ======= 8 bits/pixel tables for 8x8 font======

            // Normal size
            // aaaaaaaa bbbbbbbb
            if (j < 4) {
               normal_size_map8_8bpp[i * 2 + 1] |= 0x80 << (8 * (3 - j));  // bbbbbbbb
            } else if (j < 8) {
               normal_size_map8_8bpp[i * 2    ] |= 0x80 << (8 * (7 - j));  // aaaaaaaa
            }

            // Double size
            // aaaaaaaa bbbbbbbb cccccccc dddddddd
            if (j < 2) {
               double_size_map8_8bpp[i * 4 + 3] |= 0x8080 << (16 * (1 - j));  // dddddddd
            } else if (j < 4) {
               double_size_map8_8bpp[i * 4 + 2] |= 0x8080 << (16 * (3 - j));  // cccccccc
            } else if (j < 6) {
               double_size_map8_8bpp[i * 4 + 1] |= 0x8080 << (16 * (5 - j));  // bbbbbbbb
            } else if (j < 8) {
               double_size_map8_8bpp[i * 4    ] |= 0x8080 << (16 * (7 - j));  // aaaaaaaa
            }

            // ======= 16 bits/pixel tables ======


            // Normal size

            if (j < 2) {
               normal_size_map_16bpp[i * 6 + 5] |= 0xffff << (16 * (1 - j));
            } else if (j < 4) {
               normal_size_map_16bpp[i * 6 + 4] |= 0xffff << (16 * (3 - j));
            } else if (j < 6) {
               normal_size_map_16bpp[i * 6 + 3] |= 0xffff << (16 * (5 - j));
            } else if (j < 8) {
               normal_size_map_16bpp[i * 6 + 2] |= 0xffff << (16 * (7 - j));
            } else if (j < 10) {
               normal_size_map_16bpp[i * 6 + 1] |= 0xffff << (16 * (9 - j));
            } else {
               normal_size_map_16bpp[i * 6    ] |= 0xffff << (16 * (11 - j));
            }

            // Double size

            if (j < 1) {
               double_size_map_16bpp[i * 12 + 11] |= 0xffffffff;
            } else if (j < 2) {
               double_size_map_16bpp[i * 12 + 10] |= 0xffffffff;
            } else if (j < 3) {
               double_size_map_16bpp[i * 12 + 9] |= 0xffffffff;
            } else if (j < 4) {
               double_size_map_16bpp[i * 12 + 8] |= 0xffffffff;
            } else if (j < 5) {
               double_size_map_16bpp[i * 12 + 7] |= 0xffffffff;
            } else if (j < 6) {
               double_size_map_16bpp[i * 12 + 6] |= 0xffffffff;
            } else if (j < 7) {
               double_size_map_16bpp[i * 12 + 5] |= 0xffffffff;
            } else if (j < 8) {
               double_size_map_16bpp[i * 12 + 4] |= 0xffffffff;
            } else if (j < 9) {
               double_size_map_16bpp[i * 12 + 3] |= 0xffffffff;
            } else if (j < 10) {
               double_size_map_16bpp[i * 12 + 2] |= 0xffffffff;
            } else if (j < 11) {
               double_size_map_16bpp[i * 12 + 1] |= 0xffffffff;
            } else {
               double_size_map_16bpp[i * 12    ] |= 0xffffffff;
            }


            // ======= 16 bits/pixel tables for 8x8 font======
            // Normal size
            // aaaaaaaa bbbbbbbb
            if (j < 2) {
               normal_size_map8_16bpp[i * 4 + 3] |= 0xffff << (16 * (1 - j));
            } else if (j < 4) {
               normal_size_map8_16bpp[i * 4 + 2] |= 0xffff << (16 * (3 - j));
            } else if (j < 6) {
               normal_size_map8_16bpp[i * 4 + 1] |= 0xffff << (16 * (5 - j));
            } else {
               normal_size_map8_16bpp[i * 4    ] |= 0xffff << (16 * (7 - j));
            }


            // Double size
            // aaaaaaaa bbbbbbbb cccccccc dddddddd
            if (j < 1) {
               double_size_map8_16bpp[i * 8 + 7] |= 0xffffffff;
            } else if (j < 2) {
               double_size_map8_16bpp[i * 8 + 6] |= 0xffffffff;
            } else if (j < 3) {
               double_size_map8_16bpp[i * 8 + 5] |= 0xffffffff;
            } else if (j < 4) {
               double_size_map8_16bpp[i * 8 + 4] |= 0xffffffff;
            } else if (j < 5) {
               double_size_map8_16bpp[i * 8 + 3] |= 0xffffffff;
            } else if (j < 6) {
               double_size_map8_16bpp[i * 8 + 2] |= 0xffffffff;
            } else if (j < 7) {
               double_size_map8_16bpp[i * 8 + 1] |= 0xffffffff;
            } else {
               double_size_map8_16bpp[i * 8    ] |= 0xffffffff;
            }
         }
      }
   }

   generate_palettes();
   features[F_PALETTE].max  = create_and_scan_palettes(palette_names, palette_array) - 1;

   // default resolution entry of not found
   features[F_RESOLUTION].max = 0;
   strcpy(resolution_names[0], NOT_FOUND_STRING);
   size_t rcount = 0;
   scan_names(resolution_names, "/Resolutions", ".txt", &rcount);
   if (rcount !=0) {
      features[F_RESOLUTION].max = rcount - 1;
      for (int i = 0; i < rcount; i++) {
         log_info("FOUND RESOLUTION: %s", resolution_names[i]);
      }
   }

   int cbytes = file_load("/config.txt", config_buffer, MAX_CONFIG_BUFFER_SIZE);

   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "#resolution");
      log_info("Read resolution: %s", prop);
   }
   if (!prop || !cbytes) {
      prop = "Default@60Hz";
   }
   for (int i=0; i< rcount; i++) {
      if (strcmp(resolution_names[i], prop) == 0) {
         log_info("Match resolution: %d %s", i, prop);
         set_resolution(i, prop, 0);
         break;
      }
   }
   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "#scaling");
      log_info("Read scaling: %s", prop);
   }
   if (!prop || !cbytes) {
      prop = "0";
   }
   int val = atoi(prop);
   set_scaling(val, 0);

   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "scaling_kernel");
      log_info("Read scaling_kernel: %s", prop);
   }
   if (!prop || !cbytes) {
      prop = "0";
   }
   val = atoi(prop);
   set_filtering(val);


   if (cbytes) {
       char frontname[256];
       for (int i=0; i< 16; i++) {
           sprintf(frontname, "#interface_%X", i);
           prop = get_prop_no_space(config_buffer, frontname);
           if (!prop) {
               prop = "0";
           } else {
               log_info("Read interface_%X = %s", i, prop);
           }
           all_frontends[i] = atoi(prop);
       }
   }
   set_frontend(all_frontends[(cpld->get_version() >> VERSION_DESIGN_BIT) & 0x0F], 0);

   // default profile entry of not found
   features[F_PROFILE].max = 0;
   strcpy(profile_names[0], NOT_FOUND_STRING);
   default_buffer[0] = 0;
   has_sub_profiles[0] = 0;

   char path[100] = "/Profiles/";
   strncat(path, cpld->name, 80);
   char name[100];

   // pre-read default profile
   unsigned int bytes = file_read_profile(DEFAULT_STRING, NULL, 0, default_buffer, MAX_BUFFER_SIZE - 4);
   if (bytes != 0) {
      size_t count = 0;
      scan_profiles(profile_names, has_sub_profiles, path, &count);
      if (count != 0) {
         features[F_PROFILE].max = count - 1;
         for (int i = 0; i < count; i++) {
            if (has_sub_profiles[i]) {
               log_info("FOUND SUB-FOLDER: %s", profile_names[i]);
            } else {
               log_info("FOUND PROFILE: %s", profile_names[i]);
            }
         }
         // The default profile is provided by the CPLD
         prop = cpld->default_profile;
         // It can be over-ridden by a local profile.txt file
         sprintf(name, "/profile_%s.txt", cpld->name);
         cbytes = file_load(name, config_buffer, MAX_CONFIG_BUFFER_SIZE);
         if (cbytes) {
            prop = get_prop_no_space(config_buffer, "profile");
         }
         int found_profile = 0;
         if (prop) {
            for (int i=0; i<count; i++) {
               if (strcmp(profile_names[i], prop) == 0) {
                  set_profile(i);
                  load_profiles(i, 0);
                  process_profile(i);
                  set_feature(F_SUBPROFILE, 0);
                  log_info("Profile = %s", prop);
                  found_profile = 1;
                  break;
               }
            }
            if (!found_profile) {
                  set_profile(0);
                  load_profiles(0, 0);
                  process_profile(0);
                  set_feature(F_SUBPROFILE, 0);
            }

         }
      }
   }
}

void osd_update(uint32_t *osd_base, int bytes_per_line) {
   if (!active) {
      return;
   }
   if (capinfo->bpp == 16) {
       if (capinfo->video_type == VIDEO_INTERLACED && (capinfo->sync_type & SYNC_BIT_INTERLACED) && get_deinterlace() == DEINTERLACE_NONE) {
           clear_full_screen();
       }
   }

   // SAA5050 character data is 12x20
   int bufferCharWidth = (capinfo->chars_per_line << 3) / 12;         // SAA5050 character data is 12x20
   if (capinfo->sample_width == SAMPLE_WIDTH_6 && capinfo->bpp == 16) { //special double rate 6 bpp mode
         bufferCharWidth >>= 1;
   }
   uint32_t *line_ptr = osd_base;
   int words_per_line = bytes_per_line >> 2;
   int allow1220font = 0;
   switch (get_feature(F_FONTSIZE)) {
   case FONTSIZE_12X20_4:
      if (capinfo->bpp < 8) {
         allow1220font = 1;
      }
      break;
   case FONTSIZE_12X20_8:
      allow1220font = 1;
      break;
   }

   if (((capinfo->sizex2 & 1) && capinfo->nlines >  FONT_THRESHOLD * 10)  && (bufferCharWidth >= LINELEN) && allow1220font) {       // if frame buffer is large enough and not 8bpp use SAA5050 font
      for (int line = 0; line <= osd_hwm; line++) {
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
               switch (capinfo->bpp) {
                   case 4:
                      if (attr & ATTR_DOUBLE_SIZE) {
                         // Map to three 32-bit words in frame buffer format
                         uint32_t *map_ptr = double_size_map_4bpp + data * 3;
                         *word_ptr &= 0x77777777;
                         *word_ptr |= *map_ptr;
                         *(word_ptr + words_per_line) &= 0x77777777;
                         *(word_ptr + words_per_line) |= *map_ptr;
                         word_ptr++;
                         map_ptr++;
                         *word_ptr &= 0x77777777;
                         *word_ptr |= *map_ptr;
                         *(word_ptr + words_per_line) &= 0x77777777;
                         *(word_ptr + words_per_line) |= *map_ptr;
                         word_ptr++;
                         map_ptr++;
                         *word_ptr &= 0x77777777;
                         *word_ptr |= *map_ptr;
                         *(word_ptr + words_per_line) &= 0x77777777;
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
                   break;
                   default:
                   case 8:
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
                   break;
                   case 16:
                      if (attr & ATTR_DOUBLE_SIZE) {
                         uint32_t *map_ptr = double_size_map_16bpp + data * 12;
                         for (int k = 0; k < 12; k++) {
                            *word_ptr |= *map_ptr;
                            *(word_ptr + words_per_line) |= *map_ptr;
                            word_ptr++;
                            map_ptr++;
                         }
                      } else {
                         uint32_t *map_ptr = normal_size_map_16bpp + data * 6;
                         for (int k = 0; k < 6; k++) {
                            *word_ptr |= *map_ptr;
                            word_ptr++;
                            map_ptr++;
                         }
                      }
                   break;
               }
            }
            if (attr & ATTR_DOUBLE_SIZE) {
               line_ptr += 2 * words_per_line;
            } else {
               line_ptr += words_per_line;
            }
         }
      }

   } else {
      // use 8x8 fonts for smaller frame buffer
      for (int line = 0; line <= osd_hwm; line++) {
         int attr = attributes[line];
         int len = (attr & ATTR_DOUBLE_SIZE) ? (LINELEN >> 1) : LINELEN;
         for (int y = 0; y < 8; y++) {
            uint32_t *word_ptr = line_ptr;
            for (int i = 0; i < len; i++) {
               int c = buffer[line * LINELEN + i];
               // Deal with unprintable characters
               if (c < 32 || c > 127) {
                  c = 32;
               }
               // Character row is 12 pixels
               int data = (int) fontdata8[8 * c + y];
               // Map to the screen pixel format
               switch (capinfo->bpp) {
                   case 4:
                      if (attr & ATTR_DOUBLE_SIZE) {
                         // Map to three 32-bit words in frame buffer format
                         uint32_t *map_ptr = double_size_map8_4bpp + data * 2;
                         *word_ptr &= 0x77777777;
                         *word_ptr |= *map_ptr;
                         *(word_ptr + words_per_line) &= 0x77777777;
                         *(word_ptr + words_per_line) |= *map_ptr;
                         word_ptr++;
                         map_ptr++;
                         *word_ptr &= 0x77777777;
                         *word_ptr |= *map_ptr;
                         *(word_ptr + words_per_line) &= 0x77777777;
                         *(word_ptr + words_per_line) |= *map_ptr;
                         word_ptr++;
                         map_ptr++;
                      } else {
                         // Map to one 32-bit words in frame buffer format
                         uint32_t *map_ptr = normal_size_map8_4bpp + data;
                         *word_ptr &= 0x77777777;
                         *word_ptr |= *map_ptr;
                         word_ptr++;
                         map_ptr++;
                      }
                   break;
                   default:
                   case 8:
                      if (attr & ATTR_DOUBLE_SIZE) {
                         uint32_t *map_ptr = double_size_map8_8bpp + data * 4;
                         for (int k = 0; k < 4; k++) {
                            *word_ptr &= 0x7f7f7f7f;
                            *word_ptr |= *map_ptr;
                            *(word_ptr + words_per_line) &= 0x7f7f7f7f;
                            *(word_ptr + words_per_line) |= *map_ptr;
                            word_ptr++;
                            map_ptr++;
                         }
                      } else {
                         uint32_t *map_ptr = normal_size_map8_8bpp + data * 2;
                         for (int k = 0; k < 2; k++) {
                            *word_ptr &= 0x7f7f7f7f;
                            *word_ptr |= *map_ptr;
                            word_ptr++;
                            map_ptr++;
                         }
                      }
                   break;
                   case 16:
                      if (attr & ATTR_DOUBLE_SIZE) {
                         uint32_t *map_ptr = double_size_map8_16bpp + data * 8;
                         for (int k = 0; k < 8; k++) {
                            *word_ptr |= *map_ptr;
                            *(word_ptr + words_per_line) |= *map_ptr;
                            word_ptr++;
                            map_ptr++;
                         }
                      } else {
                         uint32_t *map_ptr = normal_size_map8_16bpp + data * 4;
                         for (int k = 0; k < 4; k++) {
                            *word_ptr |= *map_ptr;
                            word_ptr++;
                            map_ptr++;
                         }
                      }

                   break;
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
   if (capinfo->bpp == 16 && capinfo->video_type == VIDEO_INTERLACED && (capinfo->detected_sync_type & SYNC_BIT_INTERLACED) && get_deinterlace() == DEINTERLACE_NONE) {
      clear_screen();
   }
   // SAA5050 character data is 12x20
   int bufferCharWidth = (capinfo->chars_per_line << 3) / 12;         // SAA5050 character data is 12x20
   if (capinfo->sample_width == SAMPLE_WIDTH_6 && capinfo->bpp == 16) { //special double rate 6 bpp mode
         bufferCharWidth >>= 1;
   }
   uint32_t *line_ptr = osd_base;
   int words_per_line = bytes_per_line >> 2;
   int allow1220font = 0;
   switch (get_feature(F_FONTSIZE)) {
   case FONTSIZE_12X20_4:
      if (capinfo->bpp < 8) {
         allow1220font = 1;
      }
      break;
   case FONTSIZE_12X20_8:
      allow1220font = 1;
      break;
   }

   if (((capinfo->sizex2 & 1) && capinfo->nlines > FONT_THRESHOLD * 10)  && (bufferCharWidth >= LINELEN) && allow1220font) {       // if frame buffer is large enough and not 8bpp use SAA5050 font
      for (int line = 0; line <= osd_hwm; line++) {
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
               switch (capinfo->bpp) {
                   case 4:
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
                   break;
                   default:
                   case 8:
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
                   break;
                   case 16:
                      if (attr & ATTR_DOUBLE_SIZE) {
                         uint32_t *map_ptr = double_size_map_16bpp + data * 12;
                         for (int k = 0; k < 12; k++) {
                            *word_ptr |= *map_ptr;
                            *(word_ptr + words_per_line) |= *map_ptr;
                            word_ptr++;
                            map_ptr++;
                         }
                      } else {
                         uint32_t *map_ptr = normal_size_map_16bpp + data * 6;
                         for (int k = 0; k < 6; k++) {
                            *word_ptr |= *map_ptr;
                            word_ptr++;
                            map_ptr++;
                         }
                      }

                   break;
               }
            }
            if (attr & ATTR_DOUBLE_SIZE) {
               line_ptr += 2 * words_per_line;
            } else {
               line_ptr += words_per_line;
            }
         }
      }

   } else {

      // use 8x8 fonts for smaller frame buffer
      for (int line = 0; line <= osd_hwm; line++) {
         int attr = attributes[line];
         int len = (attr & ATTR_DOUBLE_SIZE) ? (LINELEN >> 1) : LINELEN;
         for (int y = 0; y < 8; y++) {
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
               // Character row is 8 pixels
               int data = (int) fontdata8[8 * c + y];
               // Map to the screen pixel format
               switch (capinfo->bpp) {
                   case 4:
                      if (attr & ATTR_DOUBLE_SIZE) {
                         // Map to two 32-bit words in frame buffer format
                         uint32_t *map_ptr = double_size_map8_4bpp + data * 2;
                         *word_ptr |= *map_ptr;
                         *(word_ptr + words_per_line) |= *map_ptr;
                         word_ptr++;
                         map_ptr++;
                         *word_ptr |= *map_ptr;
                         *(word_ptr + words_per_line) |= *map_ptr;
                         word_ptr++;
                      } else {
                         // Map to one 32-bit words in frame buffer format
                         uint32_t *map_ptr = normal_size_map8_4bpp + data;
                         *word_ptr |= *map_ptr;
                         word_ptr++;
                         map_ptr++;
                      }
                   break;
                   default:
                   case 8:
                      if (attr & ATTR_DOUBLE_SIZE) {
                         uint32_t *map_ptr = double_size_map8_8bpp + data * 4;
                         for (int k = 0; k < 4; k++) {
                            *word_ptr |= *map_ptr;
                            *(word_ptr + words_per_line) |= *map_ptr;
                            word_ptr++;
                            map_ptr++;
                         }
                      } else {
                         uint32_t *map_ptr = normal_size_map8_8bpp + data * 2;
                         for (int k = 0; k < 2; k++) {
                            *word_ptr |= *map_ptr;
                            word_ptr++;
                            map_ptr++;
                         }
                      }
                   break;
                   case 16:
                      if (attr & ATTR_DOUBLE_SIZE) {
                         uint32_t *map_ptr = double_size_map8_16bpp + data * 8;
                         for (int k = 0; k < 8; k++) {
                            *word_ptr |= *map_ptr;
                            *(word_ptr + words_per_line) |= *map_ptr;
                            word_ptr++;
                            map_ptr++;
                         }
                      } else {
                         uint32_t *map_ptr = normal_size_map8_16bpp + data * 4;
                         for (int k = 0; k < 4; k++) {
                            *word_ptr |= *map_ptr;
                            word_ptr++;
                            map_ptr++;
                         }
                      }
                   break;
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
}
