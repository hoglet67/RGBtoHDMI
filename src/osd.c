#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
#define MONO_BOARD_DEFAULT "Commodore_/Commodore_64_Lumacode_"

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
   "RGBI_(XRGB-Apple)",
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
   "Mono_(3_level_Bright)",
   "Mono_(4_level)",
   "Mono_(6_level)",
   "Mono_(8_level_RGB)",
   "Mono_(8_level_YUV)",
   "TI-99-4a",
   "Spectrum_48K_9Col",
   "Colour_Genie_S24",
   "Colour_Genie_S25",
   "Colour_Genie_N25",
   "Commodore_64",
   "Commodore_64_Rev1",
   "VIC_20",
   "Atari_800_PAL",
   "Atari_800_NTSC",
   "Atari_2600_PAL",
   "Atari_2600_NTSC",
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
   "Commodore 64 YUV",
   "Atari GTIA YUV",
   "4 Bit Lumacode",
   "6/8 Bit Lumacode",
   "8 Bit Lumacode"
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

static const char *pal_odd_names[] = {
   "Off",
   "Blended Colours",
   "All Colours"
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
   {         F_NTSC_PHASE,         "NTSC Phase",        "ntsc_phase", 0,                    3, 1 },
   {          F_NTSC_TYPE,          "NTSC Type",         "ntsc_type", 0,     NUM_NTSCTYPE - 1, 1 },
   {       F_NTSC_QUALITY,       "NTSC Quality",      "ntsc_quality", 0,       NUM_FRINGE - 1, 1 },
   {      F_PAL_ODD_LEVEL,       "PAL Odd Level",     "pal_oddlevel", -180,             180, 1 },
   {       F_PAL_ODD_LINE,       "PAL Odd Line",       "pal_oddline", 0,      NUM_PAL_ODD - 1, 1 },
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

   {         F_PROFILE_NUM,"Custom Profile Num",    "profile_number", 0,                  9, 1 },
   {             F_H_WIDTH,       "Pixel Width",       "pixel_width", 120,               1920, 8 },
   {            F_V_HEIGHT,      "Pixel Height",      "pixel_height", 120,               1200, 2 },
   {               F_CLOCK,   "Clock Frequency",   "pixel_frequency", 1000000,    64000000, 1000 },
   {            F_LINE_LEN,       "Line Length", "pixel_line_length",       100,    5000,    1 },
   {            F_H_OFFSET,          "H Offset",    "pixel_h_offset", -256,               256, 4 },
   {            F_V_OFFSET,          "V Offset",    "pixel_v_offset", -256,               256, 1 },

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
   I_PICKMAN,  // Item is a pick manufacturer
   I_PICKPRO,  // Item is a pick profile
   I_CALIBRATE,// Item is a calibration update
   I_CALIBRATE_NO_SAVE,// Item is a calibration update without saving
   I_TEST,     // Item is a 50 Hz test option
   I_CREATE,    // Item is create profile menu
   I_SAVE_CUSTOM //Item is save custom profile
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
static void info_help_buttons(int line);
static void info_help_calibration(int line);
static void info_help_noise(int line);
static void info_help_flashing(int line);
static void info_help_artifacts(int line);
static void info_help_updates(int line);
static void info_help_custom_profile(int line);

static void info_cal_summary(int line);
static void info_cal_detail(int line);
static void info_cal_raw(int line);
static void info_save_list(int line);
static void info_save_log(int line);
static void info_credits(int line);
static void info_reboot(int line);

static void info_test_50hz(int line);
static void rebuild_geometry_menu(menu_t *menu);
static void rebuild_sampling_menu(menu_t *menu);
static void rebuild_update_cpld_menu(menu_t *menu);
static void rebuild_manufacturer_menu(menu_t *menu);
static void rebuild_profile_menu(menu_t *menu);

static void analyse_timing(int line);

static info_menu_item_t source_summary_ref      = { I_INFO, "Source Summary",      info_source_summary};
static info_menu_item_t system_summary_ref      = { I_INFO, "System Summary",      info_system_summary};
static info_menu_item_t help_buttons_ref        = { I_INFO, "Help Buttons",        info_help_buttons};
static info_menu_item_t help_calibration_ref    = { I_INFO, "Help Calibration",    info_help_calibration};
static info_menu_item_t help_noise_ref          = { I_INFO, "Help Noise",          info_help_noise};
static info_menu_item_t help_flashing_ref       = { I_INFO, "Help Flashing Screen",info_help_flashing};
static info_menu_item_t help_artifacts_ref      = { I_INFO, "Help NTSC Artifacts", info_help_artifacts};
static info_menu_item_t help_updates_ref        = { I_INFO, "Help Software Updates",info_help_updates};
static info_menu_item_t help_custom_profile_ref = { I_INFO, "Help Create Profile", info_help_custom_profile};
static info_menu_item_t cal_summary_ref         = { I_INFO, "Calibration Summary", info_cal_summary};
static info_menu_item_t cal_detail_ref          = { I_INFO, "Calibration Detail",  info_cal_detail};
static info_menu_item_t cal_raw_ref             = { I_INFO, "Calibration Raw",     info_cal_raw};
static info_menu_item_t save_list_ref           = { I_INFO, "Save Profile List",   info_save_list};
static info_menu_item_t save_log_ref            = { I_INFO, "Save Log & EDID",     info_save_log};
static info_menu_item_t credits_ref             = { I_INFO, "Credits",             info_credits};
static info_menu_item_t reboot_ref              = { I_INFO, "Reboot",              info_reboot};


static info_menu_item_t analyse_timing_ref      = { I_INFO, "Analyse Timing",    analyse_timing};

static back_menu_item_t back_ref                = { I_BACK, "Return"};
static action_menu_item_t save_ref              = { I_SAVE, "Save Configuration"};
static action_menu_item_t restore_ref           = { I_RESTORE, "Restore Default Configuration"};
static action_menu_item_t cal_sampling_ref      = { I_CALIBRATE, "Auto Calibrate Video Sampling"};
static action_menu_item_t cal_sampling_no_save_ref      = { I_CALIBRATE_NO_SAVE, "Auto Calibrate Video Sampling"};
static action_menu_item_t save_custom_profile_ref = { I_SAVE_CUSTOM, "Save Custom Profile"};
static info_menu_item_t test_50hz_ref           = { I_TEST, "Test Monitor for 50Hz Support",  info_test_50hz};

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

static menu_t manufacturer_menu = {
   "Select Profile",
   rebuild_manufacturer_menu,
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

static menu_t profile_menu = {
   "Select Profile",
   rebuild_profile_menu,
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
static param_menu_item_t paloddline_ref      = { I_FEATURE, &features[F_PAL_ODD_LINE]     };
static param_menu_item_t paloddlevel_ref     = { I_FEATURE, &features[F_PAL_ODD_LEVEL]     };
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

static param_menu_item_t profile_num_ref     = { I_FEATURE, &features[F_PROFILE_NUM]   };
static param_menu_item_t h_width_ref         = { I_FEATURE, &features[F_H_WIDTH]       };
static param_menu_item_t v_height_ref        = { I_FEATURE, &features[F_V_HEIGHT]      };
static param_menu_item_t clock_ref           = { I_FEATURE, &features[F_CLOCK]         };
static param_menu_item_t line_len_ref        = { I_FEATURE, &features[F_LINE_LEN]      };
static param_menu_item_t h_offset_ref        = { I_FEATURE, &features[F_H_OFFSET]      };
static param_menu_item_t v_offset_ref        = { I_FEATURE, &features[F_V_OFFSET]      };

#ifndef HIDE_INTERFACE_SETTING
static param_menu_item_t frontend_ref        = { I_FEATURE, &features[F_FRONTEND]       };
#endif

static menu_t custom_profile_menu = {
   "Create Custom Profile",
   NULL,
   {
      (base_menu_item_t *) &back_ref,
      (base_menu_item_t *) &analyse_timing_ref,
      (base_menu_item_t *) &h_width_ref,
      (base_menu_item_t *) &v_height_ref,
      (base_menu_item_t *) &clock_ref,
      (base_menu_item_t *) &line_len_ref,
      (base_menu_item_t *) &h_offset_ref,
      (base_menu_item_t *) &v_offset_ref,
      (base_menu_item_t *) &cal_sampling_no_save_ref,
      //(base_menu_item_t *) &palette_ref,
      (base_menu_item_t *) &profile_num_ref,
      (base_menu_item_t *) &save_custom_profile_ref,
      NULL
   }
};

static child_menu_item_t update_cpld_menu_ref  = { I_MENU, &update_cpld_menu };
static child_menu_item_t custom_profile_ref = { I_CREATE, &custom_profile_menu };

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
      (base_menu_item_t *) &help_buttons_ref,
      (base_menu_item_t *) &help_calibration_ref,
      (base_menu_item_t *) &help_artifacts_ref,
      (base_menu_item_t *) &help_flashing_ref,
      (base_menu_item_t *) &help_noise_ref,
      (base_menu_item_t *) &help_custom_profile_ref,
      (base_menu_item_t *) &help_updates_ref,
      (base_menu_item_t *) &save_list_ref,
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
      (base_menu_item_t *) &paloddline_ref,
      (base_menu_item_t *) &paloddlevel_ref,
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
      (base_menu_item_t *) &update_cpld_menu_ref,
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
static child_menu_item_t manufacturer_menu_ref = { I_MENU, &manufacturer_menu };

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
static char filename[MAX_STRING_SIZE * 5];

static char selected_manufacturer[MAX_STRING_SIZE];

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
static char manufacturer_names[MAX_PROFILES][MAX_PROFILE_WIDTH];
static char profile_names[MAX_PROFILES][MAX_PROFILE_WIDTH];
static char sub_profile_names[MAX_SUB_PROFILES][MAX_PROFILE_WIDTH];
static char resolution_names[MAX_NAMES][MAX_NAMES_WIDTH];
static char favourite_names[MAX_FAVOURITES][MAX_PROFILE_WIDTH];
static char current_cpld_prefix[MAX_PROFILE_WIDTH];
static char BBC_cpld_prefix[MAX_PROFILE_WIDTH];
static char RGB_cpld_prefix[MAX_PROFILE_WIDTH];
static char YUV_cpld_prefix[MAX_PROFILE_WIDTH];
static int cpld_prefix_length = 0;

static uint32_t palette_data[256];
static uint32_t osd_palette_data[256];
//static unsigned char equivalence[256];

static char palette_names[MAX_NAMES][MAX_NAMES_WIDTH];
static uint32_t palette_array[MAX_NAMES][MAX_PALETTE_ENTRIES];
static int ntsc_palette = 0;

static int inhibit_palette_dimming = 0;
static int single_button_mode = 0;
static int manufacturer_count = 0;
static int full_profile_count = 0;
static int favourites_count = 0;

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
   int lower_frame_limit;
   int upper_frame_limit;
} autoswitch_info_t;

static autoswitch_info_t autoswitch_info[MAX_SUB_PROFILES];

static char cpld_firmware_dir[MIN_STRING_SIZE] = DEFAULT_CPLD_FIRMWARE_DIR;

// =============================================================
// Private Methods
// =============================================================

void set_menu_table() {
      int index = 0;
      int frontend = get_parameter(F_FRONTEND);
      main_menu.items[index++] = (base_menu_item_t *) &back_ref;
      main_menu.items[index++] = (base_menu_item_t *) &info_menu_ref;
      if (frontend != FRONTEND_SIMPLE) main_menu.items[index++] = (base_menu_item_t *) &palette_menu_ref;
      main_menu.items[index++] = (base_menu_item_t *) &preferences_menu_ref;
      main_menu.items[index++] = (base_menu_item_t *) &settings_menu_ref;
      main_menu.items[index++] = (base_menu_item_t *) &geometry_menu_ref;
      main_menu.items[index++] = (base_menu_item_t *) &sampling_menu_ref;
      main_menu.items[index++] = (base_menu_item_t *) &custom_profile_ref,
      main_menu.items[index++] = (base_menu_item_t *) &save_ref;
      main_menu.items[index++] = (base_menu_item_t *) &restore_ref;
      if (frontend != FRONTEND_SIMPLE) main_menu.items[index++] = (base_menu_item_t *) &cal_sampling_ref;
      main_menu.items[index++] = (base_menu_item_t *) &test_50hz_ref,
      main_menu.items[index++] = (base_menu_item_t *) &hdmi_ref;
      main_menu.items[index++] = (base_menu_item_t *) &resolution_ref;
      main_menu.items[index++] = (base_menu_item_t *) &refresh_ref;
      main_menu.items[index++] = (base_menu_item_t *) &scaling_ref;
      main_menu.items[index++] = (base_menu_item_t *) &saved_ref;
      main_menu.items[index++] = (base_menu_item_t *) &profile_ref;
      main_menu.items[index++] = (base_menu_item_t *) &autoswitch_ref;
      main_menu.items[index++] = (base_menu_item_t *) &subprofile_ref;
      if (get_parameter(F_AUTO_SWITCH) == AUTOSWITCH_IIGS_MANUAL || get_parameter(F_AUTO_SWITCH) == AUTOSWITCH_MANUAL) main_menu.items[index++] = (base_menu_item_t *) &timingset_ref;
      main_menu.items[index++] = (base_menu_item_t *) &manufacturer_menu_ref;
      if (single_button_mode) main_menu.items[index++] = (base_menu_item_t *) &direction_ref;
      main_menu.items[index++] = NULL;
      if (!any_DAC_detected()) {
          features[F_PALETTE_CONTROL].max = PALETTECONTROL_NTSCARTIFACT_BW_AUTO;
      }
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

static int lumacode_multiplier() {
    switch (get_parameter(F_PALETTE_CONTROL)) {
        default:
            return 1;
            break;
        case PALETTECONTROL_ATARI_GTIA:
        case PALETTECONTROL_C64_LUMACODE:
            return 2;
            break;
        case PALETTECONTROL_ATARI_LUMACODE:
            return 3;
            break;
        case PALETTECONTROL_ATARI2600_LUMACODE:
            return 4;
            break;
    }

}

static void autoset_geometry() {
    geometry_set_value(H_ASPECT, 0);
    geometry_set_value(V_ASPECT, 0);

    geometry_set_value(SYNC_TYPE, capinfo->detected_sync_type & SYNC_BIT_MASK);

    if (get_parameter(F_AUTO_SWITCH) == AUTOSWITCH_MODE7 && geometry_get_value(SYNC_TYPE) < SYNC_COMP) {
        set_parameter(F_AUTO_SWITCH, AUTOSWITCH_OFF);
    }

    int fbsize_x2 = 0;
    if (geometry_get_value(MIN_H_WIDTH) <= 400 ) {
        fbsize_x2 |= SIZEX2_DOUBLE_WIDTH;
    }
    if (geometry_get_value(MIN_V_HEIGHT) <= 262 ) {
        fbsize_x2 |= SIZEX2_DOUBLE_HEIGHT;
    }

/*
    int min_h_width = geometry_get_value(MIN_H_WIDTH);
    int limit_h_width = (((geometry_get_value(LINE_LEN) * 92 / 100) + 4) >> 3) << 3;
    if (min_h_width > limit_h_width) {
        min_h_width = limit_h_width;
       geometry_set_value(MIN_H_WIDTH, min_h_width);
       set_parameter(F_H_WIDTH, min_h_width);
    }
*/

    int line_len_min = lumacode_multiplier() * geometry_get_value(MIN_H_WIDTH) * 109 / 100;
    int line_len_max = lumacode_multiplier() * geometry_get_value(MIN_H_WIDTH) * 176 / 100;
    if (geometry_get_value(LINE_LEN) < line_len_min || geometry_get_value(LINE_LEN) > line_len_max) {
        set_status_message("Line length invalid for pixel width");
    }


    if (geometry_get_value(MIN_V_HEIGHT) > get_lines_per_vsync(0)) {
       geometry_set_value(MIN_V_HEIGHT, (get_lines_per_vsync(0) >> 1) << 1);
       set_parameter(F_V_HEIGHT, (get_lines_per_vsync(0) >> 1) << 1);
    }

    geometry_set_value(FB_SIZEX2, fbsize_x2);
    geometry_set_value(LINES_FRAME, get_lines_per_vsync(0));

    int max_h_width = (((geometry_get_value(LINE_LEN) * 75 / 100 / lumacode_multiplier()) + 4) >> 3) << 3;
    if (max_h_width < geometry_get_value(MIN_H_WIDTH)) {
        max_h_width = geometry_get_value(MIN_H_WIDTH);
    }
    geometry_set_value(MAX_H_WIDTH, max_h_width);

    int max_v_height = (((geometry_get_value(LINES_FRAME) * 90 / 100) + 1) >> 1) << 1;
    if (max_v_height < geometry_get_value(MIN_V_HEIGHT)) {
        max_v_height = geometry_get_value(MIN_V_HEIGHT);
    }
    geometry_set_value(MAX_V_HEIGHT, max_v_height);

    int h_offset = ((((geometry_get_value(LINE_LEN) / lumacode_multiplier() - geometry_get_value(MIN_H_WIDTH)) / 2) + 2) >> 2) << 2;
    geometry_set_value(H_OFFSET, h_offset - get_parameter(F_H_OFFSET));

    int v_offset = ((((geometry_get_value(LINES_FRAME) - geometry_get_value(MIN_V_HEIGHT)) / 2) + 1) >> 1) << 1;
    geometry_set_value(V_OFFSET, v_offset - get_parameter(F_V_OFFSET));
}


static int get_feature(int num) {
    return get_parameter(num);
}

static void set_feature(int num, int value) {
   if (value < features[num].min) {
      value = features[num].min;
   }
   if (value > features[num].max) {
      value = features[num].max;
   }
   switch (num) {

   default:
      set_parameter(num, value);
      break;

   case F_H_OFFSET:
      set_parameter(num, value);
      autoset_geometry();
      break;

   case F_V_OFFSET:
      set_parameter(num, value);
      autoset_geometry() ;
      break;

   case F_H_WIDTH:
      int line_len = geometry_get_value(LINE_LEN);
      int line_len_min = lumacode_multiplier() * value * 110 / 100;
      int line_len_max = lumacode_multiplier() * value * 175 / 100;
      set_parameter(num, value);
      geometry_set_value(MIN_H_WIDTH, value);
      if (line_len < line_len_min) {
          set_feature(F_LINE_LEN, line_len_min);
      } else if (line_len > line_len_max) {
          set_feature(F_LINE_LEN, line_len_max);
      }
      autoset_geometry();
      break;

   case F_V_HEIGHT:
      set_parameter(num, value);
      geometry_set_value(MIN_V_HEIGHT, value);
      autoset_geometry() ;
      break;

   case F_CLOCK:
      set_parameter(num, value);
      geometry_set_value(CLOCK, value);
      double one_clock_cycle_ns = 1000000000.0f / (double)value;
      int new_line_len = (int) (((double) get_one_line_time_ns() / one_clock_cycle_ns) + 0.5f);
      geometry_set_value(LINE_LEN, new_line_len);
      set_parameter(F_LINE_LEN, new_line_len);
      autoset_geometry();
      break;

   case F_LINE_LEN:
      set_parameter(num, value);
      geometry_set_value(LINE_LEN, value);
      int new_clock = (int) ((1000000000.0f/((double) get_one_line_time_ns() / value)) + 0.5f);
      geometry_set_value(CLOCK, new_clock);
      set_parameter(F_CLOCK, new_clock);
      autoset_geometry();
      break;

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
      set_scaling(get_parameter(F_SCALING), 1);
      break;
   case F_SAVED_CONFIG:
      set_parameter(num, value);
      load_profiles(get_parameter(F_PROFILE), 1);
      process_profile(get_parameter(F_PROFILE));
      set_feature(F_SUB_PROFILE, 0);
      set_scaling(get_parameter(F_SCALING), 1);
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
   case F_PAL_ODD_LEVEL:
   case F_PAL_ODD_LINE:
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

static void analyse_timing(int line) {
    set_parameter(F_H_OFFSET, 0);
    set_parameter(F_V_OFFSET, 0);
    geometry_set_value(VSYNC_TYPE, 0);
    geometry_set_value(VIDEO_TYPE, 0);
    set_feature(F_H_WIDTH, get_parameter(F_H_WIDTH));
    set_feature(F_LINE_LEN, get_parameter(F_LINE_LEN));
    line++;
    osd_set(line++, 0, "Geometry parameters have been set based on");
    osd_set(line++, 0, "current video source and menu settings.");
    osd_set(line++, 0, "H & V Offsets have been reset to 0.");
}

// Wrapper to extract the name of a menu item
static const char *item_name(base_menu_item_t *item) {
   switch (item->type) {
   case I_MENU:
   case I_CREATE:
      return ((child_menu_item_t *)item)->child->name;
   case I_FEATURE:
   case I_GEOMETRY:
   case I_UPDATE:
   case I_PARAM:
   case I_PICKPRO:
   case I_PICKMAN:
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
    return frontend_names_8[get_parameter(F_FRONTEND)];
    } else {
    return frontend_names_6[get_parameter(F_FRONTEND)];
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
      case F_PAL_ODD_LINE:
         return pal_odd_names[value];
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
    osd_set(line++, 0, osdline);
    sprintf(osdline, "Scaling: %s", scaling_names[get_parameter(F_SCALING)]);
    osd_set(line++, 0, osdline);
    char profile_name[MAX_PROFILE_WIDTH];
    char *position = strchr(profile_names[get_parameter(F_PROFILE)], '/');
    if (position) {
       strcpy(profile_name, position + 1);
    } else {
       strcpy(profile_name, profile_names[get_parameter(F_PROFILE)] + cpld_prefix_length);
    }
    sprintf(osdline, "Profile: %s", profile_name);
    for (int j=0; j < strlen(osdline); j++) {
        if (*(osdline+j) == '_') {
           *(osdline+j) = ' ';
        }
    }
    osd_set(line++, 0, osdline);
    if (has_sub_profiles[get_parameter(F_PROFILE)]) {
        sprintf(osdline, "Sub-Profile: %s", sub_profile_names[get_parameter(F_SUB_PROFILE)]);
        for (int j=0; j < strlen(osdline); j++) {
            if (*(osdline+j) == '_') {
               *(osdline+j) = ' ';
            }
        }
        osd_set(line++, 0, osdline);
    }
#ifdef USE_ARM_CAPTURE
    osd_set(line++, 0, "Warning: ARM Capture Version");
#else
    osd_set(line++, 0, "GPU Capture Version");
#endif
    line++;
    if (get_parameter(F_FRONTEND) != FRONTEND_SIMPLE) {
        osd_set(line++, 0, "Use Auto Calibrate Video Sampling or");
        osd_set(line++, 0, "adjust sampling phase to fix noise");
    }
    line++;
    if (core_clock == 250) { // either a Pi 1 or Pi 2 which can be auto overclocked
        if (disable_overclock) {
            osd_set(line++, 0, "Set disable_overclock=0 in config.txt");
            osd_set(line++, 0, "to enable 9BPP & 12BPP on Pi 1 or Pi 2");
        } else {
            osd_set(line++, 0, "Set disable_overclock=1 in config.txt");
            osd_set(line++, 0, "if you have lockups on Pi 1 or Pi 2");
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
   if (mono_board_detected()) {
       sprintf(message, "      Interface: 8 Bit Analog Mono / LumaCode");
   } else {
       sprintf(message, "      Interface: %s", get_interface_name());
   }
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

static void info_help_buttons(int line) {
   osd_set(line++, 0, "SW1 short press: Menu on");
   osd_set(line++, 0, "SW1 long press: Scan lines on/off");
   osd_set(line++, 0, "SW1 in menu: select option");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "SW2 short press: Screencap");
   osd_set(line++, 0, "SW2 long press: NTSC/PAL artifacts on/off");
   osd_set(line++, 0, "SW2 in menu: cursor down / increase value");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "SW3 short press depends on setting:");
   osd_set(line++, 0, "Normally: Enable/test genlock");
   osd_set(line++, 0, "When NTSC artifacts on: Select Phase");
   osd_set(line++, 0, "When manual autoswitch: Toggle Set 1/2");
   osd_set(line++, 0, "SW3 long press: Auto calibrate sampling");
   osd_set(line++, 0, "SW3 in menu: cursor up / decrease value");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "Press SW2 + SW3 to screencap in menu");
   osd_set(line++, 0, "Press SW1 + SW3 for soft reset");
   osd_set(line++, 0, "(Useful if Pi not modified for reset)");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "During reset:");
   osd_set(line++, 0, "Hold SW1 for default resolution");
   osd_set(line++, 0, "Hold SW1 + SW2 + SW3 for CPLD menu");

}

static void info_help_calibration(int line) {
   osd_set(line++, 0, "To ensure a noise free image, the sampling");
   osd_set(line++, 0, "phase must be set for your particular");
   osd_set(line++, 0, "hardware. This generally only needs to be");
   osd_set(line++, 0, "done once on each machine with the setting");
   osd_set(line++, 0, "then saved to the SD card.");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "To calibrate, display a still image with");
   osd_set(line++, 0, "lots of detail such as many lines of text");
   osd_set(line++, 0, "or detailed graphics. Then either hold SW3");
   osd_set(line++, 0, "until calibration starts or select the");
   osd_set(line++, 0, "option in the main menu.");
   osd_set(line++, 0, "After calibration, press SW1 to save.");
   osd_set(line++, 0, "You can also manually adjust the phase in");
   osd_set(line++, 0, "the sampling menu.");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "If you are using the same profile with");
   osd_set(line++, 0, "several computers of the same type you may");
   osd_set(line++, 0, "have to re-run the calibration when");
   osd_set(line++, 0, "changing between them as they could be");
   osd_set(line++, 0, "different hardware revisions.");
   osd_set(line++, 0, "Alternatively use 'Saved Config' to");
   osd_set(line++, 0, "store multiple different configurations.");
}

static void info_help_flashing(int line) {
   osd_set(line++, 0, "The screen flashing continuously means");
   osd_set(line++, 0, "either the capture size is too big or");
   osd_set(line++, 0, "the timing of the video source doesn't");
   osd_set(line++, 0, "match any of the profiles in the currently");
   osd_set(line++, 0, "selected autoswitch sub-profile set.");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "Try reducing the min/max sizes and offsets");
   osd_set(line++, 0, "in the geometry menu. For timing problems,");
   osd_set(line++, 0, "a match is determined by the following:");
   osd_set(line++, 0, "1 'Lines per Frame' matches detected");
   osd_set(line++, 0, "2 'Sync Type' matches detected");
   osd_set(line++, 0, "3 PPM error < Clock Tolerance (close to 0)");
   osd_set(line++, 0, "The PPM error is determined by the 'Line");
   osd_set(line++, 0, "Length' and 'Clock Frequency' settings.");
   osd_set(line++, 0, "Adjusting the above will display detected");
   osd_set(line++, 0, "info at the top of the screen and the");
   osd_set(line++, 0, "settings should be adjusted to match.");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "The 'Create Custom Profile' menu can help");
   osd_set(line++, 0, "with changing the above settings on a");
   osd_set(line++, 0, "profile without creating a new one.");
}

static void info_help_noise(int line) {
   osd_set(line++, 0, "When set up correctly the output should");
   osd_set(line++, 0, "be noise free. If you are getting noise");
   osd_set(line++, 0, "try adjusting the following:");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "1. Calibration (See 'Help Calibration')");
   osd_set(line++, 0, "2. If using the analog interface try");
   osd_set(line++, 0, "adjusting the DAC levels in the sampling");
   osd_set(line++, 0, "menu.");
   osd_set(line++, 0, "3. If you are getting alternating vertical");
   osd_set(line++, 0, "columns of noisy and clean pixels across");
   osd_set(line++, 0, "the screen, that means the 'Line Length'");
   osd_set(line++, 0, "(in geometry) is set incorrectly so try");
   osd_set(line++, 0, "adjusting that. A test pattern of vertical");
   osd_set(line++, 0, "lines of black and white pixels is the");
   osd_set(line++, 0, "best option for adjusting that but an");
   osd_set(line++, 0, "alternative is to fill the screen with the");
   osd_set(line++, 0, "letter 'M' or 'W'");
   osd_set(line++, 0, "Use 'Save Configuration' after adjusting");
   osd_set(line++, 0, "(See also 'Help Flashing Screen' & wiki)");
}

