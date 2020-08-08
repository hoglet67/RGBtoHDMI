#ifndef OSD_H
#define OSD_H

#define OSD_SW1     1
#define OSD_SW2     2
#define OSD_SW3     3
#define OSD_EXPIRED 4

#define ATTR_DOUBLE_SIZE (1 << 0)

#define MAX_PALETTE_ENTRIES 256

extern int clock_error_ppm;
extern int customPalette[];
extern char paletteHighNibble[];
extern int paletteFlags;

enum {
   HDMI_EXACT,
   HDMI_FAST_1000PPM,
   HDMI_FAST_2000PPM,
   HDMI_ORIGINAL,
   HDMI_SLOW_2000PPM,
   HDMI_SLOW_1000PPM,
   NUM_HDMI
};

enum {
   PALETTE_RGB,
   PALETTE_RGBI,
   PALETTE_RGBICGA,
   PALETTE_RGBISPECTRUM,
   PALETTE_SPECTRUM,
   PALETTE_AMSTRAD,
   PALETTE_RrGgBb,
   PALETTE_MDA,
   PALETTE_ATOM_MKI,
   PALETTE_ATOM_MKI_FULL,
   PALETTE_ATOM_6847_EMULATORS,
   PALETTE_ATOM_MKII,
   PALETTE_ATOM_MKII_PLUS,
   PALETTE_ATOM_MKII_FULL,
   PALETTE_MONO2,
   PALETTE_MONO3,
   PALETTE_MONO4,
   PALETTE_MONO6,
   PALETTE_TI,
   PALETTE_SPECTRUM48K,
   PALETTE_CGS24,
   PALETTE_CGS25,
   PALETTE_CGN25,
   PALETTE_C64,
   PALETTE_C64_REV1,
   PALETTE_YG_4,
   PALETTE_UB_4,
   PALETTE_VR_4,
   NUM_PALETTES
};

enum {
   PALETTECONTROL_OFF,
   PALETTECONTROL_INBAND,
   PALETTECONTROL_NTSCARTIFACT_CGA,
   PALETTECONTROL_NTSCARTIFACT_BW,
   PALETTECONTROL_NTSCARTIFACT_BW_AUTO,
 //  PALETTECONTROL_PALARTIFACT,
 //  PALETTECONTROL_ATARI_GTIA,
   NUM_CONTROLS
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
   AUTOSWITCH_OFF,
   AUTOSWITCH_PC,
   AUTOSWITCH_MODE7,
   NUM_AUTOSWITCHES
};

enum {
   FRONTEND_TTL_3BIT,
   FRONTEND_SIMPLE,
   FRONTEND_ATOM,
   FRONTEND_TTL_6_8BIT,
   FRONTEND_ANALOG_ISSUE3_5259,
   FRONTEND_ANALOG_ISSUE2_5259,
   FRONTEND_ANALOG_ISSUE1_UA1,
   FRONTEND_ANALOG_ISSUE1_UB1,
   FRONTEND_YUV_ISSUE3_5259,
   FRONTEND_YUV_ISSUE2_5259,
   NUM_FRONTENDS
};

enum {
   VLOCKSPEED_SLOW,
   VLOCKSPEED_MEDIUM,
   VLOCKSPEED_FAST,
   NUM_VLOCKSPEED
};

enum {
   VLOCKADJ_NARROW,
   VLOCKADJ_165MHZ,
   VLOCKADJ_260MHZ,  //may need additional changes to work
   NUM_VLOCKADJ
};

enum {
   FONTSIZE_8X8,
   FONTSIZE_12X20_4,
   FONTSIZE_12X20_8,
   NUM_FONTSIZE
};

enum {
   SCALING_EVEN,
   SCALING_UNEVEN,
   NUM_ESCALINGS
};

void osd_init();
void osd_clear();
void osd_set(int line, int attr, char *text);
void osd_set_noupdate(int line, int attr, char *text);
void osd_set_clear(int line, int attr, char *text);
void osd_show_cpld_recovery_menu();
void osd_refresh();
void osd_update(uint32_t *osd_base, int bytes_per_line);
void osd_update_fast(uint32_t *osd_base, int bytes_per_line);
int  osd_active();
int  osd_key(int key);
void osd_update_palette();
void process_profile(int profile_number);
void process_sub_profile(int profile_number, int sub_profile_number);
void load_profiles(int profile_number, int save_selected);
void process_single_profile(char *buffer);
uint32_t osd_get_palette(int index);
int autoswitch_detect(int one_line_time_ns, int lines_per_frame, int sync_type);
int sub_profiles_available();
uint32_t osd_get_equivalence(uint32_t value);
int get_existing_frontend(int frontend);
void set_auto_name(char* name);
#endif
