/*
 * cocovga.c - CoCoVGA 64-column text emulation for MC6847 LumaCode capture
 *
 * See cocovga.h for an overview. Reference: CoCoVGA "Software Mode
 * Control" documentation (register page 0, 64-column mode W64) and the
 * NitrOS-9 CoVGA driver (level1/coco1/modules/covdg.asm).
 */
#include <string.h>
#include "cocovga.h"

// 6847 semigraphics / CG colour order -> LumaCode code
static const uint8_t sg_colour[8] = {
   CV_GREEN, CV_YELLOW, CV_BLUE, CV_RED, CV_BUFF, CV_CYAN, CV_MAGENTA, CV_ORANGE
};

// CG2 colour sets: code -> (set << 2) | value, 0xFF = not a CG colour
static uint8_t cg_lut[16];

// how many frames without CG2 before W64 is left (rides over glitches)
#define CV_LEAVE_FRAMES 2
// characters redrawn per frame by the rolling refresh
#define CV_REFRESH_CHARS 64
// characters always drawn per frame, even when over the time budget
#define CV_MIN_CHARS 32
// CG2 pixel rows always decoded per frame (a full pass takes <= 8 frames)
#define CV_MIN_DECODE_ROWS 8
// 32 column cells always looked at per frame by the lower case fix-up
#define CV_MIN_LC_CELLS 32
// maximum Hamming distance accepted when recognising a text glyph
#define CV_GLYPH_TOLERANCE 4

static int cv_leave_count;

static inline int code_at(const uint8_t *fb, int pitch, int x, int y) {
   return fb[y * pitch + x] & CV_CODE_MASK;
}

static inline int is_dark(int c) {
   return c == CV_BLANK || c == CV_BLACK || c == 11 || c == 14 || c == 15;
}

// The frame buffer is not cached on the Pi, so every single byte read is a
// full memory access (about 0.1 us on a Pi Zero W). Lines that are scanned
// are first copied to cached memory in 32 byte blocks (struct copies, which
// compile to load-multiple bursts). The copy is only valid until the next
// fetch_line() call.
typedef struct { uint32_t w[8]; } cv_block_t;
static cv_block_t cv_line_blocks[4096 / 32 + 2];   // up to 4 KB per line

static const uint8_t *fetch_line(const uint8_t *src, int len) {
   uintptr_t a = (uintptr_t) src & ~(uintptr_t) 31;
   uintptr_t e = (uintptr_t) src + (uintptr_t) len;
   const cv_block_t *p = (const cv_block_t *) a;
   cv_block_t *d = cv_line_blocks;
   while ((uintptr_t) p < e) *d++ = *p++;
   return (const uint8_t *) cv_line_blocks + ((uintptr_t) src - a);
}

void cv_reset_registers(cv_state_t *s) {
   s->regs.font = 0;
   s->regs.artifact = 0x0D;   // CoCoVGA default: colour artifacts, MESS mode
   s->regs.extras = 0x40;     // CoCoVGA default: 10 colour SG palette
   s->regs.enhanced = 0;
   memset(s->regs.pal, 0, sizeof(s->regs.pal));
   s->pal_pending = 0;
   s->pal_src = NULL;
   s->pal_gen++;
}

void cv_init(cv_state_t *s) {
   memset(s, 0, sizeof(*s));
   cv_reset_registers(s);
   memset(cg_lut, 0xFF, sizeof(cg_lut));
   cg_lut[CV_GREEN]   = 0x0 | 0; cg_lut[CV_YELLOW] = 0x0 | 1;
   cg_lut[CV_BLUE]    = 0x0 | 2; cg_lut[CV_RED]    = 0x0 | 3;
   cg_lut[CV_BUFF]    = 0x4 | 0; cg_lut[CV_CYAN]   = 0x4 | 1;
   cg_lut[CV_MAGENTA] = 0x4 | 2; cg_lut[CV_ORANGE] = 0x4 | 3;
   s->display_buf = -1;
   s->build_buf = -1;
   s->full_pos = -1;
   s->lc_buf = -1;
   s->vg_buf = -1;
   s->lc_font = CV_LC_6847T1;
   s->lc_on_mode = -1;
   cv_leave_count = 0;
}

// ---------------------------------------------------------------------
// Calibration: find the 256x192 active picture in a text-mode frame.
// In text modes the border is black and every text cell contains green
// or dark green, so the active picture is the box of non-dark pixels.
// ---------------------------------------------------------------------

static int find_box_on_row(const uint8_t *fb, int pitch, int width, int y, int *left, int *right) {
   int l = 0, r = width - 1;
   const uint8_t *line = width <= 4096 ? fetch_line(fb + y * pitch, width) : fb + y * pitch;
   while (l < width && is_dark(line[l] & CV_CODE_MASK)) l++;
   if (l >= width) return 0;
   while (r > l && is_dark(line[r] & CV_CODE_MASK)) r--;
   *left = l; *right = r;
   return 1;
}

static int find_box_on_col(const uint8_t *fb, int pitch, int height, int x, int *top, int *bottom) {
   int t = 0, b = height - 1;
   while (t < height && is_dark(code_at(fb, pitch, x, t))) t++;
   if (t >= height) return 0;
   while (b > t && is_dark(code_at(fb, pitch, x, b))) b--;
   *top = t; *bottom = b;
   return 1;
}

static int calibrate(cv_state_t *s, const uint8_t *fb, int pitch, int width, int height) {
   static const int fractions[4][2] = { {1, 2}, {1, 3}, {2, 3}, {3, 8} };
   for (int i = 0; i < 4; i++) {
      int y = height * fractions[i][0] / fractions[i][1];
      int l, r, t, b;
      if (!find_box_on_row(fb, pitch, width, y, &l, &r)) continue;
      int w = r - l + 1;
      if (w % CV_ACTIVE_W != 0) continue;
      int hs = w / CV_ACTIVE_W;
      if (hs < 1 || hs > 4) continue;
      int found = 0;
      for (int j = 0; j < 4 && !found; j++) {
         int x = l + w * fractions[j][0] / fractions[j][1];
         if (!find_box_on_col(fb, pitch, height, x, &t, &b)) continue;
         int h = b - t + 1;
         if (h % CV_ACTIVE_H != 0) continue;
         int vs = h / CV_ACTIVE_H;
         if (vs < 1 || vs > 4) continue;
         if (y < t || y > b) continue;
         s->ox = l; s->oy = t; s->hs = hs; s->vs = vs;
         found = 1;
      }
      if (found) {
         s->cal_valid = 1;
         s->fb_w = width; s->fb_h = height; s->fb_pitch = pitch;
         return 1;
      }
   }
   return 0;
}

// ---------------------------------------------------------------------
// Text cell decoding (used for the register page)
// Dragon/CoCo wiring: DD7 -> A/S (semigraphics), DD6 -> INV.
// Bytes $40-$7F are dark characters on green, $00-$3F green on dark.
// ---------------------------------------------------------------------

static int popcount8(unsigned int v) {
   v = v - ((v >> 1) & 0x55);
   v = (v & 0x33) + ((v >> 2) & 0x33);
   return (v + (v >> 4)) & 0x0F;
}

// Returns the byte, -1 if the cell can't be decoded, or CV_CELL_SOLID_GREEN
// for a solid green cell, which is either a space ($60) or a fully lit green
// semigraphics block ($8F): the two look identical.
// CoCoVGA's register page 0 is always displayed with CSS = 0, so only the
// green text colours are accepted; orange cells are semigraphics.
#define CV_CELL_SOLID_GREEN (-2)

// fb / pitch / ox / oy / hs / vs describe where the cell's picture is (the
// register page passes a copy of its top 12 dot lines, vs = 1)
// Plain MC6847 chips do not all draw the same glyphs: 'E' and 'F' have a 3
// dot middle bar on some (a real Dragon 32/64) and 4 dots on others (the
// Acorn Atom's, as captured in MAME and XRoar), and '3' a 2 or 1 dot middle
// stroke. Only row 6 differs; both forms of each are recognised.
// mc6847_rom_font holds one form, rom_alt_row6() the other (-1 = none).
static int rom_alt_row6(int code) {
   if (code == 0x05 || code == 0x06) return 0x3C;   // E, F: 4 dot bar
   if (code == 0x33) return 0x04;                   // 3: 1 dot stroke
   return -1;
}

static int rom_glyph_is(const uint8_t *rows, int code) {
   const uint8_t *g = mc6847_rom_font[code];
   if (memcmp(rows, g, 12) == 0) return 1;
   int alt = rom_alt_row6(code);
   return alt >= 0 && rows[6] == alt && memcmp(rows, g, 6) == 0 && memcmp(rows + 7, g + 7, 5) == 0;
}

// dots in which rows differ from the nearer form of a glyph
static int rom_glyph_distance(const uint8_t *rows, int code) {
   const uint8_t *g = mc6847_rom_font[code];
   int d = 0;
   for (int dy = 0; dy < 12; dy++) d += popcount8(rows[dy] ^ g[dy]);
   int alt = rom_alt_row6(code);
   if (alt >= 0) {
      int d2 = d - popcount8(rows[6] ^ g[6]) + popcount8(rows[6] ^ alt);
      if (d2 < d) d = d2;
   }
   return d;
}

