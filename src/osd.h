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
   HDMI_ORIGINAL,
   HDMI_SLOW_2000PPM,
   HDMI_SLOW_500PPM,
   HDMI_EXACT,
   HDMI_FAST_500PPM,
   HDMI_FAST_2000PPM
};
   
enum {
   PALETTE_RGB,
   PALETTE_RGBI,
   PALETTE_RGBICGA,
   PALETTE_RrGgBb,
   PALETTE_MDA,
   PALETTE_INVERSE,
   PALETTE_MONO1,
   PALETTE_MONO2,
   PALETTE_RED,
   PALETTE_GREEN,
   PALETTE_BLUE,
   PALETTE_NOT_RED,
   PALETTE_NOT_GREEN,
   PALETTE_NOT_BLUE,
   PALETTE_ATOM_COLOUR_NORMAL,
   PALETTE_ATOM_COLOUR_EXTENDED,
   PALETTE_ATOM_COLOUR_ACORN,
   PALETTE_ATOM_MONO,
   PALETTE_ATOM_EXPERIMENTAL,
   NUM_PALETTES
};

enum {
   PALETTECONTROL_OFF,
   PALETTECONTROL_INBAND,
   PALETTECONTROL_NTSCARTIFACTS,
   NUM_CONTROLS
};

enum {
   SCALING_INTEGER,
   SCALING_NON_INTEGER,
   SCALING_MANUAL,
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
   AUTOSWITCH_MODE7,
   AUTOSWITCH_PC,
   NUM_AUTOSWITCHES
};

enum {
   VSYNCTYPE_STANDARD,
   VSYNCTYPE_ELECTRON,
   NUM_VSYNCTYPES
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
void process_sub_profile(int profile_number, int sub_profile_number);
void load_profiles(int profile_number);
uint32_t osd_get_palette(int index);
#endif
