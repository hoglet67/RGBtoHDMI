/*
 * cocovga.h - CoCoVGA 64-column text emulation for MC6847 LumaCode capture
 *
 * The MC6847digitizer delivers an exact copy of the 6847 picture, one
 * colour code per 6847 dot. CoCoVGA programs (e.g. NitrOS-9 with the CoVGA
 * driver) switch on 64x32 text by writing a CoCoVGA register page and then
 * selecting the 6847's CG2 graphics mode. This module recognises the
 * register page when the 6847 displays it, reads the 2 kB of character
 * codes back out of the CG2 picture, and draws the 64x32 text screen the
 * way CoCoVGA does.
 *
 * The core (cv_*) is plain C with no RGBtoHDMI dependencies so it can be
 * tested on a PC (see cocovga_tools/).
 */
#ifndef COCOVGA_H
#define COCOVGA_H

#include <stdint.h>

// LumaCode colour codes of the MC6847digitizer (sample1 * 4 + sample2).
// These are the palette indices that land in the 8bpp frame buffer.
#define CV_BLANK      0
#define CV_BLACK      1
#define CV_RED        2
#define CV_LORANGE    3   // light orange: text in the orange colour set
#define CV_DKORANGE   4
#define CV_DKGREEN    5
#define CV_BLUE       6
#define CV_BUFF       7
#define CV_ORANGE     8
#define CV_MAGENTA    9
#define CV_CYAN      10
#define CV_GREEN     12
#define CV_YELLOW    13

#define CV_CODE_MASK 0x0F

#define CV_TEXT_COLS   64
#define CV_TEXT_ROWS   32
#define CV_VRAM_SIZE   (CV_TEXT_COLS * CV_TEXT_ROWS)   // 2048, = CG2 size

// 6847 active picture in dots
#define CV_ACTIVE_W  256
#define CV_ACTIVE_H  192

typedef enum {
   CV_FRAME_OTHER = 0,     // anything we don't care about
   CV_FRAME_TEXT,          // 6847 text/semigraphics screen (origin calibrated)
   CV_FRAME_REGISTER,      // CoCoVGA register page 0 shown in text mode
   CV_FRAME_CG2,           // CG2 graphics; character codes decoded
   CV_FRAME_VG6            // CG6 graphics shown as CoCoVGA's 128x96 16 colour VG6
} cv_frame_t;

// CoCoVGA register page 0 values we emulate
#define CV_ENTRY_HOLDS 2

// CoCoVGA palette slots (numbered as CoCoVGA's BASIC extension does: slot n
// is register page bytes 32+2n, 32+2n+1). Each entry is big endian
// E RRRRR GGGGG BBBBB; E = 0 keeps the default colour. 9 bit (AMC2) boards
// use the top 3 bits of each channel.
#define CV_PAL_SG        0    // 0-10: black green yellow blue red buff cyan
                              //       magenta orange dark-green dark-orange
#define CV_PAL_SG_N      11
#define CV_PAL_ART_CSS1  16   // 16-31: artifact / VG6 colours, colour set 1
#define CV_PAL_ART_CSS0  32   // 32-47: the same for colour set 0
#define CV_PAL_EXTRA     48   // 48-51: text fg, text bg, border, scanline
#define CV_PAL_SLOTS     52
#define CV_PAL_ENABLE    0x8000

typedef struct {
   uint8_t font;       // reg 3: bit0 = 6847T1 font, bit1 = force lower case
   uint8_t artifact;   // reg 4: bit0 = colour artifacts, bit1 = swap, bits 3-2 style
   uint8_t extras;     // reg 5: bit5 = inverse-video text, bit6 = 10 colour SG palette,
                       //        bit7 = text colours (extra palette)
   uint8_t enhanced;   // reg 8: bit1 = 64-column text
   uint16_t pal[CV_PAL_SLOTS];   // palette slots (see above), 0 = default
} cv_regs_t;