static int decode_text_cell(const uint8_t *fb, int pitch, int ox, int oy, int hs, int vs, int col, int row) {
   int x0 = ox + col * 8 * hs;
   int y0 = oy + row * 12 * vs;
   uint8_t dark_bits[12];
   unsigned int seen = 0;              // bit mask of colour codes present
   int sg_code = -1;
   for (int dy = 0; dy < 12; dy++) {
      unsigned int bits = 0;
      const uint8_t *line = fb + (y0 + dy * vs) * pitch;
      for (int dx = 0; dx < 8; dx++) {
         int c = line[x0 + dx * hs] & CV_CODE_MASK;
         seen |= 1u << c;
         if (c == CV_DKGREEN || c == CV_DKORANGE) bits |= 0x80 >> dx;
      }
      dark_bits[dy] = bits;
   }
   unsigned int text0 = (1u << CV_GREEN) | (1u << CV_DKGREEN);
   if (seen == (1u << CV_GREEN)) return CV_CELL_SOLID_GREEN;
   if ((seen & ~text0) == 0) {
      // exact match first (the 6847's output is digital), then tolerant
      for (int i = 0; i < 64; i++) {
         if (rom_glyph_is(dark_bits, i)) return 0x40 | i;
      }
      uint8_t lit_bits[12];
      for (int dy = 0; dy < 12; dy++) lit_bits[dy] = (uint8_t) ~dark_bits[dy];
      for (int i = 0; i < 64; i++) {
         if (rom_glyph_is(lit_bits, i)) return i;
      }
      int best = 99, best_code = -1;
      for (int i = 0; i < 64; i++) {
         int dn = rom_glyph_distance(dark_bits, i);
         int di = rom_glyph_distance(lit_bits, i);
         if (dn < best) { best = dn; best_code = 0x40 | i; }
         if (di < best) { best = di; best_code = i; }
      }
      return best <= CV_GLYPH_TOLERANCE ? best_code : -1;
   }
   // semigraphics 4: black plus at most one of the 8 colours
   unsigned int colours = seen & ~((1u << CV_BLACK) | (1u << CV_BLANK));
   if (colours & (colours - 1)) return -1;          // more than one colour
   int colour = 0;
   if (colours) {
      for (sg_code = 0; sg_code < 8; sg_code++) {
         if (colours == (1u << sg_colour[sg_code])) break;
      }
      if (sg_code == 8) return -1;
      colour = sg_code;
   }
   int quads = 0;
   for (int q = 0; q < 4; q++) {
      int qx = (q & 1) ? 4 : 0, qy = (q & 2) ? 6 : 0, lit = 0;
      for (int dy = 0; dy < 6; dy++) {
         for (int dx = 0; dx < 4; dx++) {
            int c = code_at(fb, pitch, x0 + (qx + dx) * hs, y0 + (qy + dy) * vs);
            if (!is_dark(c)) lit++;
         }
      }
      if (lit > 12) quads |= 8 >> q;   // bit3 = top left ... bit0 = bottom right
   }
   return 0x80 | (colour << 4) | quads;
}

// Register page byte at index i (0-511) of a page whose text rows are in
// rows[] (row r at rows + r * row_stride: 12 dot lines of row_pitch bytes,
// vs = 1), or -1. *unknown gets the bits the picture can't show: a
// semigraphics byte ($80-$FF) with no quarter lit is plain black whatever
// its colour bits 6-4 say, and a solid green cell is either $60 or $8F.
static int decode_page_byte(const cv_state_t *s, const uint8_t *rows, int row_pitch, int i, int *unknown) {
   int r = i >> 5, col = i & 31;
   int v = decode_text_cell(rows + r * row_pitch * 12, row_pitch, 0, 0, s->pal_hs, 1, col, 0);
   *unknown = 0;
   if (v == CV_CELL_SOLID_GREEN) { *unknown = 0xFF; return 0x60; }
   if (v >= 0x80 && (v & 0x0F) == 0) *unknown = 0x70;
   return v;
}

// palette groups: { reset / edit mask bit, first slot, number of slots }
static const struct { int bit, first, n; } pal_group[3] = {
   { 0x08, CV_PAL_SG, CV_PAL_SG_N },        // semigraphics palette, bytes 32-53
   { 0x10, CV_PAL_ART_CSS1, 32 },           // artifact / VG6 palette, 64-127
   { 0x20, CV_PAL_EXTRA, 4 },               // extra palette, 128-135
};

// text rows 1-4 of the last register page with palette groups (rows needed
// only), in cached memory; row r at page_rows[r * 12]
static uint8_t page_rows[5 * 12][32 * 8 * 4];
#define CV_MIN_PAL_SLOTS 4      // palette slots always decoded per frame

static void pal_finish(cv_state_t *s) {
   int changed = memcmp(s->regs.pal, s->pal_new, sizeof(s->pal_new)) != 0;
   memcpy(s->regs.pal, s->pal_new, sizeof(s->pal_new));
   s->pal_unreadable = s->pal_unread_new;
   s->pal_pending = 0;
   if (changed) s->pal_gen++;
}

// text rows with palette words: SG row 1 columns 0-21, artifact rows 2-3,
// extra row 4 columns 0-7
static const struct { int bit, row, cols; } pal_rows[4] = {
   { 0x08, 1, 22 }, { 0x10, 2, 32 }, { 0x10, 3, 32 }, { 0x20, 4, 8 } };

// Copies the next line of the page (returns 0) or returns 1 when all are copied.
static int pal_copy_line(cv_state_t *s) {
   int n = s->pal_line;
   for (int k = 0; k < 4; k++) {
      if (!(s->pal_pending & pal_rows[k].bit)) continue;
      if (n >= 12) { n -= 12; continue; }
      int y = pal_rows[k].row * 12 + n, span = pal_rows[k].cols * 8 * s->pal_hs;
      memcpy(page_rows[y], fetch_line(s->pal_src + (s->pal_oy + y * s->pal_vs) * s->pal_pitch + s->pal_ox, span), span);
      s->pal_line++;
      return 0;
   }
   return 1;
}

static int pal_over_budget(const cv_state_t *s, int all, int done) {
   return !all && s->usec && done && s->usec() - s->t_frame >= s->budget_us;
}

// Copies the page's lines, then decodes the palette slots, within the work
// limit unless all is set (at least one line or CV_MIN_PAL_SLOTS slots per
// call). Returns 1 when nothing is left.
static int pal_continue(cv_state_t *s, int all) {
   int done = 0;
   while (s->pal_pending && s->pal_src) {
      if (pal_over_budget(s, all, done)) return 0;
      if (pal_copy_line(s)) { s->pal_src = NULL; break; }
      done++;
   }
   done = 0;
   while (s->pal_pending) {
      int pos = s->pal_pos, g = 0;
      while (g < 3 && (!(s->pal_pending & pal_group[g].bit) || pos >= pal_group[g].n)) {
         if (s->pal_pending & pal_group[g].bit) pos -= pal_group[g].n;
         g++;
      }
      if (g == 3) { pal_finish(s); break; }
      if (pal_over_budget(s, all, done >= CV_MIN_PAL_SLOTS)) return 0;
      int slot = pal_group[g].first + pos, i = 32 + 2 * slot, uh, ul;
      int hi = decode_page_byte(s, &page_rows[0][0], sizeof(page_rows[0]), i, &uh);
      int lo = decode_page_byte(s, &page_rows[0][0], sizeof(page_rows[0]), i + 1, &ul);
      if (hi >= 0 && !(uh & 0x80) && !(hi & 0x80)) {
         s->pal_new[slot] = 0;                       // not enabled: the default colour
      } else if (hi < 0 || lo < 0 || uh || ul) {
         s->pal_unread_new++;                        // keep what it was
      } else {
         s->pal_new[slot] = (uint16_t) ((hi << 8) | lo);
      }
      s->pal_pos++;
      done++;
   }
   return 1;
}

// Applies the page's registers and resets now; palette groups to be loaded
// (rows copied by the caller) are decoded by pal_continue. extras_keep: bits
// of the extras register the page could not show, kept as they were.
static void apply_register_page(cv_state_t *s, const int *b, int pal_mask, int extras_keep) {
   int reset = b[0], mask = b[1];
   uint8_t old_artifact = s->regs.artifact, old_extras = s->regs.extras;
   int changed = 0;
   if (reset & 0x80) s->regs.enhanced = 0;
   if (reset & 0x04) s->regs.extras = 0x40;
   if (reset & 0x02) s->regs.artifact = 0x0D;
   if (reset & 0x01) s->regs.font = 0;
   for (int g = 0; g < 3; g++) {
      if (!(reset & pal_group[g].bit)) continue;
      for (int k = 0; k < pal_group[g].n; k++) {
         int slot = pal_group[g].first + k;
         if (s->regs.pal[slot]) { s->regs.pal[slot] = 0; changed = 1; }
         s->pal_new[slot] = 0;
      }
   }
   if (mask & 0x80) s->regs.enhanced = b[8];
   if (mask & 0x04) s->regs.extras = (uint8_t) ((b[5] & ~extras_keep) | (s->regs.extras & extras_keep));
   if (mask & 0x02) s->regs.artifact = b[4];
   if (mask & 0x01) s->regs.font = b[3];
   if (pal_mask) {
      memcpy(s->pal_new, s->regs.pal, sizeof(s->pal_new));
      s->pal_pending = pal_mask;
      s->pal_pos = 0;
      s->pal_unread_new = 0;
   }
   // extras bits that change the colour table: 2 scanlines, 5 inverse
   // video, 6 9 / 10 colour palette, 7 text colours
   if (changed || s->regs.artifact != old_artifact || ((s->regs.extras ^ old_extras) & 0xE4)) s->pal_gen++;
}

