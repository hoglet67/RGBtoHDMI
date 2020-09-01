#include <stdio.h>
#include "defs.h"
#include "cpld.h"
#include "rgb_to_fb.h"
#include "osd.h"
#include "geometry.h"

typedef struct {
   int cpld_setup_mode;
   int sp_offset[NUM_OFFSETS];
   int half_px_delay; // 0 = off, 1 = on, all modes
   int divider;       // cpld divider, 6 or 8
   int full_px_delay; // 0..15
       int filter_l;
       int sub_c;
       int alt_r;
       int edge;
       int clamptype;
   int mux;           // 0 = direct, 1 = via the 74LS08 buffer
   int rate;          // 0 = normal psync rate (3 bpp), 1 = double psync rate (6 bpp), 2 = sub-sample (odd), 3=sub-sample(even)
   int terminate;
   int coupling;
   int dac_a;
   int dac_b;
   int dac_c;
   int dac_d;
   int dac_e;
   int dac_f;
   int dac_g;
   int dac_h;
} config_t;

// The CPLD version number
static int cpld_version;

static config_t *config;

enum {
   // Sampling params
   CPLD_SETUP_MODE,
   ALL_OFFSETS,
   A_OFFSET,
   B_OFFSET,
   C_OFFSET,
   D_OFFSET,
   E_OFFSET,
   F_OFFSET,
   HALF,
   DIVIDER,
   DELAY,
       FILTER_L,
       SUB_C,
       ALT_R,
       EDGE,
       CLAMPTYPE,
   MUX,
   RATE,
   TERMINATE,
   COUPLING,
   DAC_A,
   DAC_B,
   DAC_C,
   DAC_D,
   DAC_E,
   DAC_F,
   DAC_G,
   DAC_H
};

enum {
  RGB_RATE_3,               //00
  RGB_RATE_6,               //01
  RGB_RATE_6x2_OR_4_LEVEL,  //10 - 6x2 in digital mode and 4 level in analog mode
  RGB_RATE_12,              //11
  NUM_RGB_RATE
};

enum {
   RGB_INPUT_HI,
   RGB_INPUT_TERM,
   NUM_RGB_TERM
};

enum {
   RGB_INPUT_DC,
   RGB_INPUT_AC,
   NUM_RGB_COUPLING
};

enum {
   CPLD_SETUP_NORMAL,
   CPLD_SETUP_DELAY,
   NUM_CPLD_SETUP
};

static const char *edge_names[] = {
   "Trailing with +ve PixClk",
   "Leading with +ve PixClk",
   "Trailing with -ve PixClk",
   "Leading with -ve PixClk",
   "Trailing with +- PixClk",
   "Leading with +- PixClk"
};

enum {
   EDGE_TRAIL_POS,
   EDGE_LEAD_POS,
   EDGE_TRAIL_NEG,
   EDGE_LEAD_NEG,
   EDGE_TRAIL_BOTH,
   EDGE_LEAD_BOTH,
   NUM_EDGE
};

// =============================================================
// Param definitions for OSD
// =============================================================


static param_t params[] = {
   {  CPLD_SETUP_MODE,  "Setup Mode", "setup_mode", 0,NUM_CPLD_SETUP-1, 1 },
   { ALL_OFFSETS, "All Offsets", "all_offsets", 0,   7, 1 },
   {    A_OFFSET,    "A Offset",    "a_offset", 0,   7, 1 },
   {    B_OFFSET,    "B Offset",    "b_offset", 0,   7, 1 },
   {    C_OFFSET,    "C Offset",    "c_offset", 0,   7, 1 },
   {    D_OFFSET,    "D Offset",    "d_offset", 0,   7, 1 },
   {    E_OFFSET,    "E Offset",    "e_offset", 0,   7, 1 },
   {    F_OFFSET,    "F Offset",    "f_offset", 0,   7, 1 },
   {        HALF,        "Half",        "half", 0,   1, 1 },
   {     DIVIDER,     "Divider",      "divider", 6,   8, 2 },
   {       DELAY,       "Delay",       "delay", 0,  15, 1 },
//block of hidden YUV options for file compatibility
   {    FILTER_L,  "Filter Y",      "l_filter", 0,   1, 1 },
   {       SUB_C,  "Subsample UV",     "sub_c", 0,   1, 1 },
   {       ALT_R,  "PAL switch",       "alt_r", 0,   1, 1 },
   {        EDGE,  "Sync Edge",         "edge", 0,   NUM_EDGE-1, 1 },
   {   CLAMPTYPE,  "Clamp Type",   "clamptype", 0,   4, 1 },
//end of hidden block
   {         MUX,  "Sync on G/V",   "input_mux", 0,   1, 1 },
   {        RATE,  "Sample Mode", "sample_mode", 0, NUM_RGB_RATE-1, 1 },
   {   TERMINATE,  "75R Termination", "termination", 0,   NUM_RGB_TERM-1, 1 },
   {    COUPLING,  "G Coupling", "coupling", 0,   NUM_RGB_COUPLING-1, 1 },
   {       DAC_A,  "DAC-A: G Hi",     "dac_a", 0, 255, 1 },
   {       DAC_B,  "DAC-B: G Lo",     "dac_b", 0, 255, 1 },
   {       DAC_C,  "DAC-C: RB Hi",    "dac_c", 0, 255, 1 },
   {       DAC_D,  "DAC-D: RB Lo",    "dac_d", 0, 255, 1 },
   {       DAC_E,  "DAC-E: Sync",     "dac_e", 0, 255, 1 },
   {       DAC_F,  "DAC-F: G/V Sync",  "dac_f", 0, 255, 1 },
   {       DAC_G,  "DAC-G: G Clamp",  "dac_g", 0, 255, 1 },
   {       DAC_H,  "DAC-H: Unused",   "dac_h", 0, 255, 1 },
   {          -1,          NULL,          NULL, 0,   0, 1 }
};