static void info_help_artifacts(int line) {
   osd_set(line++, 0, "Apple II, IBM CGA and Tandy CoCo use NTSC");
   osd_set(line++, 0, "artifacts to produce colours.");
   osd_set(line++, 0, "To toggle NTSC artifact emulation on/off");
   osd_set(line++, 0, "use a long press of SW2 or menu option.");
   osd_set(line++, 0, "When NTSC artifacts are on, use a short");
   osd_set(line++, 0, "press of SW3 or menu option to change the");
   osd_set(line++, 0, "artifact phase (There are four possible");
   osd_set(line++, 0, "settings which produce different colours).");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "On the Apple II and Tandy CoCo artifacts");
   osd_set(line++, 0, "will be enabled and disabled automatically");
   osd_set(line++, 0, "but you can override with the above.");
   osd_set(line++, 0, "If Apple II artifacts do not switch");
   osd_set(line++, 0, "automatically, try adjusting the Y lo DAC");
   osd_set(line++, 0, "setting (colour burst detect level).");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "For single core Pi models (e.g. zero) only");
   osd_set(line++, 0, "CGA mono mode is supported for artifacts.");
   osd_set(line++, 0, "Full CGA artifact emulation for the four");
   osd_set(line++, 0, "colour modes and the >1K colours produced");
   osd_set(line++, 0, "by the 8088mph demo requires a multicore");
   osd_set(line++, 0, "Pi model (Zero2W, 3 or 4).");
   osd_set(line++, 0, "This may change in a future update.");
}