// CoCoVGA register page 0 starts with: reset, edit mask, reserved(0), font,
// artifact, extras, reserved(0), reserved(0), enhanced modes.
static int check_register_page(cv_state_t *s, const uint8_t *fb, int pitch) {
   // value for a solid green cell at each position ($60 or $8F, see above)
   static const int solid_green[9] = { 0x8F, 0x8F, -1, -1, -1, 0x60, -1, -1, -1 };
   int b[9];
   // copy the top 12 dot lines of cells 0-8 to cached memory (burst reads)
   static uint8_t cells[12][9 * 8 * 8];
   const int span = 9 * 8 * s->hs;
   if (span > (int) sizeof(cells[0])) return 0;
   for (int dy = 0; dy < 12; dy++) {
      memcpy(cells[dy], fetch_line(fb + (s->oy + dy * s->vs) * pitch + s->ox, span), span);
   }
   // the reserved registers 2, 6 and 7 are always 0: check them first, so
   // ordinary text screens are rejected after 3 cells instead of 9
   static const int order[9] = { 2, 6, 7, 0, 1, 3, 4, 5, 8 };
   for (int k = 0; k < 9; k++) {
      int i = order[k];
      b[i] = decode_text_cell(&cells[0][0], sizeof(cells[0]), 0, 0, s->hs, 1, i, 0);
      if (b[i] == CV_CELL_SOLID_GREEN) b[i] = solid_green[i];
      if (b[i] < 0) return 0;
      if (k < 3 && b[i] != 0) return 0;
   }
   if ((b[3] & 0xF8) || (b[4] & 0xF0) || (b[8] & 0xF8)) return 0;
   if ((b[0] | b[1]) == 0) return 0;
   // palette groups: the text rows they are in are copied to cached memory
   // and decoded by pal_continue. A page still being read is finished first.
   int pal_mask = b[1] & 0x38;
   if (32 * 8 * s->hs > (int) sizeof(page_rows[0])) pal_mask = 0;
   if (pal_mask) {
      if (s->pal_pending) pal_continue(s, 1);
      s->pal_src = fb;
      s->pal_pitch = pitch;
      s->pal_ox = s->ox; s->pal_oy = s->oy; s->pal_hs = s->hs; s->pal_vs = s->vs;
      s->pal_line = 0;
      s->pal_buf = -1;
   }
   // an extras value $80-$F0 with the low bits 0 is a semigraphics cell with
   // no quarter lit: plain black, so bits 6-4 can't be seen (it reads as
   // $80). Keep them as they were: from the default $40, CoCoVGA's "text
   // palette" value $C0 then comes out right instead of losing the 10 colours
   int extras_keep = (b[5] & 0x8F) == 0x80 ? 0x70 : 0;
   apply_register_page(s, b, pal_mask, extras_keep);
   pal_continue(s, 0);
   for (int i = 0; i < 9; i++) s->last_register[i] = b[i];
   s->register_frames++;
   return 1;
}

// ---------------------------------------------------------------------
// CG2 decoding: 128x64 four colour, each pixel 2 dots wide, 3 lines high,
// 32 bytes per row, 2 bits per pixel, leftmost pixel in bits 7-6.
// ---------------------------------------------------------------------

// decode one CG2 pixel row into out[32]; returns the colour sets seen
// (bit 0 green set, bit 1 buff set), -1 if this is not a CG2 row (colours),
// -2 if the row's lines differ (a write during the scan, or CG3 / CG6)
static int decode_cg2_row(const cv_state_t *s, const uint8_t *fb, int pitch, int row, uint8_t *out, int *pairs_differ) {
   const int hs = s->hs, vs = s->vs;
   int sets = 0;
   const uint8_t *l0 = fb + (s->oy + (row * 3 + 0) * vs) * pitch + s->ox;
   const uint8_t *l2 = fb + (s->oy + (row * 3 + 2) * vs) * pitch + s->ox;
   const uint8_t *l1 = fetch_line(fb + (s->oy + (row * 3 + 1) * vs) * pitch + s->ox, 256 * hs);
   for (int b = 0; b < 32; b++) {
      int byte = 0;
      int x = b * 8 * hs;
      for (int p = 0; p < 4; p++, x += 2 * hs) {
         int v = cg_lut[l1[x] & CV_CODE_MASK];
         if (v == 0xFF) return -1;
         sets |= 1 << (v >> 2);
         byte = (byte << 2) | (v & 3);
      }
      // vertical shape check on a quarter of the bytes: all three lines
      // of a CG2 pixel row must be identical
      if ((b & 3) == (row & 3)) {
         int xc = b * 8 * hs;
         if (((l0[xc] ^ l1[xc]) | (l2[xc] ^ l1[xc])) & CV_CODE_MASK) return -2;
      }
      if (((byte >> 6) ^ (byte >> 4)) & 3 || ((byte >> 2) ^ byte) & 3) *pairs_differ = 1;
      out[b] = (uint8_t) byte;
   }
   return sets;
}

static int decode_cg2(cv_state_t *s, const uint8_t *fb, int pitch) {
   static uint8_t rows[64][32];
   const int hs = s->hs, vs = s->vs;
   // the border above the picture must be the CG border colour (cheap first test)
   if (s->oy >= vs) {
      int c = code_at(fb, pitch, s->ox + 4 * hs, s->oy - vs);
      if (c != CV_GREEN && c != CV_BUFF) {
         // the mode may have changed after the top border began: then the
         // border beside the picture already has the CG colour
         if (s->ox >= 4 * hs) {
            int m = code_at(fb, pitch, s->ox - 4 * hs, s->oy + 96 * vs);
            if (m == CV_GREEN || m == CV_BUFF) s->cg_unusable = 1;
         }
         return 0;
      }
   }
   int sets = 0, pairs_differ = 0, n = 0;
   int row = s->decode_row;
   int limit = (s->decode_rows > 0 && s->decode_rows < 64) ? s->decode_rows : 64;
   while (n < 64) {
      int r = decode_cg2_row(s, fb, pitch, row, rows[n], &pairs_differ);
      if (r < 0) {
         if (r == -2) s->cg_unusable = 1;
         return 0;
      }
      sets |= r;
      n++;
      row = (row + 1) & 63;
      if (n >= limit) break;
      if (s->usec && s->decode_budget_us && n >= CV_MIN_DECODE_ROWS &&
          s->usec() - s->t_frame >= s->decode_budget_us) break;
   }
   if (sets == 3) return 0;          // mixed colour sets: not a CG picture
   // CG1 (4 dot wide pixels) never has two different pixels in a pair; a
   // slice of a 64 column screen can look like that (rows of $00 / $FF),
   // so this only decides when W64 is not on yet
   if (!pairs_differ && !s->w64_on) return 0;
   // all rows of this frame are good: commit them
   for (int k = 0, r = s->decode_row; k < n; k++, r = (r + 1) & 63) memcpy(s->vram + r * 32, rows[k], 32);
   s->decode_row = row;
   s->rows_valid += n;
   if (s->rows_valid > 64) s->rows_valid = 64;
   s->css = (sets & 2) ? 1 : 0;
   s->cg2_frames++;
   return 1;
}

// ---------------------------------------------------------------------
// VG6: CoCoVGA's 128x96 16 colour mode in the 6 KB of a CG6 (PMODE 3)
// picture. Each VG6 row is 64 bytes, 2 pixels per byte (left pixel in the
// high nibble): CG6 rows 2r and 2r + 1 (32 bytes each), which the 6847 draws
// on the very lines the VG6 row covers (pixels 2 dots wide, 2 lines high).
// So a captured CG6 frame is converted in place, one VG6 row at a time.
// ---------------------------------------------------------------------

// Is this a CG6 picture (4 colours of one set, pixels 2 dots wide)? Samples
// 16 lines at 8 places (single reads, no line copies). *css = colour set.
static int vg6_check(cv_state_t *s, const uint8_t *fb, int pitch, int *css) {
   const int hs = s->hs, vs = s->vs;
   if (hs < 1 || vs < 1 || s->oy < vs) return 0;
   int c = code_at(fb, pitch, s->ox + 4 * hs, s->oy - vs);
   if (c != CV_GREEN && c != CV_BUFF) {
      // the mode may have changed after the top border began (e.g. just
      // after a register page): the border beside the picture then
      // already has the CG colour. The frame can't be used (cv_frame
      // keeps the previous picture instead)
      if (s->ox >= 4 * hs) {
         int m = code_at(fb, pitch, s->ox - 4 * hs, s->oy + 96 * vs);
         if (m == CV_GREEN || m == CV_BUFF) s->cg_unusable = 1;
      }
      return 0;
   }
   int sets = 0;
   for (int k = 0; k < 16; k++) {
      int y = s->oy + (k * 12 + 5) * vs;
      for (int j = 0; j < 8; j++) {
         int x = s->ox + (32 * j + 2 * ((k + j) & 15)) * hs + hs / 2;   // first dot of a pixel
         int a = fb[y * pitch + x] & CV_CODE_MASK, b = fb[y * pitch + x + hs] & CV_CODE_MASK;
         if (a != b || cg_lut[a] == 0xFF) return 0;
         sets |= 1 << (cg_lut[a] >> 2);
      }
   }
   if (sets == 3) return 0;
   *css = sets == 2;
   return 1;
}

// one CG6 line (32 bytes) from the frame buffer
static void vg6_read_row(const cv_state_t *s, const uint8_t *fb, int pitch, int cg_row, uint8_t *out) {
   const int hs = s->hs;
   const uint8_t *l = fetch_line(fb + (s->oy + cg_row * s->vs) * pitch + s->ox, 256 * hs);
   for (int b = 0; b < 32; b++) {
      int byte = 0;
      for (int p = 0; p < 4; p++) {
         int v = cg_lut[l[(b * 4 + p) * 2 * hs + hs / 2] & CV_CODE_MASK];
         byte = (byte << 2) | (v == 0xFF ? 0 : (v & 3));
      }
      out[b] = (uint8_t) byte;
   }
}