// =============================================================
// Public methods
// =============================================================

static void cpld_init(int version) {
    cpld_version = version;

    params[CPLD_SETUP_MODE].hidden = 1;
    params[ALL_OFFSETS].hidden = 1;
    params[A_OFFSET].hidden = 1;
    params[B_OFFSET].hidden = 1;
    params[C_OFFSET].hidden = 1;
    params[D_OFFSET].hidden = 1;
    params[E_OFFSET].hidden = 1;
    params[F_OFFSET].hidden = 1;
    params[HALF].hidden = 1;
    params[DIVIDER].hidden = 1;
    params[DELAY].hidden = 1;
    params[FILTER_L].hidden = 1;
    params[SUB_C].hidden = 1;
    params[ALT_R].hidden = 1;
 //   params[EDGE].hidden = 1;
    params[CLAMPTYPE].hidden = 1;
    params[MUX].hidden = 1;
    params[RATE].hidden = 1;
    params[TERMINATE].hidden = 1;
    params[COUPLING].hidden = 1;
    params[DAC_A].hidden = 1;
    params[DAC_B].hidden = 1;
    params[DAC_C].hidden = 1;
    params[DAC_D].hidden = 1;
    params[DAC_E].hidden = 1;
    params[DAC_F].hidden = 1;
    params[DAC_G].hidden = 1;
    params[DAC_H].hidden = 1;

    geometry_hide_pixel_sampling();
}

static int cpld_get_version() {
   return cpld_version;
}

static void cpld_calibrate(capture_info_t *capinfo, int elk) {
}

static void cpld_set_mode(int mode) {
}

static void cpld_set_vsync_psync(int state) {
}

static int cpld_analyse(int selected_sync_state, int analyse) {
   return SYNC_BIT_COMPOSITE_SYNC;
}

static void cpld_update_capture_info(capture_info_t *capinfo) {
    if (capinfo) {
        capinfo->sample_width = SAMPLE_WIDTH_12;
        switch (config->edge) {
           case EDGE_TRAIL_POS:
                 capinfo->capture_line = capture_line_simple_12bpp_trailing_pos_table;
                 break;
           case EDGE_LEAD_POS:
                 capinfo->capture_line = capture_line_simple_12bpp_leading_pos_table;
                 break;
           case EDGE_TRAIL_NEG:
                 capinfo->capture_line = capture_line_simple_12bpp_trailing_neg_table;
                 break;
           case EDGE_LEAD_NEG:
                 capinfo->capture_line = capture_line_simple_12bpp_leading_neg_table;
                 break;
           case EDGE_TRAIL_BOTH:
                 capinfo->capture_line = capture_line_simple_12bpp_trailing_both_table;
                 break;
           case EDGE_LEAD_BOTH:
                 capinfo->capture_line = capture_line_simple_12bpp_leading_both_table;
                 break;
        }
    }
}

static param_t *cpld_get_params() {
   return params;
}

