#ifndef OSD_H
#define OSD_H

#define OSD_SW1 1
#define OSD_SW2 2
#define OSD_SW3 3

#define ATTR_DOUBLE_SIZE (1 << 0)

extern int clock_error_ppm;

enum {
   HDMI_ORIGINAL,
   HDMI_SLOW_2000PPM,
   HDMI_SLOW_1000PPM,
   HDMI_EXACT,
   HDMI_FAST_1000PPM,
   HDMI_FAST_2000PPM
};

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

enum {
   DEINTERLACE_NONE,
   DEINTERLACE_MA1,
   DEINTERLACE_MA2,
   DEINTERLACE_MA3,
   DEINTERLACE_MA4,
   DEINTERLACE_MA1_NEW,
   DEINTERLACE_MA2_NEW,
   DEINTERLACE_MA3_NEW,
   DEINTERLACE_MA4_NEW,
   NUM_DEINTERLACES
};

void osd_init();
void osd_clear();
void osd_set(int line, int attr, char *text);
void osd_refresh();

void osd_update(uint32_t *osd_base, int bytes_per_line);
void osd_update_fast(uint32_t *osd_base, int bytes_per_line);
int  osd_active();
void osd_key(int key);
uint32_t *osd_get_palette();

void action_calibrate_clocks();
void action_calibrate_auto();

void set_h_offset(int value);
int  get_h_offset();
void set_v_offset(int value);
int  get_v_offset();
void set_scanlines(int on);
int  get_scanlines();
void set_deinterlace(int value);
int  get_deinterlace();
void set_elk(int on);
int  get_elk();
void set_debug(int on);
int  get_debug();
void set_expert(int on);
int  get_expert();
void set_m7disable(int on);
int  get_m7disable();
void set_vsync(int on);
int  get_vsync();
void set_pllh(int val);
int  get_pllh();
#ifdef MULTI_BUFFER
void set_nbuffers(int val);
int  get_nbuffers();
#endif

#endif