// converts VG6 rows from s->vg_pos on, within the work limit; 1 when done
static int vg6_convert(cv_state_t *s, uint8_t *fb, int pitch) {
   static uint8_t line[256 * 8], line_sc[256 * 8];
   const int hs = s->hs, vs = s->vs;
   if (256 * hs > (int) sizeof(line)) return 1;
   const uint8_t tag = s->vg_css ? CV_VG6_CSS1 : CV_VG6_CSS0;
   int done = 0;
   while (s->vg_pos < 96) {
      if (s->usec && done >= 1 && s->usec() - s->t_frame >= s->budget_us) return 0;
      int r = s->vg_pos++;
      int line_sc_ok = 0;
      uint8_t data[64];
      vg6_read_row(s, fb, pitch, 2 * r, data);
      vg6_read_row(s, fb, pitch, 2 * r + 1, data + 32);
      uint8_t *d = line;
      for (int x = 0; x < 128; x++) {
         uint8_t v = tag | ((x & 1) ? (data[x >> 1] & 15) : (data[x >> 1] >> 4));
         for (int k = 0; k < 2 * hs; k++) *d++ = v;
      }
      // scan lines (bit 7 on every second line, the same along a line) stay
      uint8_t *dst = fb + (s->oy + 2 * r * vs) * pitch + s->ox;
      for (int k = 0; k < 2 * vs; k++, dst += pitch) {
         if (dst[0] & 0x80) {
            if (!line_sc_ok) {
               for (int x = 0; x < 256 * hs; x++) line_sc[x] = line[x] | 0x80;
               line_sc_ok = 1;
            }
            memcpy(dst, line_sc, 256 * hs);
         } else {
            memcpy(dst, line, 256 * hs);
         }
      }
      done++;
   }
   s->vg6_frames++;
   return 1;
}

cv_frame_t cv_analyze(cv_state_t *s, const uint8_t *fb, int pitch, int width, int height) {
   s->frames++;
   s->cg_unusable = 0;
   if (s->cal_valid && (s->fb_w != width || s->fb_h != height || s->fb_pitch != pitch)) {
      s->cal_valid = 0;
   }
   // CG2 first when 64-column mode is enabled: that is the busy path
   if (s->cal_valid && (s->regs.enhanced & 0x02)) {
      if (decode_cg2(s, fb, pitch)) return CV_FRAME_CG2;
   }
   // VG6 (enhanced bit 0) in a CG6 picture
   if (s->cal_valid && (s->regs.enhanced & 0x01)) {
      int css;
      if (vg6_check(s, fb, pitch, &css)) {
         s->cg_unusable = 0;       // (decode_cg2 may have taken it for a torn CG2 row)
         s->css_vg6 = css;
         return CV_FRAME_VG6;
      }
   }
   // text frame? (re)calibrate, then look for the register page
   // (the register page may hold arbitrary data around the register bytes,
   // so if this frame can't calibrate, use the previous calibration)
   int old_ox = s->ox, old_oy = s->oy, old_hs = s->hs, old_vs = s->vs;
   int cal = calibrate(s, fb, pitch, width, height);
   if (cal && (old_ox != s->ox || old_oy != s->oy || old_hs != s->hs || old_vs != s->vs)) {
      // the picture moved: the one on screen was drawn at the old place,
      // so start again from the next captured CG2 frame
      if (s->w64_on || s->build_buf >= 0) cv_stop(s);
      s->shown_valid = 0;
   }
   if (cal || s->cal_valid) {
      if (check_register_page(s, fb, pitch)) return CV_FRAME_REGISTER;
      if (cal) return CV_FRAME_TEXT;
   }
   return CV_FRAME_OTHER;
}

// ---------------------------------------------------------------------
// W64 renderer
// ---------------------------------------------------------------------

static int renderable(const cv_state_t *s) {
   // hs <= 8: draw_char's line buffer holds 8 W64 pixels of up to 4 bytes
   return s->cal_valid && s->hs >= 2 && s->vs >= 2 && !(s->hs & 1) && !(s->vs & 1)
          && s->hs <= 8 && s->vs <= 16;
}

// The T1 table puts lower case on the baseline of its own capitals (row 7);
// the 6847's capitals sit 2 rows lower (rows 3-9). Rows 10-11 of every T1
// lower case glyph are empty, so moving them down loses nothing.
const uint8_t *cv_lower_glyph(int code, int style) {
   static uint8_t low[32][12];
   static int ready = 0;
   if (!ready) {
      for (int c = 0; c < 32; c++) {
         for (int y = 0; y < 12; y++) low[c][y] = y >= 2 ? mc6847t1_font[64 + c][y - 2] : 0;
      }
      ready = 1;
   }
   if (style == CV_LC_T1_HIGH) return mc6847t1_font[64 + (code & 31)];
   if (style == CV_LC_DRAGON200E) return dragon200e_lower_font[code & 31];
   return low[code & 31];
}

static void draw_char(const cv_state_t *s, uint8_t *fb, int pitch, int index, int value) {
   const int px = s->hs / 2, py = s->vs / 2;
   const int col = index & 63, row = index >> 6;
   uint8_t *base = fb + (s->oy + row * 12 * py) * pitch + s->ox + col * 8 * px;
   uint8_t rows[12];
   uint8_t fg, bg;
   if (value & 0x80) {
      // semigraphics 4: quadrants 4x6 W64 pixels, coloured on black
      int q_top = (value >> 2) & 3, q_bottom = value & 3;
      for (int r = 0; r < 12; r++) {
         int q = r < 6 ? q_top : q_bottom;
         rows[r] = ((q & 2) ? 0xF0 : 0) | ((q & 1) ? 0x0F : 0);
      }
      fg = sg_colour[(value >> 4) & 7];
      bg = CV_BLACK;
   } else {
      int code = value & 0x3F;
      int normal = value & 0x40;
      int t1 = s->regs.font & 1;
      int lower = s->regs.font & 2;
      const uint8_t *glyph;
      int inverse;
      if (normal) {
         glyph = t1 ? mc6847t1_font[code] : mc6847_font[code];
         inverse = 0;
      } else if (lower && code < 32) {
         glyph = cv_lower_glyph(code, t1 ? CV_LC_T1_HIGH : CV_LC_6847T1);    // lower case replaces inverse
         inverse = 0;
      } else {
         glyph = t1 ? mc6847t1_font[code] : mc6847_font[code];
         inverse = 1;
      }
      // text colours and inverse video (extras bits 7 and 5) are applied by
      // the colour table to dots with the text tag
      uint8_t tag = cv_text_tags_on(&s->regs) ? CV_TEXT_TAG : 0;
      uint8_t light = (s->css ? CV_LORANGE : CV_GREEN) | tag;   // as the 6847 draws text
      uint8_t dark  = (s->css ? CV_DKORANGE : CV_DKGREEN) | tag;
      fg = inverse ? light : dark;
      bg = inverse ? dark : light;
      memcpy(rows, glyph, 12);
   }
   if (px == 1 && (((uintptr_t) base | (uintptr_t) pitch) & 1) == 0) {
      // fast path: one W64 pixel per fb pixel, word aligned
      // (4 pixel words for each 4 bit pattern, rebuilt when the colours change)
      static uint32_t nib[16];
      static int nib_fg = -1, nib_bg = -1;
      if (fg != nib_fg || bg != nib_bg) {
         for (int n = 0; n < 16; n++) {
            uint32_t w = 0;
            for (int b = 0; b < 4; b++) {
               uint32_t c = (n & (8 >> b)) ? fg : bg;
               w |= c << (8 * b);           // little endian: leftmost pixel first
            }
            nib[n] = w;
         }
         nib_fg = fg;
         nib_bg = bg;
      }
      if ((((uintptr_t) base | (uintptr_t) pitch) & 3) == 0) {
         for (int r = 0; r < 12; r++) {
            uint32_t w0 = nib[rows[r] >> 4], w1 = nib[rows[r] & 15];
            for (int k = 0; k < py; k++) {
               ((uint32_t *) base)[0] = w0;
               ((uint32_t *) base)[1] = w1;
               base += pitch;
            }
         }
      } else {
         // picture 2 pixels off a word boundary: 16 + 32 + 16 bit stores
         for (int r = 0; r < 12; r++) {
            uint32_t w0 = nib[rows[r] >> 4], w1 = nib[rows[r] & 15];
            uint16_t h0 = (uint16_t) w0, h1 = (uint16_t) (w1 >> 16);
            uint32_t mid = (w0 >> 16) | (w1 << 16);
            for (int k = 0; k < py; k++) {
               ((uint16_t *) base)[0] = h0;
               ((uint32_t *) (base + 2))[0] = mid;
               ((uint16_t *) (base + 6))[0] = h1;
               base += pitch;
            }
         }
      }
      return;
   }
   for (int r = 0; r < 12; r++) {
      uint8_t bits = rows[r];
      uint8_t line[32];
      int n = 0;
      for (int b = 0; b < 8; b++) {
         uint8_t c = (bits & (0x80 >> b)) ? fg : bg;
         for (int k = 0; k < px; k++) line[n++] = c;
      }
      for (int k = 0; k < py; k++) {
         memcpy(base, line, n);
         base += pitch;
      }
   }
}

// work limit (see cv_state_t); checked every 8 characters
static int over_budget(const cv_state_t *s, int drawn) {
   if (s->max_chars && drawn >= s->max_chars) return 1;
   if (s->usec && drawn >= CV_MIN_CHARS && (drawn & 7) == 0) {
      return s->usec() - s->t_frame >= s->budget_us;
   }
   return 0;
}

