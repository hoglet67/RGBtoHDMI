#include <stdio.h>
#include "geometry.h"
#include "cpld.h"
#include "osd.h"
#include "defs.h"
#include "logging.h"
#include "rgb_to_hdmi.h"
#include "startup.h"

static const char *px_sampling_names[] = {
   "Normal",
   "Odd",
   "Even",
   "Half Odd",
   "Half Even",
};

static const char *sync_names[] = {
   "-H-V",
   "+H-V",
   "-H+V",
   "+H+V",
   "Composite",
   "Inverted Composite",
   "Composite -V",
   "Inverted -V"
};

static const char *vsync_names[] = {
   "Auto",
   "Interlaced",
   "Interlaced 160uS Vsync",
   "Non Interlaced",
   "Flywheel",
   "Blanking",
   "Polarity",
   "Force Interlaced"
};

static const char *setup_names[] = {
   "Normal",
   "Set Min/Offset",
   "Set Maximum",
   "Set Clock/Line",
   "Fine Set Clock"
};

static const char *fb_sizex2_names[] = {
   "Normal",
   "Double Height",
   "Double Width",
   "Double Height+Width",
};

static const char *deint_names[] = {
   "Progressive",
   "Interlaced",
   "Interlaced Teletext",
   "Line Doubled"
};

static const char *bpp_names[] = {
   "4",
   "8",
   "16"
};

static param_t params[] = {
   {  SETUP_MODE,         "Setup Mode",         "setup_mode",         0,NUM_SETUP-1, 1 },
   {    H_OFFSET,           "H Offset",           "h_offset",         1,        384, 4 },
   {    V_OFFSET,           "V Offset",           "v_offset",         0,        256, 1 },
   { MIN_H_WIDTH,        "Min H Width",        "min_h_width",       100,       1920, 8 },
   {MIN_V_HEIGHT,       "Min V Height",       "min_v_height",       100,       1200, 2 },
   { MAX_H_WIDTH,        "Max H Width",        "max_h_width",       120,       1920, 8 },
   {MAX_V_HEIGHT,       "Max V Height",       "max_v_height",       120,       1200, 2 },
   {    H_ASPECT,     "H Pixel Aspect",           "h_aspect",         0,         12, 1 },
   {    V_ASPECT,     "V Pixel Aspect",           "v_aspect",         0,         12, 1 },
   {   FB_SIZEX2,            "FB Size",            "fb_size",         0,          3, 1 },
   {      FB_BPP,      "FB Bits/Pixel",      "fb_bits_pixel",         0,  NUM_BPP-1, 1 },
   {       CLOCK,    "Clock Frequency",    "clock_frequency",   1000000,64000000, 1000 },
   {    LINE_LEN,        "Line Length",        "line_length",       100,    5000,    1 },
   {   CLOCK_PPM,    "Clock Tolerance",    "clock_tolerance",         0,  100000,  100 },
   { LINES_FRAME,    "Lines per Frame",    "lines_per_frame",       250,    1200,    1 },
   {   SYNC_TYPE,          "Sync Type",          "sync_type",         0, NUM_SYNC-1, 1 },
   {  VSYNC_TYPE,        "V Sync Type",         "vsync_type",         0,NUM_VSYNC-1, 1 },
   {  VIDEO_TYPE,         "Video Type",         "video_type",         0,NUM_VIDEO-1, 1 },
   { PX_SAMPLING,     "Pixel Sampling",     "pixel_sampling",         0,   NUM_PS-1, 1 },
   {          -1,                 NULL,                 NULL,         0,          0, 0 }
};

typedef struct {
   int setup_mode;        // dummy entry for setup
   int h_offset;          // horizontal offset (in psync clocks)
   int v_offset;          // vertical offset (in lines)
   int min_h_width;       // active horizontal width (in 8-bit characters)
   int min_v_height;      // active vertical height (in lines)
   int max_h_width;       // framebuffer width in pixels
   int max_v_height;      // framebuffer height (in pixels, before any doubling is applied)
   int h_aspect;          // horizontal pixel aspect ratio
   int v_aspect;          // vertical pixel aspect ratio
   int fb_sizex2;         // if 1 then double frame buffer height if 2 double width if 3 then both
   int fb_bpp;            // framebuffer bits per pixel
   int clock;             // cpld clock (in Hz)
   int line_len;          // number of clocks per horizontal line
   int clock_ppm;         // cpld tolerance (in ppm)
   int lines_per_frame;   // number of lines per frame
   int sync_type;         // sync type and polarity
   int vsync_type;        // vsync type auto/interlaced/non-interlaced
   int video_type;       // deinterlace type off/teletext
   int px_sampling;       // pixel sampling mode
} geometry_t;

static int modeset;
static geometry_t *geometry;
static geometry_t set1_geometry;
static geometry_t set2_geometry;
static int scaling = 0;
static int capvscale = 1;
static int caphscale = 1;
static int fhaspect = 1;
static int fvaspect = 1;
static int use_px_sampling = 1;