static void info_help_updates(int line) {
   osd_set(line++, 0, "Latest stable software is available here:");
   osd_set(line++, 0, "https://github.com/hoglet67/RGBtoHDMI");
   osd_set(line++, 0, "/releases");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "Latest beta software with new features and");
   osd_set(line++, 0, "bug fixes is available here:");
   osd_set(line++, 0, "https://github.com/IanSB/RGBtoHDMI");
   osd_set(line++, 0, "/releases");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "Documentation is available here:");
   osd_set(line++, 0, "https://github.com/hoglet67/RGBtoHDMI/wiki");
   osd_set(line++, 0, "or here:");
   osd_set(line++, 0, "https://github.com/IanSB/RGBtoHDMI/wiki");
   osd_set(line++, 0, "");
   osd_set(line++, 0, "Development / support thread:");
   osd_set(line++, 0, "https://stardot.org.uk/forums/");
   osd_set(line++, 0, "viewtopic.php?f=3&t=14430");
   osd_set(line++, 0, "Help is also available via the issues tab");
   osd_set(line++, 0, "on the above github pages");
}

static void info_help_custom_profile(int line) {
   osd_set(line++, 0, "Select an appropriate base profile with");
   osd_set(line++, 0, "suitable bit depth and palette from the");
   osd_set(line++, 0, "'Base Profiles' or use an existing one.");
   osd_set(line++, 0, "Fill the source screen with alternating");
   osd_set(line++, 0, "vertical black and white lines or the");
   osd_set(line++, 0, "letter 'M' or 'W' or similar detail.");
   osd_set(line++, 0, "Select 'Create Custom Profile' and allow");
   osd_set(line++, 0, "it to analyse sync if required.");
   osd_set(line++, 0, "Select Analyse Timing.");
   osd_set(line++, 0, "Set the source H & V pixel resolution.");
   osd_set(line++, 0, "Either set the pixel clock frequency or");
   osd_set(line++, 0, "Line Length (in clock cycles) if known.");
   osd_set(line++, 0, "If not known, adjust the line length until");
   osd_set(line++, 0, "all columns of noise have disappeared.");
   osd_set(line++, 0, "If there is no noise free setting then");
   osd_set(line++, 0, "change the sampling phase and try again.");
   osd_set(line++, 0, "Use H and V Offset to centre the image.");
   osd_set(line++, 0, "Make changes in other menus if required.");
   osd_set(line++, 0, "Run a final auto calibration.");
   osd_set(line++, 0, "Set custom profile number (0-9).");
   osd_set(line++, 0, "Select Save Custom Profile.");
   osd_set(line++, 0, "After rebooting select the new profile.");
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

static void clean_space(char *string) {
int string_length = strlen(string);
    for (int i = 0; i < string_length; i++) {
        if (string[i] == '_' && (string[i + 1] == '_' || string[i + 1] == '/' || string[i + 1] == '\r')) {
            for (int j = i + 1; j < string_length; j++) {
                string[j - 1] = string[j];
            }
        }
        if (string[i] == '_') {
            string[i] = ' ';
        }
    }
}

static void info_save_list(int line) {
char buffer[1024 * 1024];
unsigned int ptr = 0;
    if (mono_board_detected()) {
        ptr += sprintf(buffer, "Profile list for Mono / LumaCode Interface\r\n");
    } else {
        if (any_DAC_detected()) {
        ptr += sprintf(buffer, "Profile list for Analog Interface\r\n");
        } else {
        ptr += sprintf(buffer, "Profile list for Digital Interface\r\n");
        }
    }
    for (int i = 0; i < full_profile_count; i++) {
       ptr += sprintf(buffer + ptr, "%s\r\n", profile_names[i]);
    }
    clean_space(buffer);
    file_save_bin("/Profile_List.txt", buffer, ptr);
    osd_set(line++, 0, "List of profiles for supported computers");
    osd_set(line++, 0, "using current hardware interface saved to");
    osd_set(line++, 0, "SD card as: 'Profile_List.txt'");
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
static param_t manufacturer_params[MAX_PROFILES];
static param_t profile_params[MAX_PROFILES];

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

static void rebuild_manufacturer_menu(menu_t *menu) {
   int i;
   for (i = 0; i < manufacturer_count; i++) {
      manufacturer_params[i].key = i;
      manufacturer_params[i].label = manufacturer_names[i];
   }
   manufacturer_params[i].key = -1;
   rebuild_menu(menu, I_PICKMAN, manufacturer_params);
}

static void rebuild_profile_menu(menu_t *menu) {
   int i;
   int j = 0;
   if (strcmp(selected_manufacturer, FAVOURITES_MENU) == 0) {
       for (i = 0; i < (favourites_count + 1); i++) {
              profile_params[j].key = i;
              profile_params[j].label = favourite_names[i];
              j++;
       }
   } else {
       for (i = 0; i <= features[F_PROFILE].max; i++) {
          if (strncmp(selected_manufacturer, profile_names[i] + cpld_prefix_length, strlen(selected_manufacturer)) == 0) {
              log_info("Found: %s", profile_names[i]);
              profile_params[j].key = i;
              profile_params[j].label = profile_names[i];
              j++;
          }
       }
       if (full_profile_count > (features[F_PROFILE].max + 1)) {
           for (i = features[F_PROFILE].max + 1; i < full_profile_count; i++) {
              if (strncmp(selected_manufacturer, profile_names[i] + cpld_prefix_length, strlen(selected_manufacturer)) == 0) {
                  log_info("Found other CPLD: %s", profile_names[i]);
                  profile_params[j].key = i;
                  profile_params[j].label = profile_names[i];
                  j++;
              }
           }
       }
   }

   profile_params[j].key = -1;
   rebuild_menu(menu, I_PICKPRO, profile_params);
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
   } else if (osd_state == MENU || osd_state == PARAM)  {
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
         char *name = (char*) item_name(item);
         *mp++ = (osd_state != PARAM) ? sel_open : sel_none;
         if ((item)->type == I_PICKMAN) {
             if (name[0] == '_') {
                strcpy(mp, name + 1);
             } else {
                strcpy(mp, name);
             }
             if (mp[strlen(mp) - 1] == '_') {
                mp[strlen(mp) - 1] = 0;
             }
             for (int j=0; j < strlen(mp); j++) {
                if (*(mp+j) == '_') {
                   *(mp+j) = ' ';
                }
             }
         } else if ((item)->type == I_PICKPRO) {
             char *index = strchr(name, '/');
             int offset = 0;
             if (index) {
                index++;

                if (strncmp(name, current_cpld_prefix, cpld_prefix_length) != 0) {
                    if (strncmp(name, BBC_cpld_prefix, cpld_prefix_length) == 0) {
                        strcpy(mp, BBC_cpld_prefix);
                        offset = cpld_prefix_length;
                    }
                    if (strncmp(name, RGB_cpld_prefix, cpld_prefix_length) == 0) {
                        strcpy(mp, RGB_cpld_prefix);
                        offset = cpld_prefix_length;
                    }
                    if (strncmp(name, YUV_cpld_prefix, cpld_prefix_length) == 0) {
                        strcpy(mp, YUV_cpld_prefix);
                        offset = cpld_prefix_length;
                    }
                }
             } else {
                 index = name;
             }
             if (index[0] == '_') {
                 strcpy(mp + offset, index + 1);
             } else {
                 strcpy(mp + offset, index);
             }

             if (mp[strlen(mp) - 1] == '_') {
                mp[strlen(mp) - 1] = 0;
             }
             for (int j=0; j < strlen(mp); j++) {
                if (*(mp+j) == '_') {
                   *(mp+j) = ' ';
                }
             }
         } else {
             strcpy(mp, name);
         }

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
            if ((item)->type == I_FEATURE && ((param_menu_item_t *)item)->param->key == F_PROFILE) {
                char *index = strchr(get_param_string((param_menu_item_t *)item), '/');
                if (index) {
                    index++;
                } else {
                    index = (char*) get_param_string((param_menu_item_t *)item);
                }
                if (index[0] == '_') {
                    strcpy(mp, index + 1);
                } else {
                    strcpy(mp, index);
                }
            } else {
                strcpy(mp, get_param_string((param_menu_item_t *)item));
            }
            int param_len = strlen(mp);
            if (mp[param_len - 1] == '_') {
                mp[param_len - 1] = 0;
                param_len--;
            }
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

int max_palette_count;

    for(int palette = 0; palette < NUM_PALETTES; palette++) {
        max_palette_count = 64;  //default
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
                    max_palette_count = 8;
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
                    max_palette_count = 32;
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
                    max_palette_count = 32;
                    break;

                 case PALETTE_RGBISPECTRUM:
                    m = (i & 0x10) ? 0xff : 0xd7;
                    r = (i & 1) ? m : 0x00;
                    g = (i & 2) ? m : 0x00;
                    b = (i & 4) ? m : 0x00;
                    max_palette_count = 32;
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
                    max_palette_count = 32;
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
                    max_palette_count = 128;
                    break;

                 case PALETTE_MDA:
                    r = (i & 0x20) ? 0xaa : 0x00;
                    r = (i & 0x10) ? (r + 0x55) : r;
                    g = r; b = r;
                    break;

                 case PALETTE_DRAGON_COCO:
                 case PALETTE_DRAGON_COCO_FULL: {
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
                 break;

                 case PALETTE_ATOM_6847_EMULATORS: {
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
                 break;

                 case PALETTE_ATOM_MKII: {
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
                 break;

                 case PALETTE_ATOM_MKII_PLUS:
                 case PALETTE_ATOM_MKII_FULL: {
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
                 case PALETTE_MONO3_BRIGHT:
                    switch (i & 0x12) {
                        case 0x00:
                            r = 0x00; break ;
                        case 0x10:
                        case 0x02:
                            r = 0xaa; break ;
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

                 case PALETTE_MONO8_RGB:
                    switch (i) {
                        /*
                        case g3:
                            r = 0xff;g=0xff;b=0xff; break;
                        case r3:
                        case b3:
                            r = 0xff;g=0xff;b=0x00; break;
                        case g2:
                            r = 0x00;g=0xff;b=0xff; break;
                        case r2:
                            r = 0x00;g=0xff;b=0x00; break;
                        case b2:
                            r = 0xff;g=0x00;b=0xff; break;
                        case g1:
                            r = 0xff;g=0x00;b=0x00; break;
                        case r1:
                        case b1:
                            r = 0x00;g=0x00;b=0xff; break;
                        case 0:
                            r = 0x00;g=0x00;b=0x00; break;
*/

                        case 0:
                            r = 0x00;g=0x00;b=0x00; break;
                        case r1 + b1:
                            r = 0x00;g=0x00;b=0xff; break;
                        case r1 + b1 + g1:
                            r = 0xff;g=0x00;b=0x00; break;
                        case r1 + b2 + g1:
                            r = 0xff;g=0x00;b=0xff; break;
                        case r2 + b2 + g1:
                            r = 0x00;g=0xff;b=0x00; break;
                        case r2 + b2 + g2:
                            r = 0x00;g=0xff;b=0xff; break;
                        case r3 + b3 + g2:
                            r = 0xff;g=0xff;b=0x00; break;
                        case r3 + b3 + g3:
                            r = 0xff;g=0xff;b=0xff; break;

                    }
                    break;

#define u3 0x24    // b-y 3
#define u2 0x04    // b-y 2
#define u1 0x20    // b-y 1
#define u0 0x00    // b-y 0
#define v3 0x09    // r-y 3
#define v2 0x01    // r-y 2
#define v1 0x08    // r-y 1
#define v0 0x00    // r-y 0
#define y3 0x12    // y 3
#define y2 0x02    // y 2
#define y1 0x10    // y 1
#define y0 0x00    // y 0

                 case PALETTE_MONO8_YUV:
                    switch (i) {
                        case 0:
                            r = 0x00;g=0x00;b=0x00; break;
                        case u1:
                            r = 0x00;g=0x00;b=0xff; break;
                        case u1+v1:
                            r = 0xff;g=0x00;b=0x00; break;
                        case u1+v1+y1:
                            r = 0xff;g=0x00;b=0xff; break;
                        case u2+v2+y1:
                            r = 0x00;g=0xff;b=0x00; break;
                        case u3+v3+y1:
                            r = 0x00;g=0xff;b=0xff; break;
                        case u3+v3+y2:
                            r = 0xff;g=0xff;b=0x00; break;
                        case u3+v3+y3:
                            r = 0xff;g=0xff;b=0xff; break;
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

/*
                case PALETTE_C64_REV1:
                case PALETTE_C64: {
                    int revision = palette == PALETTE_C64_REV1 ? 0 : 1;
                    double brightness = 50;
                    double contrast = 100;
                    double saturation = 50;
                    r=g=b=0;
                        switch (i & 0x3f) {
                            case (g0+b1+r1):
                            c64_YUV_palette_lookup[i] = 0;
                            create_colodore_colours(0, revision, brightness, contrast, saturation, &r, &g, &b, &m); //black
                            break;
                            case (g3+b1+r1):
                            c64_YUV_palette_lookup[i] = 15;
                            create_colodore_colours(1, revision, brightness, contrast, saturation, &r, &g, &b, &m); //white
                            break;
                            case (g0+b1+r3):
                            c64_YUV_palette_lookup[i] = 2;
                            create_colodore_colours(2, revision, brightness, contrast, saturation, &r, &g, &b, &m); //red
                            break;
                            case (g3+b3+r0):
                            c64_YUV_palette_lookup[i] = 7;
                            create_colodore_colours(3, revision, brightness, contrast, saturation, &r, &g, &b, &m); //cyan
                            break;
                            case (g1+b3+r3):
                            c64_YUV_palette_lookup[i] = 3;
                            create_colodore_colours(4, revision, brightness, contrast, saturation, &r, &g, &b, &m); //violet
                            break;
                            case (g1+b0+r0):
                            c64_YUV_palette_lookup[i] = 12;
                            create_colodore_colours(5, revision, brightness, contrast, saturation, &r, &g, &b, &m); //green
                            break;

                            case (g0+b3+r1):
                            c64_YUV_palette_lookup[i] = 1;
                            create_colodore_colours(6, revision, brightness, contrast, saturation, &r, &g, &b, &m); //blue
                            break;
                            case (g3+b0+r1):
                            c64_YUV_palette_lookup[i] = 11;
                            create_colodore_colours(7, revision, brightness, contrast, saturation, &r, &g, &b, &m); //yellow
                            break;
                            case (g1+b0+r3):
                            c64_YUV_palette_lookup[i] = 8;
                            create_colodore_colours(8, revision, brightness, contrast, saturation, &r, &g, &b, &m); //orange
                            break;

                            case (g0+b0+r1):
                            c64_YUV_palette_lookup[i] = 4;
                            create_colodore_colours(9, revision, brightness, contrast, saturation, &r, &g, &b, &m); //brown
                            break;
                            case (g1+b1+r3):
                            c64_YUV_palette_lookup[i] = 13;
                            create_colodore_colours(10, revision, brightness, contrast, saturation, &r, &g, &b, &m); //light red
                            break;
                            case (g0+b1+r0):
                            c64_YUV_palette_lookup[i] = 5;
                            create_colodore_colours(11, revision, brightness, contrast, saturation, &r, &g, &b, &m); //dark grey
                            break;

                            case (g1+b1+r1):
                            c64_YUV_palette_lookup[i] = 6;
                            create_colodore_colours(12, revision, brightness, contrast, saturation, &r, &g, &b, &m); //grey2
                            break;
                            case (g3+b0+r0):
                            c64_YUV_palette_lookup[i] = 14;
                            create_colodore_colours(13, revision, brightness, contrast, saturation, &r, &g, &b, &m); //light green
                            break;
                            case (g1+b3+r1):
                            c64_YUV_palette_lookup[i] = 9;
                            create_colodore_colours(14, revision, brightness, contrast, saturation, &r, &g, &b, &m); //light blue
                            break;

                            case (g3+b1+r0):
                            c64_YUV_palette_lookup[i] = 10;
                            create_colodore_colours(15, revision, brightness, contrast, saturation, &r, &g, &b, &m); //light grey
                            break;

                        }

                 }
                 break;
*/

                case PALETTE_COMMODORE64_REV1:
                case PALETTE_COMMODORE64: {
                    static int c64_translate[] = {0, 6, 2, 4, 9, 11, 12, 3, 8, 14, 15, 7, 5, 10, 13, 1};
                    max_palette_count = 16;
                    int revision = palette == PALETTE_COMMODORE64_REV1 ? 0 : 1;
                    double brightness = 50;
                    double contrast = 100;
                    double saturation = 50;
                    create_colodore_colours(c64_translate[i & 0x0f], revision, brightness, contrast, saturation, &r, &g, &b, &m);
                    int index = i & 0x3f;
                    switch (index) {
                        case (g0+b1+r1):
                        c64_YUV_palette_lookup[index] = 0; //black 0
                        break;
                        case (g3+b1+r1):
                        c64_YUV_palette_lookup[index] = 15;  //white 1
                        break;
                        case (g0+b1+r3):
                        c64_YUV_palette_lookup[index] = 2;  //red 2
                        break;
                        case (g3+b3+r0):
                        c64_YUV_palette_lookup[index] = 7;  //cyan 3
                        break;
                        case (g1+b3+r3):
                        c64_YUV_palette_lookup[index] = 3;  //violet 4
                        break;
                        case (g1+b0+r0):
                        c64_YUV_palette_lookup[index] = 12;  //green 5
                        break;
                        case (g0+b3+r1):
                        c64_YUV_palette_lookup[index] = 1;  //blue 6
                        break;
                        case (g3+b0+r1):
                        c64_YUV_palette_lookup[index] = 11; //yellow 7
                        break;
                        case (g1+b0+r3):
                        c64_YUV_palette_lookup[index] = 8; //orange 8
                        break;
                        case (g0+b0+r1):
                        c64_YUV_palette_lookup[index] = 4; //brown 9
                        break;
                        case (g1+b1+r3):
                        c64_YUV_palette_lookup[index] = 13; //light red 10
                        break;
                        case (g0+b1+r0):
                        c64_YUV_palette_lookup[index] = 5; //dark grey 11
                        break;
                        case (g1+b1+r1):
                        c64_YUV_palette_lookup[index] = 6; //grey2 12
                        break;
                        case (g3+b0+r0):
                        c64_YUV_palette_lookup[index] = 14; //light green 13
                        break;
                        case (g1+b3+r1):
                        c64_YUV_palette_lookup[index] = 9; //light blue 14
                        break;
                        case (g3+b1+r0):
                        c64_YUV_palette_lookup[index] = 10; //light grey 15
                        break;

                    }
                 }
                 break;


                case PALETTE_VIC20: {
                       static int c64_translate[] = {0, 6, 2, 4, 9, 11, 12, 3, 8, 14, 15, 7, 5, 10, 13, 1};
                       static int palette[] = {
                            0x000000,
                            0xffffff,
                            0x6d2327,
                            0xa0fef8,
                            0x8e3c97,
                            0x7eda75,
                            0x252390,
                            0xffff86,
                            0xa4643b,
                            0xffc8a1,
                            0xf2a7ab,
                            0xdbffff,
                            0xffb4ff,
                            0xd7ffce,
                            0x9d9aff,
                            0xffffc9
                        };
                        b = palette[c64_translate[i]] & 0xff;
                        g = (palette[c64_translate[i]] >> 8) & 0xff;
                        r = (palette[c64_translate[i]] >> 16) & 0xff;
                        max_palette_count = 16;
                 }
                 break;

                case PALETTE_ATARI2600_PAL: {
                       static int palette[] = {
                            0x00000000,
                            0x28282828,
                            0x50505050,
                            0x74747474,
                            0x94949494,
                            0xB4B4B4B4,
                            0xCFD0D0D0,
                            0xEBECECEC,
                            0x00000000,
                            0x28282828,
                            0x50505050,
                            0x74747474,
                            0x94949494,
                            0xB4B4B4B4,
                            0xCFD0D0D0,
                            0xEBECECEC,
                            0x59005880,
                            0x71207094,
                            0x863C84A8,
                            0x9D589CBC,
                            0xAE70ACCC,
                            0xC184C0DC,
                            0xD29CD0EC,
                            0xE2B0E0FC,
                            0x4A005C44,
                            0x6520785C,
                            0x7E3C9074,
                            0x9858AC8C,
                            0xAD70C0A0,
                            0xC084D4B0,
                            0xD49CE8C4,
                            0xE7B0FCD4,
                            0x40003470,
                            0x5B205088,
                            0x733C68A0,
                            0x8D5884B4,
                            0xA17098C8,
                            0xB584ACDC,
                            0xC99CC0EC,
                            0xDBB0D4FC,
                            0x3C146400,
                            0x5A348020,
                            0x7450983C,
                            0x8D6CB058,
                            0xA384C470,
                            0xB89CD884,
                            0xCBB4E89C,
                            0xDFC8FCB0,
                            0x23140070,
                            0x41342088,
                            0x5C503CA0,
                            0x756C58B4,
                            0x8C8470C8,
                            0xA19C84DC,
                            0xB6B49CEC,
                            0xC9C8B0FC,
                            0x405C5C00,
                            0x5A747420,
                            0x748C8C3C,
                            0x8DA4A458,
                            0xA2B8B870,
                            0xB3C8C884,
                            0xC8DCDC9C,
                            0xDAECECB0,
                            0x2B5C0070,
                            0x47742084,
                            0x5E883C94,
                            0x779C58A8,
                            0x8BB070B4,
                            0x9DC084C4,
                            0xB1D09CD0,
                            0xC3E0B0E0,
                            0x2F703C00,
                            0x4B88581C,
                            0x67A07438,
                            0x7EB48C50,
                            0x96C8A468,
                            0xAADCB87C,
                            0xBDECCC90,
                            0xD1FCE0A4,
                            0x27700058,
                            0x4288206C,
                            0x5BA03C80,
                            0x74B45894,
                            0x89C870A4,
                            0x9CDC84B4,
                            0xB1EC9CC4,
                            0xC3FCB0D4,
                            0x1F702000,
                            0x3B883C1C,
                            0x56A05838,
                            0x70B47450,
                            0x85C88868,
                            0x9CDCA07C,
                            0xAFECB490,
                            0xC3FCC8A4,
                            0x2080003C,
                            0x3C942054,
                            0x56A83C6C,
                            0x6FBC5880,
                            0x85CC7094,
                            0x98DC84A8,
                            0xADEC9CB8,
                            0xBFFCB0C8,
                            0x0F880000,
                            0x2E9C2020,
                            0x49B03C3C,
                            0x63C05858,
                            0x7AD07070,
                            0x8EE08484,
                            0xA5EC9C9C,
                            0xB8FCB0B0,
                            0x00000000,
                            0x28282828,
                            0x50505050,
                            0x74747474,
                            0x94949494,
                            0xB4B4B4B4,
                            0xCFD0D0D0,
                            0xEBECECEC,
                            0x00000000,
                            0x28282828,
                            0x50505050,
                            0x74747474,
                            0x94949494,
                            0xB4B4B4B4,
                            0xCFD0D0D0,
                            0xEBECECEC
                        };
                        r = palette[i & 0x7f] & 0xff;
                        g = (palette[i & 0x7f] >> 8) & 0xff;
                        b = (palette[i & 0x7f] >> 16) & 0xff;
                        m = (palette[i & 0x7f] >> 24) & 0xff;
                        max_palette_count = 128;
                 }
                 break;

                case PALETTE_ATARI2600_NTSC: {
                       static int palette[] = {
                            0x00000000,
                            0x3F404040,
                            0x6B6C6C6C,
                            0x90909090,
                            0xAFB0B0B0,
                            0xC8C8C8C8,
                            0xDCDCDCDC,
                            0xEBECECEC,
                            0x3C004444,
                            0x5A106464,
                            0x79248484,
                            0x9334A0A0,
                            0xAA40B8B8,
                            0xC150D0D0,
                            0xD85CE8E8,
                            0xEB68FCFC,
                            0x38002870,
                            0x51144484,
                            0x68285C98,
                            0x803C78AC,
                            0x934C8CBC,
                            0xA55CA0CC,
                            0xB768B4DC,
                            0xC978C8EC,
                            0x35001884,
                            0x4E183498,
                            0x673050AC,
                            0x7E4868C0,
                            0x935C80D0,
                            0xA67094E0,
                            0xB780A8EC,
                            0xCA94BCFC,
                            0x28000088,
                            0x4520209C,
                            0x5E3C3CB0,
                            0x775858C0,
                            0x8C7070D0,
                            0xA28888E0,
                            0xB6A0A0EC,
                            0xC9B4B4FC,
                            0x2E5C0078,
                            0x4974208C,
                            0x62883CA0,
                            0x7A9C58B0,
                            0x8FB070C0,
                            0xA1C084D0,
                            0xB5D09CDC,
                            0xC7E0B0EC,
                            0x23780048,
                            0x3F902060,
                            0x59A43C78,
                            0x72B8588C,
                            0x88CC70A0,
                            0x9CDC84B4,
                            0xB1EC9CC4,
                            0xC3FCB0D4,
                            0x15840014,
                            0x32982030,
                            0x4DAC3C4C,
                            0x68C05868,
                            0x7ED0707C,
                            0x95E08894,
                            0xABECA0A8,
                            0xBEFCB4BC,
                            0x0F880000,
                            0x2C9C201C,
                            0x4AB04038,
                            0x63C05C50,
                            0x7AD07468,
                            0x90E08C7C,
                            0xA6ECA490,
                            0xB9FCB8A4,
                            0x1C7C1800,
                            0x3990381C,
                            0x55A85438,
                            0x6FBC7050,
                            0x86CC8868,
                            0x99DC9C7C,
                            0xAFECB490,
                            0xC3FCC8A4,
                            0x245C2C00,
                            0x42784C1C,
                            0x5E906838,
                            0x79AC8450,
                            0x90C09C68,
                            0xA6D4B47C,
                            0xBDE8CC90,
                            0xD1FCE0A4,
                            0x282C3C00,
                            0x46485C1C,
                            0x64647C38,
                            0x82809C50,
                            0x9994B468,
                            0xB2ACD07C,
                            0xC6C0E490,
                            0xDDD4FCA4,
                            0x23003C00,
                            0x43205C20,
                            0x63407C40,
                            0x815C9C5C,
                            0x9974B474,
                            0xB38CD08C,
                            0xC9A4E4A4,
                            0xDFB8FCB8,
                            0x26003814,
                            0x481C5C34,
                            0x67387C50,
                            0x8250986C,
                            0x9C68B484,
                            0xB47CCC9C,
                            0xCC90E4B4,
                            0xE2A4FCC8,
                            0x2900302C,
                            0x481C504C,
                            0x66347068,
                            0x824C8C84,
                            0x9C64A89C,
                            0xB478C0B4,
                            0xC888D4CC,
                            0xDF9CECE0,
                            0x2B002844,
                            0x4A184864,
                            0x69306884,
                            0x854484A0,
                            0x9C589CB8,
                            0xB46CB4D0,
                            0xCB7CCCE8,
                            0xDE8CE0FC
                        };
                        r = palette[i & 0x7f] & 0xff;
                        g = (palette[i & 0x7f] >> 8) & 0xff;
                        b = (palette[i & 0x7f] >> 16) & 0xff;
                        m = (palette[i & 0x7f] >> 24) & 0xff;
                        max_palette_count = 128;
                 }
                 break;

                 case PALETTE_ATARI800_PAL: {
                    max_palette_count = 256;
                    //from https://github.com/atari800/atari800/blob/master/src/colours_pal.c#L192

                    //static Colours_setup_t const presets[] = {
                    //    /* Hue, Saturation, Contrast, Brightness, Gamma adjustment, GTIA delay, Black level, White level */
                    //    { 0.0, 0.0, 0.0, 0.0, 2.35, 0.0, 16, 235 }, /* Standard preset */
                    //    { 0.0, 0.0, 0.08, -0.08, 2.35, 0.0, 16, 235 }, /* Deep blacks preset */
                    //    { 0.0, 0.26, 0.72, -0.16, 2.00, 0.0, 16, 235 } /* Vibrant colours & levels preset */

                    static double atari_sat = 0.36f;
                    static double atari_cont = 0.00f;
                    static double atari_brt = 0.00f;

                    double const scaled_black_level = (double)0.0f; //(double)16.0f / 255.0f;  //COLOURS_PAL_setup.black_level
                    double const scaled_white_level = (double)1.0f; //(double)235.0f / 255.0f;  //COLOURS_PAL_setup.white_level


                    struct del_coeff {
                        int add;
                        int mult;
                    };

                    /* Delay coefficients for each hue. */
                    static struct {
                        struct del_coeff even[15];
                        struct del_coeff odd[15];
                    } const del_coeffs = {
                        { { 1, 5 }, /* Hue $1 in even lines */
                          { 1, 6 }, /* Hue $2 in even lines */
                          { 1, 7 },
                          { 0, 0 },
                          { 0, 1 },
                          { 0, 2 },
                          { 0, 4 },
                          { 0, 5 },
                          { 0, 6 },
                          { 0, 7 },
                          { 1, 1 },
                          { 1, 2 },
                          { 1, 3 },
                          { 1, 4 },
                          { 1, 5 } /* Hue $F in even lines */
                        },
                        { { 1, 1 }, /* Hue $1 in odd lines */
                          { 1, 0 }, /* Hue $2 in odd lines */
                          { 0, 7 },
                          { 0, 6 },
                          { 0, 5 },
                          { 0, 4 },
                          { 0, 2 },
                          { 0, 1 },
                          { 0, 0 },
                          { 1, 7 },
                          { 1, 5 },
                          { 1, 4 },
                          { 1, 3 },
                          { 1, 2 },
                          { 1, 1 } /* Hue $F in odd lines */
                        }
                    };
                    int cr, lm;



                    /* NTSC luma multipliers from CGIA.PDF */
                    static double const luma_mult[16] = {
                        0.6941, 0.7091, 0.7241, 0.7401,
                        0.7560, 0.7741, 0.7931, 0.8121,
                        0.8260, 0.8470, 0.8700, 0.8930,
                        0.9160, 0.9420, 0.9690, 1.0000};

                    /* When phase shift between even and odd colorbursts is close to 180 deg, the
                       TV stops interpreting color signal. This value determines how close to 180
                       deg that phase shift must be. It is specific to a TV set. */
                    static double const color_disable_threshold = 0.05;
                    /* Base delay - 1/4.43MHz * base_del = ca. 95.2ns */
                    static double const base_del = 0.421894970414201;
                    /* Additional delay - 1/4.43MHz * add_del = ca. 100.7ns */
                    static double const add_del = 0.446563064859117;
                    /* Delay introduced by the DEL pin voltage. */
                    double const del_adj = 23.2f / 360.0;  //COLOURS_PAL_setup.color_delay chosen by eye to give a smooth rainbow

                    /* Phase delays of colorbursts in even and odd lines. They are equal to
                       Hue 1. */
                    double const even_burst_del = base_del + add_del * del_coeffs.even[0].add + del_adj * del_coeffs.even[0].mult;
                    double const odd_burst_del = base_del + add_del * del_coeffs.odd[0].add + del_adj * del_coeffs.odd[0].mult;

                    /* Reciprocal of the recreated subcarrier's amplitude. */
                    double saturation_mult;
                    /* Phase delay of the recreated amplitude. */
                    double subcarrier_del = (even_burst_del + odd_burst_del + 0.0f) / 2.0;  //COLOURS_PAL_setup.hue

                    /* Phase difference between colorbursts in even and odd lines. */
                    double burst_diff = even_burst_del - odd_burst_del;
                    burst_diff -= floor(burst_diff); /* Normalize to 0..1. */

                    if (burst_diff > 0.5 - color_disable_threshold && burst_diff < 0.5 + color_disable_threshold)
                        /* Shift between colorbursts close to 180 deg. Don't produce color. */
                        saturation_mult = 0.0;
                    else {
                        /* Subcarrier is a sum of two waves with equal frequency and amplitude,
                           but phase-shifted by 2pi*burst_diff. The formula is derived from
                           http://2000clicks.com/mathhelp/GeometryTrigEquivPhaseShift.aspx */
                        double subcarrier_amplitude = sqrt(2.0 * cos(burst_diff*2.0*M_PI) + 2.0);
                        /* Normalise saturation_mult by multiplying by sqrt(2), so that it
                           equals 1.0 when odd & even colorbursts are shifted by 90 deg (ie.
                           burst_diff == 0.25). */
                        saturation_mult = sqrt(2.0) / subcarrier_amplitude;
                    }

                    cr = (i >> 3) & 0x0f;
                    lm = ((i & 7) << 1) | ((i & 0x80) >> 7);

                    double even_u = 0.0;
                    double odd_u = 0.0;
                    double even_v = 0.0;
                    double odd_v = 0.0;
                    if (cr) {
                        struct del_coeff const *even_delay = &(del_coeffs.even[cr - 1]);
                        struct del_coeff const *odd_delay = &(del_coeffs.odd[cr - 1]);
                        double even_del = base_del + add_del * even_delay->add + del_adj * even_delay->mult;
                        double odd_del = base_del + add_del * odd_delay->add + del_adj * odd_delay->mult;
                        double even_angle = (0.5 - (even_del - subcarrier_del)) * 2.0 * M_PI;
                        double odd_angle = (0.5 + (odd_del - subcarrier_del)) * 2.0 * M_PI;
                        double saturation = (atari_sat + 1) * 0.175 * saturation_mult; //COLOURS_PAL_setup.saturation
                        even_u = cos(even_angle) * saturation;
                        even_v = sin(even_angle) * saturation;
                        odd_u = cos(odd_angle) * saturation;
                        odd_v = sin(odd_angle) * saturation;
                    }

                    /* calculate yuv for color entry */
                    double y = (luma_mult[lm] - luma_mult[0]) / (luma_mult[15] - luma_mult[0]);

                    y *= atari_cont * 0.5 + 1;  //COLOURS_PAL_setup.contrast
                    y += atari_brt * 0.5;  //COLOURS_PAL_setup.brightness
                    /* Scale the Y signal's range from 0..1 to
                       scaled_black_level..scaled_white_level */
                    y = y * (scaled_white_level - scaled_black_level) + scaled_black_level;

                    /* The different colors in odd and even lines are not
                       emulated - instead the palette contains averaged values. */
                    double u = (even_u + odd_u) / 2.0;
                    double v = (even_v + odd_v) / 2.0;

                    r = gamma_correct(y + v * 1.13983f, 1);
                    g = gamma_correct(y - u * 0.39465f - v * 0.58060f, 1);
                    b = gamma_correct(y + u * 2.03211f, 1);


/*
                    r = gamma_correct(y + 1.140 * v, 1);
                    g = gamma_correct(y - 0.395 * u - 0.581 * v, 1);
                    b = gamma_correct(y + 2.032 * u, 1);
   */

                    m = gamma_correct(y, 1);

                 }
                 break;



                 case PALETTE_ATARI800_NTSC: {
                    max_palette_count = 256;
                    //from https://github.com/atari800/atari800/blob/master/src/colours_ntsc.c

                    //static Colours_setup_t const presets[] = {
                    //    /* Hue, Saturation, Contrast, Brightness, Gamma adjustment, GTIA delay, Black level, White level */
                    //    { 0.0, 0.0, 0.0, 0.0, 2.35, 0.0, 16, 235 }, /* Standard preset */
                    //    { 0.0, 0.0, 0.08, -0.08, 2.35, 0.0, 16, 235 }, /* Deep blacks preset */
                    //    { 0.0, 0.26, 0.72, -0.16, 2.00, 0.0, 16, 235 } /* Vibrant colours & levels preset */

                    static double atari_hue = 0.00f;
                    static double atari_sat = 0.26f;
                    static double atari_cont = 0.00f;
                    static double atari_brt = 0.00f;
                    double scaled_black_level = (double)0.0f; //(double) 16.0f / 255.0;  //COLOURS_NTSC_setup.black_level
                    double scaled_white_level = (double)1.0f; //(double) 235.0f / 255.0;  //COLOURS_NTSC_setup.white_level

                    static double const colorburst_angle = (303.0f) * PI / 180.0f;
                    double start_angle = colorburst_angle + atari_hue * M_PI;
                    double start_saturation = atari_sat;

                    /* Difference between two consecutive chrominances, in radians. */
                    double color_diff = 26.8f * M_PI / 180.0;   /* color delay, chosen to match color names given in GTIA.PDF */

                    int cr, lm;

                    /* NTSC luma multipliers from CGIA.PDF */
                    double luma_mult[16]={
                        0.6941, 0.7091, 0.7241, 0.7401,
                        0.7560, 0.7741, 0.7931, 0.8121,
                        0.8260, 0.8470, 0.8700, 0.8930,
                        0.9160, 0.9420, 0.9690, 1.0000};

                    cr = (i >> 3) & 0x0f;
                    lm = ((i & 7) << 1) | ((i & 0x80) >> 7);

                    double angle = start_angle + (cr - 1) * color_diff;
                    double saturation = (cr ? (start_saturation + 1) * 0.175f: 0.0);
                    double ii = cos(angle) * saturation;
                    double qq = sin(angle) * saturation;


                    /* calculate yiq for color entry */
                    double y = (luma_mult[lm] - luma_mult[0]) / (luma_mult[15] - luma_mult[0]);
                    y *= atari_cont * 0.5 + 1;  //COLOURS_NTSC_setup.contrast
                    y += atari_brt * 0.5;  //COLOURS_NTSC_setup.brightness
                    /* Scale the Y signal's range from 0..1 to
                    * scaled_black_level..scaled_white_level */
                    y = y * (scaled_white_level - scaled_black_level) + scaled_black_level;

                    r = gamma_correct(y + 0.9563 * ii + 0.6210 * qq, 1);
                    g = gamma_correct(y - 0.2721 * ii - 0.6474 * qq, 1);
                    b = gamma_correct(y - 1.1070 * ii + 1.7046 * qq, 1);
                    m = gamma_correct(y, 1);

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

            if ((i & 0x40) && max_palette_count <= 0x40) {
                r ^= 0xff;  //vsync indicator
            }

            if (m == -1) {  // calculate mono if not already set
                m = ((299 * r + 587 * g + 114 * b + 500) / 1000);
                if (m > 255) {
                   m = 255;
                }
            }

            palette_array[palette][i] = (m << 24) | (b << 16) | (g << 8) | r;
        }
        palette_array[palette][MAX_PALETTE_ENTRIES - 1] = max_palette_count;
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
    int max_palette_count = palette_array[get_parameter(F_PALETTE)][MAX_PALETTE_ENTRIES - 1];

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
            max_palette_count = 128;
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

        if ((i >= (num_colours >> 1)) && get_feature(F_SCANLINES) && max_palette_count <= 128) {
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

    if (get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_C64_LUMACODE || get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_C64_YUV) {
        for (int i=0; i < 256; i++) {
            double R = (double)(palette_data[i & 0x0f] & 0xff) / 255;
            double G = (double)((palette_data[i & 0x0f] >> 8) & 0xff) / 255;
            double B = (double)((palette_data[i & 0x0f] >> 16) & 0xff) / 255;

            double R2 = (double)(palette_data[i >> 4] & 0xff) / 255;
            double G2 = (double)((palette_data[i >> 4] >> 8) & 0xff) / 255;
            double B2 = (double)((palette_data[i >> 4] >> 16) & 0xff) / 255;

            double Y = 0.299 * R + 0.587 * G + 0.114 * B;
            double U = -0.14713 * R - 0.28886 * G + 0.436 * B;
            double V = 0.615 * R - 0.51499 * G - 0.10001 * B;

            //double Y2 = 0.299 * R2 + 0.587 * G2 + 0.114 * B2;
            double U2 = -0.14713 * R2 - 0.28886 * G2 + 0.436 * B2;
            double V2 = 0.615 * R2 - 0.51499 * G2 - 0.10001 * B2;

            double hue = get_parameter(F_PAL_ODD_LEVEL) * PI / 180.0f;
            double U3 = (U2 * cos(hue) - V2 * sin(hue));
            double V3 = (V2 * cos(hue) - U2 * sin(hue));

            if (get_parameter(F_PAL_ODD_LINE) == PAL_ODD_ALL || (get_parameter(F_PAL_ODD_LINE) == PAL_ODD_BLENDED && (i & 0x0f) != (i >> 4))){
                U3 = (U + U3) / 2;
                V3 = (V + V3) / 2;
            } else {
                U3 = (U + U2) / 2;
                V3 = (V + V2) / 2;
            }

            U = (U + U2) / 2;
            V = (V + V2) / 2;

            R = (Y + 1.140 * V);
            G = (Y - 0.396 * U - 0.581 * V);
            B = (Y + 2.029 * U);

            R2 = (Y + 1.140 * V3);
            G2 = (Y - 0.396 * U3 - 0.581 * V3);
            B2 = (Y + 2.029 * U3);

            double normalised_gamma = 1.0f;
            R = gamma_correct(R, normalised_gamma) / 16;
            G = gamma_correct(G, normalised_gamma) / 16;
            B = gamma_correct(B, normalised_gamma) / 16;

            R2 = gamma_correct(R2, normalised_gamma) / 16;
            G2 = gamma_correct(G2, normalised_gamma) / 16;
            B2 = gamma_correct(B2, normalised_gamma) / 16;
            //log_info("%d = %04f, %04f, %04f : %04f, %04f, %04f", i,R,G,B,R2,G2,B2);
            c64_artifact_palette_16[i] = ((int)R2 << 24) | ((int)G2 << 20) | ((int)B2 << 16) | ((int)R << 8) | ((int)G << 4) | (int)B;
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
      osd_update((uint32_t *) (capinfo->fb + capinfo->pitch * capinfo->height * get_current_display_buffer() + capinfo->pitch * capinfo->v_adjust + capinfo->h_adjust), capinfo->pitch, 1);
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

int save_profile(char *path, char *name, char *buffer, char *default_buffer, char *sub_default_buffer, signed int custom_profile)
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
      if ((default_buffer != NULL && i != F_TIMING_SET && i != F_RESOLUTION && i != F_REFRESH && i != F_SCALING && i != F_FRONTEND && i != F_PROFILE && i != F_SAVED_CONFIG && i != F_SUB_PROFILE && i!= F_BUTTON_REVERSE && i != F_HDMI_MODE && i != F_PROFILE_NUM && i != F_H_WIDTH && i != F_V_HEIGHT && i != F_H_OFFSET && i != F_V_OFFSET && i != F_CLOCK && i != F_LINE_LEN && (i != F_AUTO_SWITCH || sub_default_buffer == NULL))
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

   if(custom_profile >= 0 ) {
       char custom_name[MAX_STRING_SIZE];
       sprintf(custom_name, "%s/%s%d_", CUSTOM_PROFILE_FOLDER, CUSTOM_PROFILE_NAME, custom_profile);
       return file_save_custom_profile(custom_name, buffer, pointer - buffer);
   } else {
       if (path != NULL) {
           return file_save(path + cpld_prefix_length, name, buffer, pointer - buffer, get_parameter(F_SAVED_CONFIG));
       } else {
           return file_save(path, name + cpld_prefix_length, buffer, pointer - buffer, get_parameter(F_SAVED_CONFIG));
       }
   }
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
      if (i != F_RESOLUTION && i != F_REFRESH && i != F_SCALING && i != F_FRONTEND && i != F_PROFILE && i != F_SAVED_CONFIG && i != F_SUB_PROFILE && i!= F_BUTTON_REVERSE && i != F_HDMI_MODE && i != F_PROFILE_NUM && i != F_H_WIDTH && i != F_V_HEIGHT && i != F_H_OFFSET && i != F_V_OFFSET && i != F_CLOCK && i != F_LINE_LEN) {
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

   int frame_window = autoswitch_info[index].clock_ppm * autoswitch_info[index].lines_per_frame / 1000000;
   autoswitch_info[index].lower_frame_limit = autoswitch_info[index].lines_per_frame - frame_window;
   autoswitch_info[index].upper_frame_limit = (int) autoswitch_info[index].lines_per_frame + frame_window;

   log_info("Autoswitch timings %d (%s) = %d, %d, %d, %d, %d, %d, %d", index, sub_profile_names[index], autoswitch_info[index].lower_limit, (int) line_time,
            autoswitch_info[index].upper_limit, autoswitch_info[index].lower_frame_limit, autoswitch_info[index].lines_per_frame, autoswitch_info[index].upper_frame_limit, autoswitch_info[index].sync_type);
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
      bytes = file_read_profile(profile_names[profile_number] + cpld_prefix_length, get_parameter(F_SAVED_CONFIG), DEFAULT_STRING, save_selected, sub_default_buffer, MAX_BUFFER_SIZE - 4);
      if (!bytes) {
         //if auto switching default.txt missing put a default value in buffer
         strcpy(sub_default_buffer,"auto_switch=1\r\n\0");
         log_info("Sub-profile default.txt missing, substituting %s", sub_default_buffer);
      }
      size_t count = 0;
      scan_sub_profiles(sub_profile_names, profile_names[profile_number] + cpld_prefix_length, &count);
      if (count) {
         features[F_SUB_PROFILE].max = count - 1;
         for (int i = 0; i < count; i++) {
            file_read_profile(profile_names[profile_number] + cpld_prefix_length, get_parameter(F_SAVED_CONFIG), sub_profile_names[i], 0, sub_profile_buffers[i], MAX_BUFFER_SIZE - 4);
            get_autoswitch_geometry(sub_profile_buffers[i], i);
         }
      }
   } else {
      features[F_SUB_PROFILE].max = 0;
      strcpy(sub_profile_names[0], NONE_STRING);
      sub_profile_buffers[0][0] = 0;
      if (strcmp(profile_names[profile_number], NOT_FOUND_STRING) != 0) {
         file_read_profile(profile_names[profile_number] + cpld_prefix_length, get_parameter(F_SAVED_CONFIG), NULL, save_selected, main_buffer, MAX_BUFFER_SIZE - 4);
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
                && lines_per_vsync >= autoswitch_info[i].lower_frame_limit
                && lines_per_vsync <= autoswitch_info[i].upper_frame_limit
                && sync_type == autoswitch_info[i].sync_type ) {
            log_info("Autoswitch match: %s (%d) = %d, %d, %d, %d, %d", sub_profile_names[i], i, autoswitch_info[i].lower_limit,
                     autoswitch_info[i].upper_limit, autoswitch_info[i].lower_frame_limit, autoswitch_info[i].upper_frame_limit, autoswitch_info[i].sync_type );
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

void osd_show_cpld_recovery_menu(int cpld_fail_state) {
   if (cpld_fail_state == CPLD_NOT_FITTED) {
       current_menu[0] = &main_menu;
       current_item[0] = 0;
       depth = 0;
       osd_state = MENU;
       osd_refresh();
   } else {
       static char name[] = "CPLD Recovery Menu";
       if (cpld_fail_state == CPLD_UPDATE) {
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
       current_menu[1] = &settings_menu;
       current_item[1] = 0;
       current_menu[2] = &update_cpld_menu;
       current_item[2] = 0;
       depth = 2;
       osd_state = MENU;
       // Change the font size to the large font (no profile will be loaded)
       set_feature(F_FONT_SIZE, FONTSIZE_12X20);
       // Bring up the menu
       osd_refresh();
       if (cpld_fail_state != CPLD_UPDATE) {
           // Add some warnings
           int line = 6;
           if (eight_bit_detected()) {
               line++;
               osd_set(line++, ATTR_DOUBLE_SIZE,  "IMPORTANT:");
               line++;
               osd_set(line++, 0, "RGBtoHDMI has detected an 8/12 bit board");
               osd_set(line++, 0, "Please select the correct CPLD type");
               osd_set(line++, 0, "for the computer source (See Wiki).");
               line++;
               osd_set(line++, 0, "See Wiki for Atom board CPLD programming.");
               line++;
               osd_set(line++, 0, "Hold 3 buttons during reset for this menu.");
               line++;
               osd_set(line++, 0, "Note: CPLDs bought from unofficial sources");
               osd_set(line++, 0, "may have already been programmed and that");
               osd_set(line++, 0, "can prevent initial reprogramming.");
               osd_set(line++, 0, "To fix this cut the jumpers JP1,JP2 & JP4.");
               osd_set(line++, 0, "After reprogramming, remake the jumpers");
               osd_set(line++, 0, "with solder blobs for normal operation.");
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
       asresult = save_profile(profile_names[get_feature(F_PROFILE)], "Default", save_buffer, NULL, NULL, -1);
       result = save_profile(profile_names[get_feature(F_PROFILE)], sub_profile_names[get_feature(F_SUB_PROFILE)], save_buffer, default_buffer, sub_default_buffer, -1);
       if (get_parameter(F_SAVED_CONFIG) == 0) {
          sprintf(path, "%s/%s.txt", profile_names[get_feature(F_PROFILE)], sub_profile_names[get_feature(F_SUB_PROFILE)]);
       } else {
          sprintf(path, "%s/%s_%d.txt", profile_names[get_feature(F_PROFILE)], sub_profile_names[get_feature(F_SUB_PROFILE)], get_parameter(F_SAVED_CONFIG));
       }
    } else {
       result = save_profile(NULL, profile_names[get_feature(F_PROFILE)], save_buffer, default_buffer, NULL, -1);
       if (get_parameter(F_SAVED_CONFIG) == 0) {
          sprintf(path, "%s.txt", profile_names[get_feature(F_PROFILE)]);
       } else {
          sprintf(path, "%s_%d.txt", profile_names[get_feature(F_PROFILE)], get_parameter(F_SAVED_CONFIG));
       }
    }
    if (result == 0) {
       char *position = strchr(path, '/');
       if (position) {
           sprintf(msg, "Saved: %s", position + 1);
       } else {
           sprintf(msg, "Saved: %s", path + cpld_prefix_length);
       }

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
      capture_screenshot(capinfo, profile_names[get_feature(F_PROFILE)] + cpld_prefix_length);
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
      char msg[256];
      if (get_parameter(F_AUTO_SWITCH) == AUTOSWITCH_MANUAL) {
          if (get_feature(F_TIMING_SET)) {
             sprintf(msg, "Timing Set 2 (%dMhz)", geometry_get_value(CLOCK) / 1000000);
          } else {
             sprintf(msg, "Timing Set 1 (%dMhz)", geometry_get_value(CLOCK) / 1000000);
          }
          osd_set(0, ATTR_DOUBLE_SIZE, msg);
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
      if (get_parameter(F_PALETTE_CONTROL) >= PALETTECONTROL_NTSCARTIFACT_CGA && get_parameter(F_PALETTE_CONTROL) <= PALETTECONTROL_NTSCARTIFACT_BW_AUTO) {
          if (get_feature(F_NTSC_COLOUR)) {
             osd_set(0, ATTR_DOUBLE_SIZE, "NTSC Color on");
          } else {
             osd_set(0, ATTR_DOUBLE_SIZE, "NTSC Color off");
          }
      } else if (get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_C64_LUMACODE || get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_C64_YUV) {
          if (get_feature(F_NTSC_COLOUR)) {
             osd_set(0, ATTR_DOUBLE_SIZE, "PAL Colour on");
          } else {
             osd_set(0, ATTR_DOUBLE_SIZE, "PAL Colour off");
          }

      } else {
          set_feature(F_NTSC_COLOUR, 0);
          osd_set(0, ATTR_DOUBLE_SIZE, "No PAL/NTSC Artifact");

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
      if (key == key_enter) {
         osd_state = CLOCK_CAL1;
         osd_set(0, ATTR_DOUBLE_SIZE, "Genlock Aborted");
         ret = 5;
      } else if (is_genlocked()) {
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

         case I_CREATE:
            if (get_sync_detected() == 0) {
                if (first_time_press == 0) {
                    set_status_message("No sync: Press again to analyse");
                    first_time_press = 1;
                } else {
                    set_status_message("");
                    first_time_press = 0;
                    int sync_type = cpld->analyse(capinfo->detected_sync_type ^ SYNC_BIT_COMPOSITE_SYNC, 1);
                    geometry_set_value(SYNC_TYPE, sync_type & SYNC_BIT_MASK);
                    if (get_parameter(F_AUTO_SWITCH) == AUTOSWITCH_MODE7 && geometry_get_value(SYNC_TYPE) < SYNC_COMP) {
                        set_parameter(F_AUTO_SWITCH, AUTOSWITCH_OFF);
                    }
                }
            } else {
                first_time_press = 0;
                set_parameter(F_H_WIDTH, geometry_get_value(MIN_H_WIDTH));
                set_parameter(F_V_HEIGHT, geometry_get_value(MIN_V_HEIGHT));
                set_parameter(F_CLOCK, geometry_get_value(CLOCK));
                set_parameter(F_LINE_LEN, geometry_get_value(LINE_LEN));
                depth++;
                current_menu[depth] = child_item->child;
                current_item[depth] = 0;
                // Rebuild dynamically populated menus, e.g. the sampling and geometry menus that are mode specific
                if (child_item->child && child_item->child->rebuild) {
                   child_item->child->rebuild(child_item->child);
                }
                osd_clear_no_palette();
                redraw_menu();
            }
            break;


         case I_PICKMAN:
             strcpy(selected_manufacturer, item_name(item));
             log_info("Selected Manufacturer = %s", selected_manufacturer);
             depth++;
             current_menu[depth] = &profile_menu;
             current_item[depth] = 0;
             // Rebuild dynamically populated menus, e.g. the sampling and geometry menus that are mode specific
             osd_refresh();
             osd_clear_no_palette();
             redraw_menu();
             break;
         case I_PICKPRO:
             log_info("Selected Profile = %s %s %d", item_name(item), favourite_names[favourites_count], first_time_press);
             if (strcmp(favourite_names[favourites_count], item_name(item)) == 0) {
                 favourites_count = 0;
                 strcpy(favourite_names[favourites_count], FAVOURITES_MENU_CLEAR);
                 depth--;
                 osd_refresh();
                 osd_clear_no_palette();
                 redraw_menu();
             } else {
             int duplicate = 0;
             for(int i = 0; i < favourites_count; i++) {
                 if (strcmp(favourite_names[i], item_name(item)) == 0) {
                     duplicate = 1;
                     break;
                 }
             }
             if (!duplicate) {
                 if (favourites_count >= MAX_FAVOURITES) {
                     for (int i = 1; i < MAX_FAVOURITES; i++) {
                         strcpy(favourite_names[i - 1], favourite_names[i]);
                     }
                     favourites_count--;
                 }
                 strcpy(favourite_names[favourites_count], item_name(item));
                 favourites_count++;
                 unsigned int bytes_written = 0;
                 for(int i = 0; i < favourites_count; i++) {
                    sprintf((char*)(config_buffer + bytes_written), "favourite%d=%s\r\n", i, favourite_names[i]);
                    log_info("Writing favourite%d=%s", i, favourite_names[i]);
                    bytes_written += strlen((char*) (config_buffer + bytes_written));
                 }
                 file_save_bin(FAVOURITES_PATH, config_buffer, bytes_written);
             }
             strcpy(favourite_names[favourites_count], FAVOURITES_MENU_CLEAR);
             if (strncmp(current_cpld_prefix, item_name(item), cpld_prefix_length) != 0) {
                char msg[256];
                if (first_time_press == 0) {
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
                    if (strncmp(BBC_cpld_prefix, item_name(item), cpld_prefix_length) == 0) {
                        write_profile_choice((char*)item_name(item) + cpld_prefix_length, 0, (char*)cpld->nameBBC);
                    }
                    if (strncmp(RGB_cpld_prefix, item_name(item), cpld_prefix_length) == 0) {
                        write_profile_choice((char*)item_name(item) + cpld_prefix_length, 0, (char*)cpld->nameRGB);
                    }
                    if (strncmp(YUV_cpld_prefix, item_name(item), cpld_prefix_length) == 0) {
                        write_profile_choice((char*)item_name(item) + cpld_prefix_length, 0, (char*)cpld->nameYUV);
                    }
                    int count;
                    char cpld_dir[MAX_STRING_SIZE];
                    strncpy(cpld_dir, cpld_firmware_dir, MAX_STRING_LIMIT);
                    scan_cpld_filenames(cpld_filenames, cpld_dir, &count);
                    char cpld_type[MAX_STRING_SIZE];
                    strcpy(cpld_type, item_name(item) + 1);
                    cpld_type[strlen(cpld->nameprefix)] = 0;
                    for(int i = 0; i < count; i++) {
                        if(strstr(cpld_filenames[i], cpld_type) != 0) {
                            sprintf(filename, "%s/%s.xsvf", cpld_firmware_dir, cpld_filenames[i]);
                            // Reprograme the CPLD
                            update_cpld(filename, 1);
                            break;
                        }
                    }
                    log_info("CPLD update failed");
                    sprintf(msg, "CPLD update failed");
                }
            } else {
                depth = 0;
                current_menu[depth] = &main_menu;
                current_item[depth] = 0;
                for (int i=0; i<= features[F_PROFILE].max; i++) {
                   if (strcmp(profile_names[i], item_name(item)) == 0) {
                      set_parameter(F_SAVED_CONFIG, 0);
                      set_parameter(F_PROFILE, i);
                      load_profiles(i, 1);
                      process_profile(i);
                      set_feature(F_SUB_PROFILE, 0);
                      set_scaling(get_parameter(F_SCALING), 1);
                      log_info("Profile now = %s", item_name(item));
                      break;
                   }
                }
            }


            // Rebuild dynamically populated menus, e.g. the sampling and geometry menus that are mode specific
            osd_refresh();
            osd_clear_no_palette();
            redraw_menu();
             }
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
               if (current_menu[depth] == &profile_menu) {
                   depth--;
                   osd_refresh();
               } else {
                   depth--;
               }
               if (get_parameter(F_RETURN_POSITION) == 0 && current_menu[depth] != &manufacturer_menu)
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
                   file_restore(profile_names[get_feature(F_PROFILE)] + cpld_prefix_length, "Default", get_parameter(F_SAVED_CONFIG));
                   file_restore(profile_names[get_feature(F_PROFILE)] + cpld_prefix_length, sub_profile_names[get_feature(F_SUB_PROFILE)], get_parameter(F_SAVED_CONFIG));
                } else {
                   file_restore(NULL, profile_names[get_feature(F_PROFILE)] + cpld_prefix_length, get_parameter(F_SAVED_CONFIG));
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

         case I_CALIBRATE_NO_SAVE:
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
                }
            }
            break;

         case I_SAVE_CUSTOM:
            char path[MAX_STRING_SIZE];
            sprintf(path, "%s/%s/%s/%s%d_.txt", PROFILE_BASE, cpld->name, CUSTOM_PROFILE_FOLDER, CUSTOM_PROFILE_NAME, get_feature(F_PROFILE_NUM));
            if (first_time_press == 0 && test_file(path)) {
                set_status_message("Press again to confirm file overwrite");
                first_time_press = 1;
            } else {
                first_time_press = 0;
                int result = 0;
                geometry_set_value(H_ASPECT, get_haspect());
                geometry_set_value(V_ASPECT, get_vaspect());
                result = save_profile(NULL, NULL, save_buffer, default_buffer, NULL, get_feature(F_PROFILE_NUM));
                int line = 15;
                if (result == 0) {
                    char temp[MAX_STRING_SIZE];
                    sprintf(temp, "Profile saved as: %s%d", CUSTOM_PROFILE_NAME, get_feature(F_PROFILE_NUM));
                    osd_set(line++, 0, temp);
                    osd_set(line++, 0, "To folder:");
                    sprintf(path, "%s/%s/%s", PROFILE_BASE, cpld->name, CUSTOM_PROFILE_FOLDER);
                    osd_set(line++, 0, path);
                    line++;
                    osd_set(line++, 0, "After rebooting, the new profile will");
                    osd_set(line++, 0, "be selected automatically. Following that");
                    osd_set(line++, 0, "the profile can also be accessed from the");
                    osd_set(line++, 0, "Custom entry in the Select Profile menu.");
                    sprintf(path, "%s/%s%d_", CUSTOM_PROFILE_FOLDER, CUSTOM_PROFILE_NAME, get_feature(F_PROFILE_NUM));
                    write_profile_choice(path, get_parameter(F_SAVED_CONFIG), (char*) cpld->name);
                    set_general_reboot();
                } else {
                    set_status_message("Error saving Custom Profile");
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
       if (get_parameter(F_REFRESH) == REFRESH_50) {
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


   char name[100];
   char cpld_path[100] = "/Profiles/";
   char bbcpath[100] = "/Profiles/";
   char rgbpath[100] = "/Profiles/";
   char yuvpath[100] = "/Profiles/";

   strncat(cpld_path, cpld->name, 80);
   strncat(bbcpath, cpld->nameBBC, 80);
   strncat(rgbpath, cpld->nameRGB, 80);
   strncat(yuvpath, cpld->nameYUV, 80);

   sprintf (current_cpld_prefix, "(%s)_", cpld->nameprefix);
   sprintf (BBC_cpld_prefix, "(%s)_", cpld->nameBBCprefix);
   sprintf (RGB_cpld_prefix, "(%s)_", cpld->nameRGBprefix);
   sprintf (YUV_cpld_prefix, "(%s)_", cpld->nameYUVprefix);

   cpld_prefix_length = strlen(current_cpld_prefix);

   log_info("%s %s %s %s", current_cpld_prefix, BBC_cpld_prefix, RGB_cpld_prefix, YUV_cpld_prefix);
   log_info("current cpld prefix = '%s', %d", current_cpld_prefix, cpld_prefix_length);

    favourites_count = 0;
    char favname[MAX_STRING_SIZE];
    cbytes = file_load(FAVOURITES_PATH, config_buffer, MAX_CONFIG_BUFFER_SIZE);
    config_buffer[cbytes] = 0;
    if (cbytes) {
        for(int i = 0; i < MAX_FAVOURITES; i++) {
            sprintf(favname, "favourite%d", i);
            prop = get_prop_no_space(config_buffer, favname);
            if (prop) {
               strcpy(favourite_names[favourites_count], prop);
               log_info("Reading favourite%d=%s", i, favourite_names[favourites_count]);
               favourites_count++;
            }

         }
    }
    strcpy(favourite_names[favourites_count], FAVOURITES_MENU_CLEAR);

   // default profile entry of not found
   features[F_PROFILE].max = 0;
   strcpy(profile_names[0], NOT_FOUND_STRING);
   default_buffer[0] = 0;
   has_sub_profiles[0] = 0;

   // pre-read default profile
   unsigned int bytes = file_read_profile(ROOT_DEFAULT_STRING, 0, NULL, 0, default_buffer, MAX_BUFFER_SIZE - 4);
   if (bytes != 0) {
      size_t count = 0;
      size_t mcount = 0;
      scan_profiles(current_cpld_prefix, manufacturer_names, profile_names, has_sub_profiles, cpld_path, &mcount, &count);
      features[F_PROFILE].max = count;

      if (strcmp(cpld_path, bbcpath) != 0) {
         log_info("Scanning BBC extra profiles");
         scan_profiles(BBC_cpld_prefix, manufacturer_names, profile_names, has_sub_profiles, bbcpath, &mcount, &count);
      }
      if (strcmp(cpld_path, rgbpath) != 0) {
         log_info("Scanning RGB extra profiles");
         scan_profiles(RGB_cpld_prefix, manufacturer_names, profile_names, has_sub_profiles, rgbpath, &mcount, &count);
      }
      if (strcmp(cpld_path, yuvpath) != 0) {
         log_info("Scanning YUV extra profiles");
         scan_profiles(YUV_cpld_prefix, manufacturer_names, profile_names, has_sub_profiles, yuvpath, &mcount, &count);
      }

      strcpy(manufacturer_names[mcount], FAVOURITES_MENU);
      mcount++;

      manufacturer_count = mcount;
      full_profile_count = count;

      if (features[F_PROFILE].max != 0) {
          features[F_PROFILE].max--;      //max is actually count-1
         // The default profile is provided by the CPLD
         prop = cpld->default_profile;
         if (mono_board_detected() && (cpld->get_version() >> VERSION_DESIGN_BIT) == DESIGN_YUV_ANALOG) {
             prop = MONO_BOARD_DEFAULT;
         }
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
            for (int i=0; i<= features[F_PROFILE].max; i++) {
               if (strcmp(profile_names[i] + cpld_prefix_length, prop) == 0) {
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
//************** should be capinfo->detected_sync_type below?
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