int cv_render(cv_state_t *s, uint8_t *fb, int pitch, int full) {
   if (!renderable(s)) return 0;
   int look = s->regs.font | (s->regs.extras << 8);
   if (full || !s->shown_valid || s->shown_css != s->css || s->shown_font != look) {
      // (re)start a full redraw; it may take several frames
      if (full || s->full_pos < 0 || s->shown_css != s->css || s->shown_font != look) s->full_pos = 0;
      s->shown_valid = 1;
      s->shown_css = s->css;
      s->shown_font = look;
   }
   int drawn = 0;
   while (s->full_pos >= 0 && s->full_pos < CV_VRAM_SIZE) {
      if (over_budget(s, drawn)) return 0;
      int i = s->full_pos++;
      draw_char(s, fb, pitch, i, s->vram[i]);
      s->shown[i] = s->vram[i];
      drawn++;
   }
   s->full_pos = -1;
   // characters that changed (including ones drawn earlier in a full redraw)
   for (int i = 0; i < CV_VRAM_SIZE; i++) {
      if (s->vram[i] != s->shown[i]) {
         if (over_budget(s, drawn)) return 0;
         draw_char(s, fb, pitch, i, s->vram[i]);
         s->shown[i] = s->vram[i];
         drawn++;
      }
   }
   // rolling refresh repairs the picture if anything else touched it
   for (int n = 0; n < CV_REFRESH_CHARS; n++) {
      if (over_budget(s, drawn)) break;
      int i = s->refresh_pos;
      draw_char(s, fb, pitch, i, s->vram[i]);
      s->refresh_pos = (i + 1) & (CV_VRAM_SIZE - 1);
      drawn++;
   }
   return 1;
}

// ---------------------------------------------------------------------
// Multi-buffer controller
// ---------------------------------------------------------------------

int cv_stop(cv_state_t *s) {
   int was_on = s->w64_on;
   s->w64_on = 0;
   s->shown_valid = 0;
   s->display_buf = -1;
   s->build_buf = -1;
   s->full_pos = -1;
   s->rows_valid = 0;
   s->lc_buf = -1;
   s->vg_buf = -1;
   cv_leave_count = 0;
   return was_on;
}

void cv_invalidate(cv_state_t *s) {
   // the screen was cleared (mode change): the W64 picture and its CG2
   // border are gone, so start again from the next captured CG2 frame
   cv_stop(s);
}

// 32 column lower case (below)
static int lowercase_32_fast(cv_state_t *s, uint8_t *fb, int pitch, int *pos, int limited, int *count);
static int lowercase_fast_ok(const cv_state_t *s, const uint8_t *fb, int pitch);

// a buffer for the next capture that is neither on screen nor the W64 one
static int spare_buffer(int keep, int displayed, int nbuffers) {
   int c = (keep + 1) % nbuffers;
   if (c == displayed) c = (c + 1) % nbuffers;
   if (c == keep) c = (c + 1) % nbuffers;
   return c;
}

static cv_frame_t frame_inner(cv_state_t *s, int captured, int displayed, int nbuffers,
                              cv_buffer_fn buffer, int pitch, int width, int height,
                              int *capture_next, int *inhibit_flip, int *swap_to);

int cv_text_tags_on(const cv_regs_t *r) {
   return (r->extras & 0xA0) != 0;
}

// text tags (see cv_state_t): this frame was captured with the other pair
// table than the one its picture needs. Text frames want the text tags
// while text colours or inverse video are on, all other frames none.
static int tags_wrong(const cv_state_t *s, cv_frame_t f) {
   if (!s->tags_possible || f == CV_FRAME_REGISTER) return 0;
   int want = f == CV_FRAME_TEXT && cv_text_tags_on(&s->regs);
   return want != (s->cap_text_tags != 0);
}

// such a frame is not shown (nor converted or fixed up): the picture on
// screen stays for this frame and the next capture goes into the same
// buffer, now with the right table. Returns 1 if the frame is held.
static int tag_hold(cv_state_t *s, cv_frame_t f, int captured, int displayed,
                    int *capture_next, int *inhibit_flip) {
   if (!tags_wrong(s, f) || s->tag_holds >= CV_TAG_HOLDS || captured == displayed) return 0;
   s->tag_holds++;
   s->tag_held++;
   *capture_next = captured;
   *inhibit_flip = 1;
   return 1;
}

cv_frame_t cv_frame(cv_state_t *s, int captured, int displayed, int nbuffers,
                    cv_buffer_fn buffer, int pitch, int width, int height,
                    int *capture_next, int *inhibit_flip, int *swap_to) {
   cv_frame_t f = frame_inner(s, captured, displayed, nbuffers, buffer, pitch, width, height,
                              capture_next, inhibit_flip, swap_to);
   // a register page whose palette words are still being copied: keep its
   // buffer from being captured over (or finish now if no buffer is free)
   if (s->pal_src && s->pal_buf < 0) s->pal_buf = captured;
   if (s->pal_src && (*capture_next < 0 || *capture_next == s->pal_buf)) {
      int disp = !*inhibit_flip ? captured : (*swap_to >= 0 ? *swap_to : displayed);
      int own = s->w64_on ? s->display_buf : s->build_buf;
      int c;
      for (c = 0; c < nbuffers; c++) {
         if (c != s->pal_buf && c != disp && c != own && c != s->lc_buf && c != s->vg_buf) break;
      }
      if (c < nbuffers) *capture_next = c;
      else pal_continue(s, 1);
   }
   // which pair table the next capture needs: text frames are mostly
   // followed by text frames (a register page changes nothing here)
   if (!tags_wrong(s, f)) s->tag_holds = 0;
   if (f != CV_FRAME_REGISTER) s->text_last = f == CV_FRAME_TEXT;
   s->text_tags_next = s->text_last && cv_text_tags_on(&s->regs);
   return f;
}

static cv_frame_t frame_inner(cv_state_t *s, int captured, int displayed, int nbuffers,
                              cv_buffer_fn buffer, int pitch, int width, int height,
                              int *capture_next, int *inhibit_flip, int *swap_to) {
   *capture_next = -1;
   *inhibit_flip = 0;
   *swap_to = -1;
   if (s->usec) s->t_frame = s->usec();
   if (nbuffers < 2) {
      cv_stop(s);
      return CV_FRAME_OTHER;
   }
   // The W64 picture must stay in the buffer on screen. If something else
   // (e.g. the probe after a mode change) showed another buffer, start over.
   if (s->w64_on && displayed != s->display_buf) {
      cv_stop(s);
   }
   cv_frame_t f = cv_analyze(s, buffer(captured), pitch, width, height);
   // palette words of a register page, within the work limit (a register
   // page frame has done its share in check_register_page)
   if (s->pal_pending && f != CV_FRAME_REGISTER) pal_continue(s, 0);
   // the buffer W64 owns (being built or on screen), -1 if none
   int own = s->w64_on ? s->display_buf : s->build_buf;

   if (f == CV_FRAME_REGISTER) {
      // CoCoVGA keeps showing the previous picture while it reads the
      // register page, so don't flip this frame to the screen.
      if (own >= 0) {
         *capture_next = spare_buffer(own, displayed, nbuffers);
         *inhibit_flip = 1;
      } else if (captured != displayed) {
         *capture_next = captured;
         *inhibit_flip = 1;
      }
      return f;
   }

   if (f == CV_FRAME_CG2 && (s->regs.enhanced & 0x02) && renderable(s)) {
      cv_leave_count = 0;
      if (!s->w64_on) {
         // enter W64: build the 64x32 screen over the captured CG2 picture
         // (which has the right border), off screen, and show it when
         // complete; with Num Buffers = 0 the capture is already on screen
         int done;
         if (s->build_buf < 0 && tag_hold(s, f, captured, displayed, capture_next, inhibit_flip)) {
            return f;      // (its border has text tags)
         }
         if (s->build_buf < 0) {
            s->build_buf = captured;
            done = cv_render(s, buffer(captured), pitch, 1);
         } else {
            done = cv_render(s, buffer(s->build_buf), pitch, 0);
         }
         if (done && s->rows_valid >= 64) {   // complete, up to date, every row decoded
            s->w64_on = 1;
            s->display_buf = s->build_buf;
            s->build_buf = -1;
            *swap_to = s->display_buf;
            displayed = s->display_buf;
         }
         own = s->w64_on ? s->display_buf : s->build_buf;
      } else {
         cv_render(s, buffer(s->display_buf), pitch, 0);
      }
      *capture_next = spare_buffer(own, displayed, nbuffers);
      *inhibit_flip = 1;
      return f;
   }

   if (f != CV_FRAME_OTHER || s->w64_on) s->entry_holds = 0;
   if (f == CV_FRAME_OTHER && s->cg_unusable && !s->w64_on && (s->regs.enhanced & 0x02) &&
       s->entry_holds < CV_ENTRY_HOLDS) {
      // entering W64 (see cg_unusable): keep the previous picture for this
      // frame instead of showing the raw CG2 data
      s->entry_holds++;
      if (own >= 0) {
         *capture_next = spare_buffer(own, displayed, nbuffers);
         *inhibit_flip = 1;
      } else if (captured != displayed) {
         *capture_next = captured;
         *inhibit_flip = 1;
      }
      return f;
   }

   if (own >= 0) {
      if (++cv_leave_count < CV_LEAVE_FRAMES && (s->regs.enhanced & 0x02)) {
         // ride over a single odd frame
         *capture_next = spare_buffer(own, displayed, nbuffers);
         *inhibit_flip = 1;
         return f;
      }
      cv_stop(s);       // let this frame be flipped to the screen normally
   }

   if (f == CV_FRAME_VG6 && s->hs <= 8) {
      // convert a captured CG6 frame off screen (within the work limit,
      // possibly over several frames; newer captures are skipped
      // meanwhile) and show it when complete
      s->vg_holds = 0;
      if (s->vg_buf < 0) {
         if (captured == displayed) {
            // captured straight onto the screen (Num Buffers = 0): convert
            // the next capture instead, which goes off screen
            *capture_next = spare_buffer(displayed, displayed, nbuffers);
            *inhibit_flip = 1;
            return f;
         }
         if (tag_hold(s, f, captured, displayed, capture_next, inhibit_flip)) return f;
         s->vg_buf = captured;
         s->vg_pos = 0;
         s->vg_css = s->css_vg6;
      }
      if (vg6_convert(s, buffer(s->vg_buf), pitch)) {
         *swap_to = s->vg_buf;
         displayed = s->vg_buf;
         s->vg_buf = -1;
      }
      *capture_next = spare_buffer(s->vg_buf >= 0 ? s->vg_buf : displayed, displayed, nbuffers);
      *inhibit_flip = 1;
      return f;
   }
   if (f == CV_FRAME_OTHER && s->cg_unusable && (s->regs.enhanced & 0x01) &&
       !(s->regs.enhanced & 0x02) && s->vg_holds < CV_ENTRY_HOLDS) {
      // a CG frame that changed mode in its top border: keep the picture on
      // screen (and a conversion in progress) instead of showing it raw
      s->vg_holds++;
      *capture_next = spare_buffer(s->vg_buf >= 0 ? s->vg_buf : displayed, displayed, nbuffers);
      *inhibit_flip = 1;
      return f;
   }
   s->vg_buf = -1;                              // abandon a conversion in progress

   if (f == CV_FRAME_TEXT && cv_lowercase_on(s)) {
      // 32 column lower case: fix up a captured screen off screen (within
      // the work limit, possibly over several frames; newer captures are
      // skipped meanwhile) and show it when complete. Capture always goes to
      // a buffer that is not on screen (even with "Num Buffers" = 0), so a
      // half done fix-up is never seen.
      if (s->lc_buf < 0 && tag_hold(s, f, captured, displayed, capture_next, inhibit_flip)) return f;
      if (s->lc_buf < 0) {
         s->lc_buf = captured;
         s->lc_pos = 0;
      }
      int done;
      uint8_t *fb = buffer(s->lc_buf);
      if (lowercase_fast_ok(s, fb, pitch)) {
         int count = 0;
         done = lowercase_32_fast(s, fb, pitch, &s->lc_pos, 1, &count);
      } else {
         cv_lowercase_32(s, fb, pitch);        // general path: all at once
         done = 1;
      }
      if (done) {
         *swap_to = s->lc_buf;
         displayed = s->lc_buf;
         s->lc_buf = -1;
      }
      *capture_next = spare_buffer(s->lc_buf >= 0 ? s->lc_buf : displayed, displayed, nbuffers);
      *inhibit_flip = 1;
   } else {
      s->lc_buf = -1;                           // abandon a fix-up in progress
      tag_hold(s, f, captured, displayed, capture_next, inhibit_flip);
   }
   return f;
}

