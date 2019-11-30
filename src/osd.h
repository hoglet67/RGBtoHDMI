#ifndef OSD_H
#define OSD_H

#define OSD_SW1     1
#define OSD_SW2     2
#define OSD_SW3     3
#define OSD_EXPIRED 4

#define ATTR_DOUBLE_SIZE (1 << 0)

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
   PALETTE_ATOM_MKII,
   PALETTE_ATOM_MKII_PLUS,
   PALETTE_ATOM_MKII_FULL,
   PALETTE_ATOM_6847_EMULATORS,
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

enum {
   PALETTECONTROL_OFF,
   PALETTECONTROL_INBAND,
   PALETTECONTROL_NTSCARTIFACTS,
   NUM_CONTROLS
};

enum {
   OVERSCAN_AUTO,
   OVERSCAN_MAX,
   OVERSCAN_HALF,
   OVERSCAN_MIN,
   NUM_OVERSCAN
};
enum {
   COLOUR_NORMAL,
   COLOUR_MONO,
   COLOUR_GREEN,
   COLOUR_AMBER,
   NUM_COLOURS
};

enum {
   SCALING_INTEGER_SHARP,
   SCALING_INTEGER_MEDIUM,
   SCALING_INTEGER_SOFT,
   SCALING_FILL43_MEDIUM,
   SCALING_FILL43_SOFT,
   SCALING_FILLALL_MEDIUM,
   SCALING_FILLALL_SOFT,
   NUM_SCALING
};

enum {
   DEINTERLACE_NONE,
   DEINTERLACE_BOB,
   DEINTERLACE_MA1,
   DEINTERLACE_MA2,
   DEINTERLACE_MA3,
   DEINTERLACE_MA4,
   DEINTERLACE_ADV,
   NUM_DEINTERLACES
};

enum {
   AUTOSWITCH_OFF,
   AUTOSWITCH_PC,
   AUTOSWITCH_MODE7,
   NUM_AUTOSWITCHES
};

enum {
   FRONTEND_NONE,
   FRONTEND_ANALOG_UA1,
   FRONTEND_ANALOG_UB1,
   NUM_FRONTENDS
};

enum {
   VSYNCTYPE_STANDARD,
   VSYNCTYPE_ELECTRON,
   NUM_VSYNCTYPES
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
void osd_init();
void osd_clear();
void osd_set(int line, int attr, char *text);
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

#endif