typedef struct {
   // calibration: where the 256x192 active picture sits in the frame buffer
   int cal_valid;
   int ox, oy;          // top left of active picture, in fb pixels
   int hs, vs;          // fb pixels per 6847 dot (horizontal, vertical)
   int fb_w, fb_h, fb_pitch;   // fb geometry the calibration belongs to

   cv_regs_t regs;

   // decoded 64x32 screen from the most recent CG2 frame
   uint8_t vram[CV_VRAM_SIZE];
   int css;                     // 0 = green colour set, 1 = buff/orange

   // what is currently drawn in the display buffer
   uint8_t shown[CV_VRAM_SIZE];
   int shown_valid;
   int shown_css;
   int shown_font;
   int refresh_pos;            // rolling refresh position

   // multi-buffer control (see cv_frame)
   int w64_on;                 // W64 picture is being displayed
   int display_buf;            // buffer holding the W64 picture
   int build_buf;              // W64 picture being built (not yet shown), -1 = none
   int full_pos;               // next character of a full redraw, -1 = none

   // work limit per frame (so the hook finishes before the next vsync):
   // drawing stops once usec() - frame start >= budget_us (after at least
   // CV_MIN_CHARS characters) or after max_chars characters; the rest is
   // carried to the next frame. usec == NULL and max_chars == 0: no limit.
   unsigned int (*usec)(void);
   unsigned int budget_us;
   int max_chars;
   unsigned int t_frame;       // usec() at the start of this frame

   // CG2 decoding is spread over frames too: each frame decodes CG2 pixel
   // rows from decode_row on, at most decode_rows (0 = all 64) and, with
   // usec set, until decode_budget_us has passed (but at least
   // CV_MIN_DECODE_ROWS). A typed character shows within 64 / rows frames.
   int decode_rows;
   unsigned int decode_budget_us;
   int decode_row;             // next CG2 pixel row (0-63) to decode
   int rows_valid;             // rows decoded since W64 (re)started, up to 64

   // 32 column lower case, spread over frames like W64: a captured text
   // screen is fixed up off screen (cell lc_pos onwards, within the work
   // limit) and shown when complete
   int lc_buf;                 // buffer being fixed up, -1 = none
   int lc_pos;                 // next cell (0-511)
   // VG6 (enhanced bit 0): a captured CG6 frame is redrawn in place as
   // 128x96 pixels of 16 colours, row by row within the work limit
   // (possibly over several frames; newer captures are skipped meanwhile),
   // and shown when complete
   int vg_buf;                 // buffer being converted, -1 = none
   int vg_pos;                 // next VG6 row (0-95)
   int vg_css;                 // colour set of that frame (0 green, 1 buff)
   int css_vg6;                // colour set of the last frame cv_analyze took for VG6
   int vg_holds;               // frames kept back since the last VG6 frame (see cg_unusable)
   unsigned int vg6_frames;
   // text colours / inverse video in 32 columns (see CV_TEXT_TAG): the
   // capture tags text dots when its pair table has CV_ART_TEXT, which the
   // owner sets for the next frame from text_tags_next. Set by the owner
   // before cv_frame: tags_possible = the capture uses the pair table at all,
   // cap_text_tags = this frame was captured with CV_ART_TEXT. A frame
   // captured with the wrong table for what it shows (the first frame after
   // a change between text and graphics) is held back, at most
   // CV_TAG_HOLDS frames in a row.
   int tags_possible;
   int cap_text_tags;
   int text_tags_next;         // output: the next capture should tag text dots
   int text_last;              // the last frame that was not a register page was text
   int tag_holds;
   unsigned int tag_held;      // statistics: frames held for that
   // border modes: set by the owner before cv_frame. border_wanted = the
   // border capture is meant to run, cap_border = it captured this frame.
   // 64 columns and VG6 keep the border of the frame they are built over,
   // so they are not built over a frame without it (held like the above),
   // and a 64 column picture built without it is built again
   int border_wanted;
   int cap_border;
   int w64_border;             // the 64 column picture's frame had it
   // which lower case glyphs replace an inverse cell: CV_LC_6847T1 or
   // CV_LC_DRAGON200E. A real 6847T1 draws all its characters at the top
   // of the cell (rows 1-7, see cocovga_font.c), so next to the plain
   // 6847's capitals (rows 3-9) its lower case is moved down 2 rows; the
   // unmoved shapes are used only by W64 together with T1 capitals
   int lc_font;
   // whether inverse cells are replaced at all: -1 = follow regs.font bit 1
   // (the CoCoVGA lower case bit), 0 = never (plain 6847 inverse), 1 = always
   // (as a 6847T1 with lower case on - no register page write needed)
   int lc_on_mode;

   // the last frame looked like a CG picture but could not be used to
   // enter W64: its top border still had the previous mode's colour (the
   // mode changed after the top border began, as on a PAL Dragon, where
   // field sync comes late; the W64 picture keeps the captured border), or
   // a row was torn by a write during the scan. Up to CV_ENTRY_HOLDS such
   // frames in a row keep the previous picture instead of showing raw CG2.
   int cg_unusable;
   int entry_holds;

   // statistics (handy for the OSD / tests)
   unsigned int frames;
   unsigned int register_frames;
   unsigned int cg2_frames;
   unsigned int last_register[9];
   // palette: bumped whenever a register page changes a palette slot or the
   // artifact register (so the owner can rebuild its colour table); slots of
   // the last page that could not be read from the picture (a semigraphics
   // cell with no dots lit hides its colour bits)
   unsigned int pal_gen;
   int pal_unreadable;
   // palette words of a register page are decoded within the work limit,
   // possibly over several frames, into pal_new and applied when complete
   int pal_pending;            // palette groups (edit mask bits 3-5) still to read, 0 = none
   int pal_pos;                // next slot (index into the groups' slots)
   int pal_unread_new;
   uint16_t pal_new[CV_PAL_SLOTS];
   // the register page's text rows are copied line by line within the work
   // limit too, from pal_src (the captured buffer pal_buf, which cv_frame
   // keeps from being captured over meanwhile)
   const uint8_t *pal_src;
   int pal_pitch, pal_ox, pal_oy, pal_hs, pal_vs;
   int pal_line;               // next line of the copy (index into the lines needed)
   int pal_buf;                // buffer holding the page, -1 = not known (cv_analyze only)
} cv_state_t;

