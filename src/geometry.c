#include <stdio.h>
#include "geometry.h"
#include "cpld.h"
#include "osd.h"
#include "defs.h"
#include "logging.h"

static const char *px_sampling_names[] = {
   "Normal",
   "Odd",
   "Even",
   "Half Odd",
   "Half Even",
   "Double"
};

static const char *sync_names[] = {
   "-H -V",
   "-H +V",
   "+H -V",
   "+H +V",
   "Composite",
   "Inverted"
};
static param_t params[] = {
   {    H_OFFSET,        "H offset",         0,       512, 4 },
   {    V_OFFSET,        "V offset",         0,       512, 1 },
   {     H_WIDTH,         "H width",       250,      1920, 16},
   {    V_HEIGHT,        "V height",       150,      1200, 1 },
   {    FB_WIDTH,        "FB width",       100,      1920, 16},
   {   FB_HEIGHT,       "FB height",       100,      1200, 1 },
   { FB_HEIGHTX2,"FB double height",         0,         1, 1 },
   {      FB_BPP,   "FB Bits/pixel",         4,         8, 4 },
   {       CLOCK,      "Clock freq",   1000000,  40000000, 1000 },
   {    LINE_LEN,        "Line len",       100,      5000, 1 },
   {   CLOCK_PPM, "Clock Tolerance",         0,    100000, 100 },
   { LINES_FRAME, "Lines per frame",       250,      1200, 1 },
   {   SYNC_TYPE,       "Sync type",         0,NUM_SYNC-1, 1 },
   { PX_SAMPLING,  "Pixel Sampling",         0,  NUM_PS-1, 1 },
   {          -1,              NULL,         0,         0, 0 }
};

typedef struct {
   int h_offset;      // horizontal offset (in psync clocks)
   int v_offset;      // vertical offset (in lines)
   int h_width;       // active horizontal width (in 8-bit characters)
   int v_height;      // active vertical height (in lines)
   int fb_width;      // framebuffer width in pixels
   int fb_height;     // framebuffer height (in pixels, before any doubling is applied)
   int fb_heightx2;   // if 1 then double frame buffer height
   int fb_bpp;        // framebuffer bits per pixel
   int clock;         // cpld clock (in Hz)
   int line_len;      // number of clocks per horizontal line
   int clock_ppm;     // cpld tolerance (in ppm)
   int lines_frame;   // number of lines per frame
   int sync_type;     // sync type and polarity
   int px_sampling;   // pixel sampling mode
} geometry_t;

static int mode7;
static geometry_t *geometry;
static geometry_t default_geometry;
static geometry_t mode7_geometry;
static int scaling = 0;

void geometry_init(int version) {
   // These are Beeb specific defaults so the geometry property can be ommitted
   mode7_geometry.v_offset      =       128;
   mode7_geometry.h_width       =       504;
   mode7_geometry.v_height      =       270;
   mode7_geometry.fb_width      =       504;
   mode7_geometry.fb_height     =       270;
   mode7_geometry.fb_heightx2   =         1;
   mode7_geometry.fb_bpp        =         4;
   mode7_geometry.clock         =  12000000;
   mode7_geometry.line_len      =   12 * 64;
   mode7_geometry.clock_ppm     =      5000;
   mode7_geometry.lines_frame   =       625;
   mode7_geometry.sync_type     = SYNC_COMP;
   mode7_geometry.px_sampling   = PS_NORMAL;
   default_geometry.v_offset    =       152;
   default_geometry.h_width     =       672;
   default_geometry.v_height    =       270;
   default_geometry.fb_width    =       672;
   default_geometry.fb_height   =       270;
   default_geometry.fb_heightx2 =         1;
   default_geometry.fb_bpp      =         8;
   default_geometry.clock       =  16000000;
   default_geometry.line_len    =   16 * 64;
   default_geometry.clock_ppm   =      5000;
   default_geometry.lines_frame =       625;
   default_geometry.sync_type   = SYNC_COMP;
   default_geometry.px_sampling = PS_NORMAL;
   if (((version >> VERSION_MAJOR_BIT ) & 0x0F) <= 1) {
      // For backwards compatibility with CPLDv1
      mode7_geometry.h_offset   = 0;
      default_geometry.h_offset = 0;
   } else if (((version >> VERSION_MAJOR_BIT ) & 0x0F) == 2) {
      // For backwards compatibility with CPLDv2
      mode7_geometry.h_offset   = 96;
      default_geometry.h_offset = 128;
   } else {
      // For CPLDv3 onwards
      mode7_geometry.h_offset   = 128;
      default_geometry.h_offset = 152;
   }
   geometry_set_mode(0);
}

void geometry_set_mode(int mode) {
   mode7 = mode;
   geometry = mode ? &mode7_geometry : &default_geometry;
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
   case LINES_FRAME:
      return geometry->lines_frame;
   case SYNC_TYPE:
      return geometry->sync_type;
   case PX_SAMPLING:
      return geometry->px_sampling;
   }
   return -1;
}

