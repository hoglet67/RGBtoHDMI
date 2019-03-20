#include <stdio.h>
#include "geometry.h"
#include "cpld.h"


static const char *px_sampling_names[] = {
   "Normal",
   "Odd",
   "Even",
   "Half Odd",
   "Half Even",
   "Double"
};

static param_t params[] = {
   {    H_OFFSET,        "H offset",         0,       100, 1 },
   {    V_OFFSET,        "V offset",         0,        49, 1 },
   {     H_WIDTH,         "H width",         1,       100, 1 },
   {    V_HEIGHT,        "V height",         1,       300, 1 },
   {    FB_WIDTH,        "FB width",       250,       800, 1 },
   {   FB_HEIGHT,       "FB height",       180,       600, 1 },
   { FB_HEIGHTX2,"FB double height",         0,         1, 1 },
   {      FB_BPP,   "FB Bits/pixel",         4,         8, 4 },
   {       CLOCK,      "Clock freq",   1000000,  40000000, 1 },
   {    LINE_LEN,        "Line len",       100,      5000, 1 },
   {   CLOCK_PPM, "Clock Tolerance",         0,    100000, 1 },
   { PX_SAMPLING,  "Pixel Sampling",         0,  NUM_PS-1, 1 },
   {          -1,              NULL,         0,         0, 0 }
};

typedef struct {
   int h_offset;      // horizontal offset (in psync clocks)
   int v_offset;      // vertical offset (in lines)
   int h_width;       // active horizontal width (in 8-bit characters)
   int v_height;      // active vertical height (in lines)
   int fb_width;      // framebuffer width in pixels
   int fb_height;     // framebuffer height (in pixels, before and doubling is applied)
   int fb_heightx2;   // if 1 then double frame buffer height
   int fb_bpp;        // framebuffer bits per pixel
   int clock;         // cpld clock (in Hz)
   int line_len;      // number of clocks per horizontal line
   int clock_ppm;     // cpld tolerance (in ppm)
   int px_sampling;   // pixel sampling mode
} geometry_t;

static int mode7;
static geometry_t *geometry;
static geometry_t default_geometry;
static geometry_t mode7_geometry;

static void update_param_range() {
   int max;
   // Set the range of the H_WIDTH param based on FB_WIDTH
   max = geometry->fb_width / (32 / geometry->fb_bpp);
   params[H_WIDTH].max = max;
   if (geometry->h_width > max) {
      geometry->h_width = max;
   }
   // Set the range of the V_HEIGHT param based on FB_HEIGHT
   max = geometry->fb_height;

   params[V_HEIGHT].max = max;
   if (geometry->v_height > max) {
      geometry->v_height = max;

   }

}

void geometry_init(int version) {
   // These are Beeb specific defaults so the geometry property can be ommitted
   mode7_geometry.v_offset      =        18;
   mode7_geometry.h_width       =  504 / (32 / 4);
   mode7_geometry.v_height      =       270;
   mode7_geometry.fb_width      =       504;
   mode7_geometry.fb_height     =       270;
   mode7_geometry.fb_heightx2   =         1;
   mode7_geometry.fb_bpp        =         4;
   mode7_geometry.clock         =  12000000;
   mode7_geometry.line_len      =   12 * 64;
   mode7_geometry.clock_ppm     =      5000;
   mode7_geometry.px_sampling   = PS_NORMAL;
   default_geometry.v_offset    =        21;
   default_geometry.h_width     =  672 / (32 / 4);
   default_geometry.v_height    =       270;
   default_geometry.fb_width    =       672;
   default_geometry.fb_height   =       270;
   default_geometry.fb_heightx2 =         1;
   default_geometry.fb_bpp      =         8;
   default_geometry.clock       =  16000000;
   default_geometry.line_len    =   16 * 64;
   default_geometry.clock_ppm   =      5000;
   default_geometry.px_sampling = PS_NORMAL;
   if (((version >> VERSION_MAJOR_BIT ) & 0x0F) <= 1) {
      // For backwards compatibility with CPLDv1
      mode7_geometry.h_offset   = 0;
      default_geometry.h_offset = 0;
   } else if (((version >> VERSION_MAJOR_BIT ) & 0x0F) == 2) {
      // For backwards compatibility with CPLDv2
      mode7_geometry.h_offset   = 24;
      default_geometry.h_offset = 32;
   } else {
      // For CPLDv3 onwards
      mode7_geometry.h_offset   = 32;
      default_geometry.h_offset = 38;
   }
   geometry_set_mode(0);
}

