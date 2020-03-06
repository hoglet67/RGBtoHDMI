#ifndef RGB_TO_FB_H
#define RGB_TO_FB_H

// =============================================================
// External symbols from rgb_to_fb.S
// =============================================================

extern int rgb_to_fb(capture_info_t *cap_info, int flags);

extern int poll_keys_only(capture_info_t *cap_info, int flags);

extern int key_press_reset();

extern int measure_vsync();

extern int analyse_sync();

extern int clear_full_screen();

extern int clear_menu_bits();

extern int measure_n_lines(int n);

extern int sw1counter;

extern int sw2counter;

extern int sw3counter;

extern int capture_line_mode7_3bpp_table();

extern int capture_line_normal_3bpp_table();
extern int capture_line_odd_3bpp_table();
extern int capture_line_even_3bpp_table();
extern int capture_line_double_3bpp_table();
extern int capture_line_half_odd_3bpp_table();
extern int capture_line_half_even_3bpp_table();

extern int capture_line_normal_6bpp_table();
extern int capture_line_odd_6bpp_table();
extern int capture_line_even_6bpp_table();
extern int capture_line_double_6bpp_table();
extern int capture_line_half_odd_6bpp_table();
extern int capture_line_half_even_6bpp_table();

extern int vsync_line;
extern int total_lines;
extern int lock_fail;

extern int elk_mode;

extern int hsync_period;
extern int vsync_period;
extern int hsync_comparison_lo;
extern int vsync_comparison_lo;
extern int hsync_comparison_hi;
extern int vsync_comparison_hi;
extern int sync_detected;
extern int last_sync_detected;


extern int field_type_threshold;
extern int elk_lo_field_sync_threshold;
extern int elk_hi_field_sync_threshold;
extern int odd_threshold;
extern int even_threshold;
extern int hsync_threshold;
extern int frame_minimum;
extern int line_minimum;
extern int frame_timeout;
extern int hsync_scroll;
extern int line_timeout;

extern int dummyscreen;

int recalculate_hdmi_clock_line_locked_update();

void osd_update_palette();

void delay_in_arm_cycles(int delay);

int benchmarkRAM(int address);

#endif
