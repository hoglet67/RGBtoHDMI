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
#include "startup.h"
#include "vid_cga_comp.h"
#include <math.h>

// =============================================================
// Definitions for the size of the OSD
// =============================================================

#define NLINES         30

#define FONT_THRESHOLD    25

#define LINELEN        42

#define MAX_MENU_DEPTH  4

#define DEFAULT_CPLD_FIRMWARE_DIR "/cpld_firmware/recovery"
#define DEFAULT_CPLD_FIRMWARE_DIR12 "/cpld_firmware/recovery12"
#define DEFAULT_CPLD_UPDATE_DIR "/cpld_firmware/6-12_bit"
#define DEFAULT_CPLD_UPDATE_DIR_3BIT "/cpld_firmware/3_bit"
#define DEFAULT_CPLD_UPDATE_DIR_ATOM "/cpld_firmware/atom"
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
   A1_CAPTURE_SUB2,    // Action 1: Screen capture
   CLOCK_CAL0,    // Intermediate state in clock calibration
   CLOCK_CAL1,    // Intermediate state in clock calibration
   AUTO_CAL_SAVE, // saving auto calibration result

   NTSC_MESSAGE,
   NTSC_PHASE_MESSAGE,
   TIMINGSET_MESSAGE,
   MENU,           // Browsing a menu
   MENU_SUB,       // Browsing a menu
   PARAM,          // Changing the value of a menu item
   PARAM_SUB,      // Changing the value of a menu item
   INFO            // Viewing an info panel
} osd_state_t;

// =============================================================
// Friendly names for certain OSD feature values
// =============================================================


static char *default_palette_names[] = {
   "RGB",
   "RGBI",
   "RGBI_(CGA)",
   "RGBI_(XRGB-NTSC)",
   "RGBI_(Laser)",
   "RGBI_(Spectrum)",
   "RGBrgb_(Spectrum)",
   "RGBrgb_(Amstrad)",
   "RrGgBb_(EGA)",
   "RrGgBbI_(SAM)",
   "MDA-Hercules",
   "Dragon-CoCo_Black",
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
   "C64_LumaCode",
   "C64_LumaCode_Rev1",
   "Atari_800_PAL",
   "Atari_800_NTSC",
   "Tea1002",
   "Intellivision",
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
   "PAL Artifact",
   "Atari GTIA",
   "C64 Lumacode"
};

static const char *return_names[] = {
   "Start",
   "End"
};

static const char *genlock_mode_names[] = {
   "On (Locked)",
   "Off (Unlocked)",
   "1000ppm Fast",
   "2000ppm Fast",
   "2000ppm Slow",
   "1000ppm Slow"
};

static const char *deinterlace_names[] = {
   "Weave",
   "Simple Bob",
   "Simple Motion 1",
   "Simple Motion 2",
   "Simple Motion 3",
   "Simple Motion 4",
   "Advanced Motion"
};

#ifdef MULTI_BUFFER
static const char *nbuffer_names[] = {
   "1",
   "2",
   "3",
   "4"
};
#endif

static const char *autoswitch_names[] = {
   "Off",
   "Sub-Profile Only",
   "Sub + BBC Mode 7",
   "Sub + Vsync",
   "Sub + Apple IIGS",
   "Sub + IIGS + Manual",
   "Sub + Manual"
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
   "Integer/Sharp",
   "Integer/Soft",
   "Integer/Softer",
   "Interpolate 4:3/Soft",
   "Interpolate 4:3/Softer",
   "Interpolate Full/Soft",
   "Interpolate Full/Softer"
};

static const char *frontend_names_6[] = {
   "3 Bit Digital RGB(TTL)",
   "12 Bit Simple",
   "Atom",
   "6 Bit Digital RGB(TTL)",
   "6 Bit Digital YUV(TTL)",
   "6 Bit Analog RGB Issue 3",
   "6 Bit Analog RGB Issue 2",
   "6 Bit Analog RGB Issue 1A",
   "6 Bit Analog RGB Issue 1B",
   "6 Bit Analog RGB Issue 4",
   "6 Bit Analog RGB Issue 5",
   "6 Bit Analog YUV Issue 3",
   "6 Bit Analog YUV Issue 2",
   "6 Bit Analog YUV Issue 4",
   "6 Bit Analog YUV Issue 5"
};

static const char *frontend_names_8[] = {
   "3 Bit Digital RGB(TTL)",
   "12 Bit Simple",
   "Atom",
   "8/12 Bit Digital RGB(TTL)",
   "8 Bit Digital YUV(TTL)",
   "8 Bit Analog RGB Issue 3",
   "8 Bit Analog RGB Issue 2",
   "8 Bit Analog RGB Issue 1A",
   "8 Bit Analog RGB Issue 1B",
   "8 Bit Analog RGB Issue 4",
   "8 Bit Analog RGB Issue 5",
   "8 Bit Analog YUV Issue 3",
   "8 Bit Analog YUV Issue 2",
   "8 Bit Analog YUV Issue 4",
   "8 Bit Analog YUV Issue 5"
};

static const char *genlock_speed_names[] = {
   "Slow (333PPM)",
   "Medium (1000PPM)",
   "Fast (2000PPM)"
};

static const char *genlock_adjust_names[] = {
   "-5% to +5%",
   "Unlimited"
};

static const char *fontsize_names[] = {
   "Always 8x8",
   "Auto 12x20 or 8x8"
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

static const char *phase_names[] = {
   "0 Deg",
   "90 Deg",
   "180 Deg",
   "270 Deg"
};

static const char *fringe_names[] = {
   "Normal",
   "Sharp",
   "Soft"
};

static const char *ntsctype_names[] = {
   "New CGA (Zero 2W)",
   "Old CGA (Zero 2W)",
   "Apple (Zero 2W)",
   "Simple (Old Code)"
};

static const char *hdmi_names[] = {
   "DVI Compatible",
   "HDMI (Auto RGB/YUV)",
   "HDMI (RGB limited)",
   "HDMI (RGB full)",
   "HDMI (YUV limited)",
   "HDMI (YUV full)"
};

static const char *refresh_names[] = {
   "60Hz",
   "EDID 50Hz-60Hz",
   "Force 50Hz-60Hz",
   "Force 50Hz-Any",
   "50Hz"
};

static const char *saved_config_names[] = {
   "Primary",
   "Alt 1",
   "Alt 2",
   "Alt 3",
   "Alt 4"
};

static const char *integer_names[] = {
   "Normal",
   "Maximise"
};


static const char *alt_profile_names[] = {
   "Set 1",
   "Set 2"
};


// =============================================================
// Feature definitions
// =============================================================



static param_t features[] = {
   {        F_AUTO_SWITCH,       "Auto Switch",       "auto_switch", 0, NUM_AUTOSWITCHES - 1, 1 },
   {         F_RESOLUTION,        "Resolution",        "resolution", 0,                    0, 1 },
   {            F_REFRESH,           "Refresh",           "refresh", 0,      NUM_REFRESH - 1, 1 },
   {          F_HDMI_MODE,         "HDMI Mode",         "hdmi_mode", 0,        NUM_HDMIS - 1, 1 },
   {  F_HDMI_MODE_STANDBY, "HDMI Grey Standby",      "hdmi_standby", 0,                    1, 1 },
   {            F_SCALING,           "Scaling",           "scaling", 0,      NUM_SCALING - 1, 1 },
   {            F_PROFILE,           "Profile",           "profile", 0,                    0, 1 },
   {       F_SAVED_CONFIG,      "Saved Config",      "saved_config", 0,                    4, 1 },
   {        F_SUB_PROFILE,       "Sub-Profile",        "subprofile", 0,                    0, 1 },
   {            F_PALETTE,           "Palette",           "palette", 0,                    0, 1 },
   {    F_PALETTE_CONTROL,    "Palette Control",   "palette_control", 0,     NUM_CONTROLS - 1, 1 },
   {        F_NTSC_COLOUR,    "Artifact Colour",       "ntsc_colour", 0,                    1, 1 },
   {         F_NTSC_PHASE,     "Artifact Phase",        "ntsc_phase", 0,                    3, 1 },
   {          F_NTSC_TYPE,      "Artifact Type",         "ntsc_type", 0,     NUM_NTSCTYPE - 1, 1 },
   {       F_NTSC_QUALITY,   "Artifact Quality",      "ntsc_quality", 0,       NUM_FRINGE - 1, 1 },
   {               F_TINT,               "Tint",              "tint",-60,                  60, 1 },
   {                F_SAT,         "Saturation",        "saturation", 0,                  200, 1 },
   {                F_CONT,           "Contrast",          "contrast", 0,                  200, 1 },
   {              F_BRIGHT,         "Brightness",        "brightness", 0,                  200, 1 },
   {               F_GAMMA,              "Gamma",             "gamma", 10,                 300, 1 },
   {          F_TIMING_SET,         "Timing Set",        "timing_set", 0,                    1, 1 },
   {   F_MODE7_DEINTERLACE,"Teletext Deinterlace","teletext_deinterlace", 0, NUM_M7DEINTERLACES - 1, 1 },
   {  F_NORMAL_DEINTERLACE, "Normal Deinterlace",   "normal_deinterlace", 0,   NUM_DEINTERLACES - 1, 1 },
   {       F_MODE7_SCALING,  "Teletext Scaling",  "teletext_scaling", 0,    NUM_ESCALINGS - 1, 1 },
   {      F_NORMAL_SCALING,    "Normal Scaling",    "normal_scaling", 0,    NUM_ESCALINGS - 1, 1 },
   {               F_FFOSD,     "FFOSD Overlay",     "ffosd_overlay", 0,                    1, 1 },
   {         F_SWAP_ASPECT,"Swap Aspect 625<>525",     "swap_aspect", 0,                    1, 1 },
   {       F_OUTPUT_COLOUR,     "Output Colour",     "output_colour", 0,      NUM_COLOURS - 1, 1 },
   {       F_OUTPUT_INVERT,     "Output Invert",     "output_invert", 0,       NUM_INVERT - 1, 1 },
   {           F_SCANLINES,         "Scanlines",         "scanlines", 0,                    1, 1 },
   {      F_SCANLINE_LEVEL,    "Scanline Level",    "scanline_level", 0,                   14, 1 },
   {         F_CROP_BORDER,"Crop Border (Zoom)",       "crop_border", 0,     NUM_OVERSCAN - 1, 1 },
   {      F_SCREENCAP_SIZE,    "ScreenCap Size",    "screencap_size", 0,    NUM_SCREENCAP - 1, 1 },
   {           F_FONT_SIZE,         "Font Size",         "font_size", 0,     NUM_FONTSIZE - 1, 1 },
   {       F_BORDER_COLOUR,     "Border Colour",     "border_colour", 0,                  255, 1 },
   {     F_VSYNC_INDICATOR,  "V Sync Indicator",   "vsync_indicator", 0,                    1, 1 },
   {        F_GENLOCK_MODE,       "Genlock Mode",     "genlock_mode", 0,         NUM_HDMI - 1, 1 },
   {        F_GENLOCK_LINE,      "Genlock Line",      "genlock_line",35,                  312, 1 },
   {       F_GENLOCK_SPEED,     "Genlock Speed",     "genlock_speed", 0,NUM_GENLOCK_SPEED - 1, 1 },
   {      F_GENLOCK_ADJUST,    "Genlock Adjust",    "genlock_adjust", 0,NUM_GENLOCK_ADJUST - 1, 1 },
#ifdef MULTI_BUFFER
   {         F_NUM_BUFFERS,       "Num Buffers",       "num_buffers", 0,                    3, 1 },
#endif
   {     F_RETURN_POSITION,   "Return Position",            "return", 0,                    1, 1 },
   {               F_DEBUG,             "Debug",             "debug", 0,                    1, 1 },
   {      F_BUTTON_REVERSE,    "Button Reverse",    "button_reverse", 0,                    1, 1 },

   {       F_OVERCLOCK_CPU,     "Overclock CPU",     "overclock_cpu", 0,                  200, 1 },
   {      F_OVERCLOCK_CORE,    "Overclock Core",    "overclock_core", 0,                  200, 1 },
   {     F_OVERCLOCK_SDRAM,   "Overclock SDRAM",   "overclock_sdram", 0,                  200, 1 },
   {     F_POWERUP_MESSAGE,   "Powerup Message",   "powerup_message", 0,                    1, 1 },

   {    F_YUV_PIXEL_DOUBLE,  "YUV Pixel Double",  "yuv_pixel_double", 0,                    1, 1 },
   {      F_INTEGER_ASPECT,    "Integer Aspect",    "integer_aspect", 0,                    1, 1 },

   {            F_FRONTEND,         "Interface",         "interface", 0,    NUM_FRONTENDS - 1, 1 },
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
   I_CALIBRATE,// Item is a calibration update
   I_TEST      // Item is a 50 Hz test option
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
static void info_save_log(int line);
static void info_credits(int line);
static void info_reboot(int line);

static void info_test_50hz(int line);
static void rebuild_geometry_menu(menu_t *menu);
static void rebuild_sampling_menu(menu_t *menu);
static void rebuild_update_cpld_menu(menu_t *menu);

static info_menu_item_t source_summary_ref   = { I_INFO, "Source Summary",      info_source_summary};
static info_menu_item_t system_summary_ref   = { I_INFO, "System Summary",      info_system_summary};
static info_menu_item_t cal_summary_ref      = { I_INFO, "Calibration Summary", info_cal_summary};
static info_menu_item_t cal_detail_ref       = { I_INFO, "Calibration Detail",  info_cal_detail};
static info_menu_item_t cal_raw_ref          = { I_INFO, "Calibration Raw",     info_cal_raw};
static info_menu_item_t save_log_ref         = { I_INFO, "Save Log & EDID",     info_save_log};
static info_menu_item_t credits_ref          = { I_INFO, "Credits",             info_credits};
static info_menu_item_t reboot_ref           = { I_INFO, "Reboot",              info_reboot};

static back_menu_item_t back_ref             = { I_BACK, "Return"};
static action_menu_item_t save_ref           = { I_SAVE, "Save Configuration"};
static action_menu_item_t restore_ref        = { I_RESTORE, "Restore Default Configuration"};
static action_menu_item_t cal_sampling_ref   = { I_CALIBRATE, "Auto Calibrate Video Sampling"};
static info_menu_item_t test_50hz_ref        = { I_TEST, "Test Monitor for 50Hz Support",  info_test_50hz};

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




static param_menu_item_t profile_ref         = { I_FEATURE, &features[F_PROFILE]        };
static param_menu_item_t saved_ref           = { I_FEATURE, &features[F_SAVED_CONFIG]          };
static param_menu_item_t subprofile_ref      = { I_FEATURE, &features[F_SUB_PROFILE]     };
static param_menu_item_t resolution_ref      = { I_FEATURE, &features[F_RESOLUTION]     };
static param_menu_item_t refresh_ref         = { I_FEATURE, &features[F_REFRESH]        };
static param_menu_item_t hdmi_ref            = { I_FEATURE, &features[F_HDMI_MODE]           };
static param_menu_item_t hdmi_standby_ref    = { I_FEATURE, &features[F_HDMI_MODE_STANDBY]   };
static param_menu_item_t scaling_ref         = { I_FEATURE, &features[F_SCALING]        };
static param_menu_item_t overscan_ref        = { I_FEATURE, &features[F_CROP_BORDER]       };
static param_menu_item_t capscale_ref        = { I_FEATURE, &features[F_SCREENCAP_SIZE]       };
static param_menu_item_t border_ref          = { I_FEATURE, &features[F_BORDER_COLOUR]         };
static param_menu_item_t palettecontrol_ref  = { I_FEATURE, &features[F_PALETTE_CONTROL] };
static param_menu_item_t ntsccolour_ref      = { I_FEATURE, &features[F_NTSC_COLOUR]     };
static param_menu_item_t ntscphase_ref       = { I_FEATURE, &features[F_NTSC_PHASE]      };
static param_menu_item_t ntsctype_ref        = { I_FEATURE, &features[F_NTSC_TYPE]       };
static param_menu_item_t ntscfringe_ref      = { I_FEATURE, &features[F_NTSC_QUALITY]     };
static param_menu_item_t tint_ref            = { I_FEATURE, &features[F_TINT]           };
static param_menu_item_t sat_ref             = { I_FEATURE, &features[F_SAT]            };
static param_menu_item_t cont_ref            = { I_FEATURE, &features[F_CONT]           };
static param_menu_item_t bright_ref          = { I_FEATURE, &features[F_BRIGHT]         };
static param_menu_item_t gamma_ref           = { I_FEATURE, &features[F_GAMMA]          };
static param_menu_item_t timingset_ref       = { I_FEATURE, &features[F_TIMING_SET]      };
static param_menu_item_t palette_ref         = { I_FEATURE, &features[F_PALETTE]        };
static param_menu_item_t m7deinterlace_ref   = { I_FEATURE, &features[F_MODE7_DEINTERLACE]  };
static param_menu_item_t deinterlace_ref     = { I_FEATURE, &features[F_NORMAL_DEINTERLACE]    };
static param_menu_item_t m7scaling_ref       = { I_FEATURE, &features[F_MODE7_SCALING]      };
static param_menu_item_t normalscaling_ref   = { I_FEATURE, &features[F_NORMAL_SCALING]  };
static param_menu_item_t ffosd_ref           = { I_FEATURE, &features[F_FFOSD]          };
static param_menu_item_t stretch_ref         = { I_FEATURE, &features[F_SWAP_ASPECT]        };
static param_menu_item_t scanlines_ref       = { I_FEATURE, &features[F_SCANLINES]      };
static param_menu_item_t scanlinesint_ref    = { I_FEATURE, &features[F_SCANLINE_LEVEL]   };
static param_menu_item_t colour_ref          = { I_FEATURE, &features[F_OUTPUT_COLOUR]         };
static param_menu_item_t invert_ref          = { I_FEATURE, &features[F_OUTPUT_INVERT]         };
static param_menu_item_t fontsize_ref        = { I_FEATURE, &features[F_FONT_SIZE]       };
static param_menu_item_t vsync_ref           = { I_FEATURE, &features[F_VSYNC_INDICATOR]          };
static param_menu_item_t genlock_mode_ref    = { I_FEATURE, &features[F_GENLOCK_MODE]      };
static param_menu_item_t genlock_line_ref    = { I_FEATURE, &features[F_GENLOCK_LINE]      };
static param_menu_item_t genlock_speed_ref   = { I_FEATURE, &features[F_GENLOCK_SPEED]     };
static param_menu_item_t genlock_adjust_ref  = { I_FEATURE, &features[F_GENLOCK_ADJUST]       };
#ifdef MULTI_BUFFER
static param_menu_item_t nbuffers_ref        = { I_FEATURE, &features[F_NUM_BUFFERS]       };
#endif
static param_menu_item_t autoswitch_ref      = { I_FEATURE, &features[F_AUTO_SWITCH]     };
static param_menu_item_t return_ref          = { I_FEATURE, &features[F_RETURN_POSITION]         };
static param_menu_item_t debug_ref           = { I_FEATURE, &features[F_DEBUG]          };
static param_menu_item_t direction_ref       = { I_FEATURE, &features[F_BUTTON_REVERSE]      };
static param_menu_item_t oclock_cpu_ref      = { I_FEATURE, &features[F_OVERCLOCK_CPU]     };
static param_menu_item_t oclock_core_ref     = { I_FEATURE, &features[F_OVERCLOCK_CORE]    };
static param_menu_item_t oclock_sdram_ref    = { I_FEATURE, &features[F_OVERCLOCK_SDRAM]   };
static param_menu_item_t res_status_ref      = { I_FEATURE, &features[F_POWERUP_MESSAGE]        };
static param_menu_item_t yuv_pixel_ref       = { I_FEATURE, &features[F_YUV_PIXEL_DOUBLE]      };
static param_menu_item_t aspect_ref          = { I_FEATURE, &features[F_INTEGER_ASPECT]         };
#ifndef HIDE_INTERFACE_SETTING
static param_menu_item_t frontend_ref        = { I_FEATURE, &features[F_FRONTEND]       };
#endif

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
      (base_menu_item_t *) &save_log_ref,
      (base_menu_item_t *) &credits_ref,
#ifndef HIDE_INTERFACE_SETTING
      (base_menu_item_t *) &frontend_ref,
#endif
      (base_menu_item_t *) &reboot_ref,
      NULL
   }
};

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
      (base_menu_item_t *) &ntsctype_ref,
      (base_menu_item_t *) &ntscfringe_ref,
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
      (base_menu_item_t *) &stretch_ref,
      (base_menu_item_t *) &overscan_ref,
      (base_menu_item_t *) &m7deinterlace_ref,
      (base_menu_item_t *) &deinterlace_ref,
      (base_menu_item_t *) &m7scaling_ref,
      (base_menu_item_t *) &normalscaling_ref,
      (base_menu_item_t *) &capscale_ref,
      (base_menu_item_t *) &yuv_pixel_ref,
      (base_menu_item_t *) &aspect_ref,
      (base_menu_item_t *) &res_status_ref,
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
      (base_menu_item_t *) &genlock_mode_ref,
      (base_menu_item_t *) &genlock_line_ref,
      (base_menu_item_t *) &genlock_speed_ref,
      (base_menu_item_t *) &genlock_adjust_ref,
      (base_menu_item_t *) &nbuffers_ref,
      (base_menu_item_t *) &ffosd_ref,
      (base_menu_item_t *) &hdmi_standby_ref,
      (base_menu_item_t *) &return_ref,
      (base_menu_item_t *) &oclock_cpu_ref,
      (base_menu_item_t *) &oclock_core_ref,
      (base_menu_item_t *) &oclock_sdram_ref,
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
static child_menu_item_t update_cpld_menu_ref = { I_MENU, &update_cpld_menu };