// ---------------------------------------------------------------------
// 32 column lower case
// ---------------------------------------------------------------------

// Fast path. An inverse cell is one of only 64 known glyphs, so far fewer
// than its 96 dots are needed to tell which one it is:
//  - the first LC_ID dots below give every 6847 glyph a different pattern
//    (identification by table lookup)
//  - all LC_ALL dots are then checked against the identified glyph. Over
//    these dots any two glyphs differ in at least 3 dots, the same minimum
//    as over all 96, so it takes 3 wrong dots at once to show a wrong
//    letter; 1 or 2 wrong dots leave the cell inverse for that frame.
// The lower case glyph is then written with 32 bit stores (4 pixels each).
// Used with the profile's double width and height on a 4 pixel boundary;
// anything else takes the general path below.
// (dot sets from a greedy search over mc6847_rom_font; checked by the harness)
#define LC_ID   11
#define LC_ALL  28
static const uint8_t lc_dots[LC_ALL][2] = {   // { row, column } in the 8x12 cell
   {3,3}, {5,2}, {6,4}, {8,6}, {7,4}, {7,3}, {9,3}, {4,6}, {4,5}, {5,3}, {6,6}, {3,2}, {6,3}, {9,6}, {4,3}, {5,4}, {5,6}, {8,2}, {3,5}, {6,5}, {9,5}, {6,2}, {4,4}, {5,5}, {7,2}, {3,6}, {8,3}, {8,5}
};
int cv_lowercase_fast = 1;          // 0 = always take the general path (tests)
static int lc_ready = 0, lc_ok = 0;
static int8_t lc_map[1 << LC_ID];
static uint32_t lc_expect[64];
static uint32_t lc_dontcare[64];    // dots where the two forms of E / F / 3 differ

// 'e' ($05) and 'f' ($06) are the one pair that differ in only 3 of the
// LC_ALL dots above - lc_dots[EF_D0..EF_D2], indices 6/13/20, all row 9 of
// the cell - and nowhere else, so the ordinary exact match (every dot has
// to fit) has the least margin for this pair of any in the table: on real
// hardware they're the letters most likely to be read with a dot wrong and
// left inverse. {9,4}, a 4th dot where they differ, is read on demand below
// instead of being added to lc_dots (which would cost every other glyph an
// extra dot in its signature for no benefit to it).
#define EF_D0 6     // lc_dots[6]  = {9,3}
#define EF_D1 13    // lc_dots[13] = {9,6}
#define EF_D2 20    // lc_dots[20] = {9,5}
#define EF_MASK ((1u << EF_D0) | (1u << EF_D1) | (1u << EF_D2))
static const uint8_t ef_extra_dot[2] = { 9, 4 };   // row 9, column 4

// Called only once the ordinary exact match has already failed for a cell
// whose LC_ID lookup pointed at 'e' or 'f' - i.e. every one of the LC_ALL
// dots fit one of the two except possibly the row 9 ones. If some other
// dot is also wrong, this cell isn't really 'e' or 'f' with a bad dot, so
// bail out exactly as the ordinary check would (returns -1, same as
// "leave it inverse"). Otherwise decide by a vote over all 4 dots that
// tell 'e' and 'f' apart (the usual 3, plus the one at column 4 sampled
// here): a clean majority (3-1 or 4-0) is trusted, a 2-2 split abstains -
// so this can only ever resolve to 'e', to 'f', or to "leave it inverse",
// never to any other letter.
static int ef_tiebreak(const uint8_t *fb, int pitch, int x0, int y0, uint8_t corner, uint8_t light, uint32_t sig) {
   if ((sig ^ lc_expect[5]) & ~((uint32_t) EF_MASK | lc_dontcare[5])) return -1;   // a non-row-9 dot is wrong: not this
   int c4 = fb[(y0 + ef_extra_dot[0] * 2) * pitch + x0 + ef_extra_dot[1] * 2] & CV_CODE_MASK;
   int bit3;
   if (c4 == light) bit3 = 1;
   else if (c4 == corner) bit3 = 0;
   else return -1;                                             // unreadable: leave it
   // bit=1 matches 'e' at each of these 4 dots, bit=0 matches 'f' (checked
   // against mc6847_rom_font by the test harness)
   int e_votes = ((sig >> EF_D0) & 1) + ((sig >> EF_D1) & 1) + ((sig >> EF_D2) & 1) + bit3;
   if (e_votes >= 3) return 5;
   if (e_votes <= 1) return 6;
   return -1;                                                  // 2-2: genuine tie, leave it
}

static uint32_t lc_sig_of_glyph(int code) {
   uint32_t sig = 0;
   for (int j = 0; j < LC_ALL; j++) {
      if (mc6847_rom_font[code][lc_dots[j][0]] & (0x80 >> lc_dots[j][1])) sig |= 1u << j;
   }
   return sig;
}

static void lc_init(void) {
   lc_ready = 1;
   lc_ok = 0;
   memset(lc_map, -1, sizeof(lc_map));
   for (int i = 0; i < 64; i++) {
      lc_expect[i] = lc_sig_of_glyph(i);
      lc_dontcare[i] = 0;
      // (only $00-$1F are turned into lower case; '3' needs no second form here)
      int alt = i < 32 ? rom_alt_row6(i) : -1;
      for (int j = 0; alt >= 0 && j < LC_ALL; j++) {
         if (lc_dots[j][0] == 6 && ((alt ^ mc6847_rom_font[i][6]) & (0x80 >> lc_dots[j][1]))) lc_dontcare[i] |= 1u << j;
      }
      // the identification dots must not depend on which form it is
      if (lc_dontcare[i] & ((1u << LC_ID) - 1)) return;
      unsigned int id = lc_expect[i] & ((1u << LC_ID) - 1);
      if (lc_map[id] >= 0) return;          // not unique: use the general path
      lc_map[id] = (int8_t) i;
   }
   lc_ok = 1;
}

// minimum number of dots (of the LC_ALL checked) in which two 6847 glyphs
// differ, not counting the dots where a glyph's two forms differ (E, F, 3);
// exported for the test harness
int cv_lowercase_min_distance(void) {
   if (!lc_ready) lc_init();
   int m = 99;
   for (int i = 0; i < 64; i++) {
      for (int j = i + 1; j < 64; j++) {
         for (int k = 0; k < 2; k++) {
            uint32_t v = (lc_expect[i] ^ lc_expect[j]) & ~(k ? lc_dontcare[j] : lc_dontcare[i]);
            int d = 0;
            while (v) { d += v & 1; v >>= 1; }
            if (d < m) m = d;
         }
      }
   }
   return lc_ok ? m : 0;
}

