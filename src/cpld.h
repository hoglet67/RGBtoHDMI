#ifndef CPLD_H
#define CPLD_H

// Define a common interface to abstract the calibration code
// for the two different CPLD implementations
typedef struct {
   const char *name;
   void (*init)();
   void (*change_mode)(int mode7);
   void (*calibrate)(int mode7, int elk, int chars_per_line);
} cpld_t;

int *diff_N_frames(int sp, int n, int mode7, int elk, int chars_per_line);

#endif