void geometry_init(int version) {
   // These are Beeb specific defaults so the geometry property can be ommitted
   set2_geometry.setup_mode    =         0;
   set2_geometry.v_offset      =        18;
   set2_geometry.min_h_width   =       504 & 0xfffffff8;
   set2_geometry.min_v_height  =       270 & 0xfffffffe;
   set2_geometry.max_h_width   =       504 & 0xfffffff8;
   set2_geometry.max_v_height  =       270 & 0xfffffffe;
   set2_geometry.h_aspect      =         3;
   set2_geometry.v_aspect      =         4;
   set2_geometry.fb_sizex2     =         SIZEX2_DOUBLE_HEIGHT;
   set2_geometry.fb_bpp        =         0;
   set2_geometry.clock         =  12000000;
   set2_geometry.line_len      =   12 * 64;
   set2_geometry.clock_ppm     =      5000;
   set2_geometry.lines_per_frame   =   312;
   set2_geometry.sync_type     = SYNC_COMP;
   set2_geometry.vsync_type    = VSYNC_AUTO;
   set2_geometry.video_type    = VIDEO_TELETEXT;
   set2_geometry.px_sampling   = PS_NORMAL;

   set1_geometry.setup_mode  =         0;
   set1_geometry.v_offset    =        21;
   set1_geometry.min_h_width =       672 & 0xfffffff8;
   set1_geometry.min_v_height=       270 & 0xfffffffe;
   set1_geometry.max_h_width =       672 & 0xfffffff8;
   set1_geometry.max_v_height=       270 & 0xfffffffe;
   set1_geometry.h_aspect    =         1;
   set1_geometry.v_aspect    =         2;
   set1_geometry.fb_sizex2   =         SIZEX2_DOUBLE_HEIGHT;
   set1_geometry.fb_bpp      =         1;
   set1_geometry.clock       =  16000000;
   set1_geometry.line_len    =   16 * 64;
   set1_geometry.clock_ppm   =      5000;
   set1_geometry.lines_per_frame =   312;
   set1_geometry.sync_type   = SYNC_COMP;
   set1_geometry.vsync_type  = VSYNC_AUTO;
   set1_geometry.video_type  = VIDEO_PROGRESSIVE;
   set1_geometry.px_sampling = PS_NORMAL;

   int firmware_support = cpld->old_firmware_support();

   if (firmware_support & BIT_NORMAL_FIRMWARE_V1) {
      // For backwards compatibility with CPLDv1
      set2_geometry.h_offset   = 0;
      set1_geometry.h_offset = 0;
   } else if (firmware_support & BIT_NORMAL_FIRMWARE_V2) {
      // For backwards compatibility with CPLDv2
      set2_geometry.h_offset   = 96 & 0xfffffffc;
      set1_geometry.h_offset = 128 & 0xfffffffc;
   } else {
      // For CPLDv3 onwards
      set2_geometry.h_offset   = 140 & 0xfffffffc;
      set1_geometry.h_offset = 160 & 0xfffffffc;
   }
   geometry_set_mode(MODE_SET1);
}

void geometry_set_mode(int mode) {
   modeset = mode;
   if (modeset == MODE_SET1) {
       geometry = &set1_geometry;
   } else {
       geometry = &set2_geometry;
   }
}
int geometry_get_mode() {
   return modeset;
}
int geometry_get_value(int num) {
   switch (num) {
   case SETUP_MODE:
      return geometry->setup_mode;
   case H_OFFSET:
      return geometry->h_offset & 0xfffffffc;
   case V_OFFSET:
      return geometry->v_offset;
   case MIN_H_WIDTH:
      return geometry->min_h_width & 0xfffffff8;
   case MIN_V_HEIGHT:
      return geometry->min_v_height & 0xfffffffe;
   case MAX_H_WIDTH:
      return geometry->max_h_width & 0xfffffff8;
   case MAX_V_HEIGHT:
      return geometry->max_v_height & 0xfffffffe;
   case H_ASPECT:
      return geometry->h_aspect;
   case V_ASPECT:
      return geometry->v_aspect;
   case FB_SIZEX2:
      return geometry->fb_sizex2;
   case FB_BPP:
      return geometry->fb_bpp;
   case CLOCK:
      return geometry->clock;
   case LINE_LEN:
      return geometry->line_len;
   case CLOCK_PPM:
      return geometry->clock_ppm;
   case LINES_FRAME:
      return geometry->lines_per_frame;
   case SYNC_TYPE:
      return geometry->sync_type;
   case VSYNC_TYPE:
      return geometry->vsync_type;
   case VIDEO_TYPE:
      return geometry->video_type;
   case PX_SAMPLING:
      if (use_px_sampling == 0) {
        geometry->px_sampling = 0;
      }
      return geometry->px_sampling;
   }
   return -1;
}

const char *geometry_get_value_string(int num) {
   if (num == SETUP_MODE) {
      return setup_names[geometry_get_value(num)];
   }
   if (num == PX_SAMPLING) {
      return px_sampling_names[geometry_get_value(num)];
   }
   if (num == SYNC_TYPE) {
      return sync_names[geometry_get_value(num)];
   }
   if (num == VSYNC_TYPE) {
      return vsync_names[geometry_get_value(num)];
   }
   if (num == FB_SIZEX2) {
      return fb_sizex2_names[geometry_get_value(num)];
   }
   if (num == VIDEO_TYPE) {
      return deint_names[geometry_get_value(num)];
   }
   if (num == FB_BPP) {
      return bpp_names[geometry_get_value(num)];
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
   case SETUP_MODE:
      geometry->setup_mode = value;
      if (value == SETUP_FINE) {
         params[CLOCK].step = 1;
      } else {
         params[CLOCK].step = 1000;
      }
      break;
   case H_OFFSET:
      geometry->h_offset = value & 0xfffffffc;
      break;
   case V_OFFSET:
      geometry->v_offset = value;
      break;
   case MIN_H_WIDTH:
      geometry->min_h_width = value & 0xfffffff8;
      break;
   case MIN_V_HEIGHT:
      geometry->min_v_height = value & 0xfffffffe;
      break;
   case MAX_H_WIDTH:
      geometry->max_h_width = value & 0xfffffff8;
      break;
   case MAX_V_HEIGHT:
      geometry->max_v_height = value & 0xfffffffe;
      break;
   case H_ASPECT:
      geometry->h_aspect = value;
      break;
   case V_ASPECT:
      geometry->v_aspect = value;
      break;
   case FB_SIZEX2:
      geometry->fb_sizex2 = value;
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
      geometry->lines_per_frame = value;
      break;
   case SYNC_TYPE:
      geometry->sync_type = value;
      break;
   case VSYNC_TYPE:
      geometry->vsync_type = value;
      break;
   case VIDEO_TYPE:
      geometry->video_type = value;
      break;
   case PX_SAMPLING:
      if (use_px_sampling == 0) {
         geometry->px_sampling = 0;
      } else {
         geometry->px_sampling = value;
      }
      break;
   }
}

