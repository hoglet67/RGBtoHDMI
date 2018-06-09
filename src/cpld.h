#ifndef CPLD_H
#define CPLD_H

#define VERSION_MINOR_BIT   0
#define VERSION_MAJOR_BIT   4
#define VERSION_DESIGN_BIT  8

#define DESIGN_NORMAL       0
#define DESIGN_ALTERNATIVE  1

// Define a common interface to abstract the calibration code
// for the two different CPLD implementations
typedef struct {
   const char *name;
   void (*init)();
   void (*change_mode)(int mode7);
   void (*inc_sampling_base)(int mode7);
   void (*inc_sampling_offset)(int mode7);
   void (*calibrate)(int mode7, int elk, int chars_per_line);
} cpld_t;

int *diff_N_frames(int sp, int n, int mode7, int elk, int chars_per_line);

#endif