static menu_t main_menu = {
   "Main Menu",
   NULL,
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
static char message[MAX_STRING_SIZE];

// Temporary filename for assembling OSD lines
static char filename[MAX_STRING_SIZE];

// Is the OSD currently active
static int active = 0;
static int old_active = -1;

// Main state of the OSD
osd_state_t osd_state = IDLE;

// Current menu depth
static int depth = 0;

// Currently active menu
static menu_t *current_menu[MAX_MENU_DEPTH];

// Index to the currently selected menu
static int current_item[MAX_MENU_DEPTH];

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
static uint32_t osd_palette_data[256];
//static unsigned char equivalence[256];

static char palette_names[MAX_NAMES][MAX_NAMES_WIDTH];
static uint32_t palette_array[MAX_NAMES][256];
static int ntsc_palette = 0;

static int inhibit_palette_dimming = 0;
static int single_button_mode = 0;


static int disable_overclock = 0;
static unsigned int cpu_clock = 1000;
static unsigned int core_clock = 400;
static unsigned int sdram_clock = 450;

static char EDID_buf[32768];
static unsigned int EDID_bufptr = 0;
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

static char cpld_firmware_dir[MIN_STRING_SIZE] = DEFAULT_CPLD_FIRMWARE_DIR;

// =============================================================
// Private Methods
// =============================================================

void set_menu_table() {
      int index = 0;
      int frontend = get_frontend();
      main_menu.items[index++] = (base_menu_item_t *) &back_ref;
      main_menu.items[index++] = (base_menu_item_t *) &info_menu_ref;
      if (frontend != FRONTEND_SIMPLE) main_menu.items[index++] = (base_menu_item_t *) &palette_menu_ref;
      main_menu.items[index++] = (base_menu_item_t *) &preferences_menu_ref;
      main_menu.items[index++] = (base_menu_item_t *) &settings_menu_ref;
      main_menu.items[index++] = (base_menu_item_t *) &geometry_menu_ref;
      main_menu.items[index++] = (base_menu_item_t *) &sampling_menu_ref;
      if (frontend != FRONTEND_SIMPLE) main_menu.items[index++] = (base_menu_item_t *) &update_cpld_menu_ref;
      main_menu.items[index++] = (base_menu_item_t *) &save_ref;
      main_menu.items[index++] = (base_menu_item_t *) &restore_ref;
      if (frontend != FRONTEND_SIMPLE) main_menu.items[index++] = (base_menu_item_t *) &cal_sampling_ref;
      main_menu.items[index++] = (base_menu_item_t *) &test_50hz_ref;
      main_menu.items[index++] = (base_menu_item_t *) &hdmi_ref;
      main_menu.items[index++] = (base_menu_item_t *) &resolution_ref;
      main_menu.items[index++] = (base_menu_item_t *) &refresh_ref;
      main_menu.items[index++] = (base_menu_item_t *) &scaling_ref;
      main_menu.items[index++] = (base_menu_item_t *) &saved_ref;
      main_menu.items[index++] = (base_menu_item_t *) &profile_ref;
      main_menu.items[index++] = (base_menu_item_t *) &autoswitch_ref;
      main_menu.items[index++] = (base_menu_item_t *) &subprofile_ref;
      if (get_parameter(F_AUTO_SWITCH) == AUTOSWITCH_IIGS_MANUAL || get_parameter(F_AUTO_SWITCH) == AUTOSWITCH_MANUAL) main_menu.items[index++] = (base_menu_item_t *) &timingset_ref;
      if (single_button_mode) main_menu.items[index++] = (base_menu_item_t *) &direction_ref;
      main_menu.items[index++] = NULL;

      switch (_get_hardware_id()) {
        case 4:                                  //pi 4
          features[F_OVERCLOCK_CPU].max = 200;
          features[F_OVERCLOCK_CORE].max = 200;
          features[F_OVERCLOCK_SDRAM].max = 200;
          break;
        case 3:                                  //pi zero 2 or Pi 3
          features[F_OVERCLOCK_CPU].max = 200;
          features[F_OVERCLOCK_CORE].max = 100;
          features[F_OVERCLOCK_SDRAM].max = 200;
          break;
         case 2:                                 //pi 2
          features[F_OVERCLOCK_CPU].max = 200;
          features[F_OVERCLOCK_CORE].max = 175;
          features[F_OVERCLOCK_SDRAM].max = 175;
          break;

        default:
          if (core_clock == 250) {               //pi 1
              features[F_OVERCLOCK_CPU].max = 100;
              features[F_OVERCLOCK_CORE].max = 100;
              features[F_OVERCLOCK_SDRAM].max = 175;
          } else {                               //pi zero
              features[F_OVERCLOCK_CPU].max = 75;
              features[F_OVERCLOCK_CORE].max = 175;
              features[F_OVERCLOCK_SDRAM].max = 175;
          }
          break;
      }
}

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
   if (get_parameter(F_RETURN_POSITION)) {
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

   case F_RESOLUTION:
      return get_resolution();
   case F_REFRESH:
      return get_refresh();
   case F_HDMI_MODE:
      return get_hdmi();
   case F_SCALING:
      return get_scaling();
   case F_FRONTEND:
      return get_frontend();



   case F_OVERCLOCK_CPU:
   case F_OVERCLOCK_CORE:
   case F_OVERCLOCK_SDRAM:
   case F_PALETTE:
   case F_TINT:
   case F_SAT:
   case F_CONT:
   case F_BRIGHT:
   case F_GAMMA:
   case F_RETURN_POSITION:
   case F_BUTTON_REVERSE:
   case F_CROP_BORDER:
   case F_SCREENCAP_SIZE:
   case F_SWAP_ASPECT:
   case F_NORMAL_SCALING:
   case F_MODE7_SCALING:
   case F_PROFILE:
   case F_SAVED_CONFIG:
   case F_SUB_PROFILE:
   case F_HDMI_MODE_STANDBY:
   case F_BORDER_COLOUR:
   case F_FONT_SIZE:
   case F_MODE7_DEINTERLACE:
   case F_NORMAL_DEINTERLACE:
   case F_FFOSD:
   case F_PALETTE_CONTROL:
   case F_NTSC_COLOUR:
   case F_NTSC_PHASE:
   case F_NTSC_TYPE:
   case F_NTSC_QUALITY:
   case F_TIMING_SET:
   case F_SCANLINES:
   case F_SCANLINE_LEVEL:
   case F_OUTPUT_COLOUR:
   case F_OUTPUT_INVERT:
   case F_VSYNC_INDICATOR:
   case F_GENLOCK_MODE:
   case F_GENLOCK_LINE:
   case F_GENLOCK_SPEED:
   case F_GENLOCK_ADJUST:
   case F_NUM_BUFFERS:
   case F_AUTO_SWITCH:
   case F_DEBUG:
   case F_POWERUP_MESSAGE:
   case F_YUV_PIXEL_DOUBLE:
   case F_INTEGER_ASPECT:
      return get_parameter(num);
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

   case F_RESOLUTION:
      set_resolution(value, resolution_names[value], 1);
      break;
   case F_REFRESH:
      set_refresh(value, 1);
      break;
   case F_HDMI_MODE:
      set_hdmi(value, 1);
      break;
   case F_SCALING:
      set_scaling(value, 1);
      break;
   case F_FRONTEND:
      set_frontend(value, 1);
      break;


   case F_PALETTE:
   case F_TINT:
   case F_SAT:
   case F_CONT:
   case F_BRIGHT:
   case F_GAMMA:
   case F_BUTTON_REVERSE:
   case F_CROP_BORDER:
   case F_SCREENCAP_SIZE:
   case F_SWAP_ASPECT:
   case F_MODE7_SCALING:
   case F_NORMAL_SCALING:
   case F_HDMI_MODE_STANDBY:
   case F_FFOSD:
   case F_MODE7_DEINTERLACE:
   case F_NORMAL_DEINTERLACE:
   case F_BORDER_COLOUR:
   case F_NTSC_PHASE:
   case F_NTSC_TYPE:
   case F_NTSC_QUALITY:
   case F_TIMING_SET:
   case F_SCANLINES:
   case F_SCANLINE_LEVEL:
   case F_GENLOCK_MODE:
   case F_GENLOCK_LINE:
   case F_GENLOCK_SPEED:
   case F_GENLOCK_ADJUST:
   case F_NUM_BUFFERS:
   case F_POWERUP_MESSAGE:
   case F_YUV_PIXEL_DOUBLE:
   case F_INTEGER_ASPECT:
      set_parameter(num, value);
      break;

   case F_OVERCLOCK_CPU:
      if ((disable_overclock & DISABLE_SETTINGS_OVERCLOCK) == DISABLE_SETTINGS_OVERCLOCK) {
          value = 0;
      }
      set_parameter(num, value);set_parameter(F_OVERCLOCK_CPU, value);
      if ((disable_overclock & DISABLE_PI1_PI2_OVERCLOCK) != DISABLE_PI1_PI2_OVERCLOCK && cpu_clock == 700) {
          set_clock_rate_cpu((cpu_clock + value + 200) * 1000000);    //overclock to 900
      } else {
          set_clock_rate_cpu((cpu_clock + value) * 1000000);
      }
      break;
   case F_OVERCLOCK_CORE:
      if ((disable_overclock & DISABLE_SETTINGS_OVERCLOCK) == DISABLE_SETTINGS_OVERCLOCK) {
          value = 0;
      }
      set_parameter(num, value);
#ifdef RPI4
      if (value > 100) {  //pi 4 core is already 500 Mhz (all others 400Mhz) so don't overclock unless overclock >100Mhz
          set_clock_rate_core((core_clock + value - 100) * 1000000);
      } else {
          set_clock_rate_core(core_clock * 1000000);
      }
#else
      if ((disable_overclock & DISABLE_PI1_PI2_OVERCLOCK) != DISABLE_PI1_PI2_OVERCLOCK && core_clock == 250) {
          set_clock_rate_core((core_clock + value + 150) * 1000000);
      } else {
          set_clock_rate_core((core_clock + value) * 1000000);
      }
#endif
      break;
   case F_OVERCLOCK_SDRAM:
      if ((disable_overclock & DISABLE_SETTINGS_OVERCLOCK) == DISABLE_SETTINGS_OVERCLOCK) {
          value = 0;
      }
      set_parameter(num, value);
      set_clock_rate_sdram((sdram_clock + value) * 1000000);
      break;
   case F_RETURN_POSITION:
      set_parameter(num, value);
      cycle_menus();
      break;
   case F_PROFILE:
      set_parameter(F_SAVED_CONFIG, 0);
      set_parameter(num, value);
      load_profiles(value, 1);
      process_profile(value);
      set_feature(F_SUB_PROFILE, 0);
      set_scaling(get_scaling(), 1);
      break;
   case F_SAVED_CONFIG:
      set_parameter(num, value);
      load_profiles(get_parameter(F_PROFILE), 1);
      process_profile(get_parameter(F_PROFILE));
      set_feature(F_SUB_PROFILE, 0);
      set_scaling(get_scaling(), 1);
      break;
   case F_SUB_PROFILE:
      set_parameter(num, value);
      process_sub_profile(get_parameter(F_PROFILE), value);
      break;
   case F_FONT_SIZE:
      if(active) {
         osd_clear();
         set_parameter(num, value);
         osd_refresh();
      } else {
         set_parameter(num, value);
      }
      break;
   case F_PALETTE_CONTROL:
      set_parameter(num, value);
      int hidden = (value < PALETTECONTROL_NTSCARTIFACT_CGA);
      features[F_NTSC_COLOUR].hidden = hidden;
      features[F_NTSC_PHASE].hidden = hidden;
      osd_update_palette();
      break;
   case F_NTSC_COLOUR:
   case F_OUTPUT_COLOUR:
   case F_OUTPUT_INVERT:
      set_parameter(num, value);
      osd_update_palette();
      break;
   case F_VSYNC_INDICATOR:
      set_parameter(num, value);
      features[F_GENLOCK_MODE].max = (value == 0) ? (NUM_HDMI - 5) : (NUM_HDMI - 1);
      break;
   case F_DEBUG:
      set_parameter(num, value);
      osd_update_palette();
      break;
   case F_AUTO_SWITCH:
      set_parameter(num, value);
      set_menu_table();
      osd_refresh();
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
   case I_TEST:
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

static const char *get_interface_name() {
    if (eight_bit_detected()) {
    return frontend_names_8[get_frontend()];
    } else {
    return frontend_names_6[get_frontend()];
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
      case F_SAVED_CONFIG:
         return saved_config_names[value];
      case F_SUB_PROFILE:
         return sub_profile_names[value];
      case F_RESOLUTION:
         return resolution_names[value];
      case F_REFRESH:
         return refresh_names[value];
      case F_HDMI_MODE:
         return hdmi_names[value];
      case F_SCALING:
         return scaling_names[value];
      case F_FRONTEND:
         return get_interface_name();
      case F_CROP_BORDER:
         return overscan_names[value];
      case F_OUTPUT_COLOUR:
         return colour_names[value];
      case F_OUTPUT_INVERT:
         return invert_names[value];
      case F_FONT_SIZE:
         return fontsize_names[value];
      case F_PALETTE:
         return palette_names[value];
      case F_PALETTE_CONTROL:
         return palette_control_names[value];
      case F_AUTO_SWITCH:
         return autoswitch_names[value];
      case F_MODE7_DEINTERLACE:
      case F_NORMAL_DEINTERLACE:
         return deinterlace_names[value];
      case F_MODE7_SCALING:
         return even_scaling_names[value];
      case F_NORMAL_SCALING:
         return even_scaling_names[value];
      case F_SCREENCAP_SIZE:
         return screencap_names[value];
      case F_GENLOCK_MODE:
         return genlock_mode_names[value];
      case F_GENLOCK_SPEED:
         return genlock_speed_names[value];
      case F_GENLOCK_ADJUST:
         return genlock_adjust_names[value];
#ifdef MULTI_BUFFER
      case F_NUM_BUFFERS:
         return nbuffer_names[value];
#endif
      case F_RETURN_POSITION:
         return return_names[value];
      case F_NTSC_PHASE:
         return phase_names[value];
      case F_NTSC_TYPE:
         return ntsctype_names[value];
      case F_NTSC_QUALITY:
         return fringe_names[value];
      case F_TIMING_SET:
         return alt_profile_names[value];
      case F_INTEGER_ASPECT:
         return integer_names[value];
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

static volatile uint32_t *gpioreg;

void osd_display_interface(int line) {
    gpioreg = (volatile uint32_t *)(_get_peripheral_base() + 0x101000UL);
    char osdline[256];
    sprintf(osdline, "Interface: %s", get_interface_name());
    osd_set(line, 0, osdline);
    sprintf(osdline, "Scaling: %s", scaling_names[get_scaling()]);
    osd_set(line + 1, 0, osdline);
    if (has_sub_profiles[get_parameter(F_PROFILE)]) {
        sprintf(osdline, "Profile: %s (%s)", profile_names[get_parameter(F_PROFILE)], sub_profile_names[get_parameter(F_SUB_PROFILE)]);
    } else {
        sprintf(osdline, "Profile: %s", profile_names[get_parameter(F_PROFILE)]);
    }
    osd_set(line + 2, 0, osdline);
#ifdef USE_ARM_CAPTURE
    osd_set(line + 3, 0, "ARM Capture Version");
#else
    osd_set(line + 3, 0, "GPU Capture Version");
#endif

    if (get_frontend() != FRONTEND_SIMPLE) {
        osd_set(line + 5, 0, "Use Auto Calibrate Video Sampling or");
        osd_set(line + 6, 0, "adjust sampling phase to fix noise");
    }

    if (core_clock == 250) { // either a Pi 1 or Pi 2 which can be auto overclocked
        if (disable_overclock) {
            osd_set(line + 8, 0, "Set disable_overclock=0 in config.txt");
            osd_set(line + 9, 0, "to enable 9BPP & 12BPP on Pi 1 or Pi 2");
        } else {
            osd_set(line + 8, 0, "Set disable_overclock=1 in config.txt");
            osd_set(line + 9, 0, "if you have lockups on Pi 1 or Pi 2");
        }
    }

}

static void info_system_summary(int line) {
   gpioreg = (volatile uint32_t *)(_get_peripheral_base() + 0x101000UL);
   sprintf(message, " Kernel Version: %s", GITVERSION);
   osd_set(line++, 0, message);
   sprintf(message, "   CPLD Version: %s v%x.%x",
           cpld->name,
           (cpld->get_version() >> VERSION_MAJOR_BIT) & 0xF,
           (cpld->get_version() >> VERSION_MINOR_BIT) & 0xF);
   osd_set(line++, 0, message);
   sprintf(message, "      Interface: %s", get_interface_name());
   osd_set(line++, 0, message);

   switch (_get_hardware_id()) {
       case 1:
            if (core_clock == 250) {
               sprintf(message, "      Processor: Pi 1");
            } else {
               sprintf(message, "      Processor: Pi Zero");
            }
            break;
       case 2:
            sprintf(message, "      Processor: Pi 2");
            break;
       case 3:
            sprintf(message, "      Processor: Pi Zero 2 or Pi 3");
            break;
       case 4:
            sprintf(message, "      Processor: Pi 4");
            break;
       default:
            sprintf(message, "      Unknown");
            break;
   }
   osd_set(line++, 0, message);
#ifdef USE_ARM_CAPTURE
   sprintf(message, "          Build: ARM Capture");
#else
   sprintf(message, "          Build: GPU Capture");
#endif
     osd_set(line++, 0, message);
   int ANA1_PREDIV;
#ifdef RPI4
   ANA1_PREDIV = 0;
#else
   ANA1_PREDIV = (gpioreg[PLLA_ANA1] >> 14) & 1;
#endif
   int NDIV = (gpioreg[PLLA_CTRL] & 0x3ff) << ANA1_PREDIV;
   int FRAC = gpioreg[PLLA_FRAC] << ANA1_PREDIV;
   int clockA = (double) (CRYSTAL * ((double)NDIV + ((double)FRAC) / ((double)(1 << 20))) + 0.5);
#ifdef RPI4
   ANA1_PREDIV = 0;
#else
   ANA1_PREDIV = (gpioreg[PLLB_ANA1] >> 14) & 1;
#endif
   NDIV = (gpioreg[PLLB_CTRL] & 0x3ff) << ANA1_PREDIV;
   FRAC = gpioreg[PLLB_FRAC] << ANA1_PREDIV;
   int clockB = (double) (CRYSTAL * ((double)NDIV + ((double)FRAC) / ((double)(1 << 20))) + 0.5);
#ifdef RPI4
   ANA1_PREDIV = 0;
#else
   ANA1_PREDIV = (gpioreg[PLLC_ANA1] >> 14) & 1;
#endif
   NDIV = (gpioreg[PLLC_CTRL] & 0x3ff) << ANA1_PREDIV;
   FRAC = gpioreg[PLLC_FRAC] << ANA1_PREDIV;
   int clockC = (double) (CRYSTAL * ((double)NDIV + ((double)FRAC) / ((double)(1 << 20))) + 0.5);
#ifdef RPI4
   ANA1_PREDIV = 0;
#else
   ANA1_PREDIV = (gpioreg[PLLD_ANA1] >> 14) & 1;
#endif
   NDIV = (gpioreg[PLLD_CTRL] & 0x3ff) << ANA1_PREDIV;
   FRAC = gpioreg[PLLD_FRAC] << ANA1_PREDIV;
   int clockD = (double) (CRYSTAL * ((double)NDIV + ((double)FRAC) / ((double)(1 << 20))) + 0.5);
   sprintf(message, "           PLLA: %4d Mhz", clockA);
   osd_set(line++, 0, message);
   sprintf(message, "           PLLB: %4d Mhz", clockB);
   osd_set(line++, 0, message);
   sprintf(message, "           PLLC: %4d Mhz", clockC);
   osd_set(line++, 0, message);
   sprintf(message, "           PLLD: %4d Mhz", clockD);
   osd_set(line++, 0, message);
   sprintf(message, "      CPU Clock: %4d Mhz", get_clock_rate(ARM_CLK_ID)/1000000);
   osd_set(line++, 0, message);
   sprintf(message, "     CORE Clock: %4d Mhz", get_clock_rate(CORE_CLK_ID)/1000000);
   osd_set(line++, 0, message);
   sprintf(message, "    SDRAM Clock: %4d Mhz", get_clock_rate(SDRAM_CLK_ID)/1000000);
   osd_set(line++, 0, message);
   sprintf(message, "      Core Temp: %6.2f C", get_temp());
   osd_set(line++, 0, message);
   sprintf(message, "   Core Voltage: %6.2f V", get_voltage(COMPONENT_CORE));
   osd_set(line++, 0, message);
   sprintf(message, "SDRAM_C Voltage: %6.2f V", get_voltage(COMPONENT_SDRAM_C));
   osd_set(line++, 0, message);
   sprintf(message, "SDRAM_P Voltage: %6.2f V", get_voltage(COMPONENT_SDRAM_P));
   osd_set(line++, 0, message);
   sprintf(message, "SDRAM_I Voltage: %6.2f V", get_voltage(COMPONENT_SDRAM_I));
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
   osd_set(line++, 0, "https://stardot.org.uk/forums/");
   osd_set(line++, 0, "viewtopic.php?f=3&t=14430");
}

static void info_save_log(int line) {
   log_save("/Log.txt");
   file_save_bin("/EDID.bin", EDID_buf, EDID_bufptr);
   osd_set(line++, 0, "Log.txt and EDID.bin saved to SD card");
}

static void info_test_50hz(int line) {
static char osdline[256];
static int old_50hz_state = 0;
int current_50hz_state = get_50hz_state();
int apparent_width = get_hdisplay();
int apparent_height = get_vdisplay();
   if (old_50hz_state == 1 && current_50hz_state == 0) {
      current_50hz_state = 1;
   }
   old_50hz_state = current_50hz_state;
   sprintf(osdline, "Current resolution = %d x %d", apparent_width, apparent_height);
   osd_set(line++, 0, osdline);
   if (get_parameter(F_GENLOCK_MODE) == HDMI_EXACT) {
       switch(current_50hz_state) {
          case 0:
               osd_set(line++, 0, "50Hz support is already enabled");
               osd_set(line++, 0, "");
               osd_set(line++, 0, "If menu text is unstable, change the");
               osd_set(line++, 0, "Refresh menu option to 60Hz to");
               osd_set(line++, 0, "permanently disable 50Hz support.");
               break;
          case 1:
               set_force_genlock_range(GENLOCK_RANGE_FORCE_LOW);
               set_status_message(" ");
               osd_set(line++, 0, "50Hz support enabled until reset");
               osd_set(line++, 0, "");
               osd_set(line++, 0, "If you can see this message, change the");
               osd_set(line++, 0, "Refresh menu option to Force 50Hz-60Hz");
               osd_set(line++, 0, "to permanently enable 50Hz support.");
               break;
          default:
               osd_set(line++, 0, "Unable to test: Source is not 50hz");
               break;
       }
   } else {
        osd_set(line++, 0, "Unable to test: Genlock disabled");
   }
   osd_set(line++, 0, "");
   osd_set(line++, 0, "If the current resolution doesn't match");
   osd_set(line++, 0, "the physical resolution of your lcd panel,");
   osd_set(line++, 0, "change the Resolution menu option to");
   osd_set(line++, 0, "the correct resolution.");
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
   if (!get_parameter(F_RETURN_POSITION)) {
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
   if (get_parameter(F_RETURN_POSITION)) {
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
   char cpld_dir[MAX_STRING_SIZE];
   strncpy(cpld_dir, cpld_firmware_dir, MAX_STRING_LIMIT);
   if (get_parameter(F_DEBUG)) {
       int cpld_design = cpld->get_version() >> VERSION_DESIGN_BIT;
       switch(cpld_design) {
           case DESIGN_ATOM:
                  strncat(cpld_dir, "/ATOM_old", MAX_STRING_LIMIT);
                  break;
           case DESIGN_YUV_TTL:
           case DESIGN_YUV_ANALOG:
                  strncat(cpld_dir, "/YUV_old", MAX_STRING_LIMIT);
                  break;
           case DESIGN_RGB_TTL:
           case DESIGN_RGB_ANALOG:
                  strncat(cpld_dir, "/RGB_old", MAX_STRING_LIMIT);
                  break;
           default:
           case DESIGN_BBC:
                  strncat(cpld_dir, "/BBC_old", MAX_STRING_LIMIT);
                  break;
       }
       log_info("FOLDER %d %s", cpld_design, cpld_dir);
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
      if (item->type == I_INFO || item->type == I_TEST) {
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
       osd_update((uint32_t *) (capinfo->fb + capinfo->pitch * capinfo->height * get_current_display_buffer() + capinfo->pitch * capinfo->v_adjust + capinfo->h_adjust), capinfo->pitch, 1);
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

// =============================================================
// Public Methods
// =============================================================

/*
uint32_t osd_get_equivalence(uint32_t value) {
   switch (capinfo->bpp) {
        case 4:
            return equivalence[value & 0xf] | (equivalence[(value >> 4) & 0xf] << 4) | (equivalence[(value >> 8) & 0xf] << 8) | (equivalence[(value >> 12) & 0xf] << 12)
            | (equivalence[(value >> 16) & 0xf] << 16) | (equivalence[(value >> 20) & 0xf] << 20) | (equivalence[(value >> 24) & 0xf] << 24) | (equivalence[(value >> 28) & 0xf] << 28);
        break;
        case 8:
            return equivalence[value & 0xff] | (equivalence[(value >> 8) & 0xff] << 8) | (equivalence[(value >> 16) & 0xff] << 16) | (equivalence[value >> 24] << 24);
        break;
        default:
        case 16:
            return value;
        break;
   }
}
*/

uint32_t osd_get_palette(int index) {
   if (active) {
      return osd_palette_data[index];
   } else {
      return palette_data[index];
   }
}

int normalised_gamma_correct(int old_value) {
    if (old_value >= 0) {
        double value = (double) old_value;
        double normalised_gamma = 1 / ((double)get_parameter(F_GAMMA) / 100);
        value = value < 0 ? 0 : value;
        value = pow(value, normalised_gamma) * 255;
        value = round(value);
        value = value < 0 ? 0 : value;
        value = value > 255 ? 255 : value;
        return (int) value;
    } else {
        return old_value;
    }
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
    if (get_parameter(F_TINT) !=0 || get_parameter(F_SAT) != 100 || get_parameter(F_CONT) != 100 || get_parameter(F_BRIGHT) != 100 || get_parameter(F_GAMMA) != 100) {
        double R = (double)(palette & 0xff) / 255;
        double G = (double)((palette >> 8) & 0xff) / 255;
        double B = (double)((palette >> 16) & 0xff) / 255;
        double M = (double)((palette >> 24) & 0xff) / 255;

        double normalised_contrast = (double)get_parameter(F_CONT) / 100;
        double normalised_brightness = (double)get_parameter(F_BRIGHT) / 200 - 0.5f;
        double normalised_saturation = (double)get_parameter(F_SAT) / 100;
        double normalised_gamma = 1 / ((double)get_parameter(F_GAMMA) / 100);

        double Y = 0.299 * R + 0.587 * G + 0.114 * B;
        double U = -0.14713 * R - 0.28886 * G + 0.436 * B;
        double V = 0.615 * R - 0.51499 * G - 0.10001 * B;

        Y = (Y + normalised_brightness) * normalised_contrast;
        double hue = get_parameter(F_TINT) * PI / 180.0f;
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
    double Y = 0;
    double U = 0;
    double V = 0;
    double R = 0;
    double G = 0;
    double B = 0;
    colour = ((colour << (4 - (get_adjusted_ntscphase() & 3))) & 0x0f) | (colour >> (get_adjusted_ntscphase() & 3));

    if (ntsc_palette <= features[F_PALETTE].max) {
        if (colour > 7) colour += 8;
        int RGBY = palette_array[ntsc_palette][colour];
        //if (colour < 0x18) log_info("Using %d, %x for NTSC palette %X",ntsc_palette,colour,  RGBY);
        R = (double)(RGBY & 0xff) / 255.0f;
        G = (double)((RGBY >> 8) & 0xff) / 255.0f;
        B = (double)((RGBY >> 16) & 0xff) / 255.0f;

    } else {
    //log_info("Using internal for NTSC palette");
    double phase_shift = 0.0f;

        switch (colour) {
           case 0x00:
              Y=0     ; U=0     ; V=0     ; break; //Black
           case 0x01:
              Y=0.25  ; U=0     ; V=0.5   ; phase_shift = 6.0f; break; //Magenta
           case 0x02:
              Y=0.25  ; U=0.5   ; V=0     ; phase_shift = 12.0f; break; //Dark Blue
           case 0x03:
              Y=0.5   ; U=1     ; V=1     ; phase_shift = -6.0f; break; //Purple
           case 0x04:
              Y=0.25  ; U=0     ; V=-0.5  ; phase_shift = 12.0f; break; //Dark Green
           case 0x05:
              Y=0.5   ; U=0     ; V=0     ; break; //lower Gray
           case 0x06:
              Y=0.5   ; U=1     ; V=-1    ; phase_shift = -6.0f; break; //Medium Blue
           case 0x07:
              Y=0.75  ; U=0.5   ; V=0     ; phase_shift = 6.0f; break; //Light Blue
           case 0x08:
              Y=0.25  ; U=-0.5  ; V=0     ; phase_shift = 12.0f; break; //Brown
           case 0x09:
              Y=0.5   ; U=-1    ; V=1     ; phase_shift = -6.0f; break; //Orange
           case 0x0a:
              Y=0.5   ; U=0     ; V=0     ; break; //upper Gray
           case 0x0b:
              Y=0.75  ; U=0     ; V=0.5   ; phase_shift = 6.0f; break; //Pink
           case 0x0c:
              Y=0.5   ; U=-1    ; V=-1    ; phase_shift = -6.0f; break; //Light Green
           case 0x0d:
              Y=0.75  ; U=-0.5  ; V=0     ; phase_shift = 6.0f; break; //Yellow
           case 0x0e:
              Y=0.75  ; U=0     ; V=-0.5  ; phase_shift = 6.0f; break; //Aquamarine
           case 0x0f:
              Y=1     ; U=0     ; V=0     ; break; //White
        }

        double hue = phase_shift * PI / 180.0f;

        double U2 = (U * cos(hue) + V * sin(hue));
        double V2 = (V * cos(hue) - U * sin(hue));

        R = (Y + 1.140 * V2);
        G = (Y - 0.395 * U2 - 0.581 * V2);
        B = (Y + 2.032 * U2);

    }

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

    if (filtered_bitcount >= 3) {
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

    colour = ((colour << (4 - (get_adjusted_ntscphase() & 3))) & 0x0f) | (colour >> (get_adjusted_ntscphase() & 3));

    if (index < 0x10) {
        switch (colour) {
           case 0x00:
              R=0     ; G=0     ; B=0     ; break; //Black
           case 0x08:
              R=0     ; G=117   ; B=108   ; break;
           case 0x01:
              R=0     ; G=49    ; B=111   ; break;
           case 0x09:
              R=0     ; G=83    ; B=63    ; break;
           case 0x02:
              R=123   ; G=52    ; B=0     ; break;
           case 0x0a:
              R=57    ; G=190   ; B=66    ; break;
           case 0x03:
              R=131   ; G=118   ; B=73    ; break;
           case 0x0b:
              R=83    ; G=155   ; B=14    ; break;
           case 0x04:
              R=235   ; G=50    ; B=7     ; break;
           case 0x0c:
              R=210   ; G=196   ; B=153   ; break;
           case 0x05:
              R=248   ; G=122   ; B=155   ; break;
           case 0x0d:
              R=217   ; G=160   ; B=107   ; break;
           case 0x06:
              R=180   ; G=69    ; B=0     ; break;
           case 0x0e:
              R=139   ; G=208   ; B=74    ; break;
           case 0x07:
              R=190   ; G=133   ; B=80    ; break;
           case 0x0f:
              R=152   ; G=173   ; B=20    ; break;
        }
    } else {
        switch (colour) {
           case 0x00:
              R=0     ; G=0     ; B=0     ; break; //Black
           case 0x08:
              R=0     ; G=139   ; B=172   ; break;
           case 0x01:
              R=0     ; G=73    ; B=174   ; break;
           case 0x09:
              R=0     ; G=158   ; B=232   ; break;
           case 0x02:
              R=89    ; G=28    ; B=0     ; break;
           case 0x0a:
              R=0     ; G=188   ; B=155   ; break;
           case 0x03:
              R=99    ; G=116   ; B=158   ; break;
           case 0x0b:
              R=0     ; G=206   ; B=217   ; break;
           case 0x04:
              R=237   ; G=28    ; B=39    ; break;
           case 0x0c:
              R=180   ; G=196   ; B=238   ; break;
           case 0x05:
              R=221   ; G=125   ; B=239   ; break;
           case 0x0d:
              R=188   ; G=210   ; B=255   ; break;
           case 0x06:
              R=255   ; G=73    ; B=0     ; break;
           case 0x0e:
              R=247   ; G=237   ; B=193   ; break;
           case 0x07:
              R=255   ; G=165   ; B=196   ; break;
           case 0x0f:
              R=255   ; G=255   ; B=255   ; break;
        }
    }

    double Y = 0.299 * (double)R + 0.587 * (double)G + 0.114 * (double)B;
    Y = gamma_correct(Y, 1);

    return R | (G << 8) | (B << 16) | ((int)Y << 24);
}

void yuv2rgb(int maxdesat, int mindesat, int luma_scale, int blank_ref, int y1_millivolts, int u1_millivolts, int v1_millivolts, int *r, int *g, int *b, int *m) {

   int desat = maxdesat;
   if (y1_millivolts >= 720) {
       desat = mindesat;
   }

   if (y1_millivolts == blank_ref) {
       u1_millivolts = 2000;
       v1_millivolts = 2000;
   }

   *m = luma_scale * 255 * (blank_ref - y1_millivolts) / (blank_ref - 420) / 100;
   for(int chroma_scale = 100; chroma_scale > desat; chroma_scale--) {
      int y = (luma_scale * 255 * (blank_ref - y1_millivolts) / (blank_ref - 420));
      int u = (chroma_scale * ((u1_millivolts - 2000) / 500) * 127);
      int v = (chroma_scale * ((v1_millivolts - 2000) / 500) * 127);

      //if (y1_millivolts <= 540) { // add a little differential phase shift
      //    double hue = -12 * PI / 180.0f;
      //    u = (int) ((double)u * cos(hue) + (double)v * sin(hue));
      //    v = (int) ((double)v * cos(hue) - (double)u * sin(hue));
      //}

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
            double Y=0;
            double U=0;
            double V=0;

            double phase_shift = 0;
            double hue=0;
            double U2=0;
            double V2=0;

            int luma = i & 0x12;
            int maxdesat = 99;
            int mindesat = 30;
            int luma_scale = 81;
            int blank_ref = 770;
            int black_ref = 720;
            if (palette == PALETTE_DRAGON_COCO || palette == PALETTE_ATOM_MKII_PLUS) {
                black_ref = blank_ref;
            }
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

                 case PALETTE_LASER:
                    phase_shift = 0.0f;
                    switch (i & 0x17) {
                       case 0x00:
                          Y=0     ; U=0     ; V=0     ; break; //Black
                       case 0x01:
                          Y=0.25  ; U=0     ; V=0.5   ; phase_shift = 6.0f; break; //Magenta
                       case 0x04:
                          Y=0.25  ; U=0.5   ; V=0     ; phase_shift = 12.0f; break; //Dark Blue
                       case 0x05:
                          Y=0.5   ; U=1     ; V=1     ; phase_shift = -6.0f; break; //Purple
                       case 0x02:
                          Y=0.25  ; U=0     ; V=-0.5  ; phase_shift = 12.0f; break; //Dark Green
                       case 0x03:
                          Y=0.5   ; U=0     ; V=0     ; break; //lower Gray
                       case 0x06:
                          Y=0.5   ; U=1     ; V=-1    ; phase_shift = -6.0f; break; //Medium Blue
                       case 0x07:
                          Y=0.75  ; U=0.5   ; V=0     ; phase_shift = 6.0f; break; //Light Blue
                       case 0x10:
                          Y=0.25  ; U=-0.5  ; V=0     ; phase_shift = 12.0f; break; //Brown
                       case 0x11:
                          Y=0.5   ; U=-1    ; V=1     ; phase_shift = -6.0f; break; //Orange
                       case 0x14:
                          Y=0.5   ; U=0     ; V=0     ; break; //upper Gray
                       case 0x15:
                          Y=0.75  ; U=0     ; V=0.5   ; phase_shift = 6.0f; break; //Pink
                       case 0x12:
                          Y=0.5   ; U=-1    ; V=-1    ; phase_shift = -6.0f; break; //Light Green
                       case 0x13:
                          Y=0.75  ; U=-0.5  ; V=0     ; phase_shift = 6.0f; break; //Yellow
                       case 0x16:
                          Y=0.75  ; U=0     ; V=-0.5  ; phase_shift = 6.0f; break; //Aquamarine
                       case 0x17:
                          Y=1     ; U=0     ; V=0     ; break; //White
                    }

                    hue = phase_shift * PI / 180.0f;

                    U2 = (U * cos(hue) + V * sin(hue));
                    V2 = (V * cos(hue) - U * sin(hue));

                    r = gamma_correct(Y + 1.140 * V2, 1);
                    g = gamma_correct(Y - 0.395 * U2 - 0.581 * V2, 1);
                    b = gamma_correct(Y + 2.032 * U2, 1);
                    m = gamma_correct(Y, 1);
                    break;

                 case PALETTE_XRGB:
                    phase_shift = 0.0f;
                    switch (i & 0x17) {
                       case 0x00:
                          Y=0     ; U=0     ; V=0     ; break; //Black
                       case 0x01:
                          Y=0.25  ; U=0     ; V=0.5   ; phase_shift = 6.0f; break; //Magenta
                       case 0x02:
                          Y=0.25  ; U=0.5   ; V=0     ; phase_shift = 12.0f; break; //Dark Blue
                       case 0x03:
                          Y=0.5   ; U=1     ; V=1     ; phase_shift = -6.0f; break; //Purple
                       case 0x04:
                          Y=0.25  ; U=0     ; V=-0.5  ; phase_shift = 12.0f; break; //Dark Green
                       case 0x05:
                          Y=0.5   ; U=0     ; V=0     ; break; //lower Gray
                       case 0x06:
                          Y=0.5   ; U=1     ; V=-1    ; phase_shift = -6.0f; break; //Medium Blue
                       case 0x07:
                          Y=0.75  ; U=0.5   ; V=0     ; phase_shift = 6.0f; break; //Light Blue
                       case 0x10:
                          Y=0.25  ; U=-0.5  ; V=0     ; phase_shift = 12.0f; break; //Brown
                       case 0x11:
                          Y=0.5   ; U=-1    ; V=1     ; phase_shift = -6.0f; break; //Orange
                       case 0x12:
                          Y=0.5   ; U=0     ; V=0     ; break; //upper Gray
                       case 0x13:
                          Y=0.75  ; U=0     ; V=0.5   ; phase_shift = 6.0f; break; //Pink
                       case 0x14:
                          Y=0.5   ; U=-1    ; V=-1    ; phase_shift = -6.0f; break; //Light Green
                       case 0x15:
                          Y=0.75  ; U=-0.5  ; V=0     ; phase_shift = 6.0f; break; //Yellow
                       case 0x16:
                          Y=0.75  ; U=0     ; V=-0.5  ; phase_shift = 6.0f; break; //Aquamarine
                       case 0x17:
                          Y=1     ; U=0     ; V=0     ; break; //White
                    }

                    hue = phase_shift * PI / 180.0f;

                    U2 = (U * cos(hue) + V * sin(hue));
                    V2 = (V * cos(hue) - U * sin(hue));

                    r = gamma_correct(Y + 1.140 * V2, 1);
                    g = gamma_correct(Y - 0.395 * U2 - 0.581 * V2, 1);
                    b = gamma_correct(Y + 2.032 * U2, 1);
                    m = gamma_correct(Y, 1);
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

                 case PALETTE_RrGgBbI:   //sam coupe
                    r = (i & 1) ? 0x92 : 0x00;
                    g = (i & 2) ? 0x92 : 0x00;
                    b = (i & 4) ? 0x92 : 0x00;

                    r = (i & 0x08) ? (r + 0x49) : r;
                    g = (i & 0x10) ? (g + 0x49) : g;
                    b = (i & 0x20) ? (b + 0x49) : b;

                    r = (i & 0x40) ? (r + 0x24) : r;
                    g = (i & 0x40) ? (g + 0x24) : g;
                    b = (i & 0x40) ? (b + 0x24) : b;
                    break;

                 case PALETTE_MDA:
                    r = (i & 0x20) ? 0xaa : 0x00;
                    r = (i & 0x10) ? (r + 0x55) : r;
                    g = r; b = r;
                    break;

                 case PALETTE_DRAGON_COCO:
                 case PALETTE_DRAGON_COCO_FULL: {
                  if ((i & 0x40) == 0x40) {
                    r = 0xff; g = 0; b = 0;
                  } else {
                    switch (i & 0x2d) {  //these five are luma independent
                        case (bz + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 650, 2000, 2500, &r, &g, &b, &m); r = 254; g =   0; b =  0; break; // red
                        case (bp + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 650, 2500, 2000, &r, &g, &b, &m); r =   1; g =  74; b = 253; break; // blue
                        case (bp + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 2500, 2500, &r, &g, &b, &m); r = 241; g =  67; b = 194; break; // magenta
                        case (bm + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 420, 1500, 2000, &r, &g, &b, &m); r = 216; g = 254; b =   0; break; // yellow
                        case (bz + rm):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 2000, 1500, &r, &g, &b, &m); r =  57; g = 180; b = 216; break; // cyan


                        case (bz + rz): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, black_ref, 2000, 2000, &r, &g, &b, &m); break; // black
                                case 0x10: //alt
                                case 0x02: //alt
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 420, 2000, 2000, &r, &g, &b, &m); r = 205; g = 210; b = 224; break; // white (buff)
                            }
                        }
                        break;
                        case (bm + rm): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, black_ref, 1500, 1500, &r, &g, &b, &m); break; // dark green (text)
                                case 0x10:
                                case 0x02:
                                case 0x12: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 1500, 1500, &r, &g, &b, &m); break; // green (text)
                            }
                        }
                        break;
                        case (bm + rp): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, black_ref, 1500, 2500, &r, &g, &b, &m); break; // dark orange (text)
                                case 0x10:
                                case 0x02:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 1500, 2500, &r, &g, &b, &m); r = 248; g = 105; b =  19; break; // normal orange (graphics)
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 420, 1500, 2500, &r, &g, &b, &m); break; // bright orange (text)
                            }
                        }
                        break;
                    }
                  }
                 }
                 break;

                 case PALETTE_ATOM_6847_EMULATORS: {
                  if ((i & 0x40) == 0x40) {
                    r = 0xff; g = 0; b = 0;
                  } else {
                    switch (i & 0x2d) {  //these five are luma independent
                        case (bz + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 650, 2000, 2500, &r, &g, &b, &m); r = 181; g =   5; b =  34; break; // red
                        case (bp + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 650, 2500, 2000, &r, &g, &b, &m); r =  34; g =  19; b = 181; break; // blue
                        case (bp + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 2500, 2500, &r, &g, &b, &m); r = 255; g =  28; b = 255; break; // magenta
                        case (bm + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 420, 1500, 2000, &r, &g, &b, &m); r = 255; g = 255; b =  67; break; // yellow
                        case (bz + rm):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 2000, 1500, &r, &g, &b, &m); r =  10; g = 212; b = 112; break; // cyan

                        case (bz + rz): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 720, 2000, 2000, &r, &g, &b, &m); r =   9; g =   9; b =   9; break; // black
                                case 0x10: //alt
                                case 0x02: //alt
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 420, 2000, 2000, &r, &g, &b, &m); r = 255; g = 255; b = 255; break; // white (buff)
                            }
                        }
                        break;
                        case (bm + rm): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 720, 1500, 1500, &r, &g, &b, &m); r =   0; g =  65; b =   0; break; // dark green (text)
                                case 0x10:
                                case 0x02:
                                case 0x12: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 1500, 1500, &r, &g, &b, &m); r =  10; g = 255; b =  10; break; // green (text)
                            }
                        }
                        break;
                        case (bm + rp): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 720, 1500, 2500, &r, &g, &b, &m); r = 107; g =   0; b =   0; break; // dark orange
                                case 0x10:
                                case 0x02:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 1500, 2500, &r, &g, &b, &m); r = 255; g =  67; b =  10; break; // normal orange (graphics)
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 420, 1500, 2500, &r, &g, &b, &m); r = 255; g = 181; b =  67; break; // bright orange (text)
                            }
                        }
                        break;
                    }
                  }
                 }
                 break;

                 case PALETTE_ATOM_MKII: {
                  if ((i & 0x40) == 0x40) {
                    r = 0xff; g = 0; b = 0;
                  } else {
                    switch (i & 0x2d) {  //these five are luma independent
                        case (bz + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 650, 2000, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0x00; break; // red
                        case (bp + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 650, 2500, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0xff; break; // blue
                        case (bp + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 2500, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0xff; break; // magenta
                        case (bm + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 420, 1500, 2000, &r, &g, &b, &m); r=0xff; g=0xff; b=0x00; break; // yellow
                        case (bz + rm):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 2000, 1500, &r, &g, &b, &m); r=0x00; g=0xff; b=0xff; break; // cyan

                        case (bz + rz): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, blank_ref, 2000, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0x00; break; // black
                                case 0x10: //alt
                                case 0x02: //alt
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 420, 2000, 2000, &r, &g, &b, &m); r=0xff; g=0xff; b=0xff; break; // white (buff)
                            }
                        }
                        break;
                        case (bm + rm): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, blank_ref, 2000, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0x00; break; // dark green (text) - force black
                                case 0x10:
                                case 0x02:
                                case 0x12: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 1500, 1500, &r, &g, &b, &m); r=0x00; g=0xff; b=0x00; break; // green (text)
                            }
                        }
                        break;
                        case (bm + rp): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, blank_ref, 2000, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0x00; break; // dark orange (text) - force black
                                case 0x10:
                                case 0x02:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 1500, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0x00; break; // normal orange (graphics) - force red
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 420, 1500, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0x00; break; // bright orange (text) - force red
                            }
                        }
                        break;
                    }
                  }
                 }
                 break;

                 case PALETTE_ATOM_MKII_PLUS:
                 case PALETTE_ATOM_MKII_FULL: {
                  if ((i & 0x40) == 0x40) {
                    r = 0xff; g = 0; b = 0;
                  } else {
                    switch (i & 0x2d) {  //these five are luma independent
                        case (bz + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 650, 2000, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0x00; break; // red
                        case (bp + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 650, 2500, 2000, &r, &g, &b, &m); r=0x00; g=0x00; b=0xff; break; // blue
                        case (bp + rp):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 2500, 2500, &r, &g, &b, &m); r=0xff; g=0x00; b=0xff; break; // magenta
                        case (bm + rz):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 420, 1500, 2000, &r, &g, &b, &m); r=0xff; g=0xff; b=0x00; break; // yellow
                        case (bz + rm):
                           yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 2000, 1500, &r, &g, &b, &m); r=0x00; g=0xff; b=0xff; break; // cyan

                        case (bz + rz): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, black_ref, 2000, 2000, &r, &g, &b, &m); break; // black
                                case 0x10: //alt
                                case 0x02: //alt
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 420, 2000, 2000, &r, &g, &b, &m); r=0xff; g=0xff; b=0xff; break; // white (buff)
                            }
                        }
                        break;
                        case (bm + rm): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, black_ref, 1500, 1500, &r, &g, &b, &m); break; // dark green (text)
                                case 0x10:
                                case 0x02:
                                case 0x12: //alt
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 1500, 1500, &r, &g, &b, &m); r=0x00; g=0xff; b=0x00; break; // green (text)
                            }
                        }
                        break;
                        case (bm + rp): {
                            switch (luma) {
                                case 0x00:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, black_ref, 1500, 2500, &r, &g, &b, &m); break; // dark orange (text)
                                case 0x10:
                                case 0x02:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 540, 1500, 2500, &r, &g, &b, &m); break; // normal orange (graphics)
                                case 0x12:
                                    yuv2rgb(maxdesat, mindesat, luma_scale, blank_ref, 420, 1500, 2500, &r, &g, &b, &m); break; // bright orange (text)
                            }
                        }
                        break;
                    }
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

                case PALETTE_C64LC_REV1:
                case PALETTE_C64LC: {
                    int revision = palette == PALETTE_C64LC_REV1 ? 0 : 1;
                    double brightness = 50;
                    double contrast = 100;
                    double saturation = 50;
                    r=g=b=0;

                    switch (i & 0x0f) {
                        case (0):
                        create_colodore_colours(0, revision, brightness, contrast, saturation, &r, &g, &b, &m); //black
                        break;
                        case (1):
                        create_colodore_colours(6, revision, brightness, contrast, saturation, &r, &g, &b, &m); //blue
                        break;
                        case (2):
                        create_colodore_colours(2, revision, brightness, contrast, saturation, &r, &g, &b, &m); //red
                        break;
                        case (3):
                        create_colodore_colours(4, revision, brightness, contrast, saturation, &r, &g, &b, &m); //violet
                        break;


                        case (4):
                        create_colodore_colours(9, revision, brightness, contrast, saturation, &r, &g, &b, &m); //brown
                        break;
                        case (5):
                        create_colodore_colours(11, revision, brightness, contrast, saturation, &r, &g, &b, &m); //dark grey
                        break;
                        case (6):
                        create_colodore_colours(12, revision, brightness, contrast, saturation, &r, &g, &b, &m); //grey2
                        break;
                        case (7):
                        create_colodore_colours(3, revision, brightness, contrast, saturation, &r, &g, &b, &m); //cyan


                        break;
                        case (8):
                        create_colodore_colours(8, revision, brightness, contrast, saturation, &r, &g, &b, &m); //orange
                        break;
                        case (9):
                        create_colodore_colours(14, revision, brightness, contrast, saturation, &r, &g, &b, &m); //light blue
                        break;
                        case (10):
                        create_colodore_colours(15, revision, brightness, contrast, saturation, &r, &g, &b, &m); //light grey
                        break;
                        case (11):
                        create_colodore_colours(7, revision, brightness, contrast, saturation, &r, &g, &b, &m); //yellow
                        break;


                        case (12):
                        create_colodore_colours(5, revision, brightness, contrast, saturation, &r, &g, &b, &m); //green
                        break;
                        case (13):
                        create_colodore_colours(10, revision, brightness, contrast, saturation, &r, &g, &b, &m); //light red
                        break;
                        case (14):
                        create_colodore_colours(13, revision, brightness, contrast, saturation, &r, &g, &b, &m); //light green
                        break;
                        case (15):
                        create_colodore_colours(1, revision, brightness, contrast, saturation, &r, &g, &b, &m); //white
                        break;

                    }
                 }
                 break;

                 case PALETTE_ATARI800_PAL: {
                        static int atari_palette[] = {
                                0x00000000,
                                0x00111111,
                                0x00252525,
                                0x00353535,
                                0x00464646,
                                0x00575757,
                                0x006B6B6B,
                                0x007C7C7C,
                                0x00838383,
                                0x00949494,
                                0x00A8A8A8,
                                0x00B9B9B9,
                                0x00CACACA,
                                0x00DADADA,
                                0x00EEEEEE,
                                0x00FFFFFF,
                                0x0000003C,
                                0x0000074C,
                                0x00001B60,
                                0x00002C71,
                                0x00003C82,
                                0x00004D93,
                                0x000061A6,
                                0x001172B7,
                                0x00197ABF,
                                0x002A8BD0,
                                0x003E9EE4,
                                0x004FAFF4,
                                0x005FC0FF,
                                0x0070D1FF,
                                0x0084E4FF,
                                0x0095F5FF,
                                0x0000004B,
                                0x0000005C,
                                0x00000B70,
                                0x00001C81,
                                0x00032D91,
                                0x00133EA2,
                                0x002751B6,
                                0x003862C7,
                                0x00406ACF,
                                0x00517BE0,
                                0x00658FF3,
                                0x0075A0FF,
                                0x0086B0FF,
                                0x0097C1FF,
                                0x00ABD5FF,
                                0x00BBE6FF,
                                0x00150050,
                                0x00260061,
                                0x003A0074,
                                0x004A0985,
                                0x005B1996,
                                0x006C2AA7,
                                0x00803EBB,
                                0x00914FCB,
                                0x009857D3,
                                0x00A968E4,
                                0x00BD7BF8,
                                0x00CE8CFF,
                                0x00DF9DFF,
                                0x00EFAEFF,
                                0x00FFC1FF,
                                0x00FFD2FF,
                                0x0067003D,
                                0x0078004E,
                                0x008C0062,
                                0x009D0273,
                                0x00AE1383,
                                0x00BE2494,
                                0x00D238A8,
                                0x00E348B9,
                                0x00EB50C1,
                                0x00FC61D1,
                                0x00FF75E5,
                                0x00FF86F6,
                                0x00FF96FF,
                                0x00FFA7FF,
                                0x00FFBBFF,
                                0x00FFCCFF,
                                0x00840028,
                                0x00950039,
                                0x00A9004C,
                                0x00BA075D,
                                0x00CB186E,
                                0x00DB297F,
                                0x00EF3D93,
                                0x00FF4EA3,
                                0x00FF55AB,
                                0x00FF66BC,
                                0x00FF7AD0,
                                0x00FF8BE1,
                                0x00FF9CF1,
                                0x00FFACFF,
                                0x00FFC0FF,
                                0x00FFD1FF,
                                0x0094000F,
                                0x00A5001F,
                                0x00B90033,
                                0x00C91144,
                                0x00DA2255,
                                0x00EB3365,
                                0x00FF4779,
                                0x00FF578A,
                                0x00FF5F92,
                                0x00FF70A3,
                                0x00FF84B7,
                                0x00FF95C7,
                                0x00FFA6D8,
                                0x00FFB6E9,
                                0x00FFCAFD,
                                0x00FFDBFF,
                                0x00860000,
                                0x00970A00,
                                0x00AB1E00,
                                0x00BC2F10,
                                0x00CC3F20,
                                0x00DD5031,
                                0x00F16445,
                                0x00FF7556,
                                0x00FF7D5E,
                                0x00FF8E6E,
                                0x00FFA182,
                                0x00FFB293,
                                0x00FFC3A4,
                                0x00FFD4B5,
                                0x00FFE7C8,
                                0x00FFF8D9,
                                0x006A0A00,
                                0x007B1B00,
                                0x008F2E00,
                                0x00A03F00,
                                0x00B0500B,
                                0x00C1611B,
                                0x00D5742F,
                                0x00E68540,
                                0x00EE8D48,
                                0x00FF9E59,
                                0x00FFB26C,
                                0x00FFC37D,
                                0x00FFD38E,
                                0x00FFE49F,
                                0x00FFF8B3,
                                0x00FFFFC3,
                                0x00441900,
                                0x00542A00,
                                0x00683E00,
                                0x00794F00,
                                0x008A5F00,
                                0x009A700C,
                                0x00AE841F,
                                0x00BF9530,
                                0x00C79D38,
                                0x00D8AE49,
                                0x00ECC15D,
                                0x00FCD26E,
                                0x00FFE37E,
                                0x00FFF48F,
                                0x00FFFFA3,
                                0x00FFFFB4,
                                0x00002D00,
                                0x00003E00,
                                0x00105100,
                                0x00206200,
                                0x00317300,
                                0x00428407,
                                0x0056971B,
                                0x0067A82C,
                                0x006EB034,
                                0x007FC144,
                                0x0093D558,
                                0x00A4E669,
                                0x00B5F67A,
                                0x00C5FF8B,
                                0x00D9FF9E,
                                0x00EAFFAF,
                                0x00002E00,
                                0x00003F00,
                                0x00005300,
                                0x0000630E,
                                0x0000741E,
                                0x0000852F,
                                0x00009943,
                                0x0000AA54,
                                0x0000B15C,
                                0x0010C26C,
                                0x0024D680,
                                0x0034E791,
                                0x0045F8A2,
                                0x0056FFB3,
                                0x006AFFC6,
                                0x007BFFD7,
                                0x00002400,
                                0x00003502,
                                0x00004916,
                                0x00005927,
                                0x00006A38,
                                0x00007B48,
                                0x00008F5C,
                                0x0000A06D,
                                0x0000A875,
                                0x0000B886,
                                0x0014CC9A,
                                0x0025DDAA,
                                0x0036EEBB,
                                0x0046FFCC,
                                0x005AFFE0,
                                0x006BFFF0,
                                0x0000170C,
                                0x0000271D,
                                0x00003B31,
                                0x00004C42,
                                0x00005D52,
                                0x00006E63,
                                0x00008177,
                                0x00009288,
                                0x00009A90,
                                0x0000ABA1,
                                0x0013BFB4,
                                0x0024CFC5,
                                0x0035E0D6,
                                0x0046F1E7,
                                0x005AFFFB,
                                0x006AFFFF,
                                0x00000726,
                                0x00001837,
                                0x00002B4A,
                                0x00003C5B,
                                0x00004D6C,
                                0x00005E7D,
                                0x00007191,
                                0x000082A1,
                                0x00008AA9,
                                0x000E9BBA,
                                0x0022AFCE,
                                0x0033C0DF,
                                0x0043D0EF,
                                0x0054E1FF,
                                0x0068F5FF,
                                0x0079FFFF,
                                0x0000003C,
                                0x0000074C,
                                0x00001B60,
                                0x00002C71,
                                0x00003C82,
                                0x00004D93,
                                0x000061A6,
                                0x001172B7,
                                0x00197ABF,
                                0x002A8BD0,
                                0x003E9EE4,
                                0x004FAFF4,
                                0x005FC0FF,
                                0x0070D1FF,
                                0x0084E4FF,
                                0x0095F5FF

                        };
                        int index = ((i << 1) | (i >> 7)) & 0xff;
                        r = atari_palette[index] & 0xff;
                        g = (atari_palette[index] >> 8) & 0xff;
                        b = (atari_palette[index] >> 16) & 0xff;
                 }
                 break;
                 case PALETTE_ATARI800_NTSC: {
                        static int atari_palette[] = {
                                0x00000000,
                                0x00010101,
                                0x00171516,
                                0x002B292A,
                                0x003F3C3E,
                                0x00534E51,
                                0x006A6467,
                                0x007C767A,
                                0x00857E83,
                                0x00989095,
                                0x00ADA4AA,
                                0x00C0B5BC,
                                0x00D2C6CD,
                                0x00E3D7DF,
                                0x00F8EBF3,
                                0x00FFFCFF,
                                0x00000200,
                                0x00001400,
                                0x00002A07,
                                0x00003D21,
                                0x00004F36,
                                0x0000604B,
                                0x00007562,
                                0x00008676,
                                0x00008E7F,
                                0x00009F92,
                                0x0027B3A7,
                                0x003FC4BA,
                                0x0055D5CB,
                                0x006AE6DD,
                                0x0081F9F2,
                                0x0094FFFF,
                                0x0000000C,
                                0x00000223,
                                0x0000183D,
                                0x00002B51,
                                0x00003D65,
                                0x00004F78,
                                0x0000648F,
                                0x000075A1,
                                0x00007EAA,
                                0x001C8FBC,
                                0x003AA3D2,
                                0x0050B4E4,
                                0x0064C5F5,
                                0x0078D6FF,
                                0x008FEAFF,
                                0x00A2FBFF,
                                0x00000028,
                                0x0000003F,
                                0x0000005A,
                                0x0000156F,
                                0x00002983,
                                0x00003C96,
                                0x001652AD,
                                0x002F64C0,
                                0x00396DC8,
                                0x004E7EDB,
                                0x006593F0,
                                0x0079A4FF,
                                0x008CB6FF,
                                0x009EC7FF,
                                0x00B4DBFF,
                                0x00C6ECFF,
                                0x00000036,
                                0x0000004D,
                                0x00000067,
                                0x0013007C,
                                0x00291890,
                                0x003E2EA4,
                                0x005645BB,
                                0x006957CD,
                                0x007260D6,
                                0x008572E9,
                                0x009B87FE,
                                0x00AD99FF,
                                0x00BFAAFF,
                                0x00D1BCFF,
                                0x00E7D0FF,
                                0x00F8E1FF,
                                0x00100033,
                                0x00250049,
                                0x003D0263,
                                0x00510878,
                                0x00651A8C,
                                0x00782CA0,
                                0x008E42B7,
                                0x00A055C9,
                                0x00A95DD2,
                                0x00BB6FE5,
                                0x00D184FA,
                                0x00E396FF,
                                0x00F4A7FF,
                                0x00FFB8FF,
                                0x00FFCCFF,
                                0x00FFDDFF,
                                0x00420A1A,
                                0x00560F32,
                                0x006D154C,
                                0x00801B62,
                                0x00932777,
                                0x00A6368B,
                                0x00BB4AA1,
                                0x00CD5BB4,
                                0x00D664BD,
                                0x00E875CF,
                                0x00FD89E5,
                                0x00FF9AF6,
                                0x00FFABFF,
                                0x00FFBCFF,
                                0x00FFD0FF,
                                0x00FFE1FF,
                                0x005F1300,
                                0x00721900,
                                0x00891F22,
                                0x009C273B,
                                0x00AE3451,
                                0x00C14366,
                                0x00D6567D,
                                0x00E86790,
                                0x00F16F99,
                                0x00FF80AC,
                                0x00FF94C1,
                                0x00FFA5D3,
                                0x00FFB6E5,
                                0x00FFC6F6,
                                0x00FFDAFF,
                                0x00FFEBFF,
                                0x00631400,
                                0x00761A00,
                                0x008D2300,
                                0x00A02F00,
                                0x00B23E12,
                                0x00C44E31,
                                0x00DA624C,
                                0x00EC7361,
                                0x00F47C6A,
                                0x00FF8D7D,
                                0x00FFA194,
                                0x00FFB2A6,
                                0x00FFC2B9,
                                0x00FFD3CB,
                                0x00FFE7E0,
                                0x00FFF8F1,
                                0x004D0E00,
                                0x00611500,
                                0x00782500,
                                0x008B3600,
                                0x009D4800,
                                0x00B05A00,
                                0x00C56E00,
                                0x00D88017,
                                0x00E0882A,
                                0x00F29945,
                                0x00FFAE60,
                                0x00FFBF75,
                                0x00FFD089,
                                0x00FFE09C,
                                0x00FFF4B3,
                                0x00FFFFC5,
                                0x00210400,
                                0x00361500,
                                0x004D2C00,
                                0x00613F00,
                                0x00745200,
                                0x00876400,
                                0x009D7900,
                                0x00AF8B00,
                                0x00B89300,
                                0x00CAA500,
                                0x00DFB922,
                                0x00F1CA45,
                                0x00FFDB5E,
                                0x00FFEC75,
                                0x00FFFF8E,
                                0x00FFFFA2,
                                0x00000B00,
                                0x00001F00,
                                0x00123500,
                                0x00284800,
                                0x003C5B00,
                                0x00506D00,
                                0x00678200,
                                0x007A9400,
                                0x00839C00,
                                0x0096AD00,
                                0x00ABC200,
                                0x00BED328,
                                0x00D0E34A,
                                0x00E1F464,
                                0x00F6FF7F,
                                0x00FFFF94,
                                0x00001200,
                                0x00002500,
                                0x00003B00,
                                0x00004D00,
                                0x00005F00,
                                0x000E7100,
                                0x002B8600,
                                0x00419700,
                                0x004A9F00,
                                0x005EB100,
                                0x0075C526,
                                0x0088D648,
                                0x009AE661,
                                0x00ADF777,
                                0x00C2FF90,
                                0x00D4FFA4,
                                0x00000F00,
                                0x00002300,
                                0x00003900,
                                0x00004B00,
                                0x00005D00,
                                0x00006E00,
                                0x00008300,
                                0x00009421,
                                0x000D9C31,
                                0x002BAD4B,
                                0x0046C165,
                                0x005AD27A,
                                0x006EE38E,
                                0x0082F3A1,
                                0x0098FFB7,
                                0x00ABFFC9,
                                0x00000500,
                                0x00001800,
                                0x00002F00,
                                0x00004104,
                                0x00005323,
                                0x0000643A,
                                0x00007953,
                                0x00008A67,
                                0x00009270,
                                0x0000A384,
                                0x0029B79A,
                                0x0041C8AC,
                                0x0056D9BE,
                                0x006BE9D0,
                                0x0082FDE5,
                                0x0095FFF7,
                                0x00000003,
                                0x00000719,
                                0x00001E32,
                                0x00003046,
                                0x0000435A,
                                0x0000546D,
                                0x00006984,
                                0x00007A96,
                                0x0000839F,
                                0x000D94B1,
                                0x0031A8C7,
                                0x0048B9D9,
                                0x005DCAEA,
                                0x0071DBFC,
                                0x0088EFFF,
                                0x009BFFFF

                        };
                        int index = ((i << 1) | (i >> 7)) & 0xff;
                        r = atari_palette[index] & 0xff;
                        g = (atari_palette[index] >> 8) & 0xff;
                        b = (atari_palette[index] >> 16) & 0xff;
                 }
                 break;

                 case PALETTE_TEA1002: {
                    switch (i & 0x17) {
                        case 0x0:
                            r=0x0;g=0x0;b=0x0;
                        break;
                        case 0x1:
                            r=0xC3;g=0x0;b=0x1B;
                        break;
                        case 0x2:
                            r=0x7;g=0xBF;b=0x0;
                        break;
                        case 0x3:
                            r=0xC8;g=0xB9;b=0x8;
                        break;
                        case 0x4:
                            r=0x0;g=0x6;b=0xB7;
                        break;
                        case 0x5:
                            r=0xBF;g=0x0;b=0xDF;
                        break;
                        case 0x6:
                            r=0x0;g=0xC6;b=0xA4;
                        break;
                        case 0x7:
                            r=0xFF;g=0xFF;b=0xFF;
                        break;

                        case 0x10:
                            r=0xBF;g=0xBF;b=0xBF;
                        break;
                        case 0x11:
                            r=0x40;g=0xA6;b=0x95;
                        break;
                        case 0x12:
                            r=0x83;g=0x27;b=0x90;
                        break;
                        case 0x13:
                            r=0x5;g=0xD;b=0x68;
                        break;
                        case 0x14:
                            r=0xB9;g=0xB1;b=0x56;
                        break;
                        case 0x15:
                            r=0x3B;g=0x97;b=0x2E;
                        break;
                        case 0x16:
                            r=0x7E;g=0x19;b=0x2A;
                        break;
                        case 0x17:
                            r=0x0;g=0x0;b=0x0;
                        break;
                    }
                 }
                 break;

                 case PALETTE_INTELLIVISION: {
                    switch (i & 0x17) {
                        case 0x0:  //BLACK
                            r=0x0;g=0x0;b=0x0;
                        break;
                        case 0x1:  //BLUE
                            r=0x00;g=0x2D;b=0xFF;
                        break;
                        case 0x2:  //RED
                            r=0xFF;g=0x3D;b=0x10;
                        break;
                        case 0x3:  //TAN
                            r=0xC9;g=0xCF;b=0xAB;
                        break;
                        case 0x4:  //DARK GREEN
                            r=0x38;g=0x6B;b=0x3F;
                        break;
                        case 0x5:  //GREEN
                            r=0x00;g=0xA7;b=0x56;
                        break;
                        case 0x6:  //YELLOW
                            r=0xFA;g=0xEA;b=0x50;
                        break;
                        case 0x7:  //WHITE
                            r=0xFF;g=0xFC;b=0xFF;
                        break;

                        case 0x10: //GREY
                            r=0xBD;g=0xAC;b=0xC8;
                        break;
                        case 0x11: //CYAN
                            r=0x24;g=0xB8;b=0xFF;
                        break;
                        case 0x12: //ORANGE
                            r=0xFF;g=0xB4;b=0x1F;
                        break;
                        case 0x13: //BROWN
                            r=0x54;g=0x6E;b=0x00;
                        break;
                        case 0x14: //PINK
                            r=0xFF;g=0x4E;b=0x57;
                        break;
                        case 0x15: //LIGHT BLUE
                            r=0xA4;g=0x96;b=0xFF;
                        break;
                        case 0x16: //YELLOW GREEN
                            r=0x75;g=0xCC;b=0x80;
                        break;
                        case 0x17: //PURPLE
                            r=0xB5;g=0x1A;b=0x58;
                        break;
                    }
                 }
                 break;


            }

            if ((i & 0x40) && (palette != PALETTE_RrGgBbI)) {
                r ^= 0xff;
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

int get_inhibit_palette_dimming16() {
    if (capinfo->bpp == 16) {
       return inhibit_palette_dimming;
    } else {
       return 0;
    }
}

void osd_write_palette(int new_active) {
    if (capinfo->bpp < 16) {
        if (new_active != old_active) {
            old_active = new_active;
            int num_colours = (capinfo->bpp == 8) ? 256 : 16;
            RPI_PropertyInit();
            if (new_active != 0) {
                RPI_PropertyAddTag(TAG_SET_PALETTE, num_colours, osd_palette_data);
            } else {
                RPI_PropertyAddTag(TAG_SET_PALETTE, num_colours, palette_data);
            }
            RPI_PropertyProcess();
            //log_info("***Palette change %d", new_active);
        }
    }
}

void osd_update_palette() {
    int r = 0;
    int g = 0;
    int b = 0;
    int m = 0;
    int num_colours = (capinfo->bpp >= 8) ? 256 : 16;
    int design_type = (cpld->get_version() >> VERSION_DESIGN_BIT) & 0x0F;

    //copy selected palette to current palette, translating for Atom cpld and inverted Y setting (required for 6847 direct Y connection)

    for (int i = 0; i < num_colours; i++) {

        int i_adj = i;
        if (capinfo->bpp == 8 && capinfo->sample_width >= SAMPLE_WIDTH_9LO) {
            //if capturing 9 or 12bpp to an 8bpp frame buffer bits are captured in the wrong order so rearrange the palette order to match
            //convert B1,G3,G2,R3,R2,B3,B2,R1
            //to      B1,R1,B2,G2,R2,B3,G3,R3
            i_adj = ((i & 0x01) << 6)
                  | ((i & 0x02) << 4)
                  |  (i & 0x8c)
                  | ((i & 0x10) >> 4)
                  | ((i & 0x20) >> 1)
                  | ((i & 0x40) >> 5);
        }

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


        if (((get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_NTSCARTIFACT_CGA && get_parameter(F_NTSC_COLOUR) != 0)
          || (get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_NTSCARTIFACT_BW)
          || (get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_NTSCARTIFACT_BW_AUTO))
          && capinfo->bpp == 8 && capinfo->sample_width <= SAMPLE_WIDTH_6) {
            if ((i & 0x7f) < 0x40) {
                if (get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_NTSCARTIFACT_CGA) {
                    palette_data[i] = create_NTSC_artifact_colours_palette_320(i & 0x7f);
                } else {
                    palette_data[i] = palette_array[get_parameter(F_PALETTE)][i_adj];
                }
            } else {
                int filtered_bitcount = ((i & 0x3f) >> 4) + 1;
                palette_data[i] = create_NTSC_artifact_colours(i & 0x3f, filtered_bitcount);
            }
        } else {
            if (get_feature(F_OUTPUT_INVERT) == INVERT_Y) {
                i_adj ^= 0x12;
            }
            palette_data[i] = palette_array[get_parameter(F_PALETTE)][i_adj];
        }
        palette_data[i] = adjust_palette(palette_data[i]);
    }
/*
    //scan translated palette for equivalences
    for (int i = 0; i < num_colours; i++) {
        for(int j = 0; j < num_colours; j++) {
            if (palette_data[i] == palette_data[j]) {
                equivalence[i] = (char) j;
                break;
            }
        }
    }
*/
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
        if (get_feature(F_OUTPUT_INVERT) == INVERT_RGB) {
            r = 255 - r;
            g = 255 - g;
            b = 255 - b;
            m = 255 - m;
        }
        if (get_feature(F_OUTPUT_COLOUR) != COLOUR_NORMAL) {
             switch (get_feature(F_OUTPUT_COLOUR)) {
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

        palette_data_16[i] = ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
        if (i >= (num_colours >> 1)) {
            osd_palette_data[i] = 0xFFFFFFFF;
        } else {
            if (!inhibit_palette_dimming) {
                osd_palette_data[i] = 0xFF000000 | ((b >> 1) << 16) | ((g >> 1) << 8) | (r >> 1);
            } else {
                osd_palette_data[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
            }
        }

        if ((i >= (num_colours >> 1)) && get_feature(F_SCANLINES)) {
            int scanline_intensity = get_feature(F_SCANLINE_LEVEL) ;
            r = (r * scanline_intensity) / 15;
            g = (g * scanline_intensity) / 15;
            b = (b * scanline_intensity) / 15;
            palette_data[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
        } else {
            palette_data[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }

        if (get_parameter(F_DEBUG)) {
            palette_data[i] |= 0x00101010;
            osd_palette_data[i] |= 0x00101010;
        }
    }

    if (capinfo->bpp < 16) {
        RPI_PropertyInit();
        if (active) {
            RPI_PropertyAddTag(TAG_SET_PALETTE, num_colours, osd_palette_data);
        } else {
            RPI_PropertyAddTag(TAG_SET_PALETTE, num_colours, palette_data);
        }
        RPI_PropertyProcess();
        old_active = active;
    }
}

void osd_clear() {
   if (active) {
      memset(buffer, 32, sizeof(buffer));
      osd_update((uint32_t *) (capinfo->fb + capinfo->pitch * capinfo->height * get_current_display_buffer() + capinfo->pitch * capinfo->v_adjust + capinfo->h_adjust), capinfo->pitch, 0);
      memset(buffer, 0, sizeof(buffer));
      active = 0;
      osd_update_palette();
   }
   osd_hwm = 0;
}

void osd_clear_no_palette() {
   if (active) {
      memset(buffer, 0, sizeof(buffer));
      osd_update((uint32_t *) (capinfo->fb + capinfo->pitch * capinfo->height * get_current_display_buffer() + capinfo->pitch * capinfo->v_adjust + capinfo->h_adjust), capinfo->pitch, 1);
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
      if (get_feature(F_AUTO_SWITCH) >= AUTOSWITCH_MODE7) {

         geometry_set_mode(MODE_SET2);
         cpld->set_mode(MODE_SET2);
         pointer += sprintf(pointer, "sampling2=");
         i = index;
         param = cpld->get_params() + i;
         while(param->key >= 0) {
            pointer += sprintf(pointer, "%d,", cpld->get_value(param->key));
            i++;
            param = cpld->get_params() + i;
         }
         pointer += sprintf(pointer - 1, "\r\n") - 1;
         pointer += sprintf(pointer, "geometry2=");
         i = 1;
         param = geometry_get_params() + i;
         while(param->key >= 0) {
            pointer += sprintf(pointer, "%d,", geometry_get_value(param->key));
            i++;
            param = geometry_get_params() + i;
         }
         pointer += sprintf(pointer - 1, "\r\n") - 1;
      }

      geometry_set_mode(MODE_SET1);
      cpld->set_mode(MODE_SET1);

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
      if ((default_buffer != NULL && i != F_TIMING_SET && i != F_RESOLUTION && i != F_REFRESH && i != F_SCALING && i != F_FRONTEND && i != F_PROFILE && i != F_SAVED_CONFIG && i != F_SUB_PROFILE && i!= F_BUTTON_REVERSE && i != F_HDMI_MODE && (i != F_AUTO_SWITCH || sub_default_buffer == NULL))
          || (default_buffer == NULL && i == F_TIMING_SET && get_feature(F_AUTO_SWITCH) > AUTOSWITCH_MODE7)
          || (default_buffer == NULL && i == F_AUTO_SWITCH) ) {
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
   return file_save(path, name, buffer, pointer - buffer, get_parameter(F_SAVED_CONFIG));
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
   for (int set = 0; set <= MODE_SET2; set++) {
      geometry_set_mode(set);
      cpld->set_mode(set);

      prop = get_prop(buffer, set ? "sampling2" : "sampling");
      if (!prop) {
          prop = get_prop(buffer, "sampling"); //fall back if sampling2 missing
      }
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

      prop = get_prop(buffer, set ? "geometry2" : "geometry");
      if (!prop) {
          prop = get_prop(buffer, "geometry"); //fall back if geometry2 missing
      }
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
      if (i != F_RESOLUTION && i != F_REFRESH && i != F_SCALING && i != F_FRONTEND && i != F_PROFILE && i != F_SAVED_CONFIG && i != F_SUB_PROFILE && i!= F_BUTTON_REVERSE && i != F_HDMI_MODE) {
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

   int cpld_version =  ((cpld->get_version() >> VERSION_DESIGN_BIT) & 0x0F);

   prop = get_prop(buffer, "single_button_mode");
   if (prop) {
       single_button_mode = *prop - '0';
       if (cpld_version == DESIGN_SIMPLE) {
           single_button_mode ^= 1;
       }
       set_menu_table();
       if (single_button_mode) {
           log_info("Single button mode enabled");
       }
   }

   prop = get_prop(buffer, "cpld_firmware_dir");
   if (prop) {

      if ( cpld_version == DESIGN_BBC ) {
           strcpy(cpld_firmware_dir, DEFAULT_CPLD_UPDATE_DIR_3BIT);
      } else if ( cpld_version == DESIGN_ATOM ) {
           strcpy(cpld_firmware_dir, DEFAULT_CPLD_UPDATE_DIR_ATOM);
      } else {
           strcpy(cpld_firmware_dir, prop);
      }
   }
   // Disable CPLDv2 specific features for CPLDv1
   if (cpld->old_firmware_support() & BIT_NORMAL_FIRMWARE_V1) {
      features[F_MODE7_DEINTERLACE].max = M7DEINTERLACE_MA4;
      if (get_feature(F_MODE7_DEINTERLACE) > features[F_MODE7_DEINTERLACE].max) {
         set_feature(F_MODE7_DEINTERLACE, M7DEINTERLACE_MA1); // TODO: Decide whether this is the right fallback
      }
   }
#ifdef USE_ARM_CAPTURE
   if (_get_hardware_id() == _RPI2 || _get_hardware_id() == _RPI3) {
      set_feature(F_MODE7_DEINTERLACE, M7DEINTERLACE_NONE);
   }
#endif
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
      int saved_autoswitch = get_feature(F_AUTO_SWITCH);                   // save autoswitch so it can be disabled to manually switch sub profiles
      process_single_profile(default_buffer);
      process_single_profile(sub_default_buffer);
      set_feature(F_AUTO_SWITCH, saved_autoswitch);
      process_single_profile(sub_profile_buffers[sub_profile_number]);
      cycle_menus();
   }
}

void load_profiles(int profile_number, int save_selected) {
   unsigned int bytes ;
   main_buffer[0] = 0;
   features[F_SUB_PROFILE].max = 0;
   strcpy(sub_profile_names[0], NOT_FOUND_STRING);
   sub_profile_buffers[0][0] = 0;
   if (has_sub_profiles[profile_number]) {
      bytes = file_read_profile(profile_names[profile_number], get_parameter(F_SAVED_CONFIG), DEFAULT_STRING, save_selected, sub_default_buffer, MAX_BUFFER_SIZE - 4);
      if (!bytes) {
         //if auto switching default.txt missing put a default value in buffer
         strcpy(sub_default_buffer,"auto_switch=1\r\n\0");
         log_info("Sub-profile default.txt missing, substituting %s", sub_default_buffer);
      }
      size_t count = 0;
      scan_sub_profiles(sub_profile_names, profile_names[profile_number], &count);
      if (count) {
         features[F_SUB_PROFILE].max = count - 1;
         for (int i = 0; i < count; i++) {
            file_read_profile(profile_names[profile_number], get_parameter(F_SAVED_CONFIG), sub_profile_names[i], 0, sub_profile_buffers[i], MAX_BUFFER_SIZE - 4);
            get_autoswitch_geometry(sub_profile_buffers[i], i);
         }
      }
   } else {
      features[F_SUB_PROFILE].max = 0;
      strcpy(sub_profile_names[0], NONE_STRING);
      sub_profile_buffers[0][0] = 0;
      if (strcmp(profile_names[profile_number], NOT_FOUND_STRING) != 0) {
         file_read_profile(profile_names[profile_number], get_parameter(F_SAVED_CONFIG), NULL, save_selected, main_buffer, MAX_BUFFER_SIZE - 4);
      }
   }
}

int sub_profiles_available(int profile_number) {
   return has_sub_profiles[profile_number];
}

int autoswitch_detect(int one_line_time_ns, int lines_per_vsync, int sync_type) {
   if (has_sub_profiles[get_feature(F_PROFILE)]) {
      log_info("Looking for autoswitch match = %d, %d, %d", one_line_time_ns, lines_per_vsync, sync_type);
      for (int i=0; i <= features[F_SUB_PROFILE].max; i++) {
         //log_info("Autoswitch test: %s (%d) = %d, %d, %d, %d", sub_profile_names[i], i, autoswitch_info[i].lower_limit,
         //          autoswitch_info[i].upper_limit, autoswitch_info[i].lines_per_frame, autoswitch_info[i].sync_type );
         if (   one_line_time_ns > autoswitch_info[i].lower_limit
                && one_line_time_ns < autoswitch_info[i].upper_limit
                && (lines_per_vsync == autoswitch_info[i].lines_per_frame || lines_per_vsync == (autoswitch_info[i].lines_per_frame - 1) || lines_per_vsync == (autoswitch_info[i].lines_per_frame + 1))
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
   strncpy(buffer + line * LINELEN, text, LINELEN);
}

void osd_set(int line, int attr, char *text) {
   osd_set_noupdate(line, attr, text);
   osd_update((uint32_t *) (capinfo->fb + capinfo->pitch * capinfo->height * get_current_display_buffer() + capinfo->pitch * capinfo->v_adjust + capinfo->h_adjust), capinfo->pitch, 1);
}

void osd_set_clear(int line, int attr, char *text) {
   if (capinfo->bpp >= 16) {
       clear_screen();
   }
   osd_set_noupdate(line, attr, text);
   osd_update((uint32_t *) (capinfo->fb + capinfo->pitch * capinfo->height * get_current_display_buffer() + capinfo->pitch * capinfo->v_adjust + capinfo->h_adjust), capinfo->pitch, 0);
}

int osd_active() {
   return active;
}

int menu_active() {
   return ! (osd_state == IDLE || osd_state == DURATION || osd_state == A1_CAPTURE || osd_state == A1_CAPTURE_SUB);
}

void osd_show_cpld_recovery_menu(int update) {
   static char name[] = "CPLD Recovery Menu";
   if (update) {
      strncpy(cpld_firmware_dir, DEFAULT_CPLD_UPDATE_DIR, MIN_STRING_LIMIT);
   } else {
      if (eight_bit_detected()) {
          strncpy(cpld_firmware_dir, DEFAULT_CPLD_FIRMWARE_DIR12, MIN_STRING_LIMIT);
      } else {
          strncpy(cpld_firmware_dir, DEFAULT_CPLD_FIRMWARE_DIR, MIN_STRING_LIMIT);
      }
   }
   update_cpld_menu.name = name;
   current_menu[0] = &main_menu;
   current_item[0] = 0;
   current_menu[1] = &update_cpld_menu;
   current_item[1] = 0;
   depth = 1;
   osd_state = MENU;
   // Change the font size to the large font (no profile will be loaded)
   set_feature(F_FONT_SIZE, FONTSIZE_12X20);
   // Bring up the menu
   osd_refresh();
   if (!update) {
       // Add some warnings
       int line = 6;
       if (eight_bit_detected()) {
           line++;
           osd_set(line++, ATTR_DOUBLE_SIZE,  "IMPORTANT:");
           line++;
           osd_set(line++, 0, "RGBtoHDMI has detected an 8/12 bit board");
           osd_set(line++, 0, "Please select the correct CPLD type");
           osd_set(line++, 0, "for the computer source (See Wiki)");
           line++;
           osd_set(line++, 0, "See Wiki for Atom board CPLD programming");
           line++;
           osd_set(line++, 0, "Hold 3 buttons during reset for this menu.");
       } else {
           osd_set(line++, ATTR_DOUBLE_SIZE,  "IMPORTANT:");
           line++;
           osd_set(line++, 0, "The CPLD type (3_BIT/6-12_BIT) must match");
           osd_set(line++, 0, "the RGBtoHDMI board type you have:");
           line++;
           osd_set(line++, 0, "Use 3_BIT_BBC_CPLD_vxx for Hoglet's");
           osd_set(line++, 0, "   original RGBtoHD (c) 2018 board");
           osd_set(line++, 0, "Use 6-12_BIT_BBC_CPLD_vxx for IanB's");
           osd_set(line++, 0, "   6-bit Issue 2 to 12-bit Issue 4 boards");
           line++;
           osd_set(line++, 0, "See Wiki for Atom board CPLD programming");
           line++;
           osd_set(line++, 0, "Programming the wrong CPLD type may");
           osd_set(line++, 0, "cause damage to your RGBtoHDMI board.");
           osd_set(line++, 0, "Please ask for help if you are not sure.");
           line++;
           osd_set(line++, 0, "Hold 3 buttons during reset for this menu.");
       }
   }
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

void save_configuration() {
    int result = 0;
    int asresult = -1;
    char msg[MAX_STRING_SIZE * 2];
    char path[MAX_STRING_SIZE];
    if (has_sub_profiles[get_feature(F_PROFILE)]) {
       asresult = save_profile(profile_names[get_feature(F_PROFILE)], "Default", save_buffer, NULL, NULL);
       result = save_profile(profile_names[get_feature(F_PROFILE)], sub_profile_names[get_feature(F_SUB_PROFILE)], save_buffer, default_buffer, sub_default_buffer);
       if (get_parameter(F_SAVED_CONFIG) == 0) {
          sprintf(path, "%s/%s.txt", profile_names[get_feature(F_PROFILE)], sub_profile_names[get_feature(F_SUB_PROFILE)]);
       } else {
          sprintf(path, "%s/%s_%d.txt", profile_names[get_feature(F_PROFILE)], sub_profile_names[get_feature(F_SUB_PROFILE)], get_parameter(F_SAVED_CONFIG));
       }
    } else {
       result = save_profile(NULL, profile_names[get_feature(F_PROFILE)], save_buffer, default_buffer, NULL);
       if (get_parameter(F_SAVED_CONFIG) == 0) {
          sprintf(path, "%s.txt", profile_names[get_feature(F_PROFILE)]);
       } else {
          sprintf(path, "%s_%d.txt", profile_names[get_feature(F_PROFILE)], get_parameter(F_SAVED_CONFIG));
       }
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
   static char message[256];
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
            int action;
            if (single_button_mode) {
                if (key_pressed == 0) {
                   if (get_parameter(F_AUTO_SWITCH) == AUTOSWITCH_IIGS_MANUAL || get_parameter(F_AUTO_SWITCH) == AUTOSWITCH_MANUAL) {
                      set_feature(F_TIMING_SET, 1 - get_feature(F_TIMING_SET));
                      osd_state = TIMINGSET_MESSAGE;
                   } else {
                      osd_state = A1_CAPTURE;
                   }

                } else if (key_pressed == 3) {
                    osd_state = A0_LAUNCH;
                } else {
                     osd_state = IDLE;
                }
            } else {
                action = action_map[key_pressed];
                log_debug("Action      = %d", action);
                // Transition to action state
                osd_state = action;
            }
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
      osd_clear();
      break;

   case A1_CAPTURE_SUB:
      // Capture screen shot
      capture_screenshot(capinfo, profile_names[get_feature(F_PROFILE)]);
      // Fire OSD_EXPIRED in 50 frames time
      ret = 4;
      // come back to IDLE
      osd_state = A1_CAPTURE_SUB2;
      break;

   case A1_CAPTURE_SUB2:
      // Fire OSD_EXPIRED in 50 frames time
      ret = 50;
      // come back to IDLE
      osd_state = IDLE;
      break;

   case A2_CLOCK_CAL:
      if (get_parameter(F_AUTO_SWITCH) == AUTOSWITCH_IIGS_MANUAL || get_parameter(F_AUTO_SWITCH) == AUTOSWITCH_MANUAL) {
          set_feature(F_TIMING_SET, 1 - get_feature(F_TIMING_SET));
          ret = 1;
          osd_state = TIMINGSET_MESSAGE;
      } else if (get_parameter(F_NTSC_COLOUR) && (get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_NTSCARTIFACT_CGA || get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_NTSCARTIFACT_BW || get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_NTSCARTIFACT_BW_AUTO)) {
          set_feature(F_NTSC_PHASE, (get_feature(F_NTSC_PHASE) + 1) & 3);
          ret = 1;
          osd_state = NTSC_PHASE_MESSAGE;
      } else {
          // HDMI Calibration
          clear_menu_bits();
          osd_set(0, ATTR_DOUBLE_SIZE, "Enable Genlock");
          // Record the starting value of vsync
          last_vsync = get_parameter(F_VSYNC_INDICATOR);
          // Enable vsync
          set_parameter(F_VSYNC_INDICATOR, 1);
          // Do the actual clock calibration
          if (!is_genlocked()) {
             action_calibrate_clocks();
          }
          // Initialize the counter used to limit the calibration time
          cal_count = 0;
          // Fire OSD_EXPIRED in 50 frames time
          ret = 50;
          // come back to CLOCK_CAL0
          osd_state = CLOCK_CAL0;
      }
      break;

   case NTSC_PHASE_MESSAGE:
      clear_menu_bits();
      sprintf(message, "NTSC Phase %s",phase_names[get_feature(F_NTSC_PHASE)]);
      osd_set(0, ATTR_DOUBLE_SIZE, message);
      // Fire OSD_EXPIRED in 30 frames time
      ret = 30;
      // come back to IDLE
      osd_state = IDLE;
      break;

   case TIMINGSET_MESSAGE:
      clear_menu_bits();
      if (get_parameter(F_AUTO_SWITCH) == AUTOSWITCH_MANUAL) {
          if (get_feature(F_TIMING_SET)) {
             osd_set(0, ATTR_DOUBLE_SIZE, "Timing Set 2");
          } else {
             osd_set(0, ATTR_DOUBLE_SIZE, "Timing Set 1");
          }
      } else {
          if (get_feature(F_TIMING_SET)) {
             osd_set(0, ATTR_DOUBLE_SIZE, "IIGS SHR (Set 2)");
          } else {
             osd_set(0, ATTR_DOUBLE_SIZE, "IIGS Apple II (Set 1)");
          }
      }
      // Fire OSD_EXPIRED in 30 frames time
      ret = 30;
      // come back to IDLE
      osd_state = IDLE;
      break;

   case A3_AUTO_CAL:
      clear_menu_bits();
      osd_set(0, ATTR_DOUBLE_SIZE, "Auto Calibration");
      osd_set(1, 0, "Video must be static during calibration");
      action_calibrate_auto();
      // Fire OSD_EXPIRED in 125 frames time
      ret = 300;
      // come back to IDLE
      osd_state = AUTO_CAL_SAVE;
      break;

   case AUTO_CAL_SAVE:
      osd_state = IDLE;
      if (key == key_enter) {
          ret = 50;
          save_configuration();
      } else {
          ret = 1;
      }
      break;

   case A4_SCANLINES:
      clear_menu_bits();
      if ((capinfo->video_type == VIDEO_PROGRESSIVE) || (capinfo->video_type == !VIDEO_PROGRESSIVE && !(capinfo->detected_sync_type & SYNC_BIT_INTERLACED)) ) {
          set_feature(F_SCANLINES, 1 - get_feature(F_SCANLINES));
          if (get_feature(F_SCANLINES)) {
             osd_set(0, ATTR_DOUBLE_SIZE, "Scanlines on");
          } else {
             osd_set(0, ATTR_DOUBLE_SIZE, "Scanlines off");
          }
      } else {
             osd_set(0, ATTR_DOUBLE_SIZE, "Scanlines inhibited");
      }
      // Fire OSD_EXPIRED in 50 frames time
      ret = 50;
      // come back to IDLE
      osd_state = IDLE;
      break;

   case A5_NTSCCOLOUR:
      set_feature(F_NTSC_COLOUR, 1 - get_feature(F_NTSC_COLOUR));
      ret = 1;
      osd_state = NTSC_MESSAGE;
      break;

   case NTSC_MESSAGE:
      clear_menu_bits();
      if (get_parameter(F_PALETTE_CONTROL) >= PALETTECONTROL_NTSCARTIFACT_CGA) {
          if (get_feature(F_NTSC_COLOUR)) {
             osd_set(0, ATTR_DOUBLE_SIZE, "NTSC Colour on");
          } else {
             osd_set(0, ATTR_DOUBLE_SIZE, "NTSC Colour off");
          }
      } else {
          set_feature(F_NTSC_COLOUR, 0);
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
         set_parameter(F_GENLOCK_MODE, HDMI_ORIGINAL);
      } else {
         cal_count++;
      }
      break;

   case CLOCK_CAL1:
      // Restore the original setting of vsync
      set_parameter(F_VSYNC_INDICATOR, last_vsync);
      osd_clear();
      // back to CLOCK_IDLE
      osd_state = IDLE;
      break;

   case MENU:
    if (single_button_mode) {
        if (key != OSD_EXPIRED) {
             // Remember the original key pressed
             last_key = key;
             // Fire OSD_EXPIRED in 1 frames time
             ret = 1;
             // come back to DURATION
             if (key == OSD_SW1) {
                osd_state = MENU_SUB;
             }
        }
        break;
    }
    // falls into MENU_SUB if not single button mode
   case MENU_SUB:
      if (single_button_mode) {
          // Use duration to determine action
          val = get_key_down_duration(last_key);
          // Descriminate between short and long button press as early as possible
          if (val == 0 || val > LONG_KEY_PRESS_DURATION) {
             if (val) {
                 key = key_enter;
                 set_key_down_duration(last_key, 1);
             } else {
                  if (get_parameter(F_BUTTON_REVERSE)) {
                       key = key_menu_up;
                  } else {
                       key = key_menu_down;
                  }
             }
             osd_state = MENU;
          } else {
              // Fire OSD_EXPIRED in 1 frames time
              ret = 1;
              break;
          }
      }
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
               if (type == I_FEATURE && param_item->param->key == F_RETURN_POSITION) {
                  if (get_parameter(F_RETURN_POSITION)) {
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
               if (get_parameter(F_RETURN_POSITION) == 0)
                  current_item[depth] = 0;
               osd_clear_no_palette();
               redraw_menu();
            }
            break;
         case I_SAVE: {
            save_configuration();
            break;
         }
         case I_TEST:
            if (first_time_press == 0 && get_50hz_state() == 1) {
                osd_clear_no_palette();
                first_time_press = 1;
                osd_set(0, ATTR_DOUBLE_SIZE, test_50hz_ref.name);
                int line = 3;
                osd_set(line++, 0, "Your monitor's EDID data indicates");
                osd_set(line++, 0, "it doesn't support 50Hz, however many");
                osd_set(line++, 0, "such monitors will actually work if the");
                osd_set(line++, 0, "output is forced to 50Hz ignoring the EDID.");
                line++;
                osd_set(line++, 0, "Press menu again to start the 50Hz test");
                osd_set(line++, 0, "or any other button to abort.");
                line++;
                osd_set(line++, 0, "If there is a blank screen after pressing");
                osd_set(line++, 0, "menu then forcing 50Hz will not work.");
                line++;
                osd_set(line++, 0, "However you can still try manually");
                osd_set(line++, 0, "changing the refresh rate to 50Hz and");
                osd_set(line++, 0, "selecting standard 50Hz resolutions such");
                osd_set(line++, 0, "as 720x576, 1280x720 or 1920x1080");
                line++;
                osd_set(line++, 0, "Use menu-reset to recover from no output");
            } else {
                first_time_press = 0;
                osd_state = INFO;
                osd_clear_no_palette();
                redraw_menu();
            }
            break;
         case I_RESTORE:
            if (first_time_press == 0) {
                set_status_message("Press again to confirm restore");
                first_time_press = 1;
            } else {
                first_time_press = 0;
                if (has_sub_profiles[get_feature(F_PROFILE)]) {
                   file_restore(profile_names[get_feature(F_PROFILE)], "Default", get_parameter(F_SAVED_CONFIG));
                   file_restore(profile_names[get_feature(F_PROFILE)], sub_profile_names[get_feature(F_SUB_PROFILE)], get_parameter(F_SAVED_CONFIG));
                } else {
                   file_restore(NULL, profile_names[get_feature(F_PROFILE)], get_parameter(F_SAVED_CONFIG));
                }
                set_feature(F_SAVED_CONFIG, get_parameter(F_SAVED_CONFIG));
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
                if (get_parameter(F_DEBUG)) {
                   int cpld_design = cpld->get_version() >> VERSION_DESIGN_BIT;
                   switch(cpld_design) {
                       case DESIGN_ATOM:
                              sprintf(filename, "%s/ATOM_old/%s.xsvf", cpld_firmware_dir, param_item->param->label);
                              break;
                       case DESIGN_YUV_TTL:
                       case DESIGN_YUV_ANALOG:
                              sprintf(filename, "%s/YUV_old/%s.xsvf", cpld_firmware_dir, param_item->param->label);
                              break;
                       case DESIGN_RGB_TTL:
                       case DESIGN_RGB_ANALOG:
                              sprintf(filename, "%s/RGB_old/%s.xsvf", cpld_firmware_dir, param_item->param->label);
                              break;
                       default:
                       case DESIGN_BBC:
                              sprintf(filename, "%s/BBC_old/%s.xsvf", cpld_firmware_dir, param_item->param->label);
                              break;
                   }
                } else {
                    sprintf(filename, "%s/%s.xsvf", cpld_firmware_dir, param_item->param->label);
                }
                // Reprograme the CPLD
                update_cpld(filename, 1);
            }
            break;
         case I_CALIBRATE:
            if (first_time_press == 0) {
                set_status_message("Press again to confirm calibration");
                first_time_press = 1;
            } else {
                if (first_time_press == 1) {
                    first_time_press = 2;
                    osd_clear();
                    osd_set(0, ATTR_DOUBLE_SIZE, "Auto Calibration");
                    osd_set(1, 0, "Video must be static during calibration");
                    action_calibrate_auto();
                } else {
                    first_time_press = 0;
                    osd_clear();
                    redraw_menu();
                    save_configuration();
                }
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
      if (single_button_mode) {
         if (key != OSD_EXPIRED) {
             // Remember the original key pressed
             last_key = key;
             // Fire OSD_EXPIRED in 1 frames time
             ret = 1;
             // come back to DURATION
             if (key == OSD_SW1) {
                osd_state = PARAM_SUB;
             }
         }
         break;
      }
   // falls into PARAM_SUB if not single button mode
   case PARAM_SUB:
      if (single_button_mode) {
          // Use duration to determine action
          val = get_key_down_duration(last_key);
          // Descriminate between short and long button press as early as possible
          if (val == 0 || val > LONG_KEY_PRESS_DURATION) {
             if (val) {
                 key = key_enter;
                 set_key_down_duration(last_key, 1);
             } else {
                  if (get_parameter(F_BUTTON_REVERSE)) {
                       key = key_value_dec;
                  } else {
                       key = key_value_inc;
                  }
             }
             osd_state = PARAM;
          } else {
              // Fire OSD_EXPIRED in 1 frames time
              ret = 1;
              break;
          }
      }
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

   cpu_clock = get_clock_rate(ARM_CLK_ID)/1000000;
   set_clock_rate_cpu(cpu_clock * 1000000); //sets the old value
   core_clock = get_clock_rate(CORE_CLK_ID)/1000000;
   set_clock_rate_core(core_clock * 1000000);
   sdram_clock = get_clock_rate(SDRAM_CLK_ID)/1000000;
   set_clock_rate_sdram(sdram_clock * 1000000);

   generate_palettes();
   features[F_PALETTE].max  = create_and_scan_palettes(palette_names, palette_array) - 1;

   for (ntsc_palette = 0; ntsc_palette <= features[F_PALETTE].max; ntsc_palette++) {
        if (strcmp(palette_names[ntsc_palette], default_palette_names[PALETTE_XRGB]) == 0) {
            break;
        }
   }

   // default resolution entry of not found
   features[F_RESOLUTION].max = 0;
   strcpy(resolution_names[0], NOT_FOUND_STRING);
   size_t rcount = 0;
   scan_rnames(resolution_names, "/Resolutions/60Hz", ".txt", 9, &rcount);
   size_t old_rcount = rcount;
   scan_rnames(resolution_names, "/Resolutions", ".txt", 4, &rcount);
   if (rcount != old_rcount) {
       log_info("Found non standard resolution: %d)", rcount - old_rcount);
   }
   if (rcount !=0) {
      features[F_RESOLUTION].max = rcount - 1;
      for (int i = 0; i < rcount; i++) {
         log_info("FOUND RESOLUTION: %i, %s", i, resolution_names[i]);
      }
   }

   int Vrefresh_lo = 60;

   rpi_mailbox_property_t *buf;
   int table_offset = 8;
   int blocknum = 0;
   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_GET_EDID_BLOCK, blocknum);
   RPI_PropertyProcess();
   buf = RPI_PropertyGet(TAG_GET_EDID_BLOCK);
   int year = 1990;
   int manufacturer = 0;
   int EDID_extension = 0;
   int supports1080i = 0;
   int supports1080p = 0;
   int detectedwidth = 0;
   int detectedheight = 0;


   if (buf) {
       //for(int a=2;a<34;a++){
       //    log_info("table %d, %08X", (a-2) *4, buf->data.buffer_32[a]);
       //}
       memcpy(EDID_buf + EDID_bufptr, buf->data.buffer_8 + table_offset, 128);
       EDID_bufptr += 128;
       for(int d = (table_offset + 108); d >= (table_offset + 54); d -= 18) {

           if (buf->data.buffer_8[d] != 0 && buf->data.buffer_8[d + 1] != 0) { //search for EDID timing descriptor
               detectedwidth =  buf->data.buffer_8[d + 2] | ((buf->data.buffer_8[d + 4] >> 4) << 8);
               detectedheight =  buf->data.buffer_8[d + 5] | ((buf->data.buffer_8[d + 7] >> 4) << 8);;
               log_info("Timing descriptor detected as %dx%d", detectedwidth, detectedheight);
           }

           if (buf->data.buffer_8[d] == 0 && buf->data.buffer_8[d + 1] == 0 && buf->data.buffer_8[d + 3] == 0xFD) { //search for EDID Display Range Limits Descriptor
               Vrefresh_lo = buf->data.buffer_8[d + 5];
               if ((buf->data.buffer_8[d + 4] & 3) == 3) {
                  Vrefresh_lo += 255;
               }
               log_info("Standard EDID lowest vertical refresh detected as %d Hz", Vrefresh_lo);
           }
       }

       manufacturer = buf->data.buffer_8[table_offset + 9] | (buf->data.buffer_8[table_offset + 8] << 8);  //big endian
       year = year + buf->data.buffer_8[table_offset + 17];

       int extensionblocks = buf->data.buffer_8[table_offset + 126];

       if (extensionblocks > 0 && extensionblocks < 8) {
           EDID_extension = 1;
           for (blocknum = 1; blocknum <= extensionblocks; blocknum++) {
               log_info("Reading EDID extension block #%d", blocknum);
               RPI_PropertyInit();
               RPI_PropertyAddTag(TAG_GET_EDID_BLOCK, blocknum);
               RPI_PropertyProcess();
               buf = RPI_PropertyGet(TAG_GET_EDID_BLOCK);
               if (buf) {
                    memcpy(EDID_buf + EDID_bufptr, buf->data.buffer_8 + table_offset, 128);
                    EDID_bufptr += 128;
                   //for(int a=2;a<34;a++){
                   //    log_info("table %d, %08X", (a-2) *4, buf->data.buffer_32[a]);
                   //}
                   int endptr = buf->data.buffer_8[table_offset + 2];
                   int ptr = 4;
                   if (buf->data.buffer_8[table_offset] == 0x02 && buf->data.buffer_8[table_offset + 1] == 0x03 && endptr != (table_offset + 4)) {   //is it EIA/CEA-861 extension block version 3 with data blocks (HDMI V1 or later)
                       do {
                           //log_info("hdr %x %x", buf->data.buffer_8[table_offset + ptr] & 0xe0,buf->data.buffer_8[table_offset +ptr] & 0x1f);
                           int ptrlen = (buf->data.buffer_8[table_offset + ptr] & 0x1f) + ptr + 1;
                           if ((buf->data.buffer_8[table_offset +ptr] & 0xe0) == 0x40){     // search for Video Data Blocks with Short Video Descriptors
                                ptr++;
                                do {
                                    int mode_num = buf->data.buffer_8[table_offset + ptr] & 0x7f; //mask out preferred bit
                                    if (mode_num >= 17 && mode_num <= 31)  {   // 50Hz Short Video Descriptors
                                        Vrefresh_lo = 50;
                                        if (mode_num == 20) supports1080i = 1;
                                        if (mode_num == 31) supports1080p = 1;
                                    }
                                    //log_info("mode %d", mode_num);
                                    ptr++;
                                } while(ptr < ptrlen);
                                log_info("EIA/CEA-861 EDID SVD lowest vertical refresh detected as %d Hz", Vrefresh_lo);
                           }
                           ptr = ptrlen;
                         //log_info("ptr %x", ptr);
                       } while (ptr < endptr);
                   }
                }
           }
       }
   }

   log_info("Manufacturer = %4X, year = %d", manufacturer, year);

   if (EDID_extension && supports1080i && !supports1080p) {      //could add year limit here as well
       log_info("Monitor has EIA/CEA-861 extension, supports 1080i@50 but doesn't support 1080p@50, limiting 50Hz support");
       Vrefresh_lo = 60;
   }

   log_info("Lowest vertical frequency supported by monitor = %d Hz", Vrefresh_lo);

   int cbytes = file_load("/config.txt", config_buffer, MAX_CONFIG_BUFFER_SIZE);

   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "disable_overclock");
   }
   if (!prop || !cbytes) {
      prop = "0";
   }
   log_info("disable_overclock: %s", prop);
   disable_overclock = atoi(prop);

   int overscan_detected = 0;

   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "overscan_left");
   }
   if (prop) {
      overscan_detected++;
      log_info("overscan_left: %s", prop);
   }
   if (!prop || !cbytes) {
      prop = "0";
   }

   int l = atoi(prop);

   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "overscan_right");
   }
   if (prop) {
      overscan_detected++;
      log_info("overscan_right: %s", prop);
   }
   if (!prop || !cbytes) {
      prop = "0";
   }

   int r = atoi(prop);

   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "overscan_top");
   }
   if (prop) {
      overscan_detected++;
      log_info("overscan_top: %s", prop);
   }
   if (!prop || !cbytes) {
      prop = "0";
   }
   int t = atoi(prop);
   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "overscan_bottom");
   }
   if (prop) {
      overscan_detected++;
      log_info("overscan_bottom: %s", prop);
   }
   if (!prop || !cbytes) {
      prop = "0";
   }

   int b = atoi(prop);

   set_config_overscan(l, r, t, b);

   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "#auto_overscan");
   }
   if (!prop || !cbytes || overscan_detected != 0) {
      prop = "0";
   }
   log_info("#auto_overscan: %s", prop);

   set_startup_overscan(atoi(prop));

   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "hdmi_drive");
   }
   if (!prop || !cbytes) {
      prop = "1";
   }
   log_info("HDMI drive: %s", prop);
   int val = (atoi(prop) - 1) & 1;

   if (val !=0 ) {
       if (cbytes) {
          prop = get_prop_no_space(config_buffer, "hdmi_pixel_encoding");
       }
       if (!prop || !cbytes) {
          prop = "0";
       }
       log_info("HDMI pixel encoding: %s", prop);
       val += atoi(prop);
   }

   set_hdmi(val, 0);

   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "#refresh");
   }
   if (!prop || !cbytes) {
      prop = "0";
   }
   log_info("Read refresh: %s", prop);
   val = atoi(prop);

   set_refresh(val, 0);

   int force_genlock_range = GENLOCK_RANGE_NORMAL;

   switch (val) {
       default:
       case REFRESH_60:
       case REFRESH_50:
       break;
       case REFRESH_EDID:
           if (Vrefresh_lo > 50) {
              force_genlock_range = GENLOCK_RANGE_NORMAL;
           } else {
              force_genlock_range = GENLOCK_RANGE_EDID;
           }
       break;
       case REFRESH_50_60:
           force_genlock_range = GENLOCK_RANGE_FORCE_LOW;
       break;
       case REFRESH_50_ANY:
           force_genlock_range = GENLOCK_RANGE_FORCE_ALL;
       break;
   }

   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "#resolution");
   }
   if (!prop || !cbytes) {
      log_info("New install detected");
      prop = DEFAULT_RESOLUTION;
      force_genlock_range = GENLOCK_RANGE_SET_DEFAULT;
   }

   log_info("Read resolution: %s", prop);

   int auto_detected = 0;
   if (strcmp(prop, DEFAULT_RESOLUTION) == 0)
   {
       auto_detected = 1;
       if (get_refresh() == REFRESH_50) {
           force_genlock_range = REFRESH_50_60;
           log_info("Auto 50Hz detected");
       }
   }

   set_force_genlock_range(force_genlock_range);

   for (int i=0; i< rcount; i++) {
      if (strcmp(resolution_names[i], prop) == 0) {
         log_info("Match resolution: %d %s", i, prop);
         set_resolution(i, prop, 0);
         break;
      }
   }

   int auto_workaround = 0;
   char auto_workaround_path[MAX_NAMES_WIDTH] = "";
   if (strcmp(prop, DEFAULT_RESOLUTION) == 0 && detectedwidth == 2560 && detectedheight == 1440) { //special handling for Auto in 2560x1440 as Pi won't auto detect
       sprintf(auto_workaround_path, "%dx%d/", detectedwidth, detectedheight);
       auto_workaround = 1;
   }

   log_info("Auto %dx%d workaround = %d",detectedwidth, detectedheight, auto_workaround);

   int hdmi_mode_detected = 1;
   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "hdmi_mode");
   }
   if (!prop || !cbytes) {
       hdmi_mode_detected = 0;
   }

   int reboot = 0;
   if (auto_detected != 0 && hdmi_mode_detected != auto_workaround) {
       log_info("reboot %d %d '%s'", hdmi_mode_detected, auto_workaround, auto_workaround_path);
       reboot = 1;
   } else {
       log_info("noreboot %d %d '%s'", hdmi_mode_detected,auto_workaround, auto_workaround_path);
   }
   set_auto_workaround_path(auto_workaround_path, reboot);


   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "#scaling");
   }
   if (!prop || !cbytes) {
      prop = "0";
   }
   log_info("Read scaling: %s", prop);
   val = atoi(prop);
   set_scaling(val, 0);

   if (cbytes) {
      prop = get_prop_no_space(config_buffer, "scaling_kernel");
   }
   if (!prop || !cbytes) {
      prop = "0";
   }
   log_info("Read scaling_kernel: %s", prop);
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
   unsigned int bytes = file_read_profile(ROOT_DEFAULT_STRING, 0, NULL, 0, default_buffer, MAX_BUFFER_SIZE - 4);
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
         val = 0;
         // It can be over-ridden by a local profile.txt file
         sprintf(name, "/profile_%s.txt", cpld->name);
         cbytes = file_load(name, config_buffer, MAX_CONFIG_BUFFER_SIZE);
         if (cbytes) {
            prop = get_prop_no_space(config_buffer, "saved_profile");
            if (!prop) {
               prop = "0";
            }
            val = atoi(prop);
            prop = get_prop_no_space(config_buffer, "profile");
         }
         set_parameter(F_SAVED_CONFIG, val);
         int found_profile = 0;
         if (prop) {
            for (int i=0; i<count; i++) {
               if (strcmp(profile_names[i], prop) == 0) {
                  set_parameter(F_PROFILE, i);
                  load_profiles(i, 0);
                  process_profile(i);
                  set_feature(F_SUB_PROFILE, 0);
                  log_info("Profile = %s", prop);
                  found_profile = 1;
                  break;
               }
            }
            if (!found_profile) {
                  set_parameter(F_PROFILE, 0);
                  load_profiles(0, 0);
                  process_profile(0);
                  set_feature(F_SUB_PROFILE, 0);
            }

         }
      }
   }
   set_menu_table();
}