param_t *geometry_get_params() {
   return params;
}

void set_gscaling(int value) {
   scaling = value;
}

int get_gscaling() {
   return scaling;
}

void set_setup_mode(int mode) {
    geometry_set_value(SETUP_MODE, mode);
    //log_info("setup mode = %d", mode);
}

void geometry_get_fb_params(capture_info_t *capinfo) {
    int top = 0;
    int bottom = 0;
    int left = 0;
    int right = 0;

    if (get_startup_overscan() != 0) {
        set_config_overscan(0, 0, 0, 0);
    }

    capinfo->sync_type      = geometry->sync_type;
    capinfo->vsync_type     = geometry->vsync_type;
    capinfo->video_type     = geometry->video_type;
    capinfo->autoswitch     = get_parameter(F_AUTO_SWITCH);
    capinfo->timingset      = modeset;
    capinfo->sync_edge      = cpld->get_sync_edge();

    if (capinfo->video_type == VIDEO_LINE_DOUBLED) {
        capinfo->video_type = VIDEO_PROGRESSIVE;
    }

    if (capinfo->vsync_type == VSYNC_FORCE_INTERLACE) {
        capinfo->vsync_type = VSYNC_INTERLACED;
    }

    capinfo->sizex2 = geometry->fb_sizex2;
    switch(geometry->fb_bpp) {
        case BPP_4:
           capinfo->bpp = 4;
           break;
        default:
        case BPP_8:
           capinfo->bpp = 8;
           break;
        case BPP_16:
           capinfo->bpp = 16;
           break;
    }

    capinfo->mode7 = 0;
    if (capinfo->video_type == VIDEO_TELETEXT) {
        capinfo->mode7 = 1;
        if (capinfo->bpp != 4) {
             capinfo->video_type = VIDEO_INTERLACED;
        }
    }

    if (capinfo->video_type == VIDEO_INTERLACED && capinfo->detected_sync_type & SYNC_BIT_INTERLACED && (menu_active() || osd_active())) {
        capinfo->video_type = VIDEO_PROGRESSIVE;
    }

    if (capinfo->video_type == VIDEO_TELETEXT) {
        capinfo->bpp = 4; //force 4bpp for teletext
    } else if (capinfo->sample_width >= SAMPLE_WIDTH_9LO && capinfo->bpp == 4) {
        capinfo->bpp = 8; //force at least 8bpp in 12 bit modes as no capture loops for capture into 4bpp buffer
    } else if (capinfo->sample_width == SAMPLE_WIDTH_6 && capinfo->bpp < 8) {
        capinfo->bpp = 8; //force 8bpp in 6 bit modes as no capture loops for 6 bit capture into 4 bpp buffer
    } else if (capinfo->sample_width == SAMPLE_WIDTH_6 && capinfo->bpp > 8 && (get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_C64_LUMACODE || get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_C64_YUV)
              && (get_parameter(F_NTSC_COLOUR) == 0 || (capinfo->sizex2 & SIZEX2_DOUBLE_WIDTH) == 0)) {
        capinfo->bpp = 8; //force 8bpp in 6 bit modes when pal artifact disabled
    } else if (capinfo->sample_width == SAMPLE_WIDTH_6 && capinfo->bpp > 8 && (get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_ATARI_LUMACODE)
              && (get_parameter(F_SCANLINES) == 0 || (capinfo->sizex2 & SIZEX2_DOUBLE_WIDTH) == 0)) {
        capinfo->bpp = 8; //force 8bpp in 6 bit modes when scanlines disabled
    } else if (capinfo->sample_width == SAMPLE_WIDTH_6 && capinfo->bpp > 8 && (get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_ATARI_GTIA || get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_ATARI2600_LUMACODE)) {
        capinfo->bpp = 8; //force 8bpp in 6 bit modes when Atari GTIA or 2600 as no 16 bit capture loops
    } else if (capinfo->sample_width == SAMPLE_WIDTH_6 && capinfo->bpp > 8
               && get_parameter(F_PALETTE_CONTROL) >= PALETTECONTROL_NTSCARTIFACT_CGA && get_parameter(F_PALETTE_CONTROL) <= PALETTECONTROL_NTSCARTIFACT_BW_AUTO
               && (get_core_1_available() == 0 || get_parameter(F_NTSC_TYPE) == NTSCTYPE_SIMPLE)) {
        capinfo->bpp = 8; //force 8bpp in 6 bit modes when simple ntsc artifact
    } else if (capinfo->sample_width <= SAMPLE_WIDTH_3 && capinfo->bpp > 8) {
        capinfo->bpp = 8; //force 8bpp in 1 & 3 bit modes as no capture loops for 1 or 3 bit capture into 16bpp buffer
    }

#ifdef USE_ARM_CAPTURE
    if ((_get_hardware_id() == _RPI2 || _get_hardware_id() == _RPI3) && capinfo->video_type != VIDEO_TELETEXT) {
        capinfo->sizex2 &= SIZEX2_DOUBLE_WIDTH;   //in ARM build have to inhibit double height on Pi Zero 2Pi2 / Pi 3 otherwise you get stalling
    }
    if (_get_hardware_id() == _RPI && capinfo->video_type != VIDEO_TELETEXT && capinfo->sample_width >= SAMPLE_WIDTH_9LO) {
        capinfo->sizex2 &= SIZEX2_DOUBLE_WIDTH;   //in ARM build have to inhibit double height on Pi zero / Pi 1 in 9/12bpp capture
    }
#endif

    if (capinfo->bpp <= 8 && get_parameter(F_SCANLINES) && (get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_ATARI_LUMACODE || get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_ATARI_GTIA)) {
        capinfo->sizex2 &= SIZEX2_DOUBLE_WIDTH;  //inhibit double height for Atari 800 in 8bpp mode
    }

    if ((capinfo->detected_sync_type & SYNC_BIT_INTERLACED) && capinfo->video_type != VIDEO_PROGRESSIVE) {
        capinfo->sizex2 |= SIZEX2_DOUBLE_HEIGHT;
    } else {
        if (get_parameter(F_SCANLINES) && !(menu_active() || osd_active())) {
            if ((capinfo->sizex2 & SIZEX2_DOUBLE_HEIGHT) == 0) {
                capinfo->sizex2 |= SIZEX2_BASIC_SCANLINES;      //flag basic scanlines
            }
            capinfo->sizex2 |= SIZEX2_DOUBLE_HEIGHT;    // force double height
        }
    }

    int geometry_h_offset = geometry->h_offset;
    int geometry_v_offset = geometry->v_offset;
    int geometry_min_h_width = geometry->min_h_width;
    int geometry_min_v_height = geometry->min_v_height;
    int geometry_max_h_width = geometry->max_h_width;
    int geometry_max_v_height = geometry->max_v_height;
    int h_aspect = geometry->h_aspect;
    int v_aspect = geometry->v_aspect;

    if (get_parameter(F_SWAP_ASPECT)) {
        if (geometry->lines_per_frame > 287) {
            if (h_aspect == v_aspect) {
                h_aspect = 4;
                v_aspect = 5;
            } else if ((h_aspect << 1) == v_aspect) {
                h_aspect = 2;
                v_aspect = 5;
            } else if (h_aspect == (v_aspect << 1)) {
                h_aspect = 8;
                v_aspect = 5;
            } else if (h_aspect == 7 && v_aspect == 4) {
                v_aspect = 5;
            }
            if (geometry_min_v_height > 250) {
                geometry_min_v_height = geometry_min_v_height * 4 / 5;
            }
            geometry_max_v_height = geometry_max_v_height * 4 / 5;
        } else {
            if (h_aspect == 4 && v_aspect == 5) {
                v_aspect = 4;
            } else if (h_aspect == 2 && v_aspect == 5) {
                v_aspect = 4;
            } else if (h_aspect == 8 && v_aspect == 5) {
                v_aspect = 4;
            } else if (h_aspect == 7 && v_aspect == 5) {
                v_aspect = 4;
            }
            //geometry_max_v_height = geometry_max_v_height * 5 / 4;
        }
    }

    //if (get_parameter(F_CROP_BORDER) == OVERSCAN_AUTO && (geometry->setup_mode == SETUP_NORMAL || geometry->setup_mode == SETUP_CLOCK)) {
        //reduce max area by 4% to hide offscreen imperfections
    //    geometry_max_h_width = ((geometry_max_h_width * 96) / 100) & 0xfffffff8;
    //    geometry_max_v_height = ((geometry_max_v_height * 96) / 100) & 0xfffffffe;
    //}

    if (geometry_max_h_width < geometry_min_h_width) {
        geometry_max_h_width = geometry_min_h_width;
    }
    if (geometry_max_v_height < geometry_min_v_height) {
        geometry_max_v_height = geometry_min_v_height;
    }

    if (use_px_sampling != 0) {
        capinfo->px_sampling = geometry->px_sampling;
    } else {
        capinfo->px_sampling = 0;
    }

    if (geometry->setup_mode == SETUP_NORMAL) {
        if (((get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_NTSCARTIFACT_CGA && get_parameter(F_NTSC_COLOUR) != 0)
          || (get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_NTSCARTIFACT_BW)
          || (get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_NTSCARTIFACT_BW_AUTO))
          && capinfo->bpp == 8 && capinfo->sample_width <= SAMPLE_WIDTH_6) {
            capinfo->border = get_parameter(F_BORDER_COLOUR);
        } else {
            capinfo->border = get_parameter(F_BORDER_COLOUR);
            if (get_parameter(F_OUTPUT_INVERT) == INVERT_Y) {
                capinfo->border ^= 0x12;
            }
        }
    } else {
        capinfo->border = 0x12;    // max green/Y
    }

    capinfo->ntscphase = get_adjusted_ntscphase() | (get_parameter(F_NTSC_COLOUR) << NTSC_ARTIFACT_SHIFT);
    if (get_parameter(F_OUTPUT_INVERT) == INVERT_Y) {
        capinfo->ntscphase |= NTSC_Y_INVERT;
    }
    if (get_parameter(F_OUTPUT_INVERT) == INVERT_RGB) {
        capinfo->ntscphase |= NTSC_RGB_INVERT;
    }
    if (get_parameter(F_HDMI_MODE_STANDBY)) {
        capinfo->ntscphase |= NTSC_HDMI_BLANK_ENABLE;
    }

    if (get_parameter(F_FFOSD)) {
        capinfo->ntscphase |= NTSC_FFOSD_ENABLE;
    }

    get_config_overscan(&left, &right, &top, &bottom);
    int h_size = get_hdisplay() - left - right;
    int v_size = get_vdisplay() - top - bottom;

    double ratio = (double) h_size / v_size;
    int h_size43 = h_size;
    int v_size43 = v_size;


    if (scaling == GSCALING_INTEGER) {
        if (ratio > 1.34) {
           h_size43 = v_size * 4 / 3;
        }
        if (ratio < 1.24) {               // was 1.32 but don't correct 5:4 aspect ratio (1.25) to 4:3 as it does good integer scaling for 640x256 and 640x200
           v_size43 = h_size * 3 / 4;
        }
    } else {
        if (ratio > 1.34) {
           h_size43 = v_size * 4 / 3;
        }
        if (ratio < 1.24) {
           v_size43 = h_size * 3 / 4;
        }
    }

    if (scaling == GSCALING_INTEGER && v_size43 == v_size && h_size > h_size43) {
        //if ((geometry_max_h_width >= 512 && geometry_max_h_width <= 800) || (geometry_max_h_width > 360 && geometry_max_h_width <= 400)) {
        //h_size43 = (h_size43 * 912) / 720;           //adjust 4:3 ratio on widescreen resolutions to account for up to 900 pixel wide integer sample capture
        if (geometry_min_h_width > 800) {
            h_size43 = h_size;
        } else {
            h_size43 = (h_size43 * 800) / 720;           //adjust 4:3 ratio on widescreen resolutions to account for up to 800 pixel wide integer sample capture
        }

        //if (h_size43 > h_size) {
        //    h_size43 = h_size;
        //}
    }

    int double_width = (capinfo->sizex2 & SIZEX2_DOUBLE_WIDTH) >> 1;
    int double_height = capinfo->sizex2 & SIZEX2_DOUBLE_HEIGHT;
    if ((geometry_min_h_width << double_width) > h_size43) {
        double_width =  0;
    }
    if ((geometry_min_v_height << double_height) > v_size43) {
        double_height = 0;
    }
    if (double_height && (capinfo->sizex2 & SIZEX2_BASIC_SCANLINES)) {
        capinfo->sizex2 = double_height | (double_width << 1) | SIZEX2_BASIC_SCANLINES;
    } else {
        capinfo->sizex2 = double_height | (double_width << 1);
    }

    //log_info("unadjusted integer = %d, %d, %d, %d, %d, %d", geometry_h_offset, geometry_v_offset, geometry_min_h_width, geometry_min_v_height, geometry_max_h_width, geometry_max_v_height);

    switch (geometry->setup_mode) {
        case SETUP_NORMAL:
        case SETUP_CLOCK:
        case SETUP_FINE:
        default:
            {
                int scaled_min_h_width;
                int scaled_min_v_height;
                double max_aspect = (double)geometry_max_h_width / (double)geometry_max_v_height;
                double min_aspect = (double)geometry_min_h_width / (double)geometry_min_v_height;
                if (min_aspect > max_aspect) {
                    scaled_min_h_width = geometry_min_h_width;
                    scaled_min_v_height = ((int)((double)scaled_min_h_width / max_aspect));
                    if (scaled_min_v_height < geometry_min_v_height) {
                        scaled_min_v_height = geometry_min_v_height;
                    }
                } else {
                    scaled_min_v_height = geometry_min_v_height;
                    scaled_min_h_width = ((int)((double)scaled_min_v_height * max_aspect));
                    if (scaled_min_h_width < geometry_min_h_width) {
                        scaled_min_h_width = geometry_min_h_width;
                    }
                }
                geometry_max_h_width = (geometry_max_h_width - ((geometry_max_h_width - scaled_min_h_width) * get_parameter(F_CROP_BORDER) / (NUM_OVERSCAN - 1))) & 0xfffffff8;
                geometry_max_v_height = (geometry_max_v_height - ((geometry_max_v_height - scaled_min_v_height) * get_parameter(F_CROP_BORDER) / (NUM_OVERSCAN - 1))) & 0xfffffffe;
                if (geometry_max_h_width < geometry_min_h_width) {
                    geometry_max_h_width = geometry_min_h_width;
                }
                if (geometry_max_v_height < geometry_min_v_height) {
                    geometry_max_v_height = geometry_min_v_height;
                }
                if (scaling != GSCALING_INTEGER) {
                    geometry_h_offset = geometry_h_offset - (((geometry_max_h_width - geometry_min_h_width) >> 3) << 2);
                    geometry_v_offset = geometry_v_offset - ((geometry_max_v_height - geometry_min_v_height) >> 1);
                    geometry_min_h_width = geometry_max_h_width;
                    geometry_min_v_height = geometry_max_v_height;
                }
            }
        break;
        case SETUP_MIN:
            geometry_max_h_width = geometry_min_h_width;
            geometry_max_v_height = geometry_min_v_height;
        break;
        case SETUP_MAX:
            geometry_h_offset = geometry_h_offset - (((geometry_max_h_width - geometry_min_h_width) >> 3) << 2);
            geometry_v_offset = geometry_v_offset - ((geometry_max_v_height - geometry_min_v_height) >> 1);
            geometry_min_h_width = geometry_max_h_width;
            geometry_min_v_height = geometry_max_v_height;
        break;

    }

    //log_info("adjusted integer = %d, %d, %d, %d, %d, %d", geometry_h_offset, geometry_v_offset, geometry_min_h_width, geometry_min_v_height, geometry_max_h_width, geometry_max_v_height);

    int h_size43_adj = h_size43;
    if ((capinfo->mode7 && get_parameter(F_MODE7_SCALING) == SCALING_UNEVEN)
     || (!capinfo->mode7 && get_parameter(F_NORMAL_SCALING) == SCALING_UNEVEN && geometry->h_aspect == 3 && (geometry->v_aspect == 2 || geometry->v_aspect == 4))) {
        h_size43_adj = h_size43 * 3 / 4;
        if (h_aspect == 3 && v_aspect == 2) {
            h_aspect = 1;
            v_aspect = 1;
        } else if (h_aspect == 3 && v_aspect == 4) {
            h_aspect = 1;
            v_aspect = 2;
        }
    }

    int hscale = h_size43_adj / geometry_min_h_width;
    int vscale = v_size43 / geometry_min_v_height;
    if (hscale < 1) {
        hscale = 1;
    }
    if (vscale < 1) {
        vscale = 1;
    }


    if (get_hdisplay() > 3000 && hscale > 4 && vscale > 4) {       //even up scaling of small sources on 4K monitors
        hscale = (hscale >> 1) << 1;
        vscale = (vscale >> 1) << 1;
    }

    if (h_aspect != 0 && v_aspect !=0 && get_parameter(F_INTEGER_ASPECT) == 0) {
        int new_hs = hscale;
        int new_vs = vscale;
        double h_ratio;
        double v_ratio;
        int abort_count = 0;
        do {
            h_ratio = (double)hscale / h_aspect;
            v_ratio = (double)vscale / v_aspect;
            if (h_ratio != v_ratio) {
                if  (h_ratio > v_ratio) {
                    new_hs = ((int)v_ratio) * h_aspect;
                } else {
                    new_vs = ((int)h_ratio) * v_aspect;
                }
                //log_info("Aspect doesn't match: %d, %d, %d, %d, %f, %f, %d, %d", hscale, vscale, h_aspect, v_aspect, h_ratio, v_ratio, new_hs, new_vs);
                if (new_hs !=0 && new_vs != 0) {
                    hscale = new_hs;
                    vscale = new_vs;

                }
                //log_info("Aspect after loop: %d, %d", hscale, vscale);
            }
          abort_count++;
        } while (new_hs !=0 && new_vs != 0 && h_ratio != v_ratio && abort_count < 10);
    }
    //log_info("Final aspect: %d, %d", hscale, vscale);

    if (scaling == GSCALING_INTEGER) {
        int new_geometry_min_h_width = h_size43_adj / hscale;
        if (new_geometry_min_h_width > geometry_max_h_width) {
            new_geometry_min_h_width = geometry_max_h_width;
        }
        int new_geometry_min_v_height = v_size43 / vscale;
        if (new_geometry_min_v_height > geometry_max_v_height) {
            new_geometry_min_v_height = geometry_max_v_height;
        }
        geometry_h_offset = geometry_h_offset - (((new_geometry_min_h_width - geometry_min_h_width) >> 3) << 2);
        geometry_v_offset = geometry_v_offset - ((new_geometry_min_v_height - geometry_min_v_height) >> 1);
        geometry_min_h_width = new_geometry_min_h_width;
        geometry_min_v_height = new_geometry_min_v_height;
    }

    capinfo->delay = (cpld->get_delay() ^ 3) & 3;               // save delay for simple mode software implementation

    geometry_h_offset = geometry_h_offset * lumacode_multiplier() - ((cpld->get_delay() >> 2) << 2);

    if (geometry_h_offset < 0) {
       geometry_min_h_width += (geometry_h_offset << 1);
       geometry_h_offset = 0;
    }
    if (geometry_v_offset < 0) {
       geometry_min_v_height += (geometry_v_offset << 1);
       geometry_v_offset = 0;
    }

    //log_info("adjusted integer2 = %d, %d, %d, %d, %d, %d, %d", cpld->get_delay() >> 2, geometry_h_offset, geometry_v_offset, geometry_min_h_width, geometry_min_v_height, geometry_max_h_width, geometry_max_v_height);

    switch (capinfo->sample_width) {
            case SAMPLE_WIDTH_1 :
                capinfo->h_offset = geometry_h_offset >> 3;
            break;
            default:
            case SAMPLE_WIDTH_3:
                capinfo->h_offset = geometry_h_offset >> 2;
            break;
            case SAMPLE_WIDTH_6 :
                capinfo->h_offset = (geometry_h_offset >> 2) << 1;
            break;
            case SAMPLE_WIDTH_9LO :
            case SAMPLE_WIDTH_9HI :
            case SAMPLE_WIDTH_12 :
                capinfo->h_offset = (geometry_h_offset >> 2) << 2;
            break;
    }

    capinfo->v_offset = geometry_v_offset;


    capinfo->chars_per_line = ((geometry_min_h_width + 7) >> 3) << double_width;
    capinfo->nlines         = geometry_min_v_height;

    //log_info("scaling size = %d, %d, %d, %f",standard_width, adjusted_width, adjusted_height, ratio);
    //log_info("scaling h = %d, %d, %f, %d, %d, %d, %d",h_size, h_size43, hscalef, hscale, hborder, hborder43, newhborder43);
    //log_info("scaling v = %d, %d, %f, %d, %d, %d, %d",v_size, v_size43, vscalef, vscale, vborder, vborder43, newvborder43);

    caphscale = hscale;
    capvscale = vscale;

    if (caphscale == capvscale) {
        caphscale = 1;
        capvscale = 1;
    }
    while ((caphscale & 1) == 0 && (capvscale & 1) == 0) {
        caphscale >>= 1;
        capvscale >>= 1;
    }

    fhaspect = caphscale;
    fvaspect = capvscale;

    if (caphscale == 1 && capvscale == 1 && geometry->min_h_width < 512) {
        caphscale = 2;
        capvscale = 2;
    }

    //log_info("Final aspect: %dx%d, %dx%d, %dx%d %d", h_aspect, v_aspect, hscale, vscale, caphscale, capvscale, geometry_min_h_width );

    switch (scaling) {
        case    GSCALING_INTEGER:
        {

            int adjusted_width = geometry_min_h_width << double_width;
            int adjusted_height = geometry_min_v_height << double_height;

            int hborder = ((h_size - geometry_min_h_width * hscale) << double_width) / hscale;
            if ((hborder + adjusted_width) > h_size) {
                log_info("Handling invalid H ratio");
                hborder = (h_size - adjusted_width) / hscale;
            }

            int vborder  = ((v_size - geometry_min_v_height * vscale) << double_height) / vscale;
            if ((vborder + adjusted_height) > v_size) {
                log_info("Handling invalid V ratio");
                vborder  = (v_size - adjusted_height) / vscale;
            }

            capinfo->width = adjusted_width + hborder;
            capinfo->height = adjusted_height + vborder;

            if ((capinfo->mode7 && get_parameter(F_MODE7_SCALING) == SCALING_UNEVEN)             // workaround mode 7 width so it looks like other modes
             ||(!capinfo->mode7 && get_parameter(F_NORMAL_SCALING) == SCALING_UNEVEN && geometry->h_aspect == 3 && (geometry->v_aspect == 2 || geometry->v_aspect == 4))) {
                capinfo->width = capinfo->width * 3 / 4;
            }

            if  (get_parameter(F_SCREENCAP_SIZE) == SCREENCAP_FULL || get_parameter(F_SCREENCAP_SIZE) == SCREENCAP_FULL43) {
                caphscale = ((h_size << double_width) / capinfo->width);
                capvscale = ((v_size << double_height) / capinfo->height);
            }
        }
        break;
        case    GSCALING_MANUAL43:
        {
            double hscalef = (double) h_size43 / geometry_min_h_width;
            double vscalef = (double) v_size43 / geometry_min_v_height;
            capinfo->width = (geometry_max_h_width << double_width ) + (int)((double)((h_size - h_size43) <<  double_width) / hscalef);
            capinfo->height = (geometry_max_v_height << double_height) + (int)((double)((v_size - v_size43) << double_height)  / vscalef);
        }
        break;
        case    GSCALING_MANUAL:
        {
            capinfo->width          = geometry_max_h_width << double_width;          //adjust the width for capinfo according to sizex2 setting;
            capinfo->height         = geometry_max_v_height << double_height;        //adjust the height for capinfo according to sizex2 setting
        }
        break;
    };

    capinfo->width &= 0xfffffffe;
    capinfo->height &= 0xfffffffe;

    /*
    int pitchinchars = capinfo->pitch;

    switch(capinfo->bpp) {
         case 4:
            pitchinchars >>= 2;
            break;
         case 8:
            pitchinchars >>= 3;
            break;
         case 16:
            pitchinchars >>= 4;
            break;
    }

    if (capinfo->chars_per_line > pitchinchars) {
       //log_info("Clipping capture width to pitch: %d, %d", capinfo->chars_per_line, pitchinchars);
       capinfo->chars_per_line =  pitchinchars;
    }
    */

    if (capinfo->nlines > (capinfo->height >> double_height)) {
       capinfo->nlines = (capinfo->height >> double_height);
    }
    int lines = get_lines_per_vsync(1);
    int width = get_vsync_width_lines();
    if ((capinfo->nlines + capinfo->v_offset) > (lines - width - 1)) {
        capinfo->nlines = (lines - width - 1) - capinfo->v_offset;
        //log_info("Clipping capture height to %d", capinfo->nlines);
    }

    if (capinfo->video_type != VIDEO_PROGRESSIVE && capinfo->detected_sync_type & SYNC_BIT_INTERLACED) {
        capvscale >>= 1;
        if (double_width) {
            caphscale >>= 1;
        }
    } else {
        if (osd_active() || get_parameter(F_SCANLINES)) {
            if (double_width) {
                caphscale >>= 1;
            }
            if (double_height) {
                capvscale >>= 1;
            }
        } else {
            if (double_width) {
                caphscale |= 0x80000000;
            }
            if (double_height) {
                capvscale |= 0x80000000;
            }
        }
    }
    //log_info("Final aspect2: %dx%d, %dx%d, %dx%d", h_aspect, v_aspect, hscale, vscale, caphscale, capvscale);
    calculate_cpu_timings();
    //log_info("size= %d, %d, %d, %d, %d, %d, %d",capinfo->chars_per_line, capinfo->nlines, geometry_min_h_width, geometry_min_v_height,capinfo->width,  capinfo->height, capinfo->sizex2);

    if (geometry->video_type == VIDEO_LINE_DOUBLED && (capinfo->sizex2 & SIZEX2_DOUBLE_HEIGHT) != 0) {
        capinfo->nlines <<= 1;
        capinfo->sizex2 &= SIZEX2_DOUBLE_WIDTH;
    }

    int uneven = ((capinfo->mode7 && get_parameter(F_MODE7_SCALING) == SCALING_UNEVEN) ||(!capinfo->mode7 && get_parameter(F_NORMAL_SCALING) == SCALING_UNEVEN));

    if (get_startup_overscan() != 0) {        //for 16bpp modes reduce the screen area to the actual capture size and make up the rest with overscan on Pi zero due to bandwidth issues
        int apparent_width = get_hdisplay();
        int apparent_height = get_vdisplay();
        double_width = (capinfo->sizex2 & SIZEX2_DOUBLE_WIDTH) >> 1;
        double_height = capinfo->sizex2 & SIZEX2_DOUBLE_HEIGHT;
        hscale >>= double_width;
        //if (_get_hardware_id() == _RPI && !uneven && (capinfo->bpp == 16 || (capinfo->bpp != 16 && capinfo->nlines > 288))) {
        if (_get_hardware_id() == _RPI && capinfo->bpp == 16 && !uneven) {
            if (get_gscaling() == GSCALING_INTEGER) {
                int actual_width = (capinfo->chars_per_line << 3);
                int actual_height = capinfo->nlines;
                left = (apparent_width - (actual_width * hscale)) / 2;
                top = (apparent_height - (actual_height * vscale)) / 2;
                if (left >=0 && top >=0) {
                    right = left;
                    bottom = top;
                    capinfo->width = actual_width;
                    capinfo->height = actual_height << double_height;
                } else {
                    left = 0;
                    right = 0;
                    top = 0;
                    bottom = 0;
                }
                //log_info("sizes = %d %d %d %d %d %d %d %d %d %d", apparent_width,apparent_height,actual_width, actual_height ,hscale,vscale,left,right,top,bottom);
            } else {
                top = 0;
                bottom = 0;
                double aspect = (double) apparent_width / (double) apparent_height;
                int apparent_width_limit = (apparent_width * 1600 / 1920) & ~1;
                if (aspect >= 1.6 && get_gscaling() != GSCALING_MANUAL) {
                    left =  (apparent_width - apparent_width_limit) / 2;
                    right = left;
                    capinfo->width = capinfo->width * 1600 / 1920;
                } else {
                    left = 0;
                    right = 0;
                }

            //log_info("sizes = %d %d %d %d", apparent_width,capinfo->width, left,right);
            }
        } else {
            left = 0;
            right = 0;
            top = 0;
            bottom = 0;
        }
        set_config_overscan(left, right, top, bottom);
    }

}

