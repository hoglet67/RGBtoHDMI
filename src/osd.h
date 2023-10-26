#ifndef OSD_H
#define OSD_H

#define OSD_SW1     1
#define OSD_SW2     2
#define OSD_SW3     3
#define OSD_EXPIRED 4

#define ATTR_DOUBLE_SIZE (1 << 0)

#define MAX_PALETTE_ENTRIES 257

extern int clock_error_ppm;
extern int customPalette[];
extern char paletteHighNibble[];
extern int paletteFlags;
extern int palette_data_16[];
extern int c64_artifact_palette_16[];
extern char c64_YUV_palette_lookup[];

enum {
   HDMI_EXACT,
   HDMI_ORIGINAL,
   HDMI_FAST_1000PPM,
   HDMI_FAST_2000PPM,
   HDMI_SLOW_2000PPM,
   HDMI_SLOW_1000PPM,
   NUM_HDMI
};

enum {
   PALETTE_RGB,
   PALETTE_RGBI,
   PALETTE_RGBICGA,
   PALETTE_XRGB,
   PALETTE_LASER,
   PALETTE_RGBISPECTRUM,
   PALETTE_SPECTRUM,
   PALETTE_AMSTRAD,
   PALETTE_RrGgBb,
   PALETTE_RrGgBbI,
   PALETTE_MDA,
   PALETTE_DRAGON_COCO,
   PALETTE_DRAGON_COCO_FULL,
   PALETTE_ATOM_6847_EMULATORS,
   PALETTE_ATOM_MKII,
   PALETTE_ATOM_MKII_PLUS,
   PALETTE_ATOM_MKII_FULL,
   PALETTE_MONO2,
   PALETTE_MONO3,
   PALETTE_MONO3_BRIGHT,
   PALETTE_MONO4,
   PALETTE_MONO6,
   PALETTE_MONO8_RGB,
   PALETTE_MONO8_YUV,
   PALETTE_TI,
   PALETTE_SPECTRUM48K,
   PALETTE_CGS24,
   PALETTE_CGS25,
   PALETTE_CGN25,
   PALETTE_COMMODORE64,
   PALETTE_COMMODORE64_REV1,
   PALETTE_VIC20,
   PALETTE_ATARI800_PAL,
   PALETTE_ATARI800_NTSC,
   PALETTE_ATARI2600_PAL,
   PALETTE_ATARI2600_NTSC,
   PALETTE_TEA1002,
   PALETTE_INTELLIVISION,
   PALETTE_YG_4,
   PALETTE_UB_4,
   PALETTE_VR_4,
   NUM_PALETTES
};

enum {
   OVERSCAN_0,
   OVERSCAN_1,
   OVERSCAN_2,
   OVERSCAN_3,
   OVERSCAN_4,
   OVERSCAN_5,
   OVERSCAN_6,
   OVERSCAN_7,
   OVERSCAN_8,
   OVERSCAN_9,
   OVERSCAN_10,
   NUM_OVERSCAN
};

enum {
   SCREENCAP_HALF,
   SCREENCAP_FULL,
   SCREENCAP_HALF43,
   SCREENCAP_FULL43,
   NUM_SCREENCAP
};

enum {
   COLOUR_NORMAL,
   COLOUR_MONO,
   COLOUR_GREEN,
   COLOUR_AMBER,
   NUM_COLOURS
};

enum {
   INVERT_NORMAL,
   INVERT_RGB,
   INVERT_Y,
   NUM_INVERT
};

enum {
   SCALING_AUTO,
   SCALING_INTEGER_SHARP,
   SCALING_INTEGER_SOFT,
   SCALING_INTEGER_VERY_SOFT,
   SCALING_FILL43_SOFT,
   SCALING_FILL43_VERY_SOFT,
   SCALING_FILLALL_SOFT,
   SCALING_FILLALL_VERY_SOFT,
   NUM_SCALING
};

enum {
   M7DEINTERLACE_NONE,
   M7DEINTERLACE_BOB,
   M7DEINTERLACE_MA1,
   M7DEINTERLACE_MA2,
   M7DEINTERLACE_MA3,
   M7DEINTERLACE_MA4,
   M7DEINTERLACE_ADV,
   NUM_M7DEINTERLACES
};

enum {
   DEINTERLACE_NONE,
   DEINTERLACE_BOB,
   NUM_DEINTERLACES
};

enum {
   FRONTEND_TTL_3BIT,
   FRONTEND_SIMPLE,
   FRONTEND_ATOM,
   FRONTEND_TTL_6_8BIT,
   FRONTEND_YUV_TTL_6_8BIT,
   FRONTEND_ANALOG_ISSUE3_5259,
   FRONTEND_ANALOG_ISSUE2_5259,
   FRONTEND_ANALOG_ISSUE1_UA1,
   FRONTEND_ANALOG_ISSUE1_UB1,
   FRONTEND_ANALOG_ISSUE4,
   FRONTEND_ANALOG_ISSUE5,
   FRONTEND_YUV_ISSUE3_5259,
   FRONTEND_YUV_ISSUE2_5259,
   FRONTEND_YUV_ISSUE4,
   FRONTEND_YUV_ISSUE5,
   NUM_FRONTENDS
};