// Frame buffer accessor used by cv_frame
typedef uint8_t *(*cv_buffer_fn)(int index);

extern const uint8_t mc6847_font[64][12];
extern const uint8_t mc6847t1_font[96][12];
extern const uint8_t mc6847_rom_font[64][12];   // the 6847's own glyphs (for recognising cells)
extern const uint8_t dragon200e_lower_font[32][12];
// lower case glyph for code $00-$1F, in one of these styles:
#define CV_LC_6847T1      0   // MC6847T1 lower case moved down 2 rows to the
                              // baseline of the 6847's capitals (rows 3-9)
#define CV_LC_T1_HIGH     1   // as in the MC6847T1 table, next to T1 capitals
                              // (rows 1-7; W64 with the font register's T1 bit)
#define CV_LC_DRAGON200E  2   // the Dragon 200-E's lower case ROM
const uint8_t *cv_lower_glyph(int code, int style);

void       cv_init(cv_state_t *s);
void       cv_reset_registers(cv_state_t *s);

// Analyse one captured frame (8bpp, LumaCode colour codes).
cv_frame_t cv_analyze(cv_state_t *s, const uint8_t *fb, int pitch, int width, int height);

// Draw the decoded 64x32 screen into fb. full != 0 starts a redraw of every
// character, otherwise only changed characters plus a small rolling
// refresh. Stops at the work limit; returns 1 when fb is complete.
int        cv_render(cv_state_t *s, uint8_t *fb, int pitch, int full);

// Per-frame controller for RGBtoHDMI's multi-buffered capture.
//   captured     : buffer the frame was just captured into
//   displayed    : buffer currently on screen
//   nbuffers     : number of frame buffers available (4)
//   buffer       : returns the address of a buffer
// Outputs:
//   *capture_next: -1 = normal buffer choice, else capture the next frame
//                  into this buffer
//   *inhibit_flip: 1 = do NOT flip the frame just captured to the screen
//   *swap_to     : -1 = nothing, else make this buffer visible now
// Returns the frame classification.
cv_frame_t cv_frame(cv_state_t *s, int captured, int displayed, int nbuffers,
                    cv_buffer_fn buffer, int pitch, int width, int height,
                    int *capture_next, int *inhibit_flip, int *swap_to);

// 32 column text: when the font register forces lower case, replace the
// inverse characters $00-$1F of a captured text screen with lower case
// (as CoCoVGA does). Returns the number of characters replaced.
int        cv_lowercase_32(cv_state_t *s, uint8_t *fb, int pitch);
// 32 column lower case in effect (lc_on_mode, else the font register's bit)
int        cv_lowercase_on(const cv_state_t *s);
// minimum number of dots in which two 6847 glyphs differ over the dots the
// fast lower case path checks (for the test harness)
int        cv_lowercase_min_distance(void);
#define CV_HAVE_LOWERCASE_32 1

