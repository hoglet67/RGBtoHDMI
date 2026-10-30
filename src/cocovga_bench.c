/*
 * cocovga_bench.c - time the CoCoVGA per-frame work, see cocovga_bench.h
 */
#include <string.h>
#include "cocovga.h"
#include "cocovga_bench.h"
#include "cocovga_tools/gen6847.h"

#define BENCH_W      640
#define BENCH_H      480
#define BENCH_FRAMES  24      // timed frames per row
#define BENCH_ENTER    8      // timed frames for the "enter W64" rows

// MC6847 LumaCode profile geometry (double width and height), and the same
// shifted off the 4 pixel grid (slower byte-wise drawing in cv_render)
static const gen_geom_t geom_aligned   = { BENCH_W, BENCH_H, BENCH_W, 64, 48, 2, 2 };
static const gen_geom_t geom_unaligned = { BENCH_W, BENCH_H, BENCH_W, 62, 46, 2, 2 };

static const uint8_t regs_nitros9[9]  = { 0x00, 0x81, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x02 };
#ifdef CV_HAVE_LOWERCASE_32
static const uint8_t regs_lower_on[9] = { 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 };
#endif
static const uint8_t regs_t1_only[9]  = { 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 };

typedef struct {
   uint8_t *buf[4];
   const gen_geom_t *g;
   cv_state_t s;
   int last, displayed, redirect;   // RGBtoHDMI buffer selection, "Num Buffers" = 0
   cv_usec_fn usec;
} bench_t;

static bench_t b;
static uint8_t vram[6144];
static uint8_t text[512];
static unsigned int seed = 1;

static unsigned int rnd(void) {
   seed = seed * 1103515245u + 12345u;
   return seed >> 16;
}

static uint8_t *bench_buffer(int i) { return b.buf[i & 3]; }

typedef enum { FILL_GEN, FILL_BLACK } fill_t;

// Capture one frame the way rgb_to_fb.S does, then run the per-frame
// controller on it. Only the controller is timed.
static cv_frame_t step(gen_mode_t mode, const uint8_t *data, int css, fill_t fill, unsigned int *us) {
   int cap;
   if (b.redirect) cap = b.redirect - 1;
   else cap = 0;                                   // Num Buffers = 0
   if (fill == FILL_BLACK) memset(b.buf[cap], CV_BLACK, (size_t)b.g->pitch * b.g->fb_h);
   else gen_frame(b.buf[cap], b.g, mode, data, css, 0);

   int swap, flip;
   unsigned int t0 = b.usec();
#ifdef CV_HAVE_LOWERCASE_32
   int capture_next, inhibit;
   cv_frame_t f = cv_frame(&b.s, cap, b.displayed, 4, bench_buffer, b.g->pitch, b.g->fb_w, b.g->fb_h,
                           &capture_next, &inhibit, &swap);
   unsigned int t1 = b.usec();
   b.redirect = capture_next + 1;
   flip = !inhibit;
#else
   int redirect;
   cv_frame_t f = cv_frame(&b.s, cap, b.displayed, 4, bench_buffer, b.g->pitch, b.g->fb_w, b.g->fb_h,
                           &redirect, &swap);
   unsigned int t1 = b.usec();
   b.redirect = redirect;
   flip = !redirect;
#endif
   if (swap >= 0) b.displayed = swap;
   if (flip) b.displayed = cap;
   b.last = cap;
   if (us) *us = t1 - t0;
   return f;
}

static unsigned int bench_budget_us, bench_decode_budget_us;

static void start(const gen_geom_t *g) {
   b.g = g;
   cv_init(&b.s);
   if (bench_budget_us) {                // the firmware's work limits
      b.s.usec = b.usec;
      b.s.budget_us = bench_budget_us;
      b.s.decode_budget_us = bench_decode_budget_us;
   }
   b.last = b.displayed = b.redirect = 0;
}

static void page(const uint8_t *regs, int inverse_cells) {
   // a BASIC-like screen: a few lines of normal text, the rest blank
   memset(text, 0x60, sizeof(text));
   for (int i = 0; i < 6 * 32; i++) text[i] = 0x40 | (uint8_t)((i * 7) & 0x3F);
   for (int i = 0; i < inverse_cells && i < 512; i++) text[511 - i] = (uint8_t)(i & 0x1F);
   if (regs) memcpy(text, regs, 9);
}