void geometry_set_mode(int mode) {
   mode7 = mode;
   geometry = mode ? &mode7_geometry : &default_geometry;
   update_param_range();

}

int geometry_get_value(int num) {
   switch (num) {
   case H_OFFSET:
      return geometry->h_offset;
   case V_OFFSET:
      return geometry->v_offset;
   case H_WIDTH:
      return geometry->h_width;
   case V_HEIGHT:
      return geometry->v_height;
   case FB_WIDTH:
      return geometry->fb_width;
   case FB_HEIGHT:
      return geometry->fb_height;
   case FB_HEIGHTX2:
      return geometry->fb_heightx2;
   case FB_BPP:
      return geometry->fb_bpp;
   case CLOCK:
      return geometry->clock;
   case LINE_LEN:
      return geometry->line_len;
   case CLOCK_PPM:
      return geometry->clock_ppm;
   case PX_SAMPLING:
      return geometry->px_sampling;
   }
   return -1;
}

const char *geometry_get_value_string(int num) {
   if (num == PX_SAMPLING) {
      return px_sampling_names[geometry_get_value(num)];
   }
   return NULL;
}

void geometry_set_value(int num, int value) {
   if (value < params[num].min) {
      value = params[num].min;
   }
   if (value > params[num].max) {
      value = params[num].max;
   }
   switch (num) {
   case H_OFFSET:
      geometry->h_offset = value;
      break;
   case V_OFFSET:
      geometry->v_offset = value;
      break;
   case H_WIDTH:
      geometry->h_width = value;
      break;
   case V_HEIGHT:
      geometry->v_height = value;
      break;
   case FB_WIDTH:
      geometry->fb_width = value;
      update_param_range();
      break;
   case FB_HEIGHT:
      geometry->fb_height = value;
      update_param_range();
      break;
   case FB_HEIGHTX2:
      geometry->fb_heightx2 = value;
      update_param_range();
      break;
   case FB_BPP:
      geometry->fb_bpp = value;
      break;
   case CLOCK:
      geometry->clock = value;
      break;
   case LINE_LEN:
      geometry->line_len = value;
      break;
   case CLOCK_PPM:
      geometry->clock_ppm = value;
      break;
   case PX_SAMPLING:
      geometry->px_sampling = value;
      break;
   }
}

param_t *geometry_get_params() {
   return params;
}

void geometry_get_fb_params(capture_info_t *capinfo) {
   capinfo->h_offset       = geometry->h_offset;
   capinfo->v_offset       = geometry->v_offset;
   capinfo->chars_per_line = geometry->h_width;
   capinfo->nlines         = geometry->v_height;
   capinfo->width          = geometry->fb_width;
   capinfo->height         = geometry->fb_height << geometry->fb_heightx2;    //adjust the height for capinfo according to fb_heightx2 setting
   capinfo->heightx2       = geometry->fb_heightx2;
   capinfo->bpp            = geometry->fb_bpp;
   capinfo->px_sampling    = geometry->px_sampling;
}

void geometry_get_clk_params(clk_info_t *clkinfo) {
   clkinfo->clock        = geometry->clock * cpld->get_divider();
   clkinfo->line_len     = geometry->line_len * cpld->get_divider();
   clkinfo->clock_ppm    = geometry->clock_ppm;
}
