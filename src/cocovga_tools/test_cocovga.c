/*
 * test_cocovga.c - PC test harness for the CoCoVGA W64 emulation.
 *
 * Builds MC6847 frames exactly as the MC6847digitizer + RGBtoHDMI would put
 * them in the frame buffer, runs the decoder on them and checks the result.
 * Also simulates RGBtoHDMI's multi-buffer capture loop end to end.
 *
 *   make -C src/cocovga_tools test
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gen6847.h"
#include "../cocovga_bench.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// The register page NitrOS-9's CoVGA driver writes (covdg.asm, VGASetup)
static const uint8_t nitros9_regs[9] = { 0x00, 0x81, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x02 };

// NitrOS-9 CoVDG character translation (VDG-T1 "true lower case" mode)
static uint8_t covdg_code(uint8_t a) {
   if (a & 0x80) return a;
   if (a == '^') return 0x00;
   if (a == '_') return 0x1F;
   if (a == '`') return 0x67;
   if (a >= 0x60) return a - 0x60;
   if (a >= 0x40) return (uint8_t)((a - 0x40) ^ 0x40);
   return a ^ 0x40;
}

static void put_text(uint8_t *vram, int row, int col, const char *s) {
   for (; *s && col < 64; s++, col++) vram[row * 64 + col] = covdg_code((uint8_t)*s);
}

static uint8_t *alloc_fb(const gen_geom_t *g) { return calloc((size_t)g->pitch, g->fb_h); }

static unsigned int rng = 12345;
static unsigned int rnd(void) { rng = rng * 1103515245u + 12345u; return (rng >> 8) & 0xFFFFFF; }

// ---------------------------------------------------------------------
// Independent reference for what CoCoVGA shows for one W64 pixel
// ---------------------------------------------------------------------
static uint8_t ref_w64_pixel(uint8_t b, int x, int y, int font, int css) {
   static const uint8_t sgc[8] = { CV_GREEN, CV_YELLOW, CV_BLUE, CV_RED, CV_BUFF, CV_CYAN, CV_MAGENTA, CV_ORANGE };
   if (b & 0x80) {
      int q = (y < 6 ? 0 : 2) + (x < 4 ? 0 : 1);
      return ((b >> (3 - q)) & 1) ? sgc[(b >> 4) & 7] : CV_BLACK;
   }
   int code = b & 0x3F;
   int t1 = font & 1, lower = font & 2;
   const uint8_t *g;
   int dark_strokes;   // 1: dark strokes on light background
   if (b & 0x40) { g = t1 ? mc6847t1_font[code] : mc6847_font[code]; dark_strokes = 1; }
   else if (lower && code < 32) { g = cv_lower_glyph(code, t1); dark_strokes = 1; }
   else { g = t1 ? mc6847t1_font[code] : mc6847_font[code]; dark_strokes = 0; }
   int stroke = (g[y] >> (7 - x)) & 1;
   uint8_t light = css ? CV_LORANGE : CV_GREEN, dark = css ? CV_DKORANGE : CV_DKGREEN;
   if (dark_strokes) return stroke ? dark : light;
   return stroke ? light : dark;
}

// compare a frame buffer's active area with the expected W64 screen
static int compare_w64(const uint8_t *fb, const gen_geom_t *g, const uint8_t *vram, int font, int css) {
   int bad = 0;
   int px = g->hs / 2, py = g->vs / 2;
   for (int i = 0; i < CV_VRAM_SIZE; i++) {
      int col = i & 63, row = i >> 6;
      for (int y = 0; y < 12; y++)
         for (int x = 0; x < 8; x++) {
            uint8_t want = ref_w64_pixel(vram[i], x, y, font, css);
            for (int yy = 0; yy < py; yy++)
               for (int xx = 0; xx < px; xx++) {
                  int fx = g->ox + (col * 8 + x) * px + xx;
                  int fy = g->oy + (row * 12 + y) * py + yy;
                  if ((fb[fy * g->pitch + fx] & 15) != want) bad++;
               }
         }
   }
   return bad;
}

static const gen_geom_t geoms[] = {
   { 640, 480, 640, 64, 48, 2, 2 },     // MC6847 LumaCode profile, double width + height
   { 640, 480, 640, 62, 46, 2, 2 },     // shifted by a couple of pixels
   { 720, 480, 736, 104, 48, 2, 2 },    // wider frame buffer (scaling borders), padded pitch
   { 640, 240, 640, 64, 24, 2, 1 },     // double height inhibited
   { 320, 240, 320, 32, 24, 1, 1 },     // no doubling
   { 1280, 960, 1280, 128, 96, 4, 4 },  // 4x
};
#define NGEOMS (int)(sizeof(geoms) / sizeof(geoms[0]))

static void make_text_page(uint8_t *t, const uint8_t *regs) {
   memset(t, 0x60, 512);
   if (regs) memcpy(t, regs, 9);
}

static void test_calibration(void) {
   uint8_t text[512];
   make_text_page(text, NULL);
   for (int k = 0; k < NGEOMS; k++) {
      const gen_geom_t *g = &geoms[k];
      uint8_t *fb = alloc_fb(g);
      for (int css = 0; css < 2; css++) {
         gen_frame(fb, g, GEN_TEXT, text, css, k + 1);
         cv_state_t s; cv_init(&s);
         cv_frame_t f = cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h);
         CHECK(f == CV_FRAME_TEXT, "geom %d css %d: text frame not recognised (%d)", k, css, f);
         CHECK(s.cal_valid && s.ox == g->ox && s.oy == g->oy && s.hs == g->hs && s.vs == g->vs,
               "geom %d: calibration %d,%d %dx%d expected %d,%d %dx%d", k, s.ox, s.oy, s.hs, s.vs, g->ox, g->oy, g->hs, g->vs);
      }
      free(fb);
   }
}

// Register page palettes (edit mask bits 3-5): every value of a slot's high
// byte, then of its low byte, against what the picture can show. A
// semigraphics byte with no quarter lit hides its colour bits, and $60 / $8F
// look the same (solid green): such slots are counted unreadable and keep
// their previous value. A slot whose enable bit is clearly 0 reads as 0.
static int pal_byte_ambiguous(int v) {
   return v == 0x60 || v == 0x8F || (v >= 0x80 && (v & 0x0F) == 0);
}
static void test_register_palette(void) {
   for (int k = 0; k < 2; k++) {
      const gen_geom_t *g = &geoms[k];
      uint8_t *fb = alloc_fb(g);
      uint8_t text[512];
      int bad = 0;
      for (int half = 0; half < 2; half++) {
         for (int v = 0; v < 256; v++) {
            int hi = half ? 0x83 : v, lo = half ? v : 0x1C;
            uint8_t regs[9] = { 0x00, 0x08, 0, 0, 0, 0, 0, 0, 0 };
            make_text_page(text, regs);
            memset(text + 32, 0x00, 22);             // SG slots: not enabled ($00 = inverse '@')
            text[32 + 2 * 4] = (uint8_t) hi;          // slot 4 (red)
            text[32 + 2 * 4 + 1] = (uint8_t) lo;
            gen_frame(fb, g, GEN_TEXT, text, 0, 0);
            cv_state_t s; cv_init(&s);
            s.regs.pal[4] = 0x1234;                   // "previous" value
            if (cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h) != CV_FRAME_REGISTER) { bad++; continue; }
            int want, unreadable = 0;
            if (pal_byte_ambiguous(hi)) { want = 0x1234; unreadable = 1; }
            else if (!(hi & 0x80)) want = 0;
            else if (pal_byte_ambiguous(lo)) { want = 0x1234; unreadable = 1; }
            else want = (hi << 8) | lo;
            if (s.regs.pal[4] != want || s.pal_unreadable != unreadable) bad++;
            // the other SG slots are clearly not enabled
            if (s.regs.pal[0] != 0) bad++;
         }
      }
      CHECK(bad == 0, "geom %d: %d palette byte values misread", k, bad);

      // all three groups at once, then a reset of the SG group only
      uint8_t regs[9] = { 0x00, 0x38, 0, 0, 0, 0, 0, 0, 0 };
      make_text_page(text, regs);
      for (int slot = 0; slot < CV_PAL_SLOTS; slot++) {
         int i = 32 + 2 * slot;
         text[i] = (uint8_t) (0x81 + (slot & 7));    // E=1, readable low nibble
         text[i + 1] = (uint8_t) (0x01 + slot);
      }
      gen_frame(fb, g, GEN_TEXT, text, 0, 0);
      cv_state_t s; cv_init(&s);
      unsigned int gen0 = s.pal_gen;
      CHECK(cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h) == CV_FRAME_REGISTER, "geom %d: palette page", k);
      int wrong = 0;
      for (int slot = 0; slot < CV_PAL_SLOTS; slot++) {
         int used = slot < CV_PAL_SG_N || (slot >= CV_PAL_ART_CSS1 && slot < CV_PAL_SLOTS);
         int want = used ? (((0x81 + (slot & 7)) << 8) | (0x01 + slot)) : 0;
         if (s.regs.pal[slot] != want) wrong++;
      }
      CHECK(wrong == 0 && s.pal_unreadable == 0, "geom %d: %d palette slots wrong, %d unreadable", k, wrong, s.pal_unreadable);
      CHECK(s.pal_gen != gen0, "geom %d: palette change bumps pal_gen", k);
      unsigned int gen1 = s.pal_gen;
      cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h);
      CHECK(s.pal_gen == gen1, "geom %d: the same page again leaves pal_gen", k);
      uint8_t reset_sg[9] = { 0x08, 0x00, 0, 0, 0, 0, 0, 0, 0 };
      make_text_page(text, reset_sg);
      gen_frame(fb, g, GEN_TEXT, text, 0, 0);
      cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h);
      CHECK(s.regs.pal[0] == 0 && s.regs.pal[10] == 0 && s.regs.pal[CV_PAL_ART_CSS1] != 0 && s.regs.pal[CV_PAL_EXTRA] != 0,
            "geom %d: SG palette reset leaves the other groups", k);
      // artifact register change alone bumps pal_gen too
      unsigned int gen2 = s.pal_gen;
      uint8_t art[9] = { 0x00, 0x02, 0, 0, 0x03, 0, 0, 0, 0 };
      make_text_page(text, art);
      gen_frame(fb, g, GEN_TEXT, text, 0, 0);
      cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h);
      CHECK(s.regs.artifact == 0x03 && s.pal_gen != gen2, "geom %d: artifact register", k);
      free(fb);
   }
}

// palette words decoded within the work limit: a clock that is always over
// budget gives the minimum (4 slots) per frame; the palette is applied in one
// go when complete, other registers at once
static unsigned int pal_clock;
static unsigned int pal_usec(void) { return pal_clock += 1000; }
static uint8_t *pal_bufs[4];
static uint8_t *pal_buffer(int i) { return pal_bufs[i]; }
static void test_palette_spread(void) {
   const gen_geom_t *g = &geoms[0];
   for (int i = 0; i < 4; i++) pal_bufs[i] = alloc_fb(g);
   uint8_t text[512];
   uint8_t regs[9] = { 0x00, 0x3A, 0, 0, 0x03, 0, 0, 0, 0 };
   make_text_page(text, regs);
   for (int slot = 0; slot < CV_PAL_SLOTS; slot++) {
      text[32 + 2 * slot] = (uint8_t) (0x81 + (slot & 7));
      text[33 + 2 * slot] = (uint8_t) (0x01 + slot);
   }
   cv_state_t s; cv_init(&s);
   s.usec = pal_usec; s.budget_us = 250;
   int cn, inh, sw, frames = 0;
   gen_frame(pal_bufs[1], g, GEN_TEXT, text, 0, 0);
   unsigned int gen0 = s.pal_gen;
   cv_frame_t f = cv_frame(&s, 1, 0, 4, pal_buffer, g->pitch, g->fb_w, g->fb_h, &cn, &inh, &sw);
   CHECK(f == CV_FRAME_REGISTER && s.regs.artifact == 0x03, "spread: register page, artifact at once");
   CHECK(s.pal_pending && s.regs.pal[0] == 0, "spread: palette pending after the first frame");
   // ordinary screens follow, captured where cv_frame asks: never into the
   // buffer holding the page while it is still being copied
   uint8_t plain[512];
   for (int i = 0; i < 512; i++) plain[i] = 0x40 | (i & 0x3F);
   unsigned int gen1 = s.pal_gen;
   int early = 0, overwritten = 0, cap = cn, disp = inh ? 0 : 1;
   while (s.pal_pending && frames < 60) {
      if (cap < 0) cap = (disp + 1) & 3;
      if (s.pal_src && cap == 1) overwritten = 1;
      gen_frame(pal_bufs[cap], g, GEN_TEXT, plain, 0, 0);
      cv_frame(&s, cap, disp, 4, pal_buffer, g->pitch, g->fb_w, g->fb_h, &cn, &inh, &sw);
      if (sw >= 0) disp = sw;
      if (!inh) disp = cap;
      if (s.pal_pending && s.pal_gen != gen1) early = 1;
      cap = cn;
      frames++;
   }
   CHECK(!overwritten, "spread: the page's buffer is not captured over while being copied");
   // one line (12 per text row, 4 + 22 + 32 + 32 + 8 columns -> 48 lines)
   // per frame, then 4 slots per frame
   CHECK(!s.pal_pending && frames == 48 + (47 + 3) / 4 - 1, "spread: palette done after %d more frames", frames);
   CHECK(!early && s.pal_gen != gen0, "spread: palette applied in one go");
   int wrong = 0;
   for (int slot = 0; slot < CV_PAL_SLOTS; slot++) {
      int used = slot < CV_PAL_SG_N || slot >= CV_PAL_ART_CSS1;
      int want = used ? (((0x81 + (slot & 7)) << 8) | (0x01 + slot)) : 0;
      if (s.regs.pal[slot] != want) wrong++;
   }
   CHECK(wrong == 0, "spread: %d palette slots wrong", wrong);
   for (int i = 0; i < 4; i++) free(pal_bufs[i]);
}

// the colour, or -1 when the registers leave the profile's colour
static long long cpe(const cv_regs_t *r, int index, const uint32_t *base16) {
   uint32_t v;
   return cv_custom_palette_entry(r, index, base16, &v) ? (long long) v : -1;
}
#define IS_BORDER(i) ((i) == CV_BORDER_BLACK || (i) == CV_BORDER_BUFF || (i) == CV_BORDER_GREEN)
static long long prgb(const cv_regs_t *r, int slot) {
   uint32_t v;
   return cv_pal_rgb(r, slot, &v) ? (long long) v : -1;
}

// CoCoVGA palette slots -> colour table entries, and the artifact register
static void test_palette_mapping(void) {
   cv_state_t s; cv_init(&s);
   uint32_t base16[16];
   for (int c = 0; c < 16; c++) base16[c] = 0x01000000u * (uint32_t) c;
   int custom = 0;
   for (int i = 0; i < CV_VG6_CSS1; i++) custom += cpe(&s.regs, i, base16) >= 0 && !IS_BORDER(i);
   CHECK(custom == 0, "defaults: %d colour table entries changed", custom);
   CHECK(cpe(&s.regs, CV_BORDER_GREEN, base16) == (long long) base16[CV_GREEN], "defaults: border green");
   // 15 bit -> 8 bit per channel, luminance in the top byte
   s.regs.pal[CV_PAL_SG + 1] = CV_PAL_ENABLE | (31 << 10);        // green slot -> pure red
   long long red = cpe(&s.regs, CV_GREEN, base16);
   CHECK(red == (int) ((76u << 24) | 0xFFu), "green slot as red: %08llx", red);
   CHECK(cpe(&s.regs, CV_YELLOW, base16) < 0, "other slots keep their default");
   // light orange text uses the orange slot
   s.regs.pal[CV_PAL_SG + 8] = CV_PAL_ENABLE | 31;                 // orange -> blue
   CHECK(cpe(&s.regs, CV_LORANGE, base16) == prgb(&s.regs, 8) &&
         cpe(&s.regs, CV_ORANGE, base16) == prgb(&s.regs, 8), "orange slot: orange and light orange");
   // 9 colour palette (extras bit 6 clear): dark green / dark orange shown as black
   s.regs.extras = 0x00;
   CHECK(cpe(&s.regs, CV_DKGREEN, base16) == (long long) base16[CV_BLACK], "9 colours: dark green is black");
   s.regs.pal[CV_PAL_SG + 0] = CV_PAL_ENABLE | (16 << 5);
   CHECK(cpe(&s.regs, CV_DKORANGE, base16) == prgb(&s.regs, 0), "9 colours: dark orange is the black slot");
   s.regs.extras = 0x40;
   CHECK(cpe(&s.regs, CV_DKGREEN, base16) < 0, "10 colours: dark green keeps its default");
   // artifact dots: colour A slot 9, colour B slot 10 of their colour set
   s.regs.pal[CV_PAL_ART_CSS1 + 9] = CV_PAL_ENABLE | 1;
   s.regs.pal[CV_PAL_ART_CSS1 + 10] = CV_PAL_ENABLE | 2;
   s.regs.pal[CV_PAL_ART_CSS0 + 9] = CV_PAL_ENABLE | 3;
   CHECK(cpe(&s.regs, CV_ART_CSS1_A | CV_BUFF, base16) == prgb(&s.regs, CV_PAL_ART_CSS1 + 9) &&
         cpe(&s.regs, CV_ART_CSS1_B | CV_BLACK, base16) == prgb(&s.regs, CV_PAL_ART_CSS1 + 10) &&
         cpe(&s.regs, CV_ART_CSS0_A | CV_GREEN, base16) == prgb(&s.regs, CV_PAL_ART_CSS0 + 9) &&
         cpe(&s.regs, CV_ART_CSS0_B | CV_GREEN, base16) < 0, "artifact palette slots");
   CHECK(cpe(&s.regs, CV_ART_CSS1_A | CV_CYAN, base16) < 0, "tag on a non-artifact colour: untouched");
   // artifact register: colour bit and swap on top of the menu setting
   s.regs.artifact = 0x0D;
   CHECK(cv_artifact_mode(&s.regs, 0) == 0 && cv_artifact_mode(&s.regs, 1) == 1 && cv_artifact_mode(&s.regs, 2) == 2, "artifact on");
   s.regs.artifact = 0x0F;
   CHECK(cv_artifact_mode(&s.regs, 1) == 2 && cv_artifact_mode(&s.regs, 2) == 1, "artifact swap");
   s.regs.artifact = 0x0C;
   CHECK(cv_artifact_mode(&s.regs, 1) == 0, "artifact colour off");
   // bright colours (luminance >= 128 in the top byte) are colours too
   s.regs.pal[CV_PAL_SG + 1] = CV_PAL_ENABLE | 0x7FFF;              // green slot -> white
   CHECK(cpe(&s.regs, CV_GREEN, base16) == 0xFFFFFFFFLL, "white: %08llx", cpe(&s.regs, CV_GREEN, base16));
   s.regs.pal[CV_PAL_SG + 1] = 0xFFFA;                               // cream, as test disk key B
   CHECK(cpe(&s.regs, CV_GREEN, base16) == 0xFAD6FFFFLL, "cream: %08llx", cpe(&s.regs, CV_GREEN, base16));
   cv_reset_registers(&s);
   custom = 0;
   for (int i = 0; i < CV_VG6_CSS1; i++) custom += cpe(&s.regs, i, base16) >= 0 && !IS_BORDER(i);
   CHECK(custom == 0 && s.regs.artifact == 0x0D, "reset registers: defaults again");
   // VG6 colours: always set (CoCoVGA's defaults, or the artifact / VG6 slots)
   int vg = 0;
   for (int i = CV_VG6_CSS1; i < CV_VG6_CSS0 + 16; i++) vg += cpe(&s.regs, i, base16) >= 0;
   CHECK(vg == 32 && cpe(&s.regs, CV_VG6_CSS1 | 15, base16) == 0xFFFFFFFFLL &&
         cpe(&s.regs, CV_VG6_CSS1 | 0, base16) == 0 && cpe(&s.regs, CV_VG6_CSS0 | 15, base16) == 0x9600FF00LL,
         "VG6 defaults (%d)", vg);
   s.regs.pal[CV_PAL_ART_CSS0 + 3] = CV_PAL_ENABLE | (31 << 10);
   CHECK(cpe(&s.regs, CV_VG6_CSS0 | 3, base16) == prgb(&s.regs, CV_PAL_ART_CSS0 + 3), "VG6 colour from its slot");
}

static void test_register_page(void) {
   uint8_t text[512];
   for (int k = 0; k < NGEOMS; k++) {
      const gen_geom_t *g = &geoms[k];
      uint8_t *fb = alloc_fb(g);
      make_text_page(text, nitros9_regs);
      gen_frame(fb, g, GEN_TEXT, text, 0, 99);
      cv_state_t s; cv_init(&s);
      cv_frame_t f = cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h);
      CHECK(f == CV_FRAME_REGISTER, "geom %d: NitrOS-9 register page not recognised (%d)", k, f);
      CHECK(s.regs.font == 0x03 && s.regs.enhanced == 0x02, "geom %d: regs font %02x enh %02x", k, s.regs.font, s.regs.enhanced);
      // reset-all page turns 64 columns off again
      static const uint8_t reset_all[9] = { 0xFF, 0x00, 0, 0, 0, 0, 0, 0, 0 };
      make_text_page(text, reset_all);
      gen_frame(fb, g, GEN_TEXT, text, 0, 0);
      f = cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h);
      CHECK(f == CV_FRAME_REGISTER && s.regs.enhanced == 0 && s.regs.font == 0, "geom %d: reset page", k);
      // register page surrounded by black semigraphics (frame can't calibrate
      // on its own): the earlier calibration must be used
      memset(text, 0x80, 512);
      memcpy(text, nitros9_regs, 9);
      gen_frame(fb, g, GEN_TEXT, text, 0, 0);
      f = cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h);
      CHECK(f == CV_FRAME_REGISTER && s.regs.enhanced == 0x02, "geom %d: register page with black surroundings", k);
      free(fb);
   }
   // every edit-mask / value combination the decoder must read back
   const gen_geom_t *g = &geoms[0];
   uint8_t *fb = alloc_fb(g);
   int bad = 0;
   for (int font = 0; font < 8; font++)
      for (int enh = 0; enh < 8; enh++)
         for (int extras = 0; extras < 256; extras += 17) {
            uint8_t regs[9] = { 0x00, 0x85, 0x00, (uint8_t)font, 0x00, (uint8_t)extras, 0, 0, (uint8_t)enh };
            make_text_page(text, regs);
            gen_frame(fb, g, GEN_TEXT, text, 0, 0);
            cv_state_t s; cv_init(&s);
            if (cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h) != CV_FRAME_REGISTER ||
                s.regs.font != font || s.regs.enhanced != enh || s.regs.extras != extras) bad++;
         }
   CHECK(bad == 0, "%d register value combinations misread", bad);
   free(fb);
}

static void test_no_false_registers(void) {
   const gen_geom_t *g = &geoms[0];
   uint8_t *fb = alloc_fb(g);
   uint8_t text[512];
   int hits = 0, pages = 20000;
   for (int n = 0; n < pages; n++) {
      // typical screens: mostly normal text, some inverse, some semigraphics
      for (int i = 0; i < 512; i++) {
         unsigned int r = rnd() % 100;
         if (r < 60) text[i] = 0x60;                         // space
         else if (r < 90) text[i] = 0x40 | (rnd() & 0x3F);   // normal characters
         else if (r < 96) text[i] = rnd() & 0x3F;            // inverse / lower case
         else text[i] = 0x80 | (rnd() & 0x7F);               // semigraphics
      }
      gen_frame(fb, g, GEN_TEXT, text, 0, 0);
      cv_state_t s; cv_init(&s);
      if (cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h) == CV_FRAME_REGISTER) hits++;
   }
   CHECK(hits == 0, "%d of %d random text screens taken for a register page", hits, pages);
   // the "all characters" page (POKE 1024+I,I)
   for (int i = 0; i < 512; i++) text[i] = i < 256 ? i : 0x60;
   gen_frame(fb, g, GEN_TEXT, text, 0, 0);
   cv_state_t s; cv_init(&s);
   CHECK(cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h) == CV_FRAME_TEXT, "all-characters page");
   free(fb);
}

static void test_cg2_decode(void) {
   uint8_t vram[6144];
   for (int k = 0; k < NGEOMS; k++) {
      const gen_geom_t *g = &geoms[k];
      uint8_t *fb = alloc_fb(g);
      uint8_t text[512];
      make_text_page(text, nitros9_regs);
      for (int css = 0; css < 2; css++) {
         cv_state_t s; cv_init(&s);
         gen_frame(fb, g, GEN_TEXT, text, 0, 0);
         cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h);       // calibrate + registers
         for (int n = 0; n < 20; n++) {
            for (int i = 0; i < 2048; i++) vram[i] = rnd() & 0xFF;
            gen_frame(fb, g, GEN_CG2, vram, css, n + 7);
            cv_frame_t f = cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h);
            CHECK(f == CV_FRAME_CG2, "geom %d css %d: CG2 not recognised", k, css);
            CHECK(memcmp(s.vram, vram, 2048) == 0, "geom %d css %d: CG2 bytes differ", k, css);
            CHECK(s.css == css, "geom %d: css %d decoded as %d", k, css, s.css);
         }
         // other modes must not be taken for CG2
         static const gen_mode_t others[] = { GEN_TEXT, GEN_CG1, GEN_CG3, GEN_CG6, GEN_RG6 };
         for (int m = 0; m < 5; m++) {
            for (int n = 0; n < 10; n++) {
               for (int i = 0; i < 6144; i++) vram[i] = rnd() & 0xFF;
               if (others[m] == GEN_TEXT) for (int i = 0; i < 512; i++) vram[i] = 0x40 | (vram[i] & 0x3F);
               gen_frame(fb, g, others[m], vram, css, 0);
               cv_frame_t f = cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h);
               CHECK(f != CV_FRAME_CG2, "geom %d: mode %d taken for CG2", k, others[m]);
            }
         }
      }
      free(fb);
   }
}

// ---------------------------------------------------------------------
// End-to-end simulation of RGBtoHDMI's capture loop (rgb_to_fb.S)
// ---------------------------------------------------------------------
static uint8_t *sim_buffers[4];
static uint8_t *sim_buffer(int i) { return sim_buffers[i]; }

typedef struct {
   int num_buffers_param;   // OSD "Num Buffers" (0..3)
   int last, displayed, redirect;
} sim_t;

// artifact mode emulation: the capture routine replaces every dot pair
// (4 fb pixels, word aligned) with the pair table entry
static uint32_t sim_artifact_table[256];
static int sim_artifacts = 0;

static void apply_artifact_table(uint8_t *fb, const gen_geom_t *g) {
   for (int y = 0; y < g->fb_h; y++) {
      uint8_t *line = fb + y * g->pitch;
      for (int x = 0; x + 3 < g->fb_w; x += 4) {
         int first = line[x] & 15, second = line[x + 2] & 15;
         uint32_t w = sim_artifact_table[first | (second << 4)];
         for (int i = 0; i < 4; i++) line[x + i] = (w >> (8 * i)) & 0xFF;
      }
   }
}

// optional change to each generated frame before cv_frame sees it
static void (*sim_tamper)(uint8_t *fb, const gen_geom_t *g);

static int sim_capture(sim_t *m, cv_state_t *s, const gen_geom_t *g, gen_mode_t mode, const uint8_t *vram, int css, int *captured_out) {
   int cap;
   if (m->redirect) cap = m->redirect - 1;
   else if (m->last == m->num_buffers_param) cap = 0;
   else cap = (m->last + 1) & 3;
   gen_frame(sim_buffers[cap], g, mode, vram, css, 0);
   if (sim_tamper) sim_tamper(sim_buffers[cap], g);
   if (sim_artifacts) apply_artifact_table(sim_buffers[cap], g);
   int capture_next, inhibit, swap;
   cv_frame_t f = cv_frame(s, cap, m->displayed, 4, sim_buffer, g->pitch, g->fb_w, g->fb_h, &capture_next, &inhibit, &swap);
   if (swap >= 0) m->displayed = swap;
   m->last = cap;
   if (!inhibit) m->displayed = cap;
   m->redirect = capture_next + 1;
   if (captured_out) *captured_out = cap;
   return f;
}

static void test_end_to_end(void) {
   for (int k = 0; k < NGEOMS; k++) {
      const gen_geom_t *g = &geoms[k];
      int w64_possible = g->hs >= 2 && g->vs >= 2;
      for (int nb = 0; nb < 4; nb++) {
         for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
         uint8_t *expect = alloc_fb(g);
         sim_t m = { nb, 0, 0, 0 };
         cv_state_t s; cv_init(&s);
         uint8_t text[512], vram[2048];

         // BASIC screen
         make_text_page(text, NULL);
         for (int i = 0; i < 10; i++) sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
         gen_frame(expect, g, GEN_TEXT, text, 0, 0);
         CHECK(memcmp(sim_buffers[m.displayed], expect, (size_t)g->pitch * g->fb_h) == 0, "geom %d nb %d: BASIC screen", k, nb);

         // NitrOS-9 writes the register page over its screen, one frame
         make_text_page(text, nitros9_regs);
         int f = sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
         CHECK(f == CV_FRAME_REGISTER, "geom %d nb %d: register frame", k, nb);
         if (nb > 0) {
            make_text_page(text, NULL);
            gen_frame(expect, g, GEN_TEXT, text, 0, 0);
            CHECK(memcmp(sim_buffers[m.displayed], expect, (size_t)g->pitch * g->fb_h) == 0,
                  "geom %d nb %d: previous picture not held during register frame", k, nb);
         }

         // then CG2 with a cleared 64x32 screen and a shell session
         memset(vram, 0x60, sizeof(vram));
         put_text(vram, 0, 0, "NitrOS-9/6809 Level 1 V3.3.0");
         put_text(vram, 1, 0, "Dragon 64 with CoCoVGA 64 column text, lower case: abcxyz {|}~_^`");
         for (int i = 0; i < 3; i++) sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL);
         if (w64_possible) {
            CHECK(s.w64_on, "geom %d nb %d: W64 not switched on", k, nb);
            int bad = compare_w64(sim_buffers[m.displayed], g, vram, 0x03, 0);
            CHECK(bad == 0, "geom %d nb %d: %d W64 pixels wrong", k, nb, bad);
            // typing: one character per frame
            const char *cmd = "OS9: dir /d0";
            for (int i = 0; cmd[i]; i++) {
               vram[3 * 64 + i] = covdg_code((uint8_t)cmd[i]);
               sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL);
               CHECK(compare_w64(sim_buffers[m.displayed], g, vram, 0x03, 0) == 0, "geom %d nb %d: typing step %d", k, nb, i);
            }
            // scroll the whole screen
            memmove(vram, vram + 64, 2048 - 64);
            memset(vram + 2048 - 64, 0x60, 64);
            sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL);
            CHECK(compare_w64(sim_buffers[m.displayed], g, vram, 0x03, 0) == 0, "geom %d nb %d: scroll", k, nb);
            // something else scribbles over the display buffer: rolling refresh repairs it
            memset(sim_buffers[m.displayed] + g->oy * g->pitch, CV_RED, (size_t)g->pitch * 12 * g->vs);
            for (int i = 0; i < 40; i++) sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL);
            CHECK(compare_w64(sim_buffers[m.displayed], g, vram, 0x03, 0) == 0, "geom %d nb %d: rolling refresh", k, nb);
            // the capture never lands in the buffer on screen while W64 is on
            int cap;
            sim_capture(&m, &s, g, GEN_CG2, vram, 0, &cap);
            CHECK(cap != m.displayed, "geom %d nb %d: capture into displayed buffer", k, nb);
            // the probe after a mode change shows another buffer: W64 must follow the picture
            if (nb >= 1) {
               m.displayed = (m.displayed + 1) & 3;
               sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL);
               CHECK(s.w64_on && m.displayed == s.display_buf, "geom %d nb %d: W64 on wrong buffer", k, nb);
               CHECK(compare_w64(sim_buffers[m.displayed], g, vram, 0x03, 0) == 0, "geom %d nb %d: picture after buffer change", k, nb);
            }
            // the screen is cleared: one frame must redraw every character, not just a refresh
            memset(sim_buffers[m.displayed], 0, (size_t)g->pitch * g->fb_h);
            cv_invalidate(&s);
            sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL);
            CHECK(compare_w64(sim_buffers[m.displayed], g, vram, 0x03, 0) == 0, "geom %d nb %d: picture after clear", k, nb);
         } else {
            CHECK(!s.w64_on, "geom %d: W64 switched on without double size frame buffer", k);
         }

         // program exits back to the 32 column text screen
         make_text_page(text, NULL);
         memcpy(text, "\x4F\x4B", 2);
         for (int i = 0; i < 3; i++) sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
         gen_frame(expect, g, GEN_TEXT, text, 0, 0);
         CHECK(!s.w64_on && m.redirect - 1 != m.displayed, "geom %d nb %d: W64 not left", k, nb);
         CHECK(memcmp(sim_buffers[m.displayed], expect, (size_t)g->pitch * g->fb_h) == 0, "geom %d nb %d: text screen after W64", k, nb);

         for (int i = 0; i < 4; i++) free(sim_buffers[i]);
         free(expect);
      }
   }
}

// ---------------------------------------------------------------------
// Preview: what NitrOS-9 will look like (PPM, MC6847_LC palette)
// ---------------------------------------------------------------------
static void write_ppm(const char *name, const uint8_t *fb, const gen_geom_t *g) {
   static const uint8_t pal[16][3] = {
      {0,0,0},{0,0,0},{0x9A,0x32,0x36},{0xFF,0xB7,0},{0x6B,0x27,0},{0x0D,0x3A,0x06},{0x4C,0x3A,0xB4},{0xBF,0xC8,0xAD},
      {0xD4,0x7F,0},{0xC8,0x4E,0xF0},{0x41,0xAF,0x71},{0,0,0},{0x30,0xD2,0},{0xC1,0xE5,0},{0,0,0},{0,0,0}
   };
   FILE *f = fopen(name, "wb");
   if (!f) return;
   fprintf(f, "P6\n%d %d\n255\n", g->fb_w, g->fb_h);
   for (int y = 0; y < g->fb_h; y++)
      for (int x = 0; x < g->fb_w; x++) {
         int v = fb[y * g->pitch + x];
         int art = cv_artifact_palette_entry(v & 0x7f);
         if (art >= 0) {
            uint8_t rgb[3] = { (uint8_t)(art & 0xFF), (uint8_t)((art >> 8) & 0xFF), (uint8_t)((art >> 16) & 0xFF) };
            fwrite(rgb, 1, 3, f);
         } else {
            fwrite(pal[v & 15], 1, 3, f);
         }
      }
   fclose(f);
}

static void make_previews(void) {
   const gen_geom_t *g = &geoms[0];
   for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
   sim_t m = { 0, 0, 0, 0 };
   cv_state_t s; cv_init(&s);
   uint8_t text[512], vram[2048];
   make_text_page(text, nitros9_regs);
   sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
   memset(vram, 0x60, sizeof(vram));
   static const char *lines[] = {
      "NitrOS-9/6809 Level 1 V3.3.0",
      "Tano Dragon (64K) with CoCoVGA 64x32 text",
      "",
      "OS9: dir -e",
      "",
      "                       Directory of .  2026/10/24 15:42",
      "Owner Last modified    Attributes Sector Bytecount Name",
      "----- ---------------- ---------- ------ --------- ----",
      "    0 2026/10/24 15:40  d-ewrewr      2D       320 CMDS",
      "    0 2026/10/24 15:40  d-ewrewr      43       160 SYS",
      "    0 2026/10/24 15:40  --e-rewr      5A      2741 startup",
      "    0 2026/10/24 15:41  ---wr-wr      61       812 readme.txt",
      "",
      "OS9: list readme.txt",
      "The quick brown fox jumps over the lazy dog.",
      "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG. 0123456789",
      "Symbols: !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
      "",
      "OS9: _",
   };
   for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++) put_text(vram, i, 0, lines[i]);
   for (int r = 22; r < 25; r++)
      for (int c = 0; c < 64; c++) vram[r * 64 + c] = 0x80 | ((c / 8) << 4) | (c & 15);
   sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL);
   write_ppm("preview_w64.ppm", sim_buffers[m.displayed], g);
   uint8_t *raw = alloc_fb(g);
   gen_frame(raw, g, GEN_CG2, vram, 0, 0);
   write_ppm("preview_cg2_raw.ppm", raw, g);
   free(raw);
   for (int i = 0; i < 4; i++) free(sim_buffers[i]);
}

static void test_fonts(void) {
   // only the space glyphs are empty; catches rows lost in the tables
   for (int i = 0; i < 64; i++) {
      int sum = 0;
      for (int r = 0; r < 12; r++) sum |= mc6847_font[i][r];
      CHECK((sum == 0) == (i == 0x20), "6847 glyph $%02X empty=%d", i, sum == 0);
   }
   for (int i = 0; i < 96; i++) {
      int sum = 0;
      for (int r = 0; r < 12; r++) sum |= mc6847t1_font[i][r];
      CHECK((sum == 0) == (i == 0x20), "T1 glyph %d empty=%d", i, sum == 0);
   }
   // spot checks against the data sheet: 'A', 'g' descender, '_' and ']'
   static const uint8_t t1_A[12] = { 0x00, 0x08, 0x14, 0x22, 0x22, 0x3E, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00 };
   CHECK(memcmp(mc6847t1_font[1], t1_A, 12) == 0, "T1 'A'");
   CHECK(mc6847t1_font[64 + 7][9] == 0x1C, "T1 'g' descender");
   CHECK(mc6847t1_font[95][7] == 0x3E, "T1 '_'");
   // lower case next to the 6847's capitals: same baseline (row 9) and cap
   // height (row 3) as the 6847's 'A'; nothing lost by the move
   for (int c = 0; c < 32; c++) {
      CHECK(mc6847t1_font[64 + c][10] == 0 && mc6847t1_font[64 + c][11] == 0, "T1 lower %d rows 10-11", c);
      CHECK(memcmp(cv_lower_glyph(c, 0) + 2, mc6847t1_font[64 + c], 10) == 0, "lower %d moved down 2 rows", c);
   }
   CHECK(cv_lower_glyph(1, 0)[9] != 0 && cv_lower_glyph(1, 0)[10] == 0 && mc6847_rom_font[1][9] != 0 && mc6847_rom_font[1][10] == 0,
         "'a' and the 6847's 'A' end on row 9");
   CHECK(cv_lower_glyph(2, 0)[3] != 0 && mc6847_rom_font[1][3] != 0 && cv_lower_glyph(2, 0)[2] == 0, "'b' ascender at the 6847's cap height");
   CHECK(cv_lower_glyph(1, 1) == mc6847t1_font[65], "T1 capitals: T1 lower case unchanged");
   CHECK(mc6847_font[0x1D][3] == 0x0E, "6847 ']'");
}

// Work limit: entering W64 and scrolling spread over several frames; a
// half drawn 64 column screen is never shown (Num Buffers >= 1)
static unsigned int fake_now;
static unsigned int fake_usec(void) { fake_now += 7; return fake_now; }

static void test_budget(void) {
   const gen_geom_t *g = &geoms[0];
   for (int mode = 0; mode < 2; mode++) {          // 0: character limit, 1: time limit
      for (int nb = 0; nb < 4; nb += 3) {
         for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
         uint8_t *text_fb = alloc_fb(g);
         sim_t m = { nb, 0, 0, 0 };
         cv_state_t s; cv_init(&s);
         if (mode == 0) s.max_chars = 200;
         else { s.usec = fake_usec; s.budget_us = 300; }
         uint8_t text[512], vram[2048];
         make_text_page(text, NULL);
         memcpy(text, "\x4F\x4B", 2);
         for (int i = 0; i < 6; i++) sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
         gen_frame(text_fb, g, GEN_TEXT, text, 0, 0);
         make_text_page(text, nitros9_regs);
         sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
         memset(vram, 0x60, sizeof(vram));
         for (int r = 0; r < 32; r++) put_text(vram, r, 0, "The quick brown fox jumps over the lazy dog 0123456789");
         int frames = 0, bad_shown = 0;
         while (!s.w64_on && frames < 100) {
            sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL);
            frames++;
            if (!s.w64_on && nb > 0 &&
                memcmp(sim_buffers[m.displayed], text_fb, (size_t)g->pitch * g->fb_h) != 0) bad_shown++;
         }
         CHECK(s.w64_on, "budget %d nb %d: W64 never shown", mode, nb);
         CHECK(mode == 1 || frames == (2048 + 199) / 200, "budget %d nb %d: W64 shown after %d frames", mode, nb, frames);
         CHECK(frames > 1, "budget %d nb %d: work limit not applied", mode, nb);
         CHECK(bad_shown == 0, "budget %d nb %d: %d frames showed something other than the old screen", mode, nb, bad_shown);
         CHECK(compare_w64(sim_buffers[m.displayed], g, vram, 0x03, 0) == 0, "budget %d nb %d: W64 picture", mode, nb);
         // scroll the whole screen by one line: catches up within a few frames
         memmove(vram, vram + 64, 2048 - 64);
         memset(vram + 2048 - 64, 0x60, 64);
         int n = 0;
         do {
            sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL);
            n++;
         } while (compare_w64(sim_buffers[m.displayed], g, vram, 0x03, 0) != 0 && n < 50);
         CHECK(n <= 12, "budget %d nb %d: scroll took %d frames", mode, nb, n);
         for (int i = 0; i < 4; i++) free(sim_buffers[i]);
         free(text_fb);
      }
   }
}

// CG2 decoding spread over frames (16 rows per frame)
static void test_sliced_decode(void) {
   const gen_geom_t *g = &geoms[0];
   for (int nb = 0; nb < 4; nb += 3) {
      for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
      sim_t m = { nb, 0, 0, 0 };
      cv_state_t s; cv_init(&s);
      s.decode_rows = 16;
      uint8_t text[512], vram[2048];
      make_text_page(text, NULL);
      for (int i = 0; i < 4; i++) sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
      make_text_page(text, nitros9_regs);
      sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
      memset(vram, 0x60, sizeof(vram));
      for (int r = 0; r < 32; r++) put_text(vram, r, 0, "Sliced decoding 0123456789 abcdefghijklmnopqrstuvwxyz");
      int n = 0;
      while (!s.w64_on && n < 20) { sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL); n++; }
      CHECK(s.w64_on && n == 4, "sliced nb %d: W64 shown after %d frames (expected 4)", nb, n);
      CHECK(compare_w64(sim_buffers[m.displayed], g, vram, 0x03, 0) == 0, "sliced nb %d: picture", nb);
      // a typed character shows within 4 frames, wherever it is
      for (int k = 0; k < 8; k++) {
         int pos = (k * 523) & 2047;
         vram[pos] = (uint8_t) (0x41 + k);
         int f = 0;
         do { sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL); f++; }
         while (compare_w64(sim_buffers[m.displayed], g, vram, 0x03, 0) != 0 && f < 10);
         CHECK(f <= 4, "sliced nb %d: character %d shown after %d frames", nb, k, f);
      }
      // a text frame in between (ride over) must not put text rows into the screen
      make_text_page(text, NULL);
      sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
      for (int i = 0; i < 4; i++) sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL);
      CHECK(s.w64_on && compare_w64(sim_buffers[m.displayed], g, vram, 0x03, 0) == 0, "sliced nb %d: after an odd frame", nb);
      for (int i = 0; i < 4; i++) free(sim_buffers[i]);
   }
}

// ---------------------------------------------------------------------
// Stage 2: 32 column lower case
// ---------------------------------------------------------------------
static int cell_matches(const uint8_t *fb, const gen_geom_t *g, int cx, int cy, const uint8_t *glyph, uint8_t stroke, uint8_t bg) {
   int bad = 0;
   for (int dy = 0; dy < 12; dy++)
      for (int dx = 0; dx < 8; dx++) {
         uint8_t want = ((glyph[dy] >> (7 - dx)) & 1) ? stroke : bg;
         for (int yy = 0; yy < g->vs; yy++)
            for (int xx = 0; xx < g->hs; xx++) {
               int x = g->ox + (cx * 8 + dx) * g->hs + xx, y = g->oy + (cy * 12 + dy) * g->vs + yy;
               if ((fb[y * g->pitch + x] & 15) != want) bad++;
            }
      }
   return bad;
}

static void test_lowercase_32(void) {
   // font bit 1 = lower case, bit 0 = T1 shapes (left 0: this test checks
   // the plain 6847 glyphs; test_lowercase_t1_select covers bit 0 itself)
   static const uint8_t lower_on[9] = { 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
   for (int k = 0; k < NGEOMS; k++) {
      const gen_geom_t *g = &geoms[k];
      for (int nb = 0; nb < 4; nb += 3) {
         for (int css = 0; css < 2; css++) {
            for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
            sim_t m = { nb, 0, 0, 0 };
            cv_state_t s; cv_init(&s);
            uint8_t text[512];
            make_text_page(text, NULL);
            sim_capture(&m, &s, g, GEN_TEXT, text, css, NULL);
            make_text_page(text, lower_on);
            sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
            CHECK(s.regs.font == 2, "geom %d: font register for lower case", k);
            // BASIC screen: row 1 = every inverse code, row 2 normal, row 3 SG4
            make_text_page(text, NULL);
            for (int i = 0; i < 64; i++) text[32 + i] = i;            // rows 1-2
            for (int i = 0; i < 32; i++) text[96 + i] = 0x40 | i;     // row 3
            for (int i = 0; i < 32; i++) text[128 + i] = 0x80 | (i * 4); // row 4
            int captured;
            for (int n = 0; n < 3; n++) sim_capture(&m, &s, g, GEN_TEXT, text, css, &captured);
            const uint8_t *fb = sim_buffers[m.displayed];
            uint8_t light = css ? CV_LORANGE : CV_GREEN, dark = css ? CV_DKORANGE : CV_DKGREEN;
            int bad = 0;
            for (int i = 0; i < 64; i++) {
               int cx = i & 31, cy = 1 + (i >> 5);
               if (i < 32) bad += cell_matches(fb, g, cx, cy, cv_lower_glyph(i, 0), dark, light) != 0;
               else        bad += cell_matches(fb, g, cx, cy, mc6847_rom_font[i], light, dark) != 0;   // stays inverse
            }
            for (int i = 0; i < 32; i++) bad += cell_matches(fb, g, i, 3, mc6847_rom_font[i], dark, light) != 0;
            CHECK(bad == 0, "geom %d nb %d css %d: %d cells wrong in 32 column lower case", k, nb, css, bad);
            // fixed up frames are never captured into the buffer on screen
            sim_capture(&m, &s, g, GEN_TEXT, text, css, &captured);
            int next = m.redirect - 1;
            CHECK(next >= 0 && next != m.displayed, "geom %d nb %d: next capture lands on screen", k, nb);
            // lower case off again: inverse characters come back
            static const uint8_t lower_off[9] = { 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
            uint8_t regs[512];
            make_text_page(regs, lower_off);
            sim_capture(&m, &s, g, GEN_TEXT, regs, 0, NULL);
            for (int n = 0; n < 2; n++) sim_capture(&m, &s, g, GEN_TEXT, text, css, NULL);
            fb = sim_buffers[m.displayed];
            CHECK(cell_matches(fb, g, 1, 1, mc6847_rom_font[1], light, dark) == 0, "geom %d: inverse A after lower case off", k);
            for (int i = 0; i < 4; i++) free(sim_buffers[i]);
         }
      }
   }
}

// Which glyphs the 32 column lower case fixup draws for an inverse cell:
// s->lc_font (CoCoVGA or Dragon 200-E), whatever the font register's T1 bit
// says (T1 shapes are only for W64 with T1 capitals). Checked on both the
// fast and general paths.
extern int cv_lowercase_fast;
static void test_lowercase_font_select(void) {
   const gen_geom_t *g = &geoms[0];
   uint8_t *fb = alloc_fb(g), *orig = alloc_fb(g);
   uint8_t text[512];
   for (int i = 0; i < 512; i++) text[i] = (uint8_t) (i & 31);   // every cell inverse $00-$1F
   cv_state_t s; cv_init(&s);
   CHECK(s.lc_font == CV_LC_6847T1, "lower case font defaults to 6847T1");
   gen_frame(orig, g, GEN_TEXT, text, 0, 0);
   CHECK(cv_analyze(&s, orig, g->pitch, g->fb_w, g->fb_h) == CV_FRAME_TEXT, "font select: calibrate");

   static const struct { int font_t1_bit, lc_font; const char *label; } cases[] = {
      { 0, CV_LC_6847T1,    "6847T1, register plain" },
      { 1, CV_LC_6847T1,    "6847T1, register T1" },
      { 0, CV_LC_DRAGON200E, "Dragon 200-E, register plain" },
      { 1, CV_LC_DRAGON200E, "Dragon 200-E, register T1" },
   };
   for (unsigned int c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
      for (int fast = 0; fast < 2; fast++) {
         s.regs.font = (uint8_t) (0x02 | cases[c].font_t1_bit);
         s.lc_font = cases[c].lc_font;
         memcpy(fb, orig, (size_t)g->pitch * g->fb_h);
         cv_lowercase_fast = fast;
         cv_lowercase_32(&s, fb, g->pitch);
         cv_lowercase_fast = 1;
         int bad = 0;
         for (int i = 0; i < 512; i++)
            bad += cell_matches(fb, g, i & 31, i >> 5, cv_lower_glyph(i & 31, cases[c].lc_font), CV_DKGREEN, CV_GREEN) != 0;
         CHECK(bad == 0, "font select (%s path) %s: %d cells wrong", fast ? "fast" : "general", cases[c].label, bad);
      }
   }
   // the 200-E glyphs are the ROM's, on the 6847 capitals' baseline (row 9)
   CHECK(cv_lower_glyph(1, CV_LC_DRAGON200E) == dragon200e_lower_font[1], "200-E 'a' from the ROM table");
   CHECK(dragon200e_lower_font[1][9] != 0 && dragon200e_lower_font[1][10] == 0, "200-E 'a' sits on row 9");
   CHECK(dragon200e_lower_font[7][10] != 0, "200-E 'g' descends below row 9");
   int differ = 0;
   for (int c = 1; c <= 26; c++) differ += memcmp(cv_lower_glyph(c, CV_LC_6847T1), cv_lower_glyph(c, CV_LC_DRAGON200E), 12) != 0;
   CHECK(differ > 0, "200-E and 6847T1 lower case differ");
   free(fb); free(orig);
}

// Plain 6847s differ in 'E' and 'F' (3 or 4 dot middle bar, row 6): both
// forms must turn into 'e' / 'f', on the fast and the general path, and a
// 4 dot bar must not make any other cell change.
static void test_lowercase_ef_forms(void) {
   const gen_geom_t *g = &geoms[0];
   uint8_t *fb = alloc_fb(g), *orig = alloc_fb(g);
   uint8_t text[512];
   for (int i = 0; i < 512; i++) text[i] = (uint8_t) (i & 31);   // every cell inverse $00-$1F
   cv_state_t s; cv_init(&s);
   gen_frame(orig, g, GEN_TEXT, text, 0, 0);
   CHECK(cv_analyze(&s, orig, g->pitch, g->fb_w, g->fb_h) == CV_FRAME_TEXT, "ef forms: calibrate");
   CHECK(mc6847_rom_font[5][6] == 0x38 && mc6847_rom_font[6][6] == 0x38, "ef forms: table has the 3 dot bar");
   // make every E and F the 4 dot form: the dot at row 6, column 5 lit
   for (int i = 0; i < 512; i++) {
      if ((i & 31) != 5 && (i & 31) != 6) continue;
      int cx = i & 31, cy = i >> 5;
      for (int yy = 0; yy < g->vs; yy++)
         for (int xx = 0; xx < g->hs; xx++)
            orig[(g->oy + (cy * 12 + 6) * g->vs + yy) * g->pitch + g->ox + (cx * 8 + 5) * g->hs + xx] = CV_GREEN;
   }
   s.regs.font = 0x02;
   for (int fast = 0; fast < 2; fast++) {
      memcpy(fb, orig, (size_t)g->pitch * g->fb_h);
      cv_lowercase_fast = fast;
      cv_lowercase_32(&s, fb, g->pitch);
      cv_lowercase_fast = 1;
      int bad = 0;
      for (int i = 0; i < 512; i++)
         bad += cell_matches(fb, g, i & 31, i >> 5, cv_lower_glyph(i & 31, CV_LC_6847T1), CV_DKGREEN, CV_GREEN) != 0;
      CHECK(bad == 0, "ef forms (%s path): %d cells wrong with a 4 dot E / F bar", fast ? "fast" : "general", bad);
   }
   free(fb); free(orig);
}

// The menu can turn 32 column lower case on or off regardless of the CoCoVGA
// register page (s->lc_on_mode 1 / 0): BASIC typing after SHIFT-0, with no
// register page ever written, comes out as lower case when forced on, and a
// register page asking for lower case is ignored when forced off.
static void test_lowercase_forced(void) {
   static const uint8_t lower_on[9] = { 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
   static const struct { int write_regs, on_mode, font, expect_lower; const char *label; } cases[] = {
      { 0, -1, CV_LC_6847T1,    0, "auto, no register page" },
      { 1, -1, CV_LC_6847T1,    1, "auto, register page lower case" },
      { 0,  1, CV_LC_6847T1,    1, "forced 6847T1, no register page" },
      { 0,  1, CV_LC_DRAGON200E, 1, "forced Dragon 200-E, no register page" },
      { 1,  0, CV_LC_6847T1,    0, "forced off, register page lower case" },
   };
   for (int k = 0; k < 2; k++) {
      const gen_geom_t *g = &geoms[k];
      for (unsigned int c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
         for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
         sim_t m = { 3, 0, 0, 0 };
         cv_state_t s; cv_init(&s);
         s.lc_on_mode = cases[c].on_mode;
         s.lc_font = cases[c].font;
         uint8_t text[512];
         make_text_page(text, NULL);
         sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
         if (cases[c].write_regs) {
            make_text_page(text, lower_on);
            sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
         }
         make_text_page(text, NULL);
         for (int i = 0; i < 32; i++) text[64 + i] = i;     // row 2: inverse $00-$1F
         for (int n = 0; n < 3; n++) sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
         CHECK(cv_lowercase_on(&s) == cases[c].expect_lower, "geom %d %s: lower case on", k, cases[c].label);
         const uint8_t *fb = sim_buffers[m.displayed];
         int bad = 0;
         for (int i = 0; i < 32; i++) {
            if (cases[c].expect_lower)
               bad += cell_matches(fb, g, i, 2, cv_lower_glyph(i, cases[c].font), CV_DKGREEN, CV_GREEN) != 0;
            else
               bad += cell_matches(fb, g, i, 2, mc6847_rom_font[i], CV_GREEN, CV_DKGREEN) != 0;
         }
         CHECK(bad == 0, "geom %d %s: %d cells wrong", k, cases[c].label, bad);
         for (int i = 0; i < 4; i++) free(sim_buffers[i]);
      }
   }
}

// ---------------------------------------------------------------------
// Stage 2: NTSC artifact colours
// ---------------------------------------------------------------------
static void test_artifacts(void) {
   // table: only black/green and black/buff pairs change
   for (int mode = 0; mode < 3; mode++) {
      cv_build_artifact_table(sim_artifact_table, mode, 0);
      int changed = 0;
      for (int a = 0; a < 16; a++)
         for (int b = 0; b < 16; b++) {
            uint32_t w = sim_artifact_table[a | (b << 4)];
            uint32_t plain = a * 0x0101u | b * 0x01010000u;
            CHECK((w & 0x0F0F0F0Fu) == plain, "mode %d: pair %d,%d lost its colours", mode, a, b);
            if (w != plain) {
               changed++;
               int ok = mode && a != b && ((a == CV_BLACK && (b == CV_GREEN || b == CV_BUFF)) || (b == CV_BLACK && (a == CV_GREEN || a == CV_BUFF)));
               CHECK(ok, "mode %d: pair %d,%d changed", mode, a, b);
            }
         }
      CHECK(changed == (mode ? 4 : 0), "mode %d: %d pairs changed", mode, changed);
   }
   CHECK(cv_artifact_index(CV_BUFF, CV_BLACK, 1, 0) == cv_artifact_index(CV_BLACK, CV_BUFF, 2, 0), "phase swap");
   CHECK(cv_artifact_palette_entry(CV_ART_CSS1_A | CV_BUFF) > 0 && cv_artifact_palette_entry(CV_ART_CSS1_A | CV_BLACK) == cv_artifact_palette_entry(CV_ART_CSS1_A | CV_BUFF)
         && cv_artifact_palette_entry(0x05) == -1, "palette entries");
   // green set graphics with a dark green background (XRoar): only with the flag
   CHECK(cv_artifact_index(CV_DKGREEN, CV_GREEN, 1, 0) == -1 && cv_artifact_index(CV_DKGREEN, CV_GREEN, 1, CV_ART_GREEN_DARK_BG) == CV_ART_CSS0_B
         && cv_artifact_index(CV_GREEN, CV_DKGREEN, 1, CV_ART_GREEN_DARK_BG) == CV_ART_CSS0_A, "dark green background pairs");

   // with artifacts on in the capture, only RG6 pictures change, and the
   // CoCoVGA emulation still works (word aligned geometries)
   cv_build_artifact_table(sim_artifact_table, 1, 0);
   uint8_t vram[6144];
   for (int k = 0; k < NGEOMS; k++) {
      const gen_geom_t *g = &geoms[k];
      if (g->hs != 2 || g->vs != 2 || (g->ox & 3)) continue;
      uint8_t *a = alloc_fb(g), *b = alloc_fb(g);
      static const gen_mode_t modes[] = { GEN_TEXT, GEN_CG1, GEN_CG2, GEN_CG3, GEN_CG6, GEN_RG6 };
      for (int mi = 0; mi < 6; mi++) {
         for (int css = 0; css < 2; css++) {
            for (int i = 0; i < 6144; i++) vram[i] = rnd() & 0xFF;
            gen_frame(a, g, modes[mi], vram, css, 0);
            memcpy(b, a, (size_t)g->pitch * g->fb_h);
            apply_artifact_table(b, g);
            int diff = memcmp(a, b, (size_t)g->pitch * g->fb_h) != 0;
            CHECK(diff == (modes[mi] == GEN_RG6), "geom %d mode %d css %d: artifacts changed=%d", k, modes[mi], css, diff);
         }
      }
      free(a); free(b);
      // full NitrOS-9 sequence with artifacts on
      for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
      sim_artifacts = 1;
      sim_t m = { 0, 0, 0, 0 };
      cv_state_t s; cv_init(&s);
      uint8_t text[512], v[2048];
      make_text_page(text, NULL);
      sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
      make_text_page(text, nitros9_regs);
      sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
      memset(v, 0x60, sizeof(v));
      put_text(v, 0, 0, "artifacts on: still 64 columns");
      for (int i = 0; i < 2; i++) sim_capture(&m, &s, g, GEN_CG2, v, 0, NULL);
      CHECK(s.w64_on && compare_w64(sim_buffers[m.displayed], g, v, 0x03, 0) == 0, "geom %d: W64 with artifacts on", k);
      sim_artifacts = 0;
      for (int i = 0; i < 4; i++) free(sim_buffers[i]);
   }
}

static void make_artifact_preview(void) {
   const gen_geom_t *g = &geoms[0];
   uint8_t vram[6144], *fb = alloc_fb(g);
   // PMODE 4 test card: vertical stripes of 1, 2 and 4 dots, solid, checks
   for (int y = 0; y < 192; y++)
      for (int b = 0; b < 32; b++) {
         uint8_t v;
         if (b < 8) v = 0xAA; else if (b < 16) v = 0x55; else if (b < 20) v = 0xCC; else if (b < 24) v = 0xFF;
         else v = (y & 1) ? 0xAA : 0x55;
         vram[y * 32 + b] = v;
      }
   cv_build_artifact_table(sim_artifact_table, 1, 0);
   gen_frame(fb, g, GEN_RG6, vram, 1, 0);
   apply_artifact_table(fb, g);
   write_ppm("preview_artifacts.ppm", fb, g);
   free(fb);
}

// Fast 32 column lower case path must give the same picture as the general one
extern int cv_lowercase_fast;
static void test_lowercase_fast(void) {
   for (int k = 0; k < NGEOMS; k++) {
      const gen_geom_t *g = &geoms[k];
      if (g->hs != 2 || g->vs != 2) continue;
      uint8_t *a = alloc_fb(g), *b = alloc_fb(g);
      uint8_t text[512];
      for (int i = 0; i < 512; i++) text[i] = (uint8_t) ((i * 37) & 0x7F);   // inverse + normal
      for (int i = 0; i < 64; i++) text[i] = (uint8_t) i;                     // every inverse code
      for (int i = 448; i < 480; i++) text[i] = (uint8_t) (0x80 | (i & 0x7F)); // semigraphics
      for (int css = 0; css < 2; css++) {
         for (unsigned int noise = 0; noise < 2; noise++) {
            cv_state_t s; cv_init(&s);
            gen_frame(a, g, GEN_TEXT, text, css, 0);
            CHECK(cv_analyze(&s, a, g->pitch, g->fb_w, g->fb_h) == CV_FRAME_TEXT, "geom %d: calibrate", k);
            s.regs.font = 0x02;   // lower case on, T1 shapes off - see test_lowercase_32
            gen_frame(a, g, GEN_TEXT, text, css, noise ? 77 + k : 0);
            memcpy(b, a, (size_t)g->pitch * g->fb_h);
            cv_lowercase_fast = 1;
            int na = cv_lowercase_32(&s, a, g->pitch);
            cv_lowercase_fast = 0;
            int nb = cv_lowercase_32(&s, b, g->pitch);
            cv_lowercase_fast = 1;
            CHECK(na == nb && na > 32, "geom %d css %d noise %u: %d / %d cells", k, css, noise, na, nb);
            int diff = 0;
            for (size_t i = 0; i < (size_t)g->pitch * g->fb_h; i++) {
               if (noise ? ((a[i] ^ b[i]) & CV_CODE_MASK) : (a[i] != b[i])) diff++;
            }
            CHECK(diff == 0, "geom %d css %d noise %u: fast and general lower case differ in %d pixels", k, css, noise, diff);
         }
      }
      free(a); free(b);
   }
}

// Wrong dots in an inverse cell must never turn into a wrong lower case letter
static void test_lowercase_dot_errors(void) {
   CHECK(cv_lowercase_min_distance() >= 3, "lower case dot set: min distance %d", cv_lowercase_min_distance());
   const gen_geom_t *g = &geoms[0];
   uint8_t *fb = alloc_fb(g), *orig = alloc_fb(g);
   uint8_t text[512];
   memset(text, 0x60, sizeof(text));
   for (int i = 0; i < 32; i++) text[i] = (uint8_t) i;           // row 0: inverse $00-$1F
   for (int i = 0; i < 32; i++) text[32 + i] = (uint8_t) (32 + i); // row 1: inverse $20-$3F
   cv_state_t s; cv_init(&s);
   gen_frame(orig, g, GEN_TEXT, text, 0, 0);
   CHECK(cv_analyze(&s, orig, g->pitch, g->fb_w, g->fb_h) == CV_FRAME_TEXT, "dot errors: calibrate");
   s.regs.font = 0x02;   // lower case on, T1 shapes off - see test_lowercase_32
   unsigned int r = 99;
   int wrong = 0, kept = 0, fixed = 0;
   for (int trial = 0; trial < 6000; trial++) {
      memcpy(fb, orig, (size_t)g->pitch * g->fb_h);
      int cell = trial % 64, cx = cell & 31, cy = cell >> 5;
      int flips = 1 + (trial / 64) % 2;               // 1 or 2 wrong dots
      for (int f = 0; f < flips; f++) {
         r = r * 1103515245u + 12345u;
         int d = (int) ((r >> 16) % 96), dy = d / 8, dx = d % 8;
         for (int y = 0; y < 2; y++) for (int x = 0; x < 2; x++) {
            uint8_t *p = fb + (g->oy + (cy * 12 + dy) * 2 + y) * g->pitch + g->ox + (cx * 8 + dx) * 2 + x;
            *p = (*p == CV_GREEN) ? CV_DKGREEN : CV_GREEN;
         }
      }
      cv_lowercase_32(&s, fb, g->pitch);
      // what does the cell show now?
      int shown = -1;
      for (int code = 0; code < 32 && shown < 0; code++) {
         if (cell_matches(fb, g, cx, cy, cv_lower_glyph(code, 0), CV_DKGREEN, CV_GREEN) == 0) shown = code;
      }
      if (shown < 0) kept++;
      else if (shown == cell) fixed++;
      else wrong++;
   }
   CHECK(wrong == 0, "dot errors: %d cells showed a wrong lower case letter", wrong);
   printf("  lower case with 1-2 wrong dots: %d right letter, %d left inverse, %d wrong\n", fixed, kept, wrong);
   free(fb); free(orig);
}

// 'e' ($05) and 'f' ($06) differ only at row 9, columns 3/4/5/6 (the rest
// of their 96 dots are identical) - the drill-down that resolves them by a
// vote over those 4 dots when the ordinary exact match fails. Flip exactly
// one of the 4: still resolves to the right letter. Flip exactly two (every
// pairing): a genuine 2-2 tie, so it must abstain (stay inverse), never
// show either letter, let alone a third one. This exercises what the
// random 1-2 dot test above only hits by chance.
static void test_ef_tiebreak(void) {
   static const int ef_cols[4] = { 3, 4, 5, 6 };   // row 9, columns that tell 'e' and 'f' apart
   const gen_geom_t *g = &geoms[0];
   uint8_t *fb = alloc_fb(g), *orig = alloc_fb(g);
   uint8_t text[512];
   memset(text, 0x60, sizeof(text));
   text[0] = 0x05;   // cell 0: 'E' inverse
   text[1] = 0x06;   // cell 1: 'F' inverse
   cv_state_t s; cv_init(&s);
   gen_frame(orig, g, GEN_TEXT, text, 0, 0);
   CHECK(cv_analyze(&s, orig, g->pitch, g->fb_w, g->fb_h) == CV_FRAME_TEXT, "ef tiebreak: calibrate");
   s.regs.font = 0x02;   // lower case on, T1 shapes off - see test_lowercase_32
   for (int cell = 0; cell < 2; cell++) {
      int want = cell == 0 ? 5 : 6, other = cell == 0 ? 6 : 5;
      // every single column (1 dot wrong) and every pair of columns (2 dots
      // wrong) among the 4 that distinguish 'e' and 'f'
      for (int c0 = 0; c0 < 4; c0++) {
         for (int c1 = c0; c1 < 4; c1++) {
            int flips = (c0 == c1) ? 1 : 2;
            int cols[2] = { ef_cols[c0], ef_cols[c1] };
            memcpy(fb, orig, (size_t)g->pitch * g->fb_h);
            for (int i = 0; i < flips; i++) {
               int dy = 9, dx = cols[i];
               for (int y = 0; y < 2; y++) for (int x = 0; x < 2; x++) {
                  uint8_t *p = fb + (g->oy + dy * 2 + y) * g->pitch + g->ox + (cell * 8 + dx) * 2 + x;
                  *p = (*p == CV_GREEN) ? CV_DKGREEN : CV_GREEN;
               }
            }
            uint8_t *noisy = alloc_fb(g);
            memcpy(noisy, fb, (size_t)g->pitch * g->fb_h);    // the (possibly corrupted) cell before the pass
            cv_lowercase_32(&s, fb, g->pitch);
            int shown_want = cell_matches(fb, g, cell, 0, cv_lower_glyph(want, 0), CV_DKGREEN, CV_GREEN) == 0;
            int shown_other = cell_matches(fb, g, cell, 0, cv_lower_glyph(other, 0), CV_DKGREEN, CV_GREEN) == 0;
            int untouched = 1;
            for (int dy = 0; dy < 24 && untouched; dy++)
               for (int dx = 0; dx < 16 && untouched; dx++)
                  if (fb[(g->oy + dy) * g->pitch + g->ox + cell * 16 + dx] !=
                      noisy[(g->oy + dy) * g->pitch + g->ox + cell * 16 + dx]) untouched = 0;
            free(noisy);
            CHECK(shown_want + shown_other + (untouched ? 1 : 0) == 1,
                  "ef tiebreak: cell %d flips %d cols %d,%d: ambiguous display", cell, flips, cols[0], cols[1]);
            CHECK(!shown_other, "ef tiebreak: cell %d flips %d cols %d,%d: showed the wrong letter of the pair",
                  cell, flips, cols[0], cols[1]);
            if (flips == 1) CHECK(shown_want, "ef tiebreak: cell %d 1 dot wrong (col %d): did not resolve", cell, cols[0]);
            if (flips == 2) CHECK(untouched, "ef tiebreak: cell %d 2 dots wrong (cols %d,%d): did not abstain", cell, cols[0], cols[1]);
         }
      }
   }
   free(fb); free(orig);
}

// Green set artifact table: decided from the capture's border wherever the
// capture sits in the frame buffer; split screens (text below) stay plain
static void test_artifact_flags(void) {
   static const gen_geom_t pad = { 800, 560, 800, 64 + 80, 48 + 40, 2, 2 };  // capture at 80,40 640x480
   uint8_t vram[6144];
   for (int i = 0; i < 6144; i++) vram[i] = rnd() & 0xFF;
   uint8_t text[512];
   make_text_page(text, NULL);
   const gen_geom_t *g = &geoms[0];
   uint8_t *fb = alloc_fb(g);
   gen_frame(fb, g, GEN_RG6, vram, 0, 0);
   CHECK(cv_artifact_flags_for(fb, g->pitch, 0, 0, 640, 480) == CV_ART_GREEN_DARK_BG, "flags: green PMODE 4");
   gen_frame(fb, g, GEN_RG6, vram, 1, 0);
   CHECK(cv_artifact_flags_for(fb, g->pitch, 0, 0, 640, 480) == 0, "flags: buff PMODE 4");
   gen_frame(fb, g, GEN_TEXT, text, 0, 0);
   CHECK(cv_artifact_flags_for(fb, g->pitch, 0, 0, 640, 480) == 0, "flags: text");
   gen_frame(fb, g, GEN_RG6, vram, 0, 0);
   memset(fb + 300 * g->pitch, CV_BLACK, (size_t) g->pitch * 180);       // text below
   CHECK(cv_artifact_flags_for(fb, g->pitch, 0, 0, 640, 480) == 0, "flags: split screen");
   free(fb);
   fb = alloc_fb(&pad);
   gen_frame(fb, &pad, GEN_RG6, vram, 0, 0);
   for (int y = 0; y < pad.fb_h; y++)                                     // padding outside the capture
      for (int x = 0; x < pad.fb_w; x++)
         if (y < 40 || y >= 520 || x < 80 || x >= 720) fb[y * pad.pitch + x] = 0;
   CHECK(cv_artifact_flags_for(fb, pad.pitch, 80, 40, 640, 480) == CV_ART_GREEN_DARK_BG, "flags: padded frame buffer");
   free(fb);
}

// Lower case fix-up spread over frames (100 cells per frame): a half fixed
// screen is never shown (Num Buffers >= 1), the finished one is right
static void test_lowercase_spread(void) {
   // font bit 0 (T1 shapes) left 0: see test_lowercase_32
   static const uint8_t lower_on[9] = { 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
   for (int k = 0; k < 2; k++) {                    // aligned and 2 pixels off
      const gen_geom_t *g = &geoms[k];
      for (int nb = 0; nb < 4; nb += 3) {
         for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
         uint8_t *before = alloc_fb(g);
         sim_t m = { nb, 0, 0, 0 };
         cv_state_t s; cv_init(&s);
         uint8_t text[512];
         make_text_page(text, NULL);
         sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
         make_text_page(text, lower_on);
         sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
         make_text_page(text, NULL);
         for (int i = 0; i < 4; i++) sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
         memcpy(before, sim_buffers[m.displayed], (size_t)g->pitch * g->fb_h);
         s.max_chars = 100;
         for (int i = 0; i < 512; i++) text[i] = (uint8_t) (i & 31);   // all lower case
         int frames = 0, bad = 0;
         int start = m.displayed;
         (void) start;
         for (;;) {
            sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
            frames++;
            if (s.lc_buf < 0 || frames > 20) break;
            if (nb > 0 && memcmp(sim_buffers[m.displayed], before, (size_t)g->pitch * g->fb_h) != 0) bad++;
         }
         CHECK(frames >= 5 && frames <= 6, "lower spread geom %d nb %d: shown after %d frames", k, nb, frames);
         CHECK(bad == 0, "lower spread geom %d nb %d: %d frames showed a half fixed screen", k, nb, bad);
         int wrong = 0;
         for (int i = 0; i < 512; i++)
            wrong += cell_matches(sim_buffers[m.displayed], g, i & 31, i >> 5, cv_lower_glyph(i & 31, 0), CV_DKGREEN, CV_GREEN) != 0;
         CHECK(wrong == 0, "lower spread geom %d nb %d: %d cells wrong", k, nb, wrong);
         for (int i = 0; i < 4; i++) free(sim_buffers[i]);
         free(before);
      }
   }
}

// VG6: a CG6 picture with enhanced bit 0 is redrawn as 128x96 pixels of 16
// colours (frame buffer value tag | nibble), off screen, shown when complete
static void black_top_border(uint8_t *fb, const gen_geom_t *g) {
   for (int y = 0; y < g->oy; y++) memset(fb + y * g->pitch, CV_BLACK, (size_t) g->fb_w);
}
static unsigned int vg_clock;
static unsigned int vg_usec(void) { return vg_clock += 1000; }
static void test_vg6(void) {
   static uint8_t data[6144];
   for (int k = 0; k < NGEOMS; k++) {
      const gen_geom_t *g = &geoms[k];
      for (int slow = 0; slow < 2; slow++) {
         for (int nb = 0; nb < 4; nb += 3) {
            for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
            sim_t m = { nb, 0, 0, 0 };
            cv_state_t s; cv_init(&s);
            uint8_t text[512];
            make_text_page(text, NULL);
            for (int i = 0; i < 2; i++) sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
            s.regs.enhanced = 0x01;
            if (slow) { s.usec = vg_usec; s.budget_us = 250; }
            for (int css = 0; css < 2; css++) {
               for (int i = 0; i < 6144; i++) data[i] = (uint8_t) rnd();
               uint8_t *before = alloc_fb(g);
               memcpy(before, sim_buffers[m.displayed], (size_t) g->pitch * g->fb_h);
               int frames = 0, half = 0, ft;
               do {
                  ft = sim_capture(&m, &s, g, GEN_CG6, data, css, NULL);
                  frames++;
                  if (s.vg_buf >= 0 && s.vg_buf == m.displayed) half++;   // never converted on screen
               } while ((s.vg_buf >= 0 || frames == 1) && frames < 200);
               CHECK(ft == CV_FRAME_VG6, "vg6 geom %d slow %d nb %d css %d: frame type %d", k, slow, nb, css, ft);
               CHECK(slow ? frames > 2 : frames <= 2, "vg6 geom %d slow %d nb %d: done after %d frames", k, slow, nb, frames);
               CHECK(half == 0, "vg6 geom %d slow %d nb %d: a half converted picture was shown", k, slow, nb);
               const uint8_t *fb = sim_buffers[m.displayed];
               int wrong = 0;
               uint8_t tag = css ? CV_VG6_CSS1 : CV_VG6_CSS0;
               for (int y = 0; y < 192 * g->vs; y++)
                  for (int x = 0; x < 256 * g->hs; x++) {
                     int px = x / (2 * g->hs), py = y / (2 * g->vs);
                     uint8_t b = data[py * 64 + px / 2];
                     uint8_t want = tag | ((px & 1) ? (b & 15) : (b >> 4));
                     if (fb[(g->oy + y) * g->pitch + g->ox + x] != want) wrong++;
                  }
               CHECK(wrong == 0, "vg6 geom %d slow %d nb %d css %d: %d pixels wrong", k, slow, nb, css, wrong);
               free(before);
            }
            // back to text: shown normally again
            int f = sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
            CHECK(f == CV_FRAME_TEXT && s.vg_buf < 0 && m.displayed == m.last, "vg6 geom %d: text again", k);
            // VG6 off: CG6 stays CG6
            s.regs.enhanced = 0;
            f = sim_capture(&m, &s, g, GEN_CG6, data, 0, NULL);
            CHECK(f != CV_FRAME_VG6, "vg6 geom %d: not without enhanced bit 0", k);
            for (int i = 0; i < 4; i++) free(sim_buffers[i]);
         }
      }
   }
   // a CG6 frame whose mode changed in the top border (just after a
   // register page): the VG6 picture stays on screen
   {
      const gen_geom_t *g = &geoms[0];
      for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
      sim_t m = { 3, 0, 0, 0 };
      cv_state_t s; cv_init(&s);
      uint8_t text[512];
      make_text_page(text, NULL);
      for (int i = 0; i < 2; i++) sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
      s.regs.enhanced = 0x01;
      for (int i = 0; i < 6144; i++) data[i] = (uint8_t) rnd();
      for (int i = 0; i < 3; i++) sim_capture(&m, &s, g, GEN_CG6, data, 1, NULL);
      int shown = m.displayed;
      sim_tamper = black_top_border;
      int f = sim_capture(&m, &s, g, GEN_CG6, data, 1, NULL);
      sim_tamper = NULL;
      CHECK(f == CV_FRAME_OTHER && m.displayed == shown, "vg6: late mode change keeps the picture (%d)", f);
      f = sim_capture(&m, &s, g, GEN_CG6, data, 1, NULL);
      CHECK(f == CV_FRAME_VG6, "vg6: next frame VG6 again");
      for (int i = 0; i < 4; i++) free(sim_buffers[i]);
   }
   // with W64 on too, CG2 frames are still W64 and CG6 frames VG6
   const gen_geom_t *g = &geoms[0];
   for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
   sim_t m = { 3, 0, 0, 0 };
   cv_state_t s; cv_init(&s);
   uint8_t text[512], vram[2048];
   make_text_page(text, NULL);
   for (int i = 0; i < 2; i++) sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
   s.regs.enhanced = 0x03;
   for (int i = 0; i < 2048; i++) vram[i] = (uint8_t) rnd();
   int f = sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL);
   CHECK(f == CV_FRAME_CG2, "W64 + VG6: CG2 frame is W64 (%d)", f);
   for (int i = 0; i < 6144; i++) data[i] = (uint8_t) rnd();
   f = sim_capture(&m, &s, g, GEN_CG6, data, 1, NULL);
   CHECK(f == CV_FRAME_VG6, "W64 + VG6: CG6 frame is VG6 (%d)", f);
   for (int i = 0; i < 4; i++) free(sim_buffers[i]);
}

// ---------------------------------------------------------------------
// Benchmark (the one the firmware runs from the Info menu): every row must
// take the path it is meant to measure. PC timings are only a sanity check.
// ---------------------------------------------------------------------
static unsigned int pc_usec(void) {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (unsigned int) (ts.tv_sec * 1000000u + ts.tv_nsec / 1000u);
}

// a clock that runs fast compared with the work (each call is 5 us later)
static unsigned int slow_clock;
static unsigned int slow_usec(void) { return slow_clock += 5; }

static void test_bench(void) {
   static uint8_t mem[CV_BENCH_MEM_SIZE];
   cv_bench_result_t res;
   cv_bench_run(mem, pc_usec, 0, 0, &res);
   CHECK(res.rows >= 10, "bench: %d rows", res.rows);
   for (int i = 0; i < res.rows; i++) {
      const cv_bench_row_t *r = &res.row[i];
      CHECK(r->ok && r->n > 0, "bench row '%s' did not take the expected path", r->name);
      printf("  bench (PC) %-22s %s %5u %5u %5u us\n", r->name, r->group ? "lower" : "     ", r->min_us, r->avg_us, r->max_us);
   }
   // with the Pi's limits and a slow clock (entering W64 takes several
   // frames, as on a Pi Zero W) every row must still take its path
   cv_bench_run(mem, slow_usec, 250, 120, &res);
   int max_frames = 0;
   for (int i = 0; i < res.rows; i++) {
      const cv_bench_row_t *r = &res.row[i];
      CHECK(r->ok && r->n > 0, "bench (slow clock) row '%s' did not take the expected path", r->name);
      if (r->frames > max_frames) max_frames = r->frames;
   }
   CHECK(max_frames > 3, "bench (slow clock): W64 entered in %d frames, clock not slow enough", max_frames);
}

// The mode change to CG2 came after the top border began (as on a PAL
// Dragon, where field sync comes late): the top border is still black
static void tamper_late_border(uint8_t *fb, const gen_geom_t *g) {
   int by0 = g->oy - 24 * g->vs;
   if (by0 < 0) by0 = 0;
   for (int y = by0; y < g->oy; y++) memset(fb + y * g->pitch, CV_BLACK, (size_t)g->fb_w);
}
// a CG2 pixel row torn by a write while the 6847 drew it (first line differs
// at a byte the vertical check samples)
static void tamper_torn_row(uint8_t *fb, const gen_geom_t *g) {
   int row = 10, b = 2;
   uint8_t *p = fb + (g->oy + row * 3 * g->vs) * g->pitch + g->ox + b * 8 * g->hs;
   uint8_t c = (*p == CV_BLUE) ? CV_RED : CV_BLUE;
   for (int x = 0; x < 2 * g->hs; x++) p[x] = c;
}

static void test_entry_holds(void) {
   for (int k = 0; k < NGEOMS; k++) {
      const gen_geom_t *g = &geoms[k];
      if (g->hs < 2 || g->vs < 2) continue;
      for (int t = 0; t < 3; t++) {
         for (int nb = 1; nb < 4; nb++) {
            for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
            uint8_t *expect = alloc_fb(g);
            sim_t m = { nb, 0, 0, 0 };
            cv_state_t s; cv_init(&s);
            uint8_t text[512], vram[2048];
            make_text_page(text, NULL);
            for (int i = 0; i < 5; i++) sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
            make_text_page(text, nitros9_regs);
            sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
            make_text_page(text, NULL);
            sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
            gen_frame(expect, g, GEN_TEXT, text, 0, 0);
            memset(vram, 0x60, sizeof(vram));
            put_text(vram, 0, 0, "entering W64");
            if (t < 2) {
               // one unusable frame: the text stays, then W64 from the next
               sim_tamper = t == 0 ? tamper_late_border : tamper_torn_row;
               int cap;
               sim_capture(&m, &s, g, GEN_CG2, vram, 0, &cap);
               sim_tamper = NULL;
               CHECK(!s.w64_on && memcmp(sim_buffers[m.displayed], expect, (size_t)g->pitch * g->fb_h) == 0,
                     "geom %d nb %d %s: previous picture not held", k, nb, t ? "torn row" : "late border");
               sim_capture(&m, &s, g, GEN_CG2, vram, 0, NULL);
               CHECK(s.w64_on && compare_w64(sim_buffers[m.displayed], g, vram, 0x03, 0) == 0,
                     "geom %d nb %d %s: W64 not entered next frame", k, nb, t ? "torn row" : "late border");
               // the W64 picture has the CG border all round (not the late one)
               uint8_t top = sim_buffers[m.displayed][(g->oy - g->vs) * g->pitch + g->ox];
               CHECK(top == CV_GREEN, "geom %d nb %d: W64 top border %d", k, nb, top);
            } else {
               // a program that shows another CG picture with W64 enabled:
               // only CV_ENTRY_HOLDS frames are held, then it shows
               sim_tamper = tamper_torn_row;
               int cap = -1;
               for (int i = 0; i <= CV_ENTRY_HOLDS; i++) sim_capture(&m, &s, g, GEN_CG2, vram, 0, &cap);
               sim_tamper = NULL;
               CHECK(m.displayed == cap, "geom %d nb %d: unusable CG frames held for ever", k, nb);
            }
            for (int i = 0; i < 4; i++) free(sim_buffers[i]);
            free(expect);
         }
      }
   }
}


// ---------------------------------------------------------------------
// Text colours and inverse video (extras bits 7 and 5): text dots tagged by
// the capture's pair table, coloured by the colour table
// ---------------------------------------------------------------------

// reference: is this dot of a pair a text dot (independent of cocovga.c)
static int ref_text_dot(int c, int partner) {
   if (c == CV_DKGREEN || c == CV_DKORANGE || c == CV_LORANGE) return 1;
   return c == CV_GREEN && (partner == CV_GREEN || partner == CV_DKGREEN);
}

static void test_text_tag_table(void) {
   uint32_t plain[256], tagged[256];
   for (int mode = 0; mode < 3; mode++) {
      cv_build_artifact_table(plain, mode, 0);
      cv_build_artifact_table(tagged, mode, CV_ART_TEXT);
      int bad = 0;
      for (int a = 0; a < 16; a++)
         for (int b = 0; b < 16; b++) {
            uint32_t w = tagged[a | (b << 4)], p = plain[a | (b << 4)];
            if ((w & 0x0F0F0F0Fu) != (p & 0x0F0F0F0Fu)) bad++;          // colours kept
            if (p & 0x70707070u) { if (w != p) bad++; continue; }       // artifact pairs as before
            uint32_t want = (ref_text_dot(a, b) ? CV_TEXT_TAG * 0x0101u : 0) | (ref_text_dot(b, a) ? CV_TEXT_TAG * 0x01010000u : 0);
            if ((w & 0x70707070u) != want) bad++;
         }
      CHECK(bad == 0, "text tag table mode %d: %d entries wrong", mode, bad);
   }
   // a captured text / semigraphics screen: every alphanumeric dot is
   // tagged, semigraphics only where green (looks like text), border never
   uint8_t vram[512];
   for (int i = 0; i < 512; i++) vram[i] = (uint8_t) rnd();
   cv_build_artifact_table(sim_artifact_table, 0, CV_ART_TEXT);
   for (int k = 0; k < NGEOMS; k++) {
      const gen_geom_t *g = &geoms[k];
      if (g->hs != 2 || (g->ox & 3)) continue;             // pairs on even dots
      for (int css = 0; css < 2; css++) {
         uint8_t *fb = alloc_fb(g);
         gen_frame(fb, g, GEN_TEXT, vram, css, 0);
         uint8_t *raw = alloc_fb(g);
         memcpy(raw, fb, (size_t) g->pitch * g->fb_h);
         apply_artifact_table(fb, g);
         int wrong = 0;
         for (int y = 0; y < g->fb_h; y++)
            for (int x = 0; x < g->fb_w; x++) {
               int inside = x >= g->ox && x < g->ox + 256 * g->hs && y >= g->oy && y < g->oy + 192 * g->vs;
               uint8_t v = fb[y * g->pitch + x], c = raw[y * g->pitch + x];
               int want = 0;
               if (inside) {
                  int cell = ((y - g->oy) / g->vs / 12) * 32 + (x - g->ox) / g->hs / 8;
                  int alpha = !(vram[cell] & 0x80);
                  want = alpha || c == CV_GREEN;
               }
               if ((v & 15) != c || (v & 0x70) != (want ? CV_TEXT_TAG : 0)) wrong++;
            }
         CHECK(wrong == 0, "text tags geom %d css %d: %d pixels wrong", k, css, wrong);
         free(fb); free(raw);
      }
   }
}

static void test_text_palette(void) {
   cv_state_t s; cv_init(&s);
   uint32_t base16[16];
   for (int i = 0; i < 16; i++) base16[i] = 0x10000u * (uint32_t) i + 0x0100u * (uint32_t) (15 - i) + 7u;
   // effective colour of a frame buffer value
   #define EFF(v) (cpe(&s.regs, (v), base16) < 0 ? (long long) base16[(v) & 15] : cpe(&s.regs, (v), base16))
   static const int text_codes[4] = { CV_GREEN, CV_DKGREEN, CV_LORANGE, CV_DKORANGE };
   static const int swapped[4] = { CV_DKGREEN, CV_GREEN, CV_DKORANGE, CV_LORANGE };
   for (int ex = 0; ex < 2; ex++) {
      s.regs.extras = ex ? 0x00 : 0x40;          // 10 and 9 colours
      for (int i = 0; i < 4; i++)
         CHECK(EFF(CV_TEXT_TAG | text_codes[i]) == EFF(text_codes[i]), "text tag, no text colours: code %d", text_codes[i]);
      s.regs.extras |= 0x20;                      // inverse video
      for (int i = 0; i < 4; i++)
         CHECK(EFF(CV_TEXT_TAG | text_codes[i]) == EFF(swapped[i]) && EFF(text_codes[i]) == EFF(text_codes[i] | 0),
               "inverse: code %d shown as %d", text_codes[i], swapped[i]);
      CHECK(EFF(CV_GREEN) != EFF(CV_TEXT_TAG | CV_GREEN), "inverse: untagged (semigraphics) green unchanged");
   }
   // text palette: dark dots = foreground (slot 48), light = background (49)
   s.regs.extras = 0xC0;
   CHECK(EFF(CV_TEXT_TAG | CV_DKGREEN) == EFF(CV_DKGREEN), "text palette without colours set: SG colours");
   s.regs.pal[CV_PAL_EXTRA + 0] = CV_PAL_ENABLE | (31 << 10) | (31 << 5) | 31;   // white
   s.regs.pal[CV_PAL_EXTRA + 1] = CV_PAL_ENABLE | (0 << 10) | (0 << 5) | 20;     // blue
   long long fg = prgb(&s.regs, CV_PAL_EXTRA + 0), bg = prgb(&s.regs, CV_PAL_EXTRA + 1);
   CHECK(EFF(CV_TEXT_TAG | CV_DKGREEN) == fg && EFF(CV_TEXT_TAG | CV_DKORANGE) == fg, "text palette: foreground");
   CHECK(EFF(CV_TEXT_TAG | CV_GREEN) == bg && EFF(CV_TEXT_TAG | CV_LORANGE) == bg, "text palette: background");
   CHECK(EFF(CV_GREEN) == (long long) base16[CV_GREEN], "text palette: semigraphics green unchanged");
   s.regs.extras = 0xE0;
   CHECK(EFF(CV_TEXT_TAG | CV_DKGREEN) == bg && EFF(CV_TEXT_TAG | CV_GREEN) == fg, "text palette + inverse: swapped");
   s.regs.pal[CV_PAL_EXTRA + 1] = 0;
   CHECK(EFF(CV_TEXT_TAG | CV_DKGREEN) == EFF(CV_GREEN), "text palette, background not set, inverse: SG green");
   #undef EFF
   // registers: bits 5 and 7 change the colour table; $C0 / $E0 can't be
   // seen on the page (a black semigraphics cell): bits 6-4 are kept
   const gen_geom_t *g = &geoms[0];
   uint8_t *fb = alloc_fb(g);
   uint8_t text[512];
   static const struct { uint8_t before, page, want; } ex_cases[] = {
      { 0x40, 0xC0, 0xC0 }, { 0x40, 0x80, 0xC0 }, { 0x00, 0xC0, 0x80 }, { 0x60, 0xE0, 0xE0 },
      { 0x40, 0x60, 0x60 }, { 0x40, 0xC4, 0xC4 }, { 0x60, 0x40, 0x40 }, { 0x40, 0x44, 0x44 },
      { 0x44, 0x41, 0x41 }, { 0x40, 0x41, 0x41 },
   };
   for (unsigned int i = 0; i < sizeof(ex_cases) / sizeof(ex_cases[0]); i++) {
      cv_init(&s);
      make_text_page(text, NULL);
      gen_frame(fb, g, GEN_TEXT, text, 0, 0);
      cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h);
      s.regs.extras = ex_cases[i].before;
      unsigned int gen0 = s.pal_gen;
      uint8_t regs[9] = { 0x00, 0x04, 0x00, 0x00, 0x00, ex_cases[i].page, 0, 0, 0x00 };
      make_text_page(text, regs);
      gen_frame(fb, g, GEN_TEXT, text, 0, 0);
      int f = cv_analyze(&s, fb, g->pitch, g->fb_w, g->fb_h);
      CHECK(f == CV_FRAME_REGISTER && s.regs.extras == ex_cases[i].want, "extras $%02X over $%02X: $%02X (want $%02X)",
            ex_cases[i].page, ex_cases[i].before, s.regs.extras, ex_cases[i].want);
      int colours = ((ex_cases[i].before ^ s.regs.extras) & 0xE7) != 0;   // border, scanlines, inverse, 9/10, text
      CHECK((s.pal_gen != gen0) == colours, "extras $%02X over $%02X: colour table rebuild %d", ex_cases[i].page, ex_cases[i].before, colours);
   }
   free(fb);
}

// capture emulation with the firmware's per-frame table switching
static int sim_tags = 0;          // on: tags_possible, table from text_tags_next
static int sim_tag_flags = 0;     // the table the next capture uses

static int sim_capture_tags(sim_t *m, cv_state_t *s, const gen_geom_t *g, gen_mode_t mode, const uint8_t *vram, int css, int *cap_out) {
   cv_build_artifact_table(sim_artifact_table, 0, sim_tag_flags);
   s->tags_possible = 1;
   s->cap_text_tags = (sim_tag_flags & CV_ART_TEXT) != 0;
   sim_artifacts = 1;
   int f = sim_capture(m, s, g, mode, vram, css, cap_out);
   sim_artifacts = 0;
   sim_tag_flags = s->text_tags_next ? CV_ART_TEXT : 0;
   return f;
}

// tags in a frame buffer's active area: text dots without the tag, other
// dots with it (green semigraphics can be either)
static int tag_errors(const uint8_t *fb, const gen_geom_t *g, int want_tags) {
   int bad = 0;
   for (int y = g->oy; y < g->oy + 192 * g->vs; y++)
      for (int x = g->ox; x < g->ox + 256 * g->hs; x++) {
         uint8_t v = fb[y * g->pitch + x], c = v & 15;
         int text = c == CV_DKGREEN || c == CV_DKORANGE || c == CV_LORANGE;
         if (c == CV_GREEN) continue;
         if ((v & 0x70) != ((text && want_tags) ? CV_TEXT_TAG : 0)) bad++;
      }
   return bad;
}

static int border_tagged(const uint8_t *fb, const gen_geom_t *g) {
   int n = 0;
   for (int y = 0; y < g->fb_h; y++)
      for (int x = 0; x < g->fb_w; x++) {
         int inside = x >= g->ox && x < g->ox + 256 * g->hs && y >= g->oy && y < g->oy + 192 * g->vs;
         if (!inside && (fb[y * g->pitch + x] & 0x70)) n++;
      }
   return n;
}

static void test_text_tag_frames(void) {
   uint8_t text[512], cg[6144], v64[2048];
   for (int i = 0; i < 512; i++) text[i] = (uint8_t) rnd();
   for (int i = 0; i < 6144; i++) cg[i] = (uint8_t) rnd();
   memset(v64, 0x60, sizeof(v64));
   put_text(v64, 0, 0, "W64 in text colours");
   v64[100] = 0x00; v64[101] = 0x9F;
   for (int k = 0; k < NGEOMS; k++) {
      const gen_geom_t *g = &geoms[k];
      if (g->hs != 2 || g->vs != 2 || (g->ox & 3)) continue;
      for (int nb = 1; nb < 4; nb++) {
         for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
         uint8_t *expect = alloc_fb(g);
         sim_t m = { nb, 0, 0, 0 };
         cv_state_t s; cv_init(&s);
         sim_tags = 1; sim_tag_flags = 0;
         for (int i = 0; i < 3; i++) sim_capture_tags(&m, &s, g, GEN_TEXT, text, 0, NULL);
         gen_frame(expect, g, GEN_TEXT, text, 0, 0);
         CHECK(memcmp(sim_buffers[m.displayed], expect, (size_t) g->pitch * g->fb_h) == 0 && s.tag_held == 0,
               "tags geom %d nb %d: plain text without text colours", k, nb);
         // text colours on: the first frame (captured without tags) is held
         s.regs.extras = 0xC0;
         int before = m.displayed;
         int f = sim_capture_tags(&m, &s, g, GEN_TEXT, text, 0, NULL);
         CHECK(f == CV_FRAME_TEXT && m.displayed == before && s.tag_held == 1, "tags geom %d nb %d: untagged text frame held", k, nb);
         sim_capture_tags(&m, &s, g, GEN_TEXT, text, 0, NULL);
         cv_build_artifact_table(sim_artifact_table, 0, CV_ART_TEXT);
         apply_artifact_table(expect, g);
         CHECK(memcmp(sim_buffers[m.displayed], expect, (size_t) g->pitch * g->fb_h) == 0, "tags geom %d nb %d: tagged text shown", k, nb);
         // to graphics: one held frame, then the plain picture
         for (int css = 0; css < 2; css++) {
            before = m.displayed;
            unsigned int held = s.tag_held;
            sim_capture_tags(&m, &s, g, GEN_CG3, cg, css, NULL);
            CHECK(m.displayed == before && s.tag_held == held + 1, "tags geom %d nb %d css %d: graphics captured with text tags held", k, nb, css);
            sim_capture_tags(&m, &s, g, GEN_CG3, cg, css, NULL);
            gen_frame(expect, g, GEN_CG3, cg, css, 0);
            CHECK(memcmp(sim_buffers[m.displayed], expect, (size_t) g->pitch * g->fb_h) == 0, "tags geom %d nb %d css %d: plain graphics", k, nb, css);
            sim_capture_tags(&m, &s, g, GEN_TEXT, text, css, NULL);       // held
            sim_capture_tags(&m, &s, g, GEN_TEXT, text, css, NULL);
            CHECK(tag_errors(sim_buffers[m.displayed], g, 1) == 0 && border_tagged(sim_buffers[m.displayed], g) == 0,
                  "tags geom %d nb %d css %d: text again", k, nb, css);
         }
         // 32 column lower case keeps the tags
         s.lc_on_mode = 1;
         for (int i = 0; i < 3; i++) sim_capture_tags(&m, &s, g, GEN_TEXT, text, 0, NULL);
         CHECK(tag_errors(sim_buffers[m.displayed], g, 1) == 0, "tags geom %d nb %d: lower case", k, nb);
         s.lc_on_mode = -1;
         // W64: not built over a frame captured with text tags (its border
         // stays), its text drawn with the tag, semigraphics without
         s.regs.enhanced = 0x02;
         for (int i = 0; i < 4; i++) sim_capture_tags(&m, &s, g, GEN_CG2, v64, 0, NULL);
         const uint8_t *fb = sim_buffers[m.displayed];
         CHECK(s.w64_on && compare_w64(fb, g, v64, 0, 0) == 0 && border_tagged(fb, g) == 0, "tags geom %d nb %d: W64", k, nb);
         CHECK(tag_errors(fb, g, 1) == 0 && (fb[g->oy * g->pitch + g->ox] & 0x70) == CV_TEXT_TAG, "tags geom %d nb %d: W64 tags", k, nb);
         // VG6 the same
         s.regs.enhanced = 0x01;
         for (int i = 0; i < 2; i++) sim_capture_tags(&m, &s, g, GEN_TEXT, text, 0, NULL);
         for (int i = 0; i < 4; i++) sim_capture_tags(&m, &s, g, GEN_CG6, cg, 1, NULL);
         CHECK(border_tagged(sim_buffers[m.displayed], g) == 0 && (sim_buffers[m.displayed][g->oy * g->pitch + g->ox] & 0x70) == CV_VG6_CSS1,
               "tags geom %d nb %d: VG6", k, nb);
         s.regs.enhanced = 0;
         // text colours off again: the tagged frame is held, then plain
         for (int i = 0; i < 2; i++) sim_capture_tags(&m, &s, g, GEN_TEXT, text, 0, NULL);
         s.regs.extras = 0x40;
         before = m.displayed;
         sim_capture_tags(&m, &s, g, GEN_TEXT, text, 0, NULL);
         CHECK(m.displayed == before, "tags geom %d nb %d: tagged frame held after text colours off", k, nb);
         sim_capture_tags(&m, &s, g, GEN_TEXT, text, 0, NULL);
         gen_frame(expect, g, GEN_TEXT, text, 0, 0);
         CHECK(memcmp(sim_buffers[m.displayed], expect, (size_t) g->pitch * g->fb_h) == 0, "tags geom %d nb %d: plain text again", k, nb);
         // a program switching between text and graphics every frame is
         // never held more than CV_TAG_HOLDS frames in a row
         s.regs.extras = 0x20;
         int run = 0, maxrun = 0;
         for (int i = 0; i < 12; i++) {
            before = m.displayed;
            int cap;
            sim_capture_tags(&m, &s, g, (i & 1) ? GEN_CG3 : GEN_TEXT, (i & 1) ? cg : text, 0, &cap);
            run = m.displayed == before ? run + 1 : 0;
            if (run > maxrun) maxrun = run;
         }
         CHECK(maxrun <= CV_TAG_HOLDS, "tags geom %d nb %d: held %d frames in a row", k, nb, maxrun);
         sim_tags = 0;
         for (int i = 0; i < 4; i++) free(sim_buffers[i]);
         free(expect);
      }
   }
   // no pair table in the capture (tags_possible = 0): nothing is held
   const gen_geom_t *g = &geoms[0];
   for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
   sim_t m = { 3, 0, 0, 0 };
   cv_state_t s; cv_init(&s);
   s.regs.extras = 0xE0;
   for (int i = 0; i < 6; i++) sim_capture(&m, &s, g, (i & 2) ? GEN_CG3 : GEN_TEXT, (i & 2) ? cg : text, 0, NULL);
   CHECK(s.tag_held == 0 && m.displayed == m.last, "no pair table: no frames held");
   for (int i = 0; i < 4; i++) free(sim_buffers[i]);
}


// CoCoVGA scanlines use RGBtoHDMI's scan lines (bit 7 on every second frame
// buffer line): pictures the emulation redraws in 32 columns and VG6 keep them
static void scan_lines(uint8_t *fb, const gen_geom_t *g) {
   for (int y = 1; y < g->fb_h; y += 2)
      for (int x = 0; x < g->fb_w; x++) fb[y * g->pitch + x] |= 0x80;
}
static int scan_line_errors(const uint8_t *fb, const gen_geom_t *g) {
   int bad = 0;
   for (int y = 0; y < g->fb_h; y++)
      for (int x = 0; x < g->fb_w; x++)
         if (((fb[y * g->pitch + x] & 0x80) != 0) != (y & 1)) bad++;
   return bad;
}

static void test_scanlines(void) {
   uint8_t text[512], cg[6144];
   for (int i = 0; i < 512; i++) text[i] = (uint8_t) rnd();
   for (int i = 0; i < 6144; i++) cg[i] = (uint8_t) rnd();
   for (int k = 0; k < NGEOMS; k++) {
      const gen_geom_t *g = &geoms[k];
      if (g->vs < 2) continue;                         // scan lines need double height
      for (int i = 0; i < 4; i++) sim_buffers[i] = alloc_fb(g);
      sim_t m = { 3, 0, 0, 0 };
      cv_state_t s; cv_init(&s);
      sim_tamper = scan_lines;
      s.lc_on_mode = 1;
      for (int i = 0; i < 4; i++) sim_capture(&m, &s, g, GEN_TEXT, text, 0, NULL);
      CHECK(scan_line_errors(sim_buffers[m.displayed], g) == 0, "scan lines geom %d: 32 column lower case", k);
      s.lc_on_mode = -1;
      s.regs.enhanced = 0x01;
      for (int i = 0; i < 4; i++) sim_capture(&m, &s, g, GEN_CG6, cg, 1, NULL);
      const uint8_t *fb = sim_buffers[m.displayed];
      CHECK(scan_line_errors(fb, g) == 0 && (fb[g->oy * g->pitch + g->ox] & 0x70) == CV_VG6_CSS1, "scan lines geom %d: VG6", k);
      sim_tamper = NULL;
      for (int i = 0; i < 4; i++) free(sim_buffers[i]);
   }
}


// ---------------------------------------------------------------------
// Border modes black / custom: a C model of the border capture routine
// (capture_line_c64lc6847_border_double_8bpp) on generated frames
// ---------------------------------------------------------------------
static uint32_t border_table[256];

// the routine, line by line: r5 counts down from nlines; on the picture's
// lines r11 counts pairs and the tables swap at 0 (then 128 pairs again)
static void apply_border_capture(uint8_t *fb, const gen_geom_t *g, int x0, int y0, int w, int nlines, int dh,
                                 const uint32_t *params, const uint32_t *pic) {
   for (int i = 0; i < nlines; i++) {
      int r5 = nlines - i;
      int r11 = (r5 <= (int) params[0] && r5 >= (int) params[1]) ? (int) params[2] : 0x10000;
      const uint32_t *t14 = border_table, *t12 = pic;
      for (int k = 0; k < dh; k++) {
         uint8_t *line = fb + (y0 + i * dh + k) * g->pitch + x0;
         int c11 = r11;
         const uint32_t *a = t14, *b = t12;
         for (int p = 0; 4 * p + 3 < w; p++) {
            if (--c11 == 0) { const uint32_t *t = a; a = b; b = t; c11 = 128; }
            uint32_t word = a[(line[4 * p] & 15) | ((line[4 * p + 2] & 15) << 4)];
            for (int j = 0; j < 4; j++) line[4 * p + j] = (uint8_t) (word >> (8 * j));
         }
      }
   }
}

static int is_border_value(uint8_t v) {
   v &= 0x7F;
   return v == CV_BORDER_BLACK || v == CV_BORDER_GREEN || v == CV_BORDER_BUFF;
}

static void test_border_capture(void) {
   cv_build_border_table(border_table);
   // the table: only one-colour pairs of the border colours change, colours kept
   int bad = 0;
   for (int a = 0; a < 16; a++)
      for (int b = 0; b < 16; b++) {
         uint32_t w = border_table[a | (b << 4)], plain = a * 0x0101u | b * 0x01010000u;
         int border = a == b && (a == CV_BLACK || a == CV_GREEN || a == CV_BUFF);
         if ((w & 0x0F0F0F0Fu) != plain || (border ? !is_border_value((uint8_t) w) : w != plain)) bad++;
      }
   CHECK(bad == 0, "border table: %d entries wrong", bad);
   // the border values are not used by anything else
   uint32_t art[256];
   for (int mode = 0; mode < 3; mode++)
      for (int f = 0; f < 4; f++) {
         cv_build_artifact_table(art, mode, f);
         for (int i = 0; i < 256; i++)
            for (int j = 0; j < 4; j++) if (is_border_value((uint8_t) (art[i] >> (8 * j)))) bad++;
      }
   CHECK(bad == 0, "border values also made by the pair tables (%d)", bad);
   // colours: border mode 6847 / T1 the dot's colour, black, custom slot 50
   cv_state_t s; cv_init(&s);
   uint32_t base16[16];
   for (int i = 0; i < 16; i++) base16[i] = 0x10000u * (uint32_t) i + 7u;
   static const uint8_t bv[3] = { CV_BORDER_BLACK, CV_BORDER_GREEN, CV_BORDER_BUFF };
   for (int mode = 0; mode < 4; mode++) {
      s.regs.extras = (uint8_t) (0x40 | mode);
      s.regs.pal[CV_PAL_EXTRA + 2] = CV_PAL_ENABLE | (31 << 10);           // red
      for (int i = 0; i < 3; i++) {
         long long want = mode == 2 ? (long long) base16[CV_BLACK] : mode == 3 ? prgb(&s.regs, CV_PAL_EXTRA + 2)
                                                                           : (long long) base16[bv[i] & 15];
         CHECK(cpe(&s.regs, bv[i], base16) == want && cpe(&s.regs, bv[i] | 0x80, base16) == want,
               "border mode %d: colour of %02x", mode, bv[i]);
      }
      s.regs.pal[CV_PAL_EXTRA + 2] = 0;
      CHECK(mode != 3 || cpe(&s.regs, CV_BORDER_GREEN, base16) == (long long) base16[CV_GREEN], "custom border without slot 50: its colour");
   }
   CHECK(!cv_border_mode_on(&s.regs) || (s.regs.extras & 2), "border mode on");
   // capture: every mode's picture as with the plain table (decoders and
   // all), the border tagged; with the capture an odd number of dots off,
   // only the picture's last dot column may be the border's
   static const struct { gen_geom_t g; int x0, y0, w, h; } caps[] = {
      { { 640, 480, 640, 64, 48, 2, 2 }, 0, 0, 640, 480 },
      { { 640, 480, 640, 62, 46, 2, 2 }, 0, 0, 640, 480 },
      { { 800, 560, 800, 64 + 80, 48 + 40, 2, 2 }, 80, 40, 640, 480 },
      { { 720, 480, 736, 104, 48, 2, 2 }, 0, 0, 720, 480 },
   };
   uint8_t vram[6144];
   for (int i = 0; i < 6144; i++) vram[i] = (uint8_t) rnd();
   uint32_t pic[256];
   cv_build_artifact_table(pic, 1, 0);
   for (unsigned int c = 0; c < sizeof(caps) / sizeof(caps[0]); c++) {
      const gen_geom_t *g = &caps[c].g;
      uint8_t *fb = alloc_fb(g), *plain = alloc_fb(g);
      cv_state_t st; cv_init(&st);
      gen_frame(fb, g, GEN_TEXT, vram, 0, 0);
      CHECK(cv_analyze(&st, fb, g->pitch, g->fb_w, g->fb_h) == CV_FRAME_TEXT, "border cap %u: calibrated", c);
      uint32_t params[4];
      CHECK(cv_border_params(&st, caps[c].x0, caps[c].y0, caps[c].h / 2, 2, params), "border cap %u: params", c);
      static const gen_mode_t modes[] = { GEN_TEXT, GEN_CG2, GEN_CG3, GEN_CG6, GEN_RG6 };
      for (int mi = 0; mi < 5; mi++)
         for (int css = 0; css < 2; css++) {
            gen_frame(fb, g, modes[mi], vram, css, 0);
            for (int y = 0; y < g->fb_h; y++)                       // outside the capture: nothing
               for (int x = 0; x < g->fb_w; x++)
                  if (y < caps[c].y0 || y >= caps[c].y0 + caps[c].h || x < caps[c].x0 || x >= caps[c].x0 + caps[c].w)
                     fb[y * g->pitch + x] = 0;
            memcpy(plain, fb, (size_t) g->pitch * g->fb_h);
            apply_border_capture(fb, g, caps[c].x0, caps[c].y0, caps[c].w, caps[c].h / 2, 2, params, pic);
            for (int y = caps[c].y0; y < caps[c].y0 + caps[c].h; y++) {      // the plain capture
               uint8_t *l = plain + y * g->pitch + caps[c].x0;
               for (int x = 0; x + 3 < caps[c].w; x += 4) {
                  uint32_t w = pic[(l[x] & 15) | ((l[x + 2] & 15) << 4)];
                  for (int j = 0; j < 4; j++) l[x + j] = (uint8_t) (w >> (8 * j));
               }
            }
            int wrong_in = 0, untagged_out = 0;
            for (int y = 0; y < g->fb_h; y++)
               for (int x = 0; x < g->fb_w; x++) {
                  uint8_t v = fb[y * g->pitch + x], p = plain[y * g->pitch + x];
                  int inside = x >= g->ox && x < g->ox + 512 && y >= g->oy && y < g->oy + 384;
                  int captured = y >= caps[c].y0 && y < caps[c].y0 + caps[c].h && x >= caps[c].x0 && x + 3 < caps[c].x0 + (caps[c].w & ~3) + 3;
                  if ((v & 15) != (p & 15)) wrong_in++;                       // colours always kept
                  else if (inside && v != p && !((g->ox - caps[c].x0) & 2 && x >= g->ox + 510)) wrong_in++;
                  else if (!inside && captured && !is_border_value(v)) {
                     int c4 = p & 15;
                     int pair = (x - caps[c].x0) & ~3;
                     uint8_t q0 = plain[y * g->pitch + caps[c].x0 + pair] & 15, q1 = plain[y * g->pitch + caps[c].x0 + pair + 2] & 15;
                     int straddle = (x < g->ox && x + 4 > g->ox) || (x >= g->ox + 512 && x - 4 < g->ox + 512);
                     if ((c4 == CV_BLACK || c4 == CV_GREEN || c4 == CV_BUFF) && q0 == q1 && !(straddle && (y >= g->oy && y < g->oy + 384)))
                        untagged_out++;
                  }
               }
            CHECK(wrong_in == 0 && untagged_out == 0, "border cap %u mode %d css %d: %d picture dots changed, %d border dots not tagged",
                  c, modes[mi], css, wrong_in, untagged_out);
            // the decoders see the same picture
            cv_state_t a, b; cv_init(&a); cv_init(&b);
            a.regs.enhanced = b.regs.enhanced = (modes[mi] == GEN_CG2) ? 0x02 : 0x01;
            gen_frame(plain, g, GEN_TEXT, vram, 0, 0);
            cv_analyze(&a, plain, g->pitch, g->fb_w, g->fb_h);
            cv_analyze(&b, plain, g->pitch, g->fb_w, g->fb_h);
            memcpy(plain, fb, (size_t) g->pitch * g->fb_h);
            for (int y = 0; y < g->fb_h * g->pitch; y++) plain[y] &= 15;
            int fa = cv_analyze(&a, fb, g->pitch, g->fb_w, g->fb_h), fbt = cv_analyze(&b, plain, g->pitch, g->fb_w, g->fb_h);
            CHECK(fa == fbt && a.cg_unusable == b.cg_unusable, "border cap %u mode %d css %d: frame type %d, untagged %d", c, modes[mi], css, fa, fbt);
         }
      free(fb); free(plain);
   }
   // not without double width or a calibration
   cv_state_t st; cv_init(&st);
   uint32_t params[4];
   CHECK(!cv_border_params(&st, 0, 0, 240, 2, params), "border params: not calibrated");
   const gen_geom_t *g1 = &geoms[4];                                         // no doubling
   uint8_t *fb = alloc_fb(g1);
   gen_frame(fb, g1, GEN_TEXT, vram, 0, 0);
   cv_analyze(&st, fb, g1->pitch, g1->fb_w, g1->fb_h);
   CHECK(st.cal_valid && !cv_border_params(&st, 0, 0, 240, 1, params), "border params: single width");
   free(fb);
}

int main(void) {
   test_fonts();
   test_calibration();
   test_register_page();
   test_register_palette();
   test_palette_mapping();
   test_palette_spread();
   test_no_false_registers();
   test_cg2_decode();
   test_end_to_end();
   test_entry_holds();
   test_lowercase_32();
   test_lowercase_font_select();
   test_lowercase_ef_forms();
   test_lowercase_forced();
   test_lowercase_fast();
   test_lowercase_dot_errors();
   test_ef_tiebreak();
   test_lowercase_spread();
   test_vg6();
   test_artifacts();
   test_artifact_flags();
   test_text_tag_table();
   test_text_palette();
   test_text_tag_frames();
   test_scanlines();
   test_border_capture();
   test_budget();
   test_sliced_decode();
   make_previews();
   make_artifact_preview();
   test_bench();
   printf("%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
