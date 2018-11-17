#ifndef RGB_TO_FB_H
#define RGB_TO_FB_H

// =============================================================
// External symbols from rgb_to_fb.S
// =============================================================

extern int rgb_to_fb(capture_info_t *cap_info, int flags);

extern int measure_vsync();

extern int sw1counter;

extern int sw2counter;

extern int sw3counter;

#endif