static void w64_screen(void) {
   for (int i = 0; i < 2048; i++) vram[i] = (i & 63) < 40 ? (uint8_t)(0x40 | (i % 63)) : 0x60;
}

static cv_bench_row_t *row(cv_bench_result_t *res, const char *name, int group) {
   cv_bench_row_t *r = &res->row[res->rows++];
   memset(r, 0, sizeof(*r));
   r->name = name;
   r->group = group;
   r->min_us = ~0u;
   r->ok = 1;
   return r;
}

static void add(cv_bench_row_t *r, unsigned int us, int ok) {
   unsigned long long total = (unsigned long long) r->avg_us * (unsigned) r->n + us;
   r->n++;
   r->avg_us = (unsigned int) (total / (unsigned) r->n);
   if (us < r->min_us) r->min_us = us;
   if (us > r->max_us) r->max_us = us;
   if (!ok) r->ok = 0;
}

// 32 column text screen, timed
static void bench_text(cv_bench_result_t *res, const char *name, int group,
                       const uint8_t *regs, int inverse_cells) {
   cv_bench_row_t *r = row(res, name, group);
   start(&geom_aligned);
   page(NULL, 0);
   for (int i = 0; i < 4; i++) step(GEN_TEXT, text, 0, FILL_GEN, NULL);
   if (regs) {
      page(regs, 0);
      step(GEN_TEXT, text, 0, FILL_GEN, NULL);
   }
   page(NULL, inverse_cells);
   for (int i = 0; i < 3; i++) step(GEN_TEXT, text, 0, FILL_GEN, NULL);
   for (int i = 0; i < BENCH_FRAMES; i++) {
      unsigned int us;
      cv_frame_t f = step(GEN_TEXT, text, 0, FILL_GEN, &us);
      add(r, us, f == CV_FRAME_TEXT);
   }
}

// NitrOS-9 start-up: BASIC screen, register page, first CG2 frame
static void w64_enter(void) {
   page(NULL, 0);
   for (int i = 0; i < 4; i++) step(GEN_TEXT, text, 0, FILL_GEN, NULL);
   page(regs_nitros9, 0);
   step(GEN_TEXT, text, 0, FILL_GEN, NULL);
   w64_screen();
   step(GEN_CG2, vram, 0, FILL_GEN, NULL);
}

static void bench_w64_enter(cv_bench_result_t *res, const char *name, const gen_geom_t *g) {
   cv_bench_row_t *r = row(res, name, 0);
   start(g);
   w64_enter();
   for (int i = 0; i < BENCH_ENTER; i++) {
      cv_stop(&b.s);                    // enter W64 again: full redraw
      vram[i] ^= 0x01;
      // time every frame until the 64 column screen is on screen
      int n = 0, ok = 1;
      do {
         unsigned int us;
         cv_frame_t f = step(GEN_CG2, vram, 0, FILL_GEN, &us);
         add(r, us, f == CV_FRAME_CG2);
         if (f != CV_FRAME_CG2) ok = 0;
      } while (!b.s.w64_on && ++n < 200);
      if (!b.s.w64_on || !ok) r->ok = 0;
      if (n + 1 > r->frames) r->frames = n + 1;
   }
}

enum { W64_IDLE, W64_TYPE, W64_SCROLL };

static void bench_w64(cv_bench_result_t *res, const char *name, int what) {
   cv_bench_row_t *r = row(res, name, 0);
   start(&geom_aligned);
   w64_enter();
   // with the work limit the 64 column screen takes several frames to build
   // (9 on a Pi Zero W): time only frames with it on screen
   for (int n = 0; !b.s.w64_on && n < 200; n++) step(GEN_CG2, vram, 0, FILL_GEN, NULL);
   for (int i = 0; i < 3; i++) step(GEN_CG2, vram, 0, FILL_GEN, NULL);
   for (int i = 0; i < BENCH_FRAMES; i++) {
      if (what == W64_TYPE) {
         vram[20 * 64 + i] = (uint8_t)(0x41 + i);
      } else if (what == W64_SCROLL) {
         memmove(vram, vram + 64, 2048 - 64);
         for (int c = 0; c < 64; c++) vram[2048 - 64 + c] = (uint8_t)(0x40 | ((c + i) & 0x3F));
      }
      unsigned int us;
      cv_frame_t f = step(GEN_CG2, vram, 0, FILL_GEN, &us);
      add(r, us, f == CV_FRAME_CG2 && b.s.w64_on);
   }
}