int get_hscale() {
    return caphscale;
}
int get_vscale() {
    return capvscale;
}
int get_haspect() {
    return fhaspect;
}
int get_vaspect() {
    return fvaspect;
}

int get_hdisplay() {
#if defined(RPI4)
    int h_size = ((*PIXELVALVE2_HORZB) & 0xFFFF) << 1;
#else
    int h_size = (*PIXELVALVE2_HORZB) & 0xFFFF;
#endif
    int v_size = (*PIXELVALVE2_VERTB) & 0xFFFF;
    if (h_size == 720 && v_size == 240) {
          log_info("HDMI readback of screen size indicates HDMI not connected (%dx%d) - rebooting", h_size, v_size);
          delay_in_arm_cycles_cpu_adjust(1000000000);
          reboot();
    }
    //workaround for 640x480 and 800x480 @50Hz using double rate clock so width gets doubled
    if (v_size == 480 && h_size == 1280) {
        h_size = 640;
    } else if (v_size == 480 && h_size == 1600) {
        h_size = 800;
    } else if (v_size <= 288) {
        h_size >>= 1;
    }
    return h_size;
}

int get_vdisplay() {
    int v_size = (*PIXELVALVE2_VERTB) & 0xFFFF;
    if (v_size == 2160 && get_hdisplay() == 1920){
        v_size = 1080;
    } else if (v_size <= 288) {
        v_size <<= 1;
    }
    return v_size;
}

int get_true_vdisplay() {
    int v_size = (*PIXELVALVE2_VERTB) & 0xFFFF;
    return v_size;
}

void geometry_get_clk_params(clk_info_t *clkinfo) {
   clkinfo->clock            = geometry->clock;
   clkinfo->line_len         = (double) geometry->line_len;
    // workaround for 16.363Mhz Apple II GS pixel clock
   if (clkinfo->clock > 16250000 && clkinfo->clock < 16370000 && clkinfo->line_len == 1042) {
       clkinfo->line_len = 1042.285714285f;
   }
   clkinfo->lines_per_frame  = geometry->lines_per_frame;
   if (geometry->setup_mode == SETUP_NORMAL) {
       clkinfo->clock_ppm    = geometry->clock_ppm;
   } else {
       clkinfo->clock_ppm    = 0;
   }
}

void geometry_hide_pixel_sampling() {
    params[PX_SAMPLING].key = -1;
    use_px_sampling = 0;
}
