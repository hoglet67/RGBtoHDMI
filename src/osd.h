#ifndef OSD_H
#define OSD_H

#define OSD_SW1 1
#define OSD_SW2 2
#define OSD_SW3 3

#define ATTR_DOUBLE_SIZE (1 << 0)

extern int clock_error_ppm;

void osd_init();
void osd_clear();
void osd_set(int line, int attr, char *text);
void osd_refresh();

void osd_update(uint32_t *osd_base, int bytes_per_line);
void osd_update_fast(uint32_t *osd_base, int bytes_per_line);
int  osd_active();
void osd_key(int key);
uint32_t *osd_get_palette();

void action_calibrate();

void set_scanlines(int on);
int  get_scanlines();
void set_elk(int on);
int  get_elk();
void set_vsync(int on);
int  get_vsync();
void set_pllh(int val);
int  get_pllh();
#ifdef MULTI_BUFFER
void set_nbuffers(int val);
int  get_nbuffers();
#endif

#endif
