// rgb_blur.c
//
// Lookup tables and availability checks for "RGB Blur"
// (capture_line_rgb_blur_3bpp.S).
//
// The capture routine keeps, for each of the three colour bits of a 3-bit
// source, a sliding history of that bit over the last 12 captured samples.
// For every output pixel it takes a 9-sample window of each history
// (4 samples either side of the pixel) and looks up that bit's contribution
// to the 16bpp output colour. The three contributions are added together.
//
// The palette is treated as separable: colour(index) = colour(0) + the sum,
// over the set bits, of (colour(1 << bit) - colour(0)). That is exact for the
// normal BBC palette and the mono/amber/green output options.
//
// 16bpp pixels use the palette_data_16 layout (0x0RGB, 4 bits per channel);
// the capture routine adds the alpha nibble.

#include <stdint.h>
#include <math.h>
#include "defs.h"
#include "osd.h"
#include "geometry.h"
#include "rgb_to_hdmi.h"
#include "rgb_blur.h"

// Read directly by capture_line_rgb_blur_3bpp.S
uint32_t rgb_blur_tables[RGB_BLUR_TAB_WORDS] __attribute__((aligned(64)));

static const double rgb_blur_fwhm[NUM_RGB_BLUR] = { 0.0, 2.0, 3.0, 4.0, 5.0 };

int rgb_blur_available(capture_info_t *capinfo) {
#ifdef USE_ARM_CAPTURE
   return 0;     // only the GPU capture builds leave the ARM enough time
#else
   return capinfo->sample_width == SAMPLE_WIDTH_3 &&
          capinfo->px_sampling == PS_NORMAL &&
          (capinfo->sizex2 & SIZEX2_DOUBLE_WIDTH) == 0 &&
          get_parameter(F_PALETTE_CONTROL) == PALETTECONTROL_OFF;
#endif
}

int rgb_blur_active(capture_info_t *capinfo) {
   return get_parameter(F_RGB_BLUR) != RGB_BLUR_OFF && rgb_blur_available(capinfo);
}

// Each history nibble holds 4 samples with the earliest in the top bit.
// Pixels 0,1 come from GPIO bits 7..2, pixels 2,3 from GPIO bits 13..8.
// Index bit 0 goes to bits 3..0, bit 1 to bits 19..16 and bit 2 to
// bits 31..28 (so the capture loop can merge R/G without stripping B).
static void build_deinterleave() {
   static const int channel_shift[3] = { 0, 16, 28 };
   for (int idx = 0; idx < 64; idx++) {
      uint32_t lo = 0;
      uint32_t hi = 0;
      for (int px = 0; px < 2; px++) {
         int colour = (idx >> (3 * px)) & 7;
         for (int c = 0; c < 3; c++) {
            if (colour & (1 << c)) {
               lo |= 1u << (channel_shift[c] + 3 - px);   // pixels 0,1 -> bits 3,2
               hi |= 1u << (channel_shift[c] + 1 - px);   // pixels 2,3 -> bits 1,0
            }
         }
      }
      rgb_blur_tables[RGB_BLUR_TAB_LO + idx] = lo;
      rgb_blur_tables[RGB_BLUR_TAB_HI + idx] = hi;
   }
}

static int nibble(int colour, int n) {
   return (colour >> (4 * n)) & 0xf;
}

void rgb_blur_build() {
   int level = get_parameter(F_RGB_BLUR);
   if (level < 0 || level >= NUM_RGB_BLUR) {
      level = RGB_BLUR_OFF;
   }

   // Gaussian weights for offsets -4..+4, normalised. Off puts all the
   // weight on the centre sample.
   double weight[9];
   double sum = 0;
   for (int k = 0; k < 9; k++) {
      if (level == RGB_BLUR_OFF) {
         weight[k] = (k == 4) ? 1.0 : 0.0;
      } else {
         double sigma = rgb_blur_fwhm[level] / (2.0 * sqrt(2.0 * log(2.0)));
         double x = (double)(k - 4) / sigma;
         weight[k] = exp(-0.5 * x * x);
      }
      sum += weight[k];
   }
   for (int k = 0; k < 9; k++) {
      weight[k] /= sum;
   }

   build_deinterleave();

   int base = palette_data_16[0] & 0x0fff;
   int delta[3][3];          // [index bit][nibble]
   for (int c = 0; c < 3; c++) {
      int on = palette_data_16[1 << c] & 0x0fff;
      for (int n = 0; n < 3; n++) {
         int d = nibble(on, n) - nibble(base, n);
         delta[c][n] = d > 0 ? d : 0;
      }
   }

   static const int table[3] = { RGB_BLUR_TAB_R, RGB_BLUR_TAB_G, RGB_BLUR_TAB_B };
   for (int c = 0; c < 3; c++) {
      for (int w = 0; w < 512; w++) {
         // The window is symmetric, so which end is "earliest" doesn't matter
         double frac = 0;
         for (int k = 0; k < 9; k++) {
            if (w & (1 << k)) {
               frac += weight[k];
            }
         }
         uint32_t value = 0;
         for (int n = 0; n < 3; n++) {
            int v = (int)(frac * delta[c][n] + 0.5);
            if (c == 1) {
               v += nibble(base, n);     // the base colour rides on the G table
            }
            if (v > 15) {
               v = 15;
            }
            value |= v << (4 * n);
         }
         rgb_blur_tables[table[c] + w] = value;
      }
   }
}
