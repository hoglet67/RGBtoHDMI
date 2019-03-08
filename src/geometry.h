#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "defs.h"
#include "cpld.h"

enum {
   PS_NORMAL,    // Each sampled pixel is mapped to one pixel in the frame buffer
   PS_NORMAL_O,  // Odd pixels are replicated, even pixels are ignored
   PS_NORMAL_E,  // Even pixels are replicated, odd pixels are ignored
   PS_HALF_O,    // Odd pixels are used, even pixels are ignored
   PS_HALF_E,    // Even pixels are used, odd pixels are ignored
   PS_DOUBLE,    // pixels are doubled
   PS_SIXBITS,
   NUM_PS
};

enum {
   H_OFFSET,
   V_OFFSET,
   H_WIDTH,
   V_HEIGHT,
   FB_WIDTH,
   FB_HEIGHT,
   FB_BPP,
   CLOCK,
   LINE_LEN,
   CLOCK_PPM,
   PX_SAMPLING
};

void        geometry_init(int version);
void        geometry_set_mode(int mode);
int         geometry_get_value(int num);
const char *geometry_get_value_string(int num);
void        geometry_set_value(int num, int value);
param_t    *geometry_get_params();
void        geometry_get_fb_params(capture_info_t *capinfo);
void        geometry_get_clk_params(clk_info_t *clkinfo);

#endif
