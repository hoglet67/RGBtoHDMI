#ifndef RGB_BLUR_H
#define RGB_BLUR_H

// "RGB Blur": horizontal Gaussian blur applied while capturing 3-bit RGB
// into a 16bpp framebuffer (capture_line_rgb_blur_3bpp.S), to give the soft
// look of a TTL RGB monitor such as the Microvitec CUB: pixel edges merge
// and dithers blend, while lines stay crisp.
//
// Each output pixel looks at 9 captured samples (4 either side), which is
// enough to blend dithers in BBC modes 1 and 2 (2 and 4 samples per pixel).

#define RGB_BLUR_OFF      0
#define NUM_RGB_BLUR      5     // Off, then FWHM 2, 3, 4, 5 samples

// Word offsets inside rgb_blur_tables
#define RGB_BLUR_TAB_LO   0     // 64 entries: deinterleave pixels 0,1
#define RGB_BLUR_TAB_HI   64    // 64 entries: deinterleave pixels 2,3
#define RGB_BLUR_TAB_R    128   // 512 entries: index bit 0 contribution
#define RGB_BLUR_TAB_G    640   // 512 entries: index bit 1 contribution + base colour
#define RGB_BLUR_TAB_B    1152  // 512 entries: index bit 2 contribution
#define RGB_BLUR_TAB_WORDS 1664

#ifndef __ASSEMBLER__
#include "defs.h"

// Rebuild the lookup tables from palette_data_16 and the RGB Blur level.
// Called whenever the palette is recalculated or the level changes.
void rgb_blur_build();

// True if the blur can run with the current capture setup: GPU capture
// build, 3 bits per pixel, normal palette control and pixel sampling, and
// no double-width capture.
int rgb_blur_available(capture_info_t *capinfo);

// True if the blur is switched on and available, i.e. capture will use it
int rgb_blur_active(capture_info_t *capinfo);
#endif

#endif