// W64 was on, then a graphics program (or a blank screen) takes over
static void bench_other(cv_bench_result_t *res, const char *name, int black) {
   cv_bench_row_t *r = row(res, name, 0);
   start(&geom_aligned);
   w64_enter();
   for (int i = 0; i < 6144; i++) vram[i] = (uint8_t) rnd();
   for (int i = 0; i < 4; i++) step(GEN_RG6, vram, 0, black ? FILL_BLACK : FILL_GEN, NULL);
   for (int i = 0; i < BENCH_FRAMES; i++) {
      vram[rnd() % 6144] = (uint8_t) rnd();
      unsigned int us;
      cv_frame_t f = step(GEN_RG6, vram, 0, black ? FILL_BLACK : FILL_GEN, &us);
      add(r, us, f == CV_FRAME_OTHER && !b.s.w64_on);
   }
}

// palettes: a page that also loads all three palette groups (52 slots)
static void bench_register(cv_bench_result_t *res, int palettes) {
   static const uint8_t regs_palettes[9] = { 0x00, 0x38, 0, 0, 0, 0, 0, 0, 0 };
   cv_bench_row_t *r = row(res, palettes ? "Reg page palettes" : "Register page", 0);
   start(&geom_aligned);
   page(NULL, 0);
   for (int i = 0; i < 4; i++) step(GEN_TEXT, text, 0, FILL_GEN, NULL);
   for (int i = 0; i < BENCH_FRAMES; i++) {
      unsigned int us;
      page(palettes ? regs_palettes : regs_nitros9, 0);
      for (int k = 0; palettes && k < 104; k++) text[32 + k] = (uint8_t) ((k & 1) ? 0x01 + k : 0x81 + (k & 7));
      cv_frame_t f = step(GEN_TEXT, text, 0, FILL_GEN, &us);
      add(r, us, f == CV_FRAME_REGISTER);
      page(NULL, 0);
      // palette words are decoded within the work limit over the next
      // frames: time those too
      int n = 1;
      while (b.s.pal_pending && n < 32) {
         step(GEN_TEXT, text, 0, FILL_GEN, &us);
         add(r, us, 1);
         n++;
      }
      if (palettes && n > r->frames) r->frames = n;
      if (b.s.pal_pending) r->ok = 0;
      step(GEN_TEXT, text, 0, FILL_GEN, NULL);
   }
}

void cv_bench_run(uint8_t *mem, cv_usec_fn usec, unsigned int budget_us, unsigned int decode_budget_us,
                  cv_bench_result_t *res) {
   memset(res, 0, sizeof(*res));
   bench_budget_us = budget_us;
   bench_decode_budget_us = decode_budget_us;
   for (int i = 0; i < 4; i++) b.buf[i] = mem + i * BENCH_W * BENCH_H;
   b.usec = usec;
   seed = 1;

   // lower case off
   bench_text(res, "32col text", 0, NULL, 0);
   bench_text(res, "32col 512 inverse", 0, regs_t1_only, 512);
   bench_register(res, 0);
   bench_register(res, 1);
   bench_w64_enter(res, "W64 enter (full)", &geom_aligned);
   bench_w64_enter(res, "W64 enter unaligned", &geom_unaligned);
   bench_w64(res, "W64 idle", W64_IDLE);
   bench_w64(res, "W64 typing", W64_TYPE);
   bench_w64(res, "W64 scroll", W64_SCROLL);
   bench_other(res, "Graphics (PMODE4)", 0);
   bench_other(res, "Black screen", 1);

#ifdef CV_HAVE_LOWERCASE_32
   // lower case on (font register "force lower case")
   res->lowercase_available = 1;
   bench_text(res, "32col no inverse", 1, regs_lower_on, 0);
   bench_text(res, "32col 32 lower", 1, regs_lower_on, 32);
   bench_text(res, "32col 512 lower", 1, regs_lower_on, 512);
#endif
}
