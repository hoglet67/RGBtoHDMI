/*
 * runner.c - end-to-end check of the CoCoVGA emulation (stage 1 or stage 2)
 * on frames from XRoar.
 *
 * Inputs: frames.fb (640x480 LumaCode frames, from xroar2fb.py) and
 * truth.bin (what the 6847 displayed in each frame, decoded independently
 * with XRoar's font by truth.py).
 *
 * Each frame is "captured" into RGBtoHDMI's 4 frame buffers exactly as
 * rgb_to_fb.S would (Num Buffers setting, CoCoVGA redirect / no-flip,
 * optional NTSC artifact table in stage 2), then the branch's cv_frame()
 * runs, then the picture on screen is checked against what it must be:
 *
 *   CG2 + 64-column mode on  -> the 64x32 screen, every pixel, drawn from
 *                               the truth bytes with XRoar's glyph bitmaps
 *   text (not register page) -> the captured frame; in stage 2 with "force
 *                               lower case", inverse $00-$1F cells replaced
 *                               by XRoar's 6847T1 lower case glyphs
 *   register page            -> not on screen (previous picture kept),
 *                               except Num Buffers = 0 outside W64
 *   PMODE 4 / anything else  -> the captured frame
 *
 *   runner frames.fb truth.bin [-nb N] [-art MODE] [-ox X -oy Y] [-ppm DIR -at f1,f2,..]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "cocovga.h"

extern const uint8_t font_6847[768];
extern const uint8_t font_6847t1[1536];

#define W 640
#define H 480
#define FSZ (W * H)
#define TSZ (2 + 6144)

static uint8_t *buf[4];
static uint8_t *bufp(int i) { return buf[i & 3]; }

// ---- reference glyphs (XRoar) ------------------------------------------
// shape (strokes = 1) of a normal 6847 / 6847T1 character, code $00-$3F
static uint8_t t1row(int code, int y) {          // code: 0-63 = '@'..'?'
   int idx = code < 32 ? 64 + code : code;       // XRoar order: lower, punct, upper
   uint8_t r = font_6847t1[idx * 12 + y];
   return code < 32 ? r : (uint8_t) ~r;          // XRoar stores $20-$3F inverted
}
// lower case (XRoar's 6847T1 table): next to T1 capitals as it is, next to
// the 6847's capitals (rows 3-9) moved down 2 rows to the same baseline
static uint8_t t1lower(int code, int y, int t1_capitals) {
   if (!t1_capitals) { if (y < 2) return 0; y -= 2; }
   return font_6847t1[(code & 31) * 12 + y];
}

typedef struct { int font, extras, enhanced, artifact; } regs_t;

static uint8_t w64_pixel(uint8_t b, int x, int y, const regs_t *r, int css) {
   static const uint8_t sgc[8] = { 12, 13, 6, 2, 7, 10, 9, 8 };
   if (b & 0x80) {
      int q = (y < 6 ? 0 : 2) + (x < 4 ? 0 : 1);
      return ((b >> (3 - q)) & 1) ? sgc[(b >> 4) & 7] : CV_BLACK;
   }
   int code = b & 0x3F, t1 = r->font & 1, lower = r->font & 2;
   uint8_t row;
   int inverse;
   // font 0: CoCoVGA draws its own 6847 font (taken from CCVEDIT.BIN), which
   // differs from the 6847 ROM (XRoar) in one row of E, F and 3
   if (b & 0x40) { row = t1 ? t1row(code, y) : mc6847_font[code][y]; inverse = 0; }
   else if (lower && code < 32) { row = t1lower(code, y, t1); inverse = 0; }
   else { row = t1 ? t1row(code, y) : mc6847_font[code][y]; inverse = 1; }
   if (r->extras & 0x20) inverse ^= 1;
   int stroke = (row >> (7 - x)) & 1;
   uint8_t light = css ? CV_LORANGE : CV_GREEN, dark = css ? CV_DKORANGE : CV_DKGREEN;
   return (stroke ^ inverse) ? dark : light;
}

// ---- statistics -----------------------------------------------------------
enum { K_W64, K_TEXT, K_LOWER, K_REG, K_REG_SHOWN, K_RG6, K_RIDE, K_OTHER, NK };
static const char *kname[NK] = { "64-column screen", "32-column text", "32-column lower case",
                                 "register page hidden", "register page shown (Num Buffers 0)",
                                 "PMODE 4", "ride-over frame", "unclassified" };
static int nok[NK], nbad[NK], first_bad[NK] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static long first_bad_px[NK];

static void result(int k, int frame, long badpx) {
   if (badpx == 0) nok[k]++;
   else {
      nbad[k]++;
      if (getenv("RUNNER_FAILS")) printf("fail %s frame %d (%ld)\n", kname[k], frame, badpx);
      if (first_bad[k] < 0) { first_bad[k] = frame; first_bad_px[k] = badpx; }
   }
}

static void write_ppm(const char *dir, const char *tag, int frame, const uint8_t *fb) {
   static const uint8_t pal[16][3] = {
      {0,0,0},{0,0,0},{0x9A,0x32,0x36},{0xFF,0xB7,0},{0x6B,0x27,0},{0x0D,0x3A,0x06},{0x4C,0x3A,0xB4},{0xBF,0xC8,0xAD},
      {0xD4,0x7F,0},{0xC8,0x4E,0xF0},{0x41,0xAF,0x71},{0,0,0},{0x30,0xD2,0},{0xC1,0xE5,0},{0,0,0},{0,0,0} };
   char name[512];
   snprintf(name, sizeof(name), "%s/%s_%04d.ppm", dir, tag, frame);
   FILE *f = fopen(name, "wb");
   if (!f) return;
   fprintf(f, "P6\n%d %d\n255\n", W, H);
   for (int i = 0; i < FSZ; i++) {
      int v = fb[i];
      int art = -1;
#ifdef CV_HAVE_LOWERCASE_32
      art = cv_artifact_palette_entry(v & 0x7F);
#endif
      if (art >= 0) { uint8_t c[3] = { (uint8_t) art, (uint8_t) (art >> 8), (uint8_t) (art >> 16) }; fwrite(c, 1, 3, f); }
      else fwrite(pal[v & 15], 1, 3, f);
   }
   fclose(f);
}

// RUNNER_SLOW=n: a clock that advances n us per reading, with the
// firmware's 250 us work limit (spreads work such as palette decoding over
// frames)
static unsigned int slow_clock, slow_step;
static unsigned int slow_usec(void) { return slow_clock += slow_step; }

int main(int argc, char **argv) {
   if (argc < 3) { fprintf(stderr, "usage: runner frames.fb truth.bin [opts]\n"); return 2; }
   int nb = 0, art = 0, ox = 64, oy = 48, w64from = -1, maxchars = 0, decoderows = 0;
   const char *ppmdir = NULL, *tag = "run";
   char *at = NULL;
   for (int i = 3; i < argc; i++) {
      if (!strcmp(argv[i], "-nb")) nb = atoi(argv[++i]);
      else if (!strcmp(argv[i], "-art")) art = atoi(argv[++i]);
      else if (!strcmp(argv[i], "-ox")) ox = atoi(argv[++i]);
      else if (!strcmp(argv[i], "-oy")) oy = atoi(argv[++i]);
      else if (!strcmp(argv[i], "-ppm")) ppmdir = argv[++i];
      else if (!strcmp(argv[i], "-tag")) tag = argv[++i];
      else if (!strcmp(argv[i], "-at")) at = argv[++i];
      else if (!strcmp(argv[i], "-w64from")) w64from = atoi(argv[++i]);
      else if (!strcmp(argv[i], "-maxchars")) maxchars = atoi(argv[++i]);
      else if (!strcmp(argv[i], "-decoderows")) decoderows = atoi(argv[++i]);
   }
   FILE *ff = fopen(argv[1], "rb"), *tf = fopen(argv[2], "rb");
   if (!ff || !tf) { perror("open"); return 2; }
   for (int i = 0; i < 4; i++) buf[i] = calloc(1, FSZ);
   uint8_t *frame = malloc(FSZ), *shown_before = calloc(1, FSZ), *expect = malloc(FSZ);
   uint8_t truth[TSZ];
   uint32_t table[256];
#ifdef CV_HAVE_LOWERCASE_32
   int art_flags = 0;
   cv_build_artifact_table(table, art, art_flags);
#else
   if (art) { fprintf(stderr, "no artifacts in this build\n"); return 2; }
   (void) table;
#endif
   cv_state_t s; cv_init(&s);
   s.max_chars = maxchars;
   s.decode_rows = decoderows;
   if (getenv("RUNNER_SLOW")) {
      slow_step = (unsigned int) atoi(getenv("RUNNER_SLOW"));
      s.usec = slow_usec;
      s.budget_us = 250;
   }
   // with a work limit the screen may lag the 6847: a cell is accepted if it
   // matches its value in this or one of the previous HIST frames; after
   // HIST frames without change every cell must match
#define HIST 64      // >= frames of a full redraw at the tightest limit tested (2048 / 32 chars)
   static uint8_t hist[HIST][2048];
   int hist_n = 0, still = 0;
   regs_t r = { 0, 0x40, 0, 0x0D }, r_old = r;
   int reg_frame = -100000, w64_css = -1, css_old = 0;
   int last = 0, displayed = 0, redirect = 0, prev_type = 0, w64_frames = 0;
   int w64_was_on = 0, prev_reg = 0, entry_n = 0;

   for (int fno = 0; fread(frame, 1, FSZ, ff) == FSZ && fread(truth, 1, TSZ, tf) == TSZ; fno++) {
      // ---- capture (rgb_to_fb.S buffer selection) ----
      int cap;
      if (redirect) cap = redirect - 1;
      else if (last == nb) cap = 0;
      else cap = (last + 1) & 3;
      memcpy(buf[cap], frame, FSZ);
      if (art) {
         for (int y = 0; y < H; y++) {
            uint8_t *l = buf[cap] + y * W;
            for (int x = 0; x + 3 < W; x += 4) {
               uint32_t w = table[(l[x] & 15) | ((l[x + 2] & 15) << 4)];
               for (int k = 0; k < 4; k++) l[x + k] = (uint8_t) (w >> (8 * k));
            }
         }
      }
      // ---- the per-frame CoCoVGA hook ----
      int swap, flip;
#ifdef CV_HAVE_LOWERCASE_32
      int capture_next, inhibit;
      cv_frame_t cv_last = cv_frame(&s, cap, displayed, 4, bufp, W, W, H, &capture_next, &inhibit, &swap);
      redirect = capture_next + 1;
      flip = !inhibit;
#else
      int rd;
      cv_frame_t cv_last = cv_frame(&s, cap, displayed, 4, bufp, W, W, H, &rd, &swap);
      redirect = rd;
      flip = !rd;
#endif
#ifdef CV_HAVE_LOWERCASE_32
      if (art) {                       // as mc6847_artifact_frame() in rgb_to_hdmi.c
         int f = cv_artifact_flags_for(buf[cap], W, 0, 0, W, H);
         if (f != art_flags) { art_flags = f; cv_build_artifact_table(table, art, art_flags); }
      }
#endif
      if (swap >= 0) displayed = swap;
      if (flip) displayed = cap;
      last = cap;
      static unsigned int pal_gen_seen = 0;
      if (getenv("RUNNER_PALLOG") && s.pal_gen != pal_gen_seen) {
         // register palette / artifact register changes, for verify.sh
         pal_gen_seen = s.pal_gen;
         printf("PAL frame %d art %02x extras %02x unreadable %d:", fno, s.regs.artifact, s.regs.extras, s.pal_unreadable);
         for (int k = 0; k < CV_PAL_SLOTS; k++) if (s.regs.pal[k]) printf(" %d=%04x", k, s.regs.pal[k]);
         printf("\n");
      }
      if (getenv("RUNNER_LOG")) {
         int lo = atoi(getenv("RUNNER_LOG")), hi = getenv("RUNNER_LOG_TO") ? atoi(getenv("RUNNER_LOG_TO")) : lo + 20;
         if (fno >= lo && fno <= hi)
            printf("frame %4d: decoder %d  w64 %d  cal %d (%d,%d %dx%d)  regs font %02x enhanced %02x  shown buf %d\n",
                   fno, (int) cv_last, s.w64_on, s.cal_valid, s.ox, s.oy, s.hs, s.vs, s.regs.font, s.regs.enhanced, displayed);
      }
      const uint8_t *scr = buf[displayed];

      // ---- what must be on screen ----
      // -w64from N: the CoVGA driver's register page (00 81 00 03 .. 02) is
      // shown in frame N (no longer needed: truth.py now reads that frame)
      if (fno == w64from) { r.font = 0x03; r.enhanced = 0x02; }
      int type = truth[0], css = truth[1];
      const uint8_t *d = truth + 2;
      int is_reg = 0;
      // CoCoVGA register page 0: reset, mask, 0, font (0-7), artifact (0-15),
      // extras, 0, 0, enhanced (0-7); the rest of the page is whatever the
      // program left there (spaces, zeros, $FF)
      if (type == 1 && (d[0] | d[1]) && d[2] == 0 && d[6] == 0 && d[7] == 0 &&
          !(d[3] & 0xF8) && !(d[4] & 0xF0) && !(d[8] & 0xF8)) {
         is_reg = 1;
      }
      long bad = 0;
      if (is_reg) {
         int b0 = d[0], m = d[1];
         r_old = r;
         css_old = w64_css;
         reg_frame = fno;
         if (b0 & 0x80) r.enhanced = 0;
         if (b0 & 0x04) r.extras = 0x40;
         if (b0 & 0x02) r.artifact = 0x0D;
         if (b0 & 0x01) r.font = 0;
         if (m & 0x80) r.enhanced = d[8];
         if (m & 0x04) r.extras = d[5];
         if (m & 0x02) r.artifact = d[4];
         if (m & 0x01) r.font = d[3];
         // hidden: the screen still shows the previous picture
         for (int i = 0; i < FSZ; i++) bad += (scr[i] ^ shown_before[i]) & 15;
         if (bad && nb == 0 && !w64_was_on) {
            bad = 0;
            for (int i = 0; i < FSZ; i++) bad += (scr[i] ^ buf[cap][i]) & 15;
            result(K_REG_SHOWN, fno, bad);
         } else {
            result(K_REG, fno, bad);
         }
      } else if (type == 2 && (r.enhanced & 2)) {
         if (w64_css >= 0 && css != w64_css) { r_old = r; css_old = w64_css; reg_frame = fno; }
         w64_css = css;
         if (hist_n && memcmp(hist[(hist_n - 1) % HIST], d, 2048) == 0) still++; else still = 0;
         memcpy(hist[hist_n % HIST], d, 2048);
         hist_n++;
         for (int i = 0; i < 2048; i++) {
            int col = i & 63, row = i >> 6;
            int ok_any = 0;
            // without limits: this frame, or the previous one when this frame
            // was rejected (e.g. a row torn by a write during the scan)
            int tries = ((maxchars || decoderows) && still < HIST) ? (hist_n < HIST ? hist_n : HIST)
                                                                  : (hist_n < 2 ? hist_n : 2);
            // with a work limit a font / colour change is redrawn over
            // several frames: until then a cell may still have the old look
            int looks = ((maxchars || decoderows) && fno - reg_frame <= 2048 / (maxchars ? maxchars : 32) + HIST) ? 2 : 1;
            for (int h = 0; h < tries * looks && !ok_any; h++) {
               uint8_t v = hist[(hist_n - 1 - h % tries + HIST * 4) % HIST][i];
               const regs_t *rr = h < tries ? &r : &r_old;
               int cell_bad = 0;
               for (int y = 0; y < 12 && !cell_bad; y++)
                  for (int x = 0; x < 8; x++) {
                     uint8_t want = w64_pixel(v, x, y, rr, h < tries ? css : css_old);
                     int px = ox + col * 8 + x, py = oy + row * 12 + y;   // 1 fb pixel per W64 pixel
                     if ((scr[py * W + px] & 15) != want) { cell_bad = 1; break; }
                  }
               if (!cell_bad) ok_any = 1;
            }
            if (!ok_any) {
               bad++;
               if (getenv("RUNNER_CELLS") && fno == atoi(getenv("RUNNER_CELLS")))
                  printf("cell row %d col %d code %02x (prev %02x)\n", row, col, d[i], hist[(hist_n - 2 + HIST * 4) % HIST][i]);
            }
         }
         // while W64 is still being built (not on screen) the old picture shows
         if (!s.w64_on && (maxchars || decoderows)) bad = 0;
         // the first CV_ENTRY_HOLDS CG2 frames may keep the old picture when
         // they can't be used (mode change in the top border on a PAL Dragon,
         // a row torn by a write); likewise the frame after a register page
         // shown that way: the W64 picture rides over it
         entry_n = (prev_type == 2 && !w64_was_on) ? entry_n + 1 : 0;
         // (with Num Buffers 0 the capture goes straight to the screen, so
         // there it is the raw frame that shows)
         if (bad && (prev_reg || (!s.w64_on && entry_n < CV_ENTRY_HOLDS)) &&
             (memcmp(scr, shown_before, FSZ) == 0 || (nb == 0 && scr == buf[cap]))) {
            result(K_RIDE, fno, 0);
         } else
         result(K_W64, fno, bad);
         w64_frames++;
      } else if (type == 1 || type == 3) {
         memcpy(expect, buf[cap], FSZ);
         int lower = 0;
#ifdef CV_HAVE_LOWERCASE_32
         if (type == 1 && (r.font & 2)) {
            uint8_t light = css ? CV_LORANGE : CV_GREEN, dark = css ? CV_DKORANGE : CV_DKGREEN;
            for (int c = 0; c < 512; c++) {
               if (d[c] >= 0x20) continue;
               lower = 1;
               int cx = c & 31, cy = c >> 5;
               for (int y = 0; y < 12; y++)
                  for (int x = 0; x < 8; x++) {
                     uint8_t v = ((t1lower(d[c], y, 0) >> (7 - x)) & 1) ? dark : light;
                     for (int yy = 0; yy < 2; yy++)
                        for (int xx = 0; xx < 2; xx++) {
                           uint8_t *p = expect + (oy + (cy * 12 + y) * 2 + yy) * W + ox + (cx * 8 + x) * 2 + xx;
                           *p = (uint8_t) ((*p & ~15) | v);
                        }
                  }
            }
         }
#endif
         for (int i = 0; i < FSZ; i++) bad += (scr[i] ^ expect[i]) & 15 ? 1 : 0;
         // with a work limit the lower case fix-up may show a (complete)
         // screen from a few frames back
#define THIST 8
         static uint8_t *thist[THIST];
         static int thist_n = 0;
         if (!thist[0]) for (int h = 0; h < THIST; h++) thist[h] = malloc(FSZ);
         if (bad && (maxchars || decoderows)) {
            for (int h = 0; h < THIST && h < thist_n && bad; h++) {
               const uint8_t *e = thist[(thist_n - 1 - h) % THIST];
               long b2 = 0;
               for (int i = 0; i < FSZ && !b2; i++) b2 += (scr[i] ^ e[i]) & 15 ? 1 : 0;
               if (!b2) bad = 0;
            }
         }
         memcpy(thist[thist_n % THIST], expect, FSZ);
         thist_n++;
         if (bad && prev_type == 2 && w64_was_on && !s.w64_on) {
            result(K_RIDE, fno, 0);       // first frame after leaving W64
         } else if (bad && s.w64_on && prev_type == 2) {
            result(K_RIDE, fno, 0);
         } else {
            result(type == 3 ? K_RG6 : (lower ? K_LOWER : K_TEXT), fno, bad);
         }
      } else {
         result(K_OTHER, fno, 0);
      }
      if (ppmdir && at) {
         char list[4096]; snprintf(list, sizeof(list), ",%s,", at);
         char key[32]; snprintf(key, sizeof(key), ",%d,", fno);
         if (strstr(list, key)) write_ppm(ppmdir, tag, fno, scr);
      }
      w64_was_on = s.w64_on;
      prev_type = type;
      prev_reg = is_reg;
      memcpy(shown_before, scr, FSZ);
   }
   int fails = 0;
   printf("%-38s %6s %6s  first failure\n", "check", "ok", "fail");
   for (int k = 0; k < NK; k++) {
      if (nok[k] + nbad[k] == 0) continue;
      printf("%-38s %6d %6d", kname[k], nok[k], nbad[k]);
      if (nbad[k]) printf("  frame %d (%ld pixels)", first_bad[k], first_bad_px[k]);
      printf("\n");
      if (k != K_OTHER) fails += nbad[k];
   }
   printf("RESULT %s (%d 64-column frames checked)\n", fails ? "FAIL" : "PASS", w64_frames);
   return fails ? 1 : 0;
}
