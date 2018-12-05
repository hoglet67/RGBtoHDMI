#ifndef RGB_TO_FB_H
#define RGB_TO_FB_H

// =============================================================
// External symbols from rgb_to_fb.S
// =============================================================

extern int rgb_to_fb(capture_info_t *cap_info, int flags);

extern int measure_vsync();

extern int measure_n_lines(int n);

extern int sw1counter;

extern int sw2counter;

extern int sw3counter;

extern int capture_line_atom_4bpp();

extern int capture_line_atom_8bpp();

extern int capture_line_default_4bpp();

extern int capture_line_default_8bpp();

extern int capture_line_mode7_4bpp();

extern int vsync_line;

extern int default_vsync_line;

extern int lock_fail;

int recalculate_hdmi_clock_line_locked_update();

#endif
