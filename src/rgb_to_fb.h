#ifndef RGB_TO_FB_H
#define RGB_TO_FB_H

// =============================================================
// External symbols from rgb_to_fb.S
// =============================================================

extern int rgb_to_fb(capture_info_t *cap_info, int flags);

extern int measure_vsync();

extern int analyse_csync();

extern int analyse_vsync();

extern int measure_n_lines(int n);

extern int sw1counter;

extern int sw2counter;

extern int sw3counter;

extern int capture_line_mode7_4bpp_table();
extern int capture_line_normal_4bpp_table();
extern int capture_line_odd_4bpp_table();
extern int capture_line_even_4bpp_table();
extern int capture_line_double_4bpp_table();
extern int capture_line_half_odd_4bpp_table();
extern int capture_line_half_even_4bpp_table();
extern int capture_line_normal_8bpp_table();
extern int capture_line_odd_8bpp_table();
extern int capture_line_even_8bpp_table();
extern int capture_line_double_8bpp_table();
extern int capture_line_half_odd_8bpp_table();
extern int capture_line_half_even_8bpp_table();
extern int capture_line_atom_4bpp_table();
extern int capture_line_atom_8bpp_table();

extern int vsync_line;

extern int default_vsync_line;

extern int lock_fail;

extern int hsync_width;

extern int hsync_period;
extern int vsync_period;
extern int hsync_comparison_lo;
extern int vsync_comparison_lo;
extern int hsync_comparison_hi;
extern int vsync_comparison_hi;


int recalculate_hdmi_clock_line_locked_update();

void osd_update_palette();

#endif
