#ifndef RGB_TO_HDMI_H
#define RGB_TO_HDMI_H

// Property setters/getters
void set_profile(int value);
int  get_profile();
void set_subprofile(int value);
int  get_subprofile();
void set_paletteControl(int value);
int  get_paletteControl();
void set_resolution(int mode, const char *name, int reboot);
int get_resolution();
void set_scaling(int mode, int reboot);
int get_scaling();
void set_frontend(int value, int save);
int  get_frontend();
void set_deinterlace(int value);
int  get_deinterlace();
void set_scanlines(int on);
int  get_scanlines();
void set_ntsccolour(int value);
int  get_ntsccolour();
void set_scanlines_intensity(int value);
int  get_scanlines_intensity();
void set_colour(int value);
int  get_colour();
void set_invert(int value);
int  get_invert();
void set_ntscphase(int value);
int  get_ntscphase();
void set_border(int value);
int  get_border();
void set_fontsize(int value);
int  get_fontsize();
void set_vsync(int on);
int  get_vsync();
void set_vlockmode(int val);
int  get_vlockmode();
void set_vlockline(int val);
int  get_vlockline();
void set_vlockspeed(int val);
int  get_vlockspeed();
void set_vlockadj(int val);
int  get_vlockadj();
#ifdef MULTI_BUFFER
void set_nbuffers(int val);
int  get_nbuffers();
#endif
void set_autoswitch(int on);
int  get_autoswitch();
void set_debug(int on);
int  get_debug();
int  get_lines_per_vsync();
int show_detected_status(int line);
void delay_in_arm_cycles_cpu_adjust(int cycles);

int get_current_display_buffer();

// Actions
void action_calibrate_clocks();
void action_calibrate_auto();

// Status
int is_genlocked();
void set_status_message(char *msg);
void force_reinit();
void set_helper_flag();

// Reboot the system immediately
void reboot();

#endif
