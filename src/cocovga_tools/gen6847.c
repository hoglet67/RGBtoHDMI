/*
 * gen6847.c - render MC6847 frames the way RGBtoHDMI captures them from the
 * MC6847digitizer (8bpp frame buffer of LumaCode colour codes).
 *
 * Independent of cocovga.c except for the shared font table.
 */
#include <stdlib.h>
#include <string.h>
#include "gen6847.h"

static const uint8_t sg_colour[8] = {
   CV_GREEN, CV_YELLOW, CV_BLUE, CV_RED, CV_BUFF, CV_CYAN, CV_MAGENTA, CV_ORANGE
};
static const uint8_t cg_set[2][4] = {
   { CV_GREEN, CV_YELLOW, CV_BLUE, CV_RED },
   { CV_BUFF, CV_CYAN, CV_MAGENTA, CV_ORANGE }
};

static void put_dot(uint8_t *fb, const gen_geom_t *g, int dx, int dy, uint8_t c) {
   for (int y = 0; y < g->vs; y++) {
      uint8_t *p = fb + (g->oy + dy * g->vs + y) * g->pitch + g->ox + dx * g->hs;
      for (int x = 0; x < g->hs; x++) p[x] = c;
   }
}

// text / semigraphics 4 screen, 32x16 cells of 8x12 dots (Dragon wiring)
static void render_text(uint8_t *fb, const gen_geom_t *g, const uint8_t *vram, int css) {
   uint8_t light = css ? CV_LORANGE : CV_GREEN;   // the 6847 draws orange set text in light orange
   uint8_t dark = css ? CV_DKORANGE : CV_DKGREEN;
   for (int cy = 0; cy < 16; cy++) {
      for (int cx = 0; cx < 32; cx++) {
         uint8_t b = vram[cy * 32 + cx];
         for (int r = 0; r < 12; r++) {
            for (int d = 0; d < 8; d++) {
               uint8_t c;
               if (b & 0x80) {
                  int q = (r < 6 ? 0 : 2) + (d < 4 ? 0 : 1);   // 0 TL 1 TR 2 BL 3 BR
                  int lit = (b >> (3 - q)) & 1;
                  c = lit ? sg_colour[(b >> 4) & 7] : CV_BLACK;
               } else {
                  int stroke = (mc6847_rom_font[b & 0x3F][r] >> (7 - d)) & 1;
                  int inv_pin = (b & 0x40) != 0;   // DD6 -> INV: dark on light
                  if (inv_pin) c = stroke ? dark : light;
                  else         c = stroke ? light : dark;
               }
               put_dot(fb, g, cx * 8 + d, cy * 12 + r, c);
            }
         }
      }
   }
}

// colour graphics: pixel w x h dots, bytes per row
static void render_cg(uint8_t *fb, const gen_geom_t *g, const uint8_t *vram, int css,
                      int pw, int ph, int bytes_per_row, int rows) {
   for (int r = 0; r < rows; r++) {
      for (int b = 0; b < bytes_per_row; b++) {
         uint8_t v = vram[r * bytes_per_row + b];
         for (int p = 0; p < 4; p++) {
            uint8_t c = cg_set[css][(v >> (6 - 2 * p)) & 3];
            for (int y = 0; y < ph; y++)
               for (int x = 0; x < pw; x++)
                  put_dot(fb, g, (b * 4 + p) * pw + x, r * ph + y, c);
         }
      }
   }
}

static void render_rg6(uint8_t *fb, const gen_geom_t *g, const uint8_t *vram, int css) {
   for (int r = 0; r < 192; r++)
      for (int b = 0; b < 32; b++)
         for (int p = 0; p < 8; p++) {
            int on = (vram[r * 32 + b] >> (7 - p)) & 1;
            put_dot(fb, g, b * 8 + p, r, on ? (css ? CV_BUFF : CV_GREEN) : CV_BLACK);
         }
}

void gen_frame(uint8_t *fb, const gen_geom_t *g, gen_mode_t mode, const uint8_t *vram, int css, unsigned int noise_seed) {
   // outside the captured border: blanking; border: 32 dots / 24 lines
   memset(fb, CV_BLANK, (size_t)g->pitch * g->fb_h);
   uint8_t border = (mode == GEN_TEXT) ? CV_BLACK : (css ? CV_BUFF : CV_GREEN);
   int bx0 = g->ox - 32 * g->hs, bx1 = g->ox + (256 + 32) * g->hs;
   int by0 = g->oy - 24 * g->vs, by1 = g->oy + (192 + 24) * g->vs;
   if (bx0 < 0) bx0 = 0;
   if (by0 < 0) by0 = 0;
   if (bx1 > g->fb_w) bx1 = g->fb_w;
   if (by1 > g->fb_h) by1 = g->fb_h;
   for (int y = by0; y < by1; y++) memset(fb + y * g->pitch + bx0, border, bx1 - bx0);
   switch (mode) {
      case GEN_TEXT: render_text(fb, g, vram, css); break;
      case GEN_CG1:  render_cg(fb, g, vram, css, 4, 3, 16, 64); break;
      case GEN_CG2:  render_cg(fb, g, vram, css, 2, 3, 32, 64); break;
      case GEN_CG3:  render_cg(fb, g, vram, css, 2, 2, 32, 96); break;
      case GEN_CG6:  render_cg(fb, g, vram, css, 2, 1, 32, 192); break;
      case GEN_RG6:  render_rg6(fb, g, vram, css); break;
   }
   if (noise_seed) {
      // the decoder must only look at the low 4 bits of each pixel
      srand(noise_seed);
      for (int y = 0; y < g->fb_h; y++)
         for (int x = 0; x < g->fb_w; x++)
            fb[y * g->pitch + x] |= (rand() & 3) << 4;
   }
}

// text rows drawn with INT/EXT set (Dragon / CoCo: GM0), as a plain 6847
// does (XRoar's model): an alphanumeric byte is its own dot pattern on all 12
// lines (inverted when bit 6, the INV pin, is set), a semigraphics byte is
// SG6 (3 x 2 blocks, bits 5-0) in colour bits 7-6 of the CG colour set on black
void gen_text_ext(uint8_t *fb, const gen_geom_t *g, const uint8_t *vram, int css, unsigned int ext_rows) {
   gen_frame(fb, g, GEN_TEXT, vram, css, 0);
   uint8_t light = css ? CV_LORANGE : CV_GREEN, dark = css ? CV_DKORANGE : CV_DKGREEN;
   for (int cy = 0; cy < 16; cy++) {
      if (!(ext_rows & (1u << cy))) continue;
      for (int cx = 0; cx < 32; cx++) {
         uint8_t b = vram[cy * 32 + cx];
         for (int r = 0; r < 12; r++)
            for (int d = 0; d < 8; d++) {
               uint8_t c;
               if (b & 0x80) {
                  int shift = r < 4 ? 4 : r < 8 ? 2 : 0;
                  int bit = d < 4 ? 1 : 0;
                  c = ((b >> (shift + bit)) & 1) ? cg_set[css][(b >> 6) & 3] : CV_BLACK;
               } else {
                  uint8_t data = (b & 0x40) ? (uint8_t) ~b : b;
                  c = ((data >> (7 - d)) & 1) ? light : dark;
               }
               put_dot(fb, g, cx * 8 + d, cy * 12 + r, c);
            }
      }
   }
}