const char *geometry_get_value_string(int num) {
   if (num == PX_SAMPLING) {
      return px_sampling_names[geometry_get_value(num)];
   }
   if (num == SYNC_TYPE) {
      return sync_names[geometry_get_value(num)];
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
      geometry->h_offset = value & 0xfffffffc;
      break;
   case V_OFFSET:
      geometry->v_offset = value;
      break;
   case H_WIDTH:
      geometry->h_width = value & 0xfffffff0;
      break;
   case V_HEIGHT:
      geometry->v_height = value;
      break;
   case FB_WIDTH:
      geometry->fb_width = value & 0xfffffff0;
      break;
   case FB_HEIGHT:
      geometry->fb_height = value;
      break;
   case FB_HEIGHTX2:
      geometry->fb_heightx2 = value;
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
   case LINES_FRAME:
      geometry->lines_frame = value;
      break;
   case SYNC_TYPE:
      geometry->sync_type = value;
      break;
   case PX_SAMPLING:
      geometry->px_sampling = value;
      break;
   }
}

param_t *geometry_get_params() {
   return params;
}

void set_scaling(int value) {
   scaling = value;
}

int get_scaling() {
   return scaling;
}

void geometry_get_fb_params(capture_info_t *capinfo) {
   capinfo->h_offset       = geometry->h_offset;
   if (capinfo->h_offset < 0) {
       capinfo->h_offset = 0;
   }
   capinfo->h_offset = ((capinfo->h_offset >> 2) - (cpld->get_delay() >> 2));
   capinfo->v_offset       = geometry->v_offset;
   capinfo->chars_per_line = (geometry->h_width >> 4) << 1;
   capinfo->nlines         = geometry->v_height;
   capinfo->width          = geometry->fb_width;
   capinfo->height         = geometry->fb_height << geometry->fb_heightx2;    //adjust the height for capinfo according to fb_heightx2 setting
   capinfo->heightx2       = geometry->fb_heightx2;
   capinfo->bpp            = geometry->fb_bpp;
   capinfo->px_sampling    = geometry->px_sampling;
   capinfo->period = 1000000000 / geometry->clock;

   if (capinfo->chars_per_line > (geometry->fb_width >> 3)) {
       capinfo->chars_per_line = geometry->fb_width >> 3;
   }

   if (capinfo->nlines > geometry->fb_height) {
       capinfo->nlines = geometry->fb_height;
   }

   uint32_t h_size = (*PIXELVALVE2_HORZB) & 0xFFFF;
   uint32_t v_size = (*PIXELVALVE2_VERTB) & 0xFFFF;

   //log_info("           H-Total: %d pixels", h_size);
   //log_info("           V-Total: %d pixels", v_size);

   double ratio = (double) h_size / v_size;
   int h_size43 = h_size;
   int v_size43 = v_size;
   if (ratio > 1.34) {
       h_size43 = v_size * 4 / 3;
   }
   if (ratio < 1.32) {
       v_size43 = h_size * 3 / 4;
   }

   int adjusted_width = mode7 ? geometry->h_width * 4 / 3 : geometry->h_width;    // workaround mode 7 width so it looks like other modes
   int hscale = h_size43 / adjusted_width;
   int hborder = (h_size - (adjusted_width * hscale)) / hscale;
   int hborder43 = (h_size43 - (adjusted_width * hscale)) / hscale;
   int vscale = v_size43 / (geometry->v_height << geometry->fb_heightx2);
   int vborder = (v_size - ((geometry->v_height << geometry->fb_heightx2) * vscale)) / vscale;
   int vborder43 = (v_size43 - ((geometry->v_height << geometry->fb_heightx2) * vscale)) / vscale;
   int newhborder43 = vborder43 * 4 / 3;
   int newvborder43 = hborder43 * 3 / 4;

   //log_info("scaling = %d, %f, %d, %d, %d, %d",adjusted_width, ratio, hscale, hborder, hborder43, newhborder43);

   switch (scaling) {
       case    SCALING_INTEGER:
          capinfo->width = geometry->h_width + hborder;
          capinfo->height = (geometry->v_height << geometry->fb_heightx2) + vborder;
       break;
       case    SCALING_NON_INTEGER:
          if (newhborder43 < hborder43) {
              newhborder43 = hborder43 - newhborder43;
              newvborder43 = 0;
          } else {
              newhborder43 = 0;
              newvborder43 = vborder43 - newvborder43;
          }
          capinfo->width = geometry->h_width + hborder - hborder43 + newhborder43;
          capinfo->height = (geometry->v_height << geometry->fb_heightx2) + vborder - vborder43 + newvborder43;
       break;
       case    SCALING_MANUAL:
       break;
   };

   //log_info("size= %d, %d",geometry->h_width, capinfo->width);

}

void geometry_get_clk_params(clk_info_t *clkinfo) {
   clkinfo->clock        = geometry->clock;
   clkinfo->line_len     = geometry->line_len;
   clkinfo->clock_ppm    = geometry->clock_ppm;
}