// cells from *pos on; stops at the work limit (limited != 0) and returns 0,
// or returns 1 when all 512 cells are done. *count += cells changed.
static int lowercase_32_fast(cv_state_t *s, uint8_t *fb, int pitch, int *pos, int limited, int *count) {
   uint32_t keep[24];
   int keep_row = -1, done = 0, written = 0;
   const int style = s->lc_font;
   for (int cell = *pos; cell < 512; cell++) {
      if (limited && (done & 7) == 0 && done) {
         if ((s->max_chars && written >= s->max_chars) ||
             (s->usec && done >= CV_MIN_LC_CELLS && s->usec() - s->t_frame >= s->budget_us)) {
            *pos = cell;
            return 0;
         }
      }
      done++;
      const int cy = cell >> 5, cx = cell & 31;
      const int y0 = s->oy + cy * 24;
      const int x0 = s->ox + cx * 16;
      int corner = fb[y0 * pitch + x0] & CV_CODE_MASK;
      if (corner != CV_DKGREEN && corner != CV_DKORANGE) continue;
      const uint8_t light = corner == CV_DKORANGE ? CV_LORANGE : CV_GREEN;
      uint32_t sig = 0;
      int ok = 1;
      for (int j = 0; j < LC_ALL; j++) {
         int c = fb[(y0 + lc_dots[j][0] * 2) * pitch + x0 + lc_dots[j][1] * 2] & CV_CODE_MASK;
         if (c == light) sig |= 1u << j;
         else if (c != corner) { ok = 0; break; }
      }
      if (!ok) continue;
      int code = lc_map[sig & ((1u << LC_ID) - 1)];
      if (code < 0 || code >= 32) continue;   // $20-$3F stay inverse, as on the 6847T1
      if ((sig ^ lc_expect[code]) & ~lc_dontcare[code]) {
         if (code != 5 && code != 6) continue;        // dots don't fit that glyph: leave it
         code = ef_tiebreak(fb, pitch, x0, y0, corner, light, sig);
         if (code < 0) continue;                      // not actually 'e'/'f', or a genuine tie
      }
      if (keep_row != cy) {
         // scanline / upper bits: the same along a frame buffer line
         for (int l = 0; l < 24; l++) keep[l] = (fb[(y0 + l) * pitch + x0] & ~CV_CODE_MASK) * 0x01010101u;
         keep_row = cy;
      }
      // a pair of dots -> one 32 bit word (2 pixels per dot, little endian)
      uint32_t pair[4];
      for (int b = 0; b < 4; b++) {
         uint32_t c1 = (b & 2) ? corner : light;      // glyph dots are dark
         uint32_t c2 = (b & 1) ? corner : light;
         pair[b] = c1 | (c1 << 8) | (c2 << 16) | (c2 << 24);
      }
      const uint8_t *glyph = cv_lower_glyph(code, style);   // next to the 6847's capitals
      const int odd = x0 & 2;                     // cell starts 2 pixels off a word
      for (int dy = 0; dy < 12; dy++) {
         const unsigned int g = glyph[dy];
         const uint32_t w0 = pair[(g >> 6) & 3], w1 = pair[(g >> 4) & 3];
         const uint32_t w2 = pair[(g >> 2) & 3], w3 = pair[g & 3];
         for (int k = 0; k < 2; k++) {
            uint8_t *p = fb + (y0 + dy * 2 + k) * pitch + x0;
            const uint32_t kw = keep[dy * 2 + k];
            if (!odd) {
               uint32_t *q = (uint32_t *) p;
               q[0] = w0 | kw; q[1] = w1 | kw; q[2] = w2 | kw; q[3] = w3 | kw;
            } else {
               // 16 + 32 + 32 + 32 + 16 bit stores
               ((uint16_t *) p)[0] = (uint16_t) (w0 | kw);
               ((uint32_t *) (p + 2))[0] = ((w0 >> 16) | (w1 << 16)) | kw;
               ((uint32_t *) (p + 6))[0] = ((w1 >> 16) | (w2 << 16)) | kw;
               ((uint32_t *) (p + 10))[0] = ((w2 >> 16) | (w3 << 16)) | kw;
               ((uint16_t *) (p + 14))[0] = (uint16_t) ((w3 >> 16) | kw);
            }
         }
      }
      written++;
   }
   *pos = 512;
   *count += written;
   return 1;
}

static int lowercase_fast_ok(const cv_state_t *s, const uint8_t *fb, int pitch) {
   return cv_lowercase_fast && lc_ok && s->hs == 2 && s->vs == 2 && !(s->ox & 1) && !(pitch & 3)
          && !(((uintptr_t) fb) & 3);
}

int cv_lowercase_on(const cv_state_t *s) {
   return s->lc_on_mode < 0 ? (s->regs.font & 0x02) != 0 : s->lc_on_mode;
}

int cv_lowercase_32(cv_state_t *s, uint8_t *fb, int pitch) {
   if (!s->cal_valid) return 0;
   const int hs = s->hs, vs = s->vs;
   if (!lc_ready) lc_init();
   if (lowercase_fast_ok(s, fb, pitch)) {
      int pos = 0, count = 0;
      lowercase_32_fast(s, fb, pitch, &pos, 0, &count);
      return count;
   }
   int count = 0;
   const int style = s->lc_font;
   for (int cy = 0; cy < 16; cy++) {
      for (int cx = 0; cx < 32; cx++) {
         int x0 = s->ox + cx * 8 * hs;
         int y0 = s->oy + cy * 12 * vs;
         // inverse characters ($00-$3F) have a dark background; every
         // 6847 glyph leaves the top left dot empty
         int corner = code_at(fb, pitch, x0, y0);
         if (corner != CV_DKGREEN && corner != CV_DKORANGE) continue;
         int css = corner == CV_DKORANGE;
         uint8_t light = css ? CV_LORANGE : CV_GREEN;
         uint8_t dark = css ? CV_DKORANGE : CV_DKGREEN;
         uint8_t strokes[12];
         int ok = 1;
         for (int dy = 0; dy < 12 && ok; dy++) {
            const uint8_t *line = fb + (y0 + dy * vs) * pitch + x0;
            unsigned int bits = 0;
            for (int dx = 0; dx < 8; dx++) {
               int c = line[dx * hs] & CV_CODE_MASK;
               if (c == light) bits |= 0x80 >> dx;
               else if (c != dark) { ok = 0; break; }
            }
            strokes[dy] = bits;
         }
         if (!ok) continue;
         int code = -1;
         for (int i = 0; i < 32; i++) {
            if (rom_glyph_is(strokes, i)) { code = i; break; }
         }
         if (code < 0) continue;      // $20-$3F stay inverse, as on the 6847T1
         const uint8_t *glyph = cv_lower_glyph(code, style);   // next to the 6847's capitals
         for (int dy = 0; dy < 12; dy++) {
            for (int k = 0; k < vs; k++) {
               uint8_t *p = fb + (y0 + dy * vs + k) * pitch + x0;
               uint8_t keep = p[0] & ~CV_CODE_MASK;    // keep scanline bits
               for (int dx = 0; dx < 8; dx++) {
                  uint8_t c = ((glyph[dy] >> (7 - dx)) & 1) ? dark : light;
                  for (int j = 0; j < hs; j++) *p++ = c | keep;
               }
            }
         }
         count++;
      }
   }
   return count;
}

// ---------------------------------------------------------------------
// MC6847 NTSC artifact colours
// ---------------------------------------------------------------------

int cv_artifact_index(int first, int second, int mode, int flags) {
   if (mode == 0 || first == second) return -1;
   int lit_first;
   int css;
   if (first == CV_BLACK && second == CV_BUFF)       { lit_first = 0; css = 1; }
   else if (first == CV_BUFF && second == CV_BLACK)  { lit_first = 1; css = 1; }
   else if (first == CV_BLACK && second == CV_GREEN) { lit_first = 0; css = 0; }
   else if (first == CV_GREEN && second == CV_BLACK) { lit_first = 1; css = 0; }
   else if ((flags & CV_ART_GREEN_DARK_BG) && first == CV_DKGREEN && second == CV_GREEN) { lit_first = 0; css = 0; }
   else if ((flags & CV_ART_GREEN_DARK_BG) && first == CV_GREEN && second == CV_DKGREEN) { lit_first = 1; css = 0; }
   else return -1;
   int a = (mode == 1) ? lit_first : !lit_first;
   if (css) return a ? CV_ART_CSS1_A : CV_ART_CSS1_B;
   return a ? CV_ART_CSS0_A : CV_ART_CSS0_B;
}

int cv_artifact_flags_for(const uint8_t *fb, int pitch, int x0, int y0, int w, int h) {
   // The border is green only for graphics modes in the green colour set;
   // text has a black border. Sample the middle of the capture's first and
   // last lines (inside the 6847 border); both must be green, so a screen
   // that switches to text part way down (split screen) keeps plain text.
   if (w < 64 || h < 8) return 0;
   int green = 0;
   const int xm = x0 + w / 2 - 24;
   for (int x = 0; x < 7; x++) {
      green += (fb[(y0 + 2) * pitch + xm + x * 8] & CV_CODE_MASK) == CV_GREEN;
      green += (fb[(y0 + h - 3) * pitch + xm + x * 8] & CV_CODE_MASK) == CV_GREEN;
   }
   return green >= 13 ? CV_ART_GREEN_DARK_BG : 0;
}

