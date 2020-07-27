#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "defs.h"
#include "cpld.h"
#include "rpi-base.h"

enum {
   PS_NORMAL,    // Each sampled pixel is mapped to one pixel in the frame buffer
   PS_NORMAL_O,  // Odd pixels are replicated, even pixels are ignored
   PS_NORMAL_E,  // Even pixels are replicated, odd pixels are ignored
   PS_HALF_O,    // Odd pixels are used, even pixels are ignored
   PS_HALF_E,    // Even pixels are used, odd pixels are ignored
   NUM_PS
};

enum {
   SYNC_NH_NV,
   SYNC_NH_PV,
   SYNC_PH_NV,
   SYNC_PH_PV,
   SYNC_COMP,
   SYNC_INVERT,
   NUM_SYNC
};

enum {
   VSYNC_AUTO,
   VSYNC_INTERLACED,
   VSYNC_INTERLACED_160,
   VSYNC_NONINTERLACED,
   VSYNC_NONINTERLACED_DEJITTER,
   NUM_VSYNC
};

enum {
   VIDEO_PROGRESSIVE,
   VIDEO_TELETEXT,
   NUM_VIDEO
};

enum {
   SETUP_MODE,
   H_OFFSET,
   V_OFFSET,
   MIN_H_WIDTH,
   MIN_V_HEIGHT,
   MAX_H_WIDTH,
   MAX_V_HEIGHT,
   H_ASPECT,
   V_ASPECT,
   FB_SIZEX2,
   FB_BPP,
   CLOCK,
   LINE_LEN,
   CLOCK_PPM,
   LINES_FRAME,
   SYNC_TYPE,
   VSYNC_TYPE,
   VIDEO_TYPE,
   PX_SAMPLING
};

enum {
   GSCALING_INTEGER,
   GSCALING_MANUAL43,
   GSCALING_MANUAL
};

enum {
   SETUP_NORMAL,
   SETUP_MIN,
   SETUP_MAX,
   SETUP_CLOCK,
   SETUP_FINE,
   NUM_SETUP
};

enum {
   BPP_4,
   BPP_8,
   BPP_16,
   NUM_BPP
};

void        geometry_init(int version);
void        geometry_set_mode(int mode);
int         geometry_get_mode();
int         geometry_get_value(int num);
const char *geometry_get_value_string(int num);
void        geometry_set_value(int num, int value);
param_t    *geometry_get_params();
void        geometry_get_fb_params(capture_info_t *capinfo);
void        geometry_get_clk_params(clk_info_t *clkinfo);
void set_gscaling(int value);
int get_gscaling();
void set_overscan(int value);
int  get_overscan();
void set_m7scaling(int value);
int  get_m7scaling();
void set_normalscaling(int value);
int  get_normalscaling();
void set_capscale(int value);
int  get_capscale();
int get_hscale();
int get_vscale();
int get_haspect();
int get_vaspect();
int get_hdisplay();
int get_vdisplay();
void set_setup_mode(int mode);
void geometry_hide_pixel_sampling();
#endif