static int cpld_get_value(int num) {
   switch (num) {

   case CPLD_SETUP_MODE:
      return config->cpld_setup_mode;
   case ALL_OFFSETS:
      return config->sp_offset[0];
   case A_OFFSET:
      return config->sp_offset[0];
   case B_OFFSET:
      return config->sp_offset[1];
   case C_OFFSET:
      return config->sp_offset[2];
   case D_OFFSET:
      return config->sp_offset[3];
   case E_OFFSET:
      return config->sp_offset[4];
   case F_OFFSET:
      return config->sp_offset[5];
   case HALF:
      return config->half_px_delay;
   case DIVIDER:
      return config->divider;
   case DELAY:
      return config->full_px_delay;
   case RATE:
      return config->rate;
   case MUX:
      return config->mux;
   case DAC_A:
      return config->dac_a;
   case DAC_B:
      return config->dac_b;
   case DAC_C:
      return config->dac_c;
   case DAC_D:
      return config->dac_d;
   case DAC_E:
      return config->dac_e;
   case DAC_F:
      return config->dac_f;
   case DAC_G:
      return config->dac_g;
   case DAC_H:
      return config->dac_h;
   case TERMINATE:
      return config->terminate;
   case COUPLING:
      return config->coupling;

   case FILTER_L:
      return config->filter_l;
   case SUB_C:
      return config->sub_c;
   case ALT_R:
      return config->alt_r;
   case EDGE:
      return config->edge;
   case CLAMPTYPE:
      return config->clamptype;

   }
   return 0;
}

static const char *cpld_get_value_string(int num) {
   if (num == EDGE) {
      return edge_names[config->edge];
   }
   return NULL;
}

static void cpld_set_value(int num, int value) {
   if (value < params[num].min) {
      value = params[num].min;
   }
   if (value > params[num].max) {
      value = params[num].max;
   }
   switch (num) {
   case CPLD_SETUP_MODE:
      config->cpld_setup_mode = value;
      break;
   case ALL_OFFSETS:
      config->sp_offset[0] = value;
      config->sp_offset[1] = value;
      config->sp_offset[2] = value;
      config->sp_offset[3] = value;
      config->sp_offset[4] = value;
      config->sp_offset[5] = value;
      break;
   case A_OFFSET:
      config->sp_offset[0] = value;
      break;
   case B_OFFSET:
      config->sp_offset[1] = value;
      break;
   case C_OFFSET:
      config->sp_offset[2] = value;
      break;
   case D_OFFSET:
      config->sp_offset[3] = value;
      break;
   case E_OFFSET:
      config->sp_offset[4] = value;
      break;
   case F_OFFSET:
      config->sp_offset[5] = value;
      break;
   case HALF:
      config->half_px_delay = value;
      break;
   case DIVIDER:
      config->divider = value;
      break;
   case DELAY:
      config->full_px_delay = value;
      break;
   case RATE:
      config->rate = value;
      break;
   case MUX:
      config->mux = value;
      break;
   case DAC_A:
      config->dac_a = value;
      break;
   case DAC_B:
      config->dac_b = value;
      break;
   case DAC_C:
      config->dac_c = value;
      break;
   case DAC_D:
      config->dac_d = value;
      break;
   case DAC_E:
      config->dac_e = value;
      break;
   case DAC_F:
      config->dac_f = value;
      break;
   case DAC_G:
      config->dac_g = value;
      break;
   case DAC_H:
      config->dac_h = value;
      break;
   case TERMINATE:
      config->terminate = value;
      break;
   case COUPLING:
      config->coupling = value;
      break;

   case FILTER_L:
      config->filter_l = value;
      break;
   case SUB_C:
      config->sub_c = value;
      break;
   case ALT_R:
      config->alt_r = value;
      break;
   case EDGE:
      config->edge = value;
      break;
   case CLAMPTYPE:
      config->clamptype = value;
      break;


   }
}

static int cpld_show_cal_summary(int line) {
    return line;
}

static void cpld_show_cal_details(int line) {
}

static void cpld_show_cal_raw(int line) {
}

static int cpld_old_firmware_support() {
    return 0;
}

static int cpld_get_divider() {
    return 6;
}

static int cpld_get_delay() {
    return 0;
}

static int cpld_get_sync_edge() {
    return config->edge;
}

static int cpld_frontend_info() {
    return FRONTEND_SIMPLE | FRONTEND_SIMPLE << 16;
}

static void cpld_set_frontend(int value)
{
}

cpld_t cpld_simple = {
   .name = "Simple",
   .default_profile = "Amiga",
   .init = cpld_init,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .set_vsync_psync = cpld_set_vsync_psync,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .get_sync_edge = cpld_get_sync_edge,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};
