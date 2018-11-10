#ifndef CPLD_H
#define CPLD_H

#define VERSION_MINOR_BIT   0
#define VERSION_MAJOR_BIT   4
#define VERSION_DESIGN_BIT  8

#define DESIGN_NORMAL       0
#define DESIGN_ALTERNATIVE  1

typedef struct {
   const char *name;
   int min;
   int max;
} param_t;

// Define a common interface to abstract the calibration code
// for the two different CPLD implementations
typedef struct {
   const char *name;
   void (*init)(int cpld_version);
   int (*get_version)();
   void (*set_mode)(int mode7);
   void (*calibrate)(int elk, int chars_per_line);
   // Support for the UI
   param_t *(*get_params)();
   int (*get_value)(int num);
   void (*set_value)(int num, int value);
   // Support for info page
   void (*show_cal_summary)(int line);
   void (*show_cal_details)(int line);
   void (*show_cal_raw)(int line);
} cpld_t;

int *diff_N_frames(int n, int mode7, int elk, int chars_per_line);
int *diff_N_frames_by_sample(int n, int mode7, int elk, int chars_per_line);

int analyze_mode7_alignment();

// These are global variables defined in rgb_to_hdmi
extern cpld_t        *cpld;
extern unsigned char *fb;
extern int            pitch;

#endif