// A dot of a text cell (CV_ART_TEXT). Dark green, dark orange and light
// orange only ever come from text. A green dot is text next to green or dark
// green, semigraphics next to black or another colour: semigraphics cells
// change colour only at 4 dot boundaries, so with pairs on even dots a
// semigraphics pair is one colour - and a green one can't be told from the
// background of text (as a space and a solid green block look the same).
static int text_dot(int c, int partner) {
   if (c == CV_DKGREEN || c == CV_DKORANGE || c == CV_LORANGE) return 1;
   return c == CV_GREEN && (partner == CV_GREEN || partner == CV_DKGREEN);
}

void cv_build_artifact_table(uint32_t *table, int mode, int flags) {
   for (int first = 0; first < 16; first++) {
      for (int second = 0; second < 16; second++) {
         int a = cv_artifact_index(first, second, mode, flags);
         uint32_t w;
         w = (uint32_t) first * 0x0101u | (uint32_t) second * 0x01010000u;
         if (a >= 0) {
            w |= (uint32_t) a * 0x01010101u;     // tag both dots, keep their colours
         } else if (flags & CV_ART_TEXT) {
            if (text_dot(first, second)) w |= (uint32_t) CV_TEXT_TAG * 0x0101u;
            if (text_dot(second, first)) w |= (uint32_t) CV_TEXT_TAG * 0x01010000u;
         }
         table[first | (second << 4)] = w;
      }
   }
}

// Colours of a dot pair on an NTSC monitor, taken from XRoar's simulated
// composite video (-ccr simulated) of a PMODE 4 test card on an NTSC
// Dragon: buff set orange-red / blue, green set dark green / mid green.
int cv_artifact_palette_entry(int index) {
   int r, g, b;
   int lit = index & CV_CODE_MASK;
   switch (index & 0x70) {
      case CV_ART_CSS1_A: r = 0xA1; g = 0x3E; b = 0x05; if (lit != CV_BUFF && lit != CV_BLACK) return -1; break;   // orange-red
      case CV_ART_CSS1_B: r = 0x16; g = 0x54; b = 0xFF; if (lit != CV_BUFF && lit != CV_BLACK) return -1; break;   // blue
      case CV_ART_CSS0_A: r = 0x0F; g = 0x33; b = 0x14; if (lit != CV_GREEN && lit != CV_BLACK && lit != CV_DKGREEN) return -1; break;  // dark green
      case CV_ART_CSS0_B: r = 0x03; g = 0x80; b = 0x03; if (lit != CV_GREEN && lit != CV_BLACK && lit != CV_DKGREEN) return -1; break;  // mid green
      default: return -1;
   }
   unsigned int m = (299u * r + 587u * g + 114u * b + 500u) / 1000u;
   return (int) ((m << 24) | ((unsigned) b << 16) | ((unsigned) g << 8) | (unsigned) r);
}

// ---------------------------------------------------------------------
// CoCoVGA palette registers
// ---------------------------------------------------------------------

// semigraphics palette slot of each LumaCode colour code, -1 = none. Text
// in the orange colour set (light orange) uses CoCoVGA's orange slot.
static const int8_t sg_slot_of_code[16] = {
   [CV_BLANK] = -1, [CV_BLACK] = 0, [CV_RED] = 4, [CV_LORANGE] = 8, [CV_DKORANGE] = 10,
   [CV_DKGREEN] = 9, [CV_BLUE] = 3, [CV_BUFF] = 5, [CV_ORANGE] = 8, [CV_MAGENTA] = 7,
   [CV_CYAN] = 6, [11] = -1, [CV_GREEN] = 1, [CV_YELLOW] = 2, [14] = -1, [15] = -1,
};

// 5 bit R, G, B -> palette_data word (m << 24 | b << 16 | g << 8 | r)
static uint32_t rgb15(unsigned int R, unsigned int G, unsigned int B) {
   R = (R << 3) | (R >> 2); G = (G << 3) | (G >> 2); B = (B << 3) | (B >> 2);
   unsigned int m = (299u * R + 587u * G + 114u * B + 500u) / 1000u;
   return (m << 24) | (B << 16) | (G << 8) | R;
}

// CoCoVGA's default artifact / VG6 colours (Artifact Emulation p.1-3; 5 bit
// R, G, B): colour set 0 shades of green, colour set 1 the MESS colours.
// Colour set 0 slot 0 is misprinted there; black is used.
static const uint8_t vg6_default[2][16][3] = {
   { {0,0,0}, {0,8,0}, {0,8,0}, {0,24,0}, {0,28,0}, {0,8,0}, {0,8,0}, {0,16,0},
     {0,20,0}, {0,24,0}, {0,16,0}, {0,28,0}, {0,28,0}, {0,8,0}, {0,8,0}, {0,31,0} },
   { {0,0,0}, {4,0,4}, {0,4,0}, {28,24,28}, {24,28,24}, {0,4,20}, {12,4,0}, {12,16,28},
     {28,24,8}, {28,8,8}, {0,16,28}, {24,28,28}, {28,28,12}, {0,0,4}, {4,0,0}, {31,31,31} },
};

// a palette slot as a palette_data word (m << 24 | b << 16 | g << 8 | r,
// m = luminance) in *rgb: returns 0 when the slot is not enabled (default
// colour). (Not an int with -1 for "none": bright colours have m >= 128.)
int cv_pal_rgb(const cv_regs_t *r, int slot, uint32_t *rgb) {
   if (slot < 0 || slot >= CV_PAL_SLOTS) return 0;
   unsigned int v = r->pal[slot];
   if (!(v & CV_PAL_ENABLE)) return 0;
   *rgb = rgb15((v >> 10) & 31, (v >> 5) & 31, v & 31);
   return 1;
}

// The colour CoCoVGA's registers give frame buffer value index, in *rgb;
// returns 0 for "as the profile's palette". base16[c] is the profile's
// colour of LumaCode code c. Plain codes take the semigraphics palette
// (text included: CoCoVGA draws text with it unless extras bit 7 asks for
// the text palette, which needs a redraw, see COCOVGA.md); with extras bit
// 6 clear (9 colour palette) dark green / dark orange are drawn as black.
// Artifact dots (tagged in bits 4-6) take the artifact palette of their
// colour set: colour A (orange-red / dark green by default) slot 9,
// CoCoVGA's red, colour B (blue / mid green) slot 10, its blue.
int cv_custom_palette_entry(const cv_regs_t *r, int index, const uint32_t *base16, uint32_t *rgb) {
   int code = index & CV_CODE_MASK;
   int tag = index & 0x70;
   if (tag == CV_TEXT_TAG) {
      // text dot: the dark dots of normal text are the foreground (slot 48),
      // the light ones the background (slot 49); inverse video swaps them
      static const uint8_t swap[16] = { [CV_GREEN] = CV_DKGREEN, [CV_DKGREEN] = CV_GREEN,
                                        [CV_LORANGE] = CV_DKORANGE, [CV_DKORANGE] = CV_LORANGE };
      int text = swap[code] != 0;
      int dark = code == CV_DKGREEN || code == CV_DKORANGE;
      int c = code;
      if (text && (r->extras & 0x20)) {
         dark = !dark;
         c = swap[code];
      }
      if (text && (r->extras & 0x80) && cv_pal_rgb(r, CV_PAL_EXTRA + (dark ? 0 : 1), rgb)) return 1;
      // else the semigraphics palette's colour (of the swapped dot)
      if (!cv_custom_palette_entry(r, c, base16, rgb)) *rgb = base16[c];
      return 1;
   }
   if (tag == 0) {
      int slot = sg_slot_of_code[code];
      if (slot < 0) return 0;
      if ((slot == 9 || slot == 10) && !(r->extras & 0x40)) {
         if (!cv_pal_rgb(r, 0, rgb)) *rgb = base16[CV_BLACK];
         return 1;
      }
      return cv_pal_rgb(r, CV_PAL_SG + slot, rgb);
   }
   if (tag == CV_VG6_CSS1 || tag == CV_VG6_CSS0) {
      int first = tag == CV_VG6_CSS1 ? CV_PAL_ART_CSS1 : CV_PAL_ART_CSS0;
      if (cv_pal_rgb(r, first + code, rgb)) return 1;
      const uint8_t *d = vg6_default[tag == CV_VG6_CSS1 ? 1 : 0][code];
      *rgb = rgb15(d[0], d[1], d[2]);
      return 1;
   }
   if (cv_artifact_palette_entry(index) < 0) return 0;
   switch (tag) {
      case CV_ART_CSS1_A: return cv_pal_rgb(r, CV_PAL_ART_CSS1 + 9, rgb);
      case CV_ART_CSS1_B: return cv_pal_rgb(r, CV_PAL_ART_CSS1 + 10, rgb);
      case CV_ART_CSS0_A: return cv_pal_rgb(r, CV_PAL_ART_CSS0 + 9, rgb);
      case CV_ART_CSS0_B: return cv_pal_rgb(r, CV_PAL_ART_CSS0 + 10, rgb);
   }
   return 0;
}

// The 6847 Artifacts setting (0 off, 1 red/blue, 2 blue/red) as CoCoVGA's
// artifact register changes it: colour off turns artifacts off, swap
// exchanges the two colours. The menu chooses the base order, as the
// 6847's colour phase at power up decides it on a real machine.
int cv_artifact_mode(const cv_regs_t *r, int menu_mode) {
   if (menu_mode == 0 || !(r->artifact & 0x01)) return 0;
   return (r->artifact & 0x02) ? 3 - menu_mode : menu_mode;
}
