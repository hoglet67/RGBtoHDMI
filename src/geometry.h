#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "defs.h"
#include "cpld.h"

void     geometry_init(int version);
void     geometry_set_mode(int mode);
int      geometry_get_value(int num);
void     geometry_set_value(int num, int value);
param_t *geometry_get_params();
void     geometry_get_fb_params(capture_info_t *capinfo);
void     geometry_get_clk_params(clk_info_t *clkinfo);

#endif