void osd_update(uint32_t *osd_base, int bytes_per_line, int relocate) {
   if (!active) {
      return;
   }

#if defined(USE_CACHED_SCREEN )
   if (capinfo->video_type == VIDEO_TELETEXT && relocate) {
        osd_base += (CACHED_SCREEN_OFFSET >> 2);
   }
#endif

   if (capinfo->bpp == 16) {
       if (capinfo->video_type == VIDEO_INTERLACED && (capinfo->sync_type & SYNC_BIT_INTERLACED) && get_parameter(F_NORMAL_DEINTERLACE) == DEINTERLACE_NONE) {
           clear_full_screen();
       }
   }

   // SAA5050 character data is 12x20
   int bufferCharWidth = (capinfo->chars_per_line << 3) / 12;         // SAA5050 character data is 12x20
   uint32_t *line_ptr = osd_base;
   int words_per_line = bytes_per_line >> 2;

   if (((capinfo->sizex2 & SIZEX2_DOUBLE_HEIGHT) && capinfo->nlines >  FONT_THRESHOLD * 10)  && (bufferCharWidth >= LINELEN) && get_feature(F_FONT_SIZE) == FONTSIZE_12X20) {       // if frame buffer is large enough and not 8bpp use SAA5050 font
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

void __attribute__ ((aligned (64))) osd_update_fast(uint32_t *osd_base, int bytes_per_line) {
   if (!active) {
      return;
   }
   if (capinfo->bpp == 16 && capinfo->video_type == VIDEO_INTERLACED && (capinfo->detected_sync_type & SYNC_BIT_INTERLACED) && get_parameter(F_NORMAL_DEINTERLACE) == DEINTERLACE_NONE) {
      clear_screen();
   }
   // SAA5050 character data is 12x20
   int bufferCharWidth = (capinfo->chars_per_line << 3) / 12;         // SAA5050 character data is 12x20
   uint32_t *line_ptr = osd_base;
   int words_per_line = bytes_per_line >> 2;

   if (((capinfo->sizex2 & SIZEX2_DOUBLE_HEIGHT) && capinfo->nlines > FONT_THRESHOLD * 10)  && (bufferCharWidth >= LINELEN) && get_feature(F_FONT_SIZE) == FONTSIZE_12X20) {       // if frame buffer is large enough and not 8bpp use SAA5050 font
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