// ---------------------------------------------------------------------
// MC6847 NTSC artifact colours (used with capture_line_mc6847_8bpp.S)
// ---------------------------------------------------------------------
// An artifact dot keeps its own colour code in the low 4 bits (so the
// CoCoVGA decoders, which only look at those, never see artifacts, whatever
// the capture alignment) and gets an artifact tag in bits 4-6. The palette
// shows every dot carrying a tag in that tag's artifact colour.
#define CV_ART_CSS1_A 0x10    // buff/black pair, lit dot first
#define CV_ART_CSS1_B 0x20    // buff/black pair, lit dot second
#define CV_ART_CSS0_A 0x30    // green/black pair, lit dot first
#define CV_ART_CSS0_B 0x40    // green/black pair, lit dot second
// VG6 pixels: frame buffer value = tag | palette slot (0-15)
#define CV_VG6_CSS1   0x50    // colour set 1: artifact / VG6 palette slots 16-31
#define CV_VG6_CSS0   0x60    // colour set 0: slots 32-47
// Text dots (alphanumeric cells: green / dark green, light / dark orange):
// frame buffer value = tag | colour code. 32 column text gets the tag at
// capture time (pair table with CV_ART_TEXT), W64 when it draws. The colour
// table shows them in CoCoVGA's text colours (extras bit 7: extra palette
// slots 48 text foreground = the dark dots of normal text, 49 background =
// the light ones) and swaps the two for inverse video (extras bit 5);
// semigraphics keep the SG palette. Only used while cv_text_tags_on().
#define CV_TEXT_TAG   0x70
#define CV_TAG_HOLDS  2
// text colours or inverse video asked for (the text tags are needed)
int        cv_text_tags_on(const cv_regs_t *r);

// Border modes "black" and "custom" (extras bits 1-0 = 2, 3): a capture
// routine for them (capture_line_mc6847_8bpp.S) maps the 6847's border
// through a second pair table, which keeps the colour codes but marks the
// border colours with tag / code pairs nothing else uses. The colour table
// shows those in the border colour (black, or extra palette slot 50).
#define CV_BORDER_BLACK  (CV_TEXT_TAG | CV_BLACK)      // 0x71
#define CV_BORDER_BUFF   (CV_TEXT_TAG | CV_BUFF)       // 0x77
#define CV_BORDER_GREEN  (CV_ART_CSS1_A | CV_GREEN)    // 0x1C
// a border mode that needs the border capture
int        cv_border_mode_on(const cv_regs_t *r);
void       cv_build_border_table(uint32_t *table);
// The border capture's parameters for the picture where the calibration
// found it: params[0] = r5 (rgb_to_fb's line count down, from nlines) of the
// picture's first line, [1] = r5 of its last line, [2] = dot pairs before
// it on a line + 1. x0, y0: where the capture starts in the frame buffer,
// nlines: lines captured, dh: frame buffer lines per captured line. Needs
// double width (pairs of 4 pixels); returns 0 when it can't be used.
int        cv_border_params(const cv_state_t *s, int x0, int y0, int nlines, int dh, uint32_t *params);

// mode: 0 = off, 1 = red/blue, 2 = blue/red
// flags: CV_ART_GREEN_DARK_BG - in the green colour set the "black" dots of
// PMODE 4 are dark green (as XRoar draws them); only safe for graphics
// frames, see cv_artifact_flags_for(). CV_ART_TEXT - tag text dots with
// CV_TEXT_TAG (for text frames only: a green dot paired with green or dark
// green counts as text, so solid green semigraphics look like text then)
// table[first | second << 4] = 32 bit frame buffer word for the dot pair
// (4 pixels, double width, little endian)
#define CV_ART_GREEN_DARK_BG 1
#define CV_ART_TEXT          2
void       cv_build_artifact_table(uint32_t *table, int mode, int flags);
// flags for the next frame, from a captured frame: graphics in the green
// colour set (green border at the top and the bottom of the capture) ->
// CV_ART_GREEN_DARK_BG, else 0 (text keeps its dark green / green dots).
// x0, y0, w, h: where the capture is in the frame buffer (pixels / lines)
int        cv_artifact_flags_for(const uint8_t *fb, int pitch, int x0, int y0, int w, int h);
// palette value (0xMMBBGGRR, MM = luma) for an artifact index, else -1
// (all artifact colours are dark enough for MM < 128)
int        cv_artifact_palette_entry(int index);
// CoCoVGA palette / artifact registers (see cocovga.c): 1 and the colour
// in *rgb (0xMMBBGGRR) when the registers set one, else 0
int        cv_pal_rgb(const cv_regs_t *r, int slot, uint32_t *rgb);
int        cv_custom_palette_entry(const cv_regs_t *r, int index, const uint32_t *base16, uint32_t *rgb);
int        cv_artifact_mode(const cv_regs_t *r, int menu_mode);
// which colour a (first, second) dot pair produces with the given mode,
// or -1 if the pair is not an artifact pair
int        cv_artifact_index(int first, int second, int mode, int flags);

// Leave W64 mode (e.g. when the OSD is opened or emulation switched off).
// Returns 1 if the caller should resume normal flipping.
int        cv_stop(cv_state_t *s);

// Mark the picture on screen as lost (the screen was cleared): the next
// W64 frame redraws every character.
void       cv_invalidate(cv_state_t *s);

#endif