enum {
   GENLOCK_SPEED_SLOW,
   GENLOCK_SPEED_MEDIUM,
   GENLOCK_SPEED_FAST,
   NUM_GENLOCK_SPEED
};

enum {
   GENLOCK_ADJUST_NARROW,
   GENLOCK_ADJUST_165MHZ,
   NUM_GENLOCK_ADJUST
};

enum {
   FONTSIZE_8X8,
   FONTSIZE_12X20,
   NUM_FONTSIZE
};

enum {
   SCALING_EVEN,
   SCALING_UNEVEN,
   NUM_ESCALINGS
};

enum {
   GENLOCK_RANGE_NORMAL,
   GENLOCK_RANGE_EDID,
   GENLOCK_RANGE_FORCE_LOW,
   GENLOCK_RANGE_FORCE_ALL,
   GENLOCK_RANGE_INHIBIT,
   GENLOCK_RANGE_SET_DEFAULT
};

enum {
    HDMI_DVI,
    HDMI_AUTO,
    HDMI_RGB_LIMITED,
    HDMI_RGB_FULL,
    HDMI_YUV_LIMITED,
    HDMI_YUV_FULL,
    NUM_HDMIS
};

enum {
    REFRESH_60,
    REFRESH_EDID,
    REFRESH_50_60,
    REFRESH_50_ANY,
    REFRESH_50,
    NUM_REFRESH
};

enum {
    FRINGE_MEDIUM,
    FRINGE_SHARP,
    FRINGE_SOFT,
    NUM_FRINGE
};

enum {
    NTSCTYPE_NEW_CGA,
    NTSCTYPE_OLD_CGA,
    NTSCTYPE_APPLE,
    NTSCTYPE_SIMPLE,
    NUM_NTSCTYPE
};

enum {
    PAL_ODD_NORMAL,
    PAL_ODD_BLENDED,
    PAL_ODD_ALL,
    NUM_PAL_ODD
};

enum {
   F_AUTO_SWITCH,
   F_RESOLUTION,
   F_REFRESH,
   F_HDMI_AUTO,
   F_HDMI_MODE,
   F_HDMI_MODE_STANDBY,
   F_SCALING,
   F_PROFILE,
   F_SAVED_CONFIG,
   F_SUB_PROFILE,
   F_PALETTE,
   F_PALETTE_CONTROL,
   F_NTSC_COLOUR,
   F_NTSC_PHASE,
   F_NTSC_TYPE,
   F_NTSC_QUALITY,
   F_PAL_ODD_LEVEL,
   F_PAL_ODD_LINE,
   F_TINT,
   F_SAT,
   F_CONT,
   F_BRIGHT,
   F_GAMMA,
   F_TIMING_SET,
   F_MODE7_DEINTERLACE,
   F_NORMAL_DEINTERLACE,
   F_MODE7_SCALING,
   F_NORMAL_SCALING,
   F_FFOSD,
   F_SWAP_ASPECT,
   F_OUTPUT_COLOUR,
   F_OUTPUT_INVERT,
   F_SCANLINES,
   F_SCANLINE_LEVEL,
   F_CROP_BORDER,
   F_SCREENCAP_SIZE,
   F_FONT_SIZE,
   F_BORDER_COLOUR,
   F_VSYNC_INDICATOR,
   F_GENLOCK_MODE,
   F_GENLOCK_LINE,
   F_GENLOCK_SPEED,
   F_GENLOCK_ADJUST,
#ifdef MULTI_BUFFER
   F_NUM_BUFFERS,
#endif
   F_RETURN_POSITION,
   F_DEBUG,
   F_BUTTON_REVERSE,
   F_OVERCLOCK_CPU,
   F_OVERCLOCK_CORE,
   F_OVERCLOCK_SDRAM,
   F_POWERUP_MESSAGE,
   F_YUV_PIXEL_DOUBLE,
   F_INTEGER_ASPECT,

   F_PROFILE_NUM,
   F_H_WIDTH,
   F_V_HEIGHT,
   F_CLOCK,
   F_LINE_LEN,
   F_H_OFFSET,
   F_V_OFFSET,

   F_FRONTEND,       //must be last

   MAX_PARAMETERS

};


void osd_init();
void osd_clear();
void osd_write_palette(int new_active);
void osd_set(int line, int attr, char *text);
void osd_set_noupdate(int line, int attr, char *text);
void osd_set_clear(int line, int attr, char *text);
void osd_show_cpld_recovery_menu(int update);
void osd_refresh();
void osd_update(uint32_t *osd_base, int bytes_per_line, int relocate);
void osd_update_fast(uint32_t *osd_base, int bytes_per_line);
void osd_display_interface(int line);
int lumacode_multiplier();
int  osd_active();
int menu_active();
int  osd_key(int key);
void osd_update_palette();
void process_profile(int profile_number);
void process_sub_profile(int profile_number, int sub_profile_number);
void load_profiles(int profile_number, int save_selected);
void process_single_profile(char *buffer);
uint32_t osd_get_palette(int index);
int autoswitch_detect(int one_line_time_ns, int lines_per_frame, int sync_type);
int sub_profiles_available();
//uint32_t osd_get_equivalence(uint32_t value);
int get_existing_frontend(int frontend);
void set_auto_name(char* name);
int normalised_gamma_correct(int old_value);
int get_inhibit_palette_dimming16();
#endif
