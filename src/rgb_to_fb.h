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
extern int clear_screen();
extern int clear_menu_bits();

extern int measure_n_lines(int n);

extern int get_cycle_counter();
extern int validate_cga(int rgbi_pixels);
extern int cga_render_words(uint32_t srgb0, uint32_t srgb1, uint32_t srgb2, uint32_t srgb3);

extern int sw1counter;

extern int sw2counter;

extern int sw3counter;

extern int capture_line_mode7_3bpp_table();

extern int capture_line_normal_1bpp_table();
extern int capture_line_normal_3bpp_table();
extern int capture_line_normal_6bpp_table();
extern int capture_line_normal_odd_even_6bpp_table();
extern int capture_line_normal_9bpplo_table();
extern int capture_line_normal_9bpphi_table();
extern int capture_line_normal_12bpp_table();


extern int capture_line_odd_3bpp_table();
extern int capture_line_even_3bpp_table();
extern int capture_line_double_3bpp_table();
extern int capture_line_half_odd_3bpp_table();
extern int capture_line_half_even_3bpp_table();

extern int capture_line_simple_6bpp_table();
extern int capture_line_simple_9bpplo_table();
extern int capture_line_simple_9bpplo_blank_table();
extern int capture_line_simple_9bpphi_table();
extern int capture_line_simple_12bpp_table();

extern int vsync_line;
extern int total_lines;
extern int lock_fail;

extern int elk_mode;

extern int hsync_period;
extern int hsync_width;
extern int total_hsync_period;
extern int vsync_period;
extern int hsync_comparison_lo;
extern int vsync_comparison_lo;
extern int hsync_comparison_hi;
extern int vsync_comparison_hi;
extern int sync_detected;
extern int last_sync_detected;
extern int last_but_one_sync_detected;
extern int jitter_offset;
extern int debug_value;
extern int ntsc_status;
extern int sw1_power_up;
extern int osd_timer;
extern int field_type_threshold;
extern int elk_lo_field_sync_threshold;
extern int elk_hi_field_sync_threshold;
extern int odd_threshold;
extern int even_threshold;
extern int hsync_threshold;
extern int normal_hsync_threshold;
extern int equalising_threshold;
extern int frame_minimum;
extern int line_minimum;
extern int frame_timeout;
extern int hsync_scroll;
extern int line_timeout;
extern int vsync_retry_count;
extern int dummyscreen;
extern int core_1_available;
extern int start_core_1_code;

int recalculate_hdmi_clock_line_locked_update();

void set_vsync_psync(int state);

void osd_update_palette();

void delay_in_arm_cycles(int delay);

void wait_for_pi_fieldsync();
int scan_for_single_pixels_4bpp(uint32_t * start, int length);
int scan_for_single_pixels_12bpp(uint32_t * start, int length);
void scan_for_diffs_12bpp(uint32_t *fbp, uint32_t *lastp, int length, int diff[NUM_OFFSETS]);

int benchmarkRAM(int address);

#endif
