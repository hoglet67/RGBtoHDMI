#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "defs.h"
#include "cpld.h"

enum {
   PS_NORMAL,    // Each sampled pixel is mapped to one pixel in the frame buffer
   PS_SUBSAMP_E, // Even pixels are replicated, odd pixels are ignored
   PS_SUBSAMP_O, // Odd pixels are replicated, even pixels are ignored
   PS_DOUBLE,    // Each sampled pixel is mapped to two pixels in the frame buffer
   NUM_PS
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
