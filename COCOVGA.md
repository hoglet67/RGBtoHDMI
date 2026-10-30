# CoCoVGA 64-column text for RGBtoHDMI (MC6847 LumaCode)

This adds to RGBtoHDMI a
software emulation of [CoCoVGA](http://www.cocovga.com/)'s 64x32 text mode
for Dragon and Tandy CoCo machines fitted with c0pperdragon's
[MC6847digitizer](https://github.com/c0pperdragon/LumaCode/wiki/MC6847digitizer).

Programs written for CoCoVGA — in particular **NitrOS-9 with the CoVGA
driver** — show their 64-column, mixed-case text on HDMI exactly as they
would on a CoCoVGA board. No hardware changes, no changes to the digitizer
firmware: only the Pi's SD card.

## How it works

The digitizer sends an exact copy of the 6847 picture (one colour code per
6847 dot). CoCoVGA software switches 64-column mode on in two steps, and
both are visible in that picture:

1. **Register page.** The program writes CoCoVGA register page 0 into
   video memory and shows it for one frame as a normal 6847 text screen.
   The emulation recognises that page (the reserved register bytes always
   show as inverse `@`) and reads the font, extras and enhanced-mode
   registers from the characters on screen. As on CoCoVGA, the previous
   picture stays on screen during that frame.
2. **CG2.** The program then selects the 6847's CG2 mode (128x64, 4
   colours, 2 kB). Each CG2 pixel carries 2 bits, so the 2048 bytes of
   video memory are read back out of the picture without loss. With
   64-column mode enabled they are drawn as 64x32 characters using the
   MC6847 or MC6847T1 character set, with lower case, exactly where the
   6847 picture was.

While 64-column text is on screen, capture goes to a spare frame buffer and
only the characters that changed are redrawn in the buffer on screen (plus a
small rolling refresh).

The work runs between the end of one capture and the next vsync, and if it
runs past that vsync RGBtoHDMI drops a frame and may re-initialise the mode.
So the work per frame is limited (`CV_HOOK_BUDGET_US`, 250 us including
decoding, at least 32 characters per frame): characters that don't fit are
drawn in the next frame. Reading the CG2 picture back is spread too
(`CV_DECODE_BUDGET_US`, 120 us, at least 8 of the 64 CG2 pixel rows per
frame): measured on a Pi Zero W, reading the whole picture takes about
0.5 ms because the frame buffer is not cached, so a typed character appears
within a few frames (each slice is only used if every row in it is good).
Lines that are scanned are read in 32 byte bursts. On entering 64 columns the screen is built in a
frame buffer that is not on screen and shown when it is complete and up to
date (with Num Buffers = 0 the captured frame is already on screen, so the
build is visible for a few frames). Under very fast scrolling the 64 column
screen may lag the 6847 by a few frames and catch up when it stops.

### Supported

| CoCoVGA feature | Status |
|---|---|
| 64x32 text (W64), semigraphics 4 in 64 columns | yes |
| Font register: 6847 / 6847T1 character set, force lower case | yes |
| Extras: inverse-video text | yes |
| Reset / edit-mask handling of register page 0 | yes |
| Leaving W64 (any non-CG2 picture), returning to 32 columns | yes |
| 32-column lower case (font register "force lower case", or the menu) | yes |
| MC6847 NTSC artifact colours in PMODE 4 | yes |
| Register page palettes: semigraphics (SG) palette, artifact colours | yes (see "Register palettes") |
| Artifact register: colour on/off, swap | yes |
| Artifact styles fat bits, smArtifact, MESS | drawn as standard |
| Extras: 9 / 10 colour palette | yes |
| Extra palette (text, border, scanline colours), border modes, scanlines | not yet |
| VG6 16-colour mode, uploaded character sets, T1 lower case instead of SG6 | not yet |

Not possible without help from the digitizer firmware: CoCoVGA notices the
register page through a "combination lock" on the 6847 mode pins during
vertical blanking, which leaves no trace in the picture. The emulation
therefore recognises register pages by their content instead.

## Fonts

* MC6847: the default character set built into CoCoVGA's own `CCVEDIT.BIN`
  (CoCoVGA 0.92 software).
* MC6847T1: transcribed from Motorola's MC6847T1 data sheet, Figure 24
  "Character ROM Pattern", and checked against an independent
  reconstruction (DamienG's FontStruct): 90 of 93 glyphs identical, the
  remaining 3 re-checked against the data sheet scan. Photos and RGBtoHDMI
  captures of real 6847T1s (XC80652P in later CoCo 2s) match the data sheet,
  with every character at the top of the cell (rows 1-7) rather than the
  bottom; the "T1" fonts in MAME / MESS are the CoCo 3 GIME's instead
  (stardot.org.uk, "6847t1 Font Mystery").
* Dragon 200-E lower case (32 columns only): the 200-E's lower case
  character ROM, as dumped in
  [go4retro/dragon200e-lowercase](https://github.com/go4retro/dragon200e-lowercase)
  (`charsets/fixrom/rom26.ic1.0.bin`, codes $00-$1F).

## Installing

1. Start from an RGBtoHDMI Beta 66 (or later) release on your SD card.
2. Replace `kernelrpi.img` with one built from this source
   (Pi Zero / Zero W / Zero 2 W with the default GPU capture kernel).
3. Copy `src/scripts/Palettes/MC6847_LC.bin` to `/Palettes/` and
   `src/scripts/Profiles/6-12_BIT_YUV_Analog/MC6847/` to the same place
   under `/Profiles/` (these are c0pperdragon's MC6847 LumaCode profile
   files, with `cocovga=1` added).
4. Optional: add `cocovga=0` to `/Profiles/Default.txt` (the firmware
   assumes it if the line is missing).

The setting is **Palette Menu → CoCoVGA 64 Col** (On/Off). It is on in the
MC6847 LumaCode profile and off everywhere else. The emulation only runs
with the 4-bit LumaCode palette control, 8bpp frame buffer and a double
width + height frame buffer (the profile default); it pauses while the
RGBtoHDMI menu is open.

The 6847 picture is found by the black border round it, so the capture must
include some border. With integer scaling RGBtoHDMI fits the capture to the
output resolution, starting from the profile's minimum size; with a minimum
of exactly 256x192, outputs such as 1024x768, 1280x960, 1280x1024 and
1366x768 captured exactly the 256x192 picture with no border, and nothing was
recognised. The MC6847 profiles therefore ask for at least 264x196 (4 dots /
2 lines of border each side). That changes nothing at 800x600, 1280x720,
1920x1080 and the other common resolutions; at the four above the picture is
scaled one step smaller, with a border. The Live Timing page shows the
capture size and whether it has a border.

## Building

```
cd src/scripts
cmake -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-none-eabi-rpi.cmake ../
make
```

Produces `src/scripts/kernelrpi.img` (tested with arm-none-eabi-gcc 13.2).

## PC test harness

`src/cocovga_tools/` renders 6847 frames the way the digitizer and
RGBtoHDMI put them in the frame buffer (text, CG1/2/3/6, RG6, both colour
sets, several frame buffer geometries, random upper pixel bits) and checks
the decoder end to end, including a simulation of RGBtoHDMI's multi-buffered
capture loop:

```
make -C src/cocovga_tools test
```

It also writes `preview_w64.ppm`, a NitrOS-9 style screen as it will appear.

## Files

* `src/cocovga.c`, `src/cocovga.h` — decoder, renderer and buffer control
  (plain C, no RGBtoHDMI dependencies)
* `src/cocovga_font.c` — MC6847 and MC6847T1 character sets
* `src/rgb_to_hdmi.c` — `cocovga_frame_hook()`, called once per frame
* `src/rgb_to_fb.S` — buffer selection / flip helpers (moved out of line;
  the capture loop's size is unchanged)
* `src/osd.c`, `src/osd.h` — the `cocovga` setting

## Frame time check (Info menu)

The CoCoVGA work runs once per frame, between the end of the capture and
the next vsync. If it runs past that vsync, a frame is dropped and
RGBtoHDMI may re-initialise the mode (screen clear, re-probe). The frame
buffers are in non-cacheable memory, so this has to be measured on the Pi.
Two pages in the **Info** menu do that:

* **CoCoVGA Benchmark** - needs no Dragon. Runs every CoCoVGA path on
  generated 6847 frames in uncached RAM (the same memory type as the frame
  buffers, the screen is not touched) and shows min / avg / max time per
  frame in microseconds: 32 column text, register page, entering W64 (full
  redraw, aligned and unaligned), W64 idle / typing / scrolling, graphics,
  black screen, with lower case off and on. A "!" means a row did not take the path it measures.
* **CoCoVGA Live Timing** - with the Dragon running. Since the page was
  last opened: frames, dropped frames, frame rate, hook time per frame type
  (avg / max); since boot: re-inits caused by timing or sync changes and the
  longest hook time just before them. Both pages show the time budget,
  estimated from the measured sync: lines between the end of the capture
  and the next vsync.

To isolate lower case, compare the "Lower case on" rows / the "Lower 32"
line with the rest, or set **32 Col Lower Case** to Off.

## Verified with XRoar

`src/cocovga_tools/xroar/` runs the emulation on frames from XRoar's MC6847
(see its README). Every frame is checked pixel by pixel against what must be
on screen, decoded independently with XRoar's character ROMs:

* a test ROM driving the 6847 like the CoVGA driver (register page, CG2
  64x32 with typing and scrolling, back to 32 columns) on XRoar's NTSC
  Dragon, with the capture shifted by 0, 1 and 2 dots;
* 32 column lower case (green and orange colour sets) against
  XRoar's 6847T1 lower case glyphs, lower case off again, and PMODE 4 in
  both colour sets with artifacts off, Red/Blue and Blue/Red; the artifact
  colours match XRoar's NTSC rendering;
* NitrOS-9 Level 1 with the CoVGA driver booting to its prompt on XRoar's
  NTSC CoCo (1390 64-column frames). The real driver shows its register
  page for one frame with a mode change part way down the screen (green
  border at the top) and its screen contents around the register bytes;
  it is recognised using the previous calibration;
* Dragon 64 BASIC frames (no false register pages);
* the Dragon BASIC test disk (`src/cocovga_tools/dragon/W64TEST.VDK`, all
  tests, both fonts, orange set, inverse video, scroll and full screen
  stress) on XRoar's PAL Dragon 32 with DragonDOS, also with a work limit;
* Num Buffers 0 and 3.

On the PAL Dragon the mode change a program makes right after field sync
lands part way down the top border (the PAL padding lines come before it),
so the first CG2 frame still has a black top border. W64 keeps the border of
the frame it was built on, so that frame is not used: the previous picture
stays for it (instead of one frame of raw CG2 data) and W64 starts from the
next frame. The same is done for a first CG2 frame with a row torn by a
write during the scan, for at most `CV_ENTRY_HOLDS` (2) frames in a row.

Cells drawn by the 6847 are recognised with the 6847's own glyphs
(`mc6847_rom_font`), which differ from CoCoVGA's set. Plain 6847 chips also
differ among themselves in 'E', 'F' and '3' (row 6 only): a real Dragon's
6847 draws 'E' / 'F' with a 3 dot middle bar, the Acorn Atom's (and XRoar)
with 4 dots, and '3' with a 2 or 1 dot middle stroke (MAME has both sets).
Both forms of each are recognised; the PC harness checks the 3 dot form,
XRoar the 4 dot one.

## Frame buffer depth and HDMI audio

The emulation works on the 8 bpp frame buffer of the MC6847 LumaCode
profile. RGBtoHDMI switches to 16 bpp when Audio Capture and Optimise
Profile are both on, so the profiles set `optimise=0` (the 50 Hz profile
also `audio_capture=1`): HDMI audio from the digitizer works with Optimise
off (checked on a Dragon 32). The Info pages say when the emulation can't
run and why.

## References

* CoCoVGA documentation: Software Mode Control, Theory
* NitrOS-9 `level1/coco1/modules/covdg.asm` (CoVGA register page, character
  translation)
* Motorola MC6847T1 data sheet
* c0pperdragon, LumaCode wiki: MC6847digitizer colour encoding

## 32-column lower case and NTSC artifacts

### 32-column lower case

Setting: **Palette Menu → 32 Col Lower Case** (`cocovga_lower_case=`):

| Value | Lower case | Glyphs |
|---|---|---|
| Auto (default) | when a CoCoVGA register page sets the font register's "force lower case" bit (NitrOS-9's CoVGA driver does; from BASIC use CCVCNF32.BAS) | 6847T1 lower case |
| Off | never: inverse stays inverse, as on a plain 6847 | - |
| 6847T1 | always, no register page needed (e.g. BASIC typing after SHIFT-0) | 6847T1 lower case |
| Dragon 200-E | always, no register page needed | the Dragon 200-E's lower case ROM |

Both fonts sit on the baseline of the 6847's capitals. A real 6847T1 draws
all its characters two rows higher (at the top of the cell), so its lower
case is moved down 2 rows here to line up with the plain 6847's capitals;
W64 with the font register's T1 bit draws T1 capitals and lower case
unmoved, as a 6847T1 does.

It needs **CoCoVGA 64 Col** on. With lower case on, inverse
characters $00-$1F on the normal 32x16 text screen are shown as lower case,
dark on green like normal text (also in programs that use inverse letters
on purpose, as on a 6847T1). $20-$3F stay inverse, as on a
real 6847T1. Only the inverse cells are looked at (one pixel per cell finds
them), so the cost per frame stays small. While it is active, frames are
captured into a buffer that is not on screen, fixed up, then flipped.

The fix-up has to be done on every captured frame (each capture overwrites
the last one). It runs under the same work limit as the 64 column screen:
a captured screen is fixed up off screen, over several frames if needed
(newer captures are skipped meanwhile), and shown when complete. With a
few lower case letters on screen that takes one frame; with a screen full
of them (about 2-3 ms of work on a Pi Zero W) the 32 column screen updates
a few times a second. Each cell is kept cheap: each inverse cell is identified from 11 dots
(they tell all 64 inverse 6847 glyphs apart) and checked against 28 dots,
instead of reading all 96. Over those 28 dots any two glyphs differ in at
least 3 dots, as over all 96, so 1 or 2 wrong dots can never show a wrong
letter (the cell stays inverse for that frame, as before). The glyphs are
the 6847's own (`mc6847_rom_font`); in the orange colour set the 6847 draws
text in light orange (LumaCode code 3). The lower case glyph is written
with 32 bit stores. A full screen of lower case is about 14,500 frame
buffer reads and 49,000 word writes per frame (was about
62,000 reads and 197,000 byte writes). Pictures
not on a 4 pixel boundary, or not double width and height, use the general
(slower) path.

### NTSC artifact colours (PMODE 4)

Setting: **Palette Menu → 6847 Artifacts** = Off / Red/Blue / Blue/Red
(`mc6847_artifact=`; on in the 60 Hz MC6847 profile, off elsewhere).

The MC6847 LumaCode capture routine is replaced by
`capture_line_c64lc6847_double_8bpp` (`src/capture_line_mc6847_8bpp.S`),
which runs the original routine when artifacts are off. When on, every dot
pair goes through a 256-entry table as it is captured: a black dot next to
a buff dot is shown orange-red or blue, depending on which of the two is lit;
in the green colour set (where PMODE 4's "black" is dark green) dark or mid
green. The colours are taken from XRoar's simulated NTSC composite video.
Only PMODE 4 mixes two colours inside one dot pair. No extra frames of
delay: the colour is decided while the line is captured.

An artifact dot keeps its own colour code in the low 4 bits and only gets
a tag in bits 4-6, which the palette turns into the artifact colour. The
CoCoVGA decoders only look at the low 4 bits, so 64 column text and lower
case work with artifacts on however the capture lines up with the 6847's
dot pairs (checked with the capture shifted by 0, 1 and 2 dots). Which table
is used (green set graphics or not) is decided per frame from the border of
the frame just captured: the middle of the capture's first and last lines
must both be green, so text below green graphics (split screen) stays plain.
The table applies from the next frame, so the first text frame after green
graphics can show green artifact fringes for one frame.

Don't turn on RGBtoHDMI's own **NTSC Colour** setting together with **6847
Artifacts**: it also uses the upper bits of each pixel (as the neighbouring
pixel's colour) and the two would fight. It is off in the MC6847 profile.

Checked on a real Dragon with a Pi Zero W. Notes:

* Red and blue depend on which dot of each pair is first, i.e. on the
  capture alignment: if they come out swapped, use the other setting.
* The vsync marker / debug colours are not drawn while artifacts are on.
* Capture loop cost: 3 extra instructions per dot pair.

## Register palettes

CoCoVGA software (CCVCONF, the VPALETTE / VARTIFACT commands, palette
editors) sets colours by writing register page 0 with edit-mask bits 3-5
set. The palette words (`E RRRRR GGGGG BBBBB`, E = 1 for a custom colour)
are read back from text rows 1-4 of the register page:

* **SG palette** (slots 0-10): black, green, yellow, blue, red, buff, cyan,
  magenta, orange, dark green, dark orange. Applied to everything drawn in
  those colours: text, semigraphics, graphics, border, 64 column text.
* **Artifact palette** (CSS1 slots 16-31, CSS0 32-47): slots 9 and 10 of
  each colour set are used for the two PMODE 4 artifact colours (standard
  artifacts use 0, 9, 10, 15).
* **Extra palette** (text foreground/background, border, scanline): read and
  shown on the info page, not applied yet.
* A reset (byte 0) puts every colour back to the default.

Artifact register (byte 4): bit 0 = 0 turns artifacts off, bit 1 swaps red
and blue (relative to the menu setting). The menu still decides whether
artifacts are on at all. The fat bits, smArtifact and MESS styles are drawn
as standard artifacts. Extras bit 6 = 0 (9 colour palette) shows dark green
and dark orange as black, as CoCoVGA does.

**Limit:** the register page is a 6847 text picture, so a palette byte
$80-$FF whose low 4 bits are 0 is a black semigraphics cell and its other
bits cannot be seen. Bytes $60-$7F and $8F show as the same solid green.
A colour containing such a byte keeps its previous value; the info page
counts these ("colours unreadable"). Most colour values are readable.

Timing: in the register page frame only the text rows holding the groups
being loaded are copied to cached memory (SG: row 1 columns 0-21,
artifact: rows 2-3, extra: row 4 columns 0-7). The palette words are then
decoded within the per-frame work limit, over as many frames as needed
(the benchmark row "Reg page palettes" shows the longest frame and how many
frames it took), and applied together when all are read. Other registers
in the page apply at once.

Colour changes rebuild the 256-entry colour table once
(`osd_update_palette`, about 260 us on a Pi Zero W). It waits for a frame
whose hook took less than half the work limit (at most 8 frames), so it
does not add to a busy frame. Info → CoCoVGA Live Timing shows how many
times it ran and the longest time taken.

Test: key B on the Dragon BASIC test disk (`src/cocovga_tools/dragon`);
`verify.sh` runs it on XRoar and checks every palette / artifact register
change, also with the work limit.

**Info → CoCoVGA Registers** shows the font, artifact, extras and enhanced
registers, artifact style, border/scanline settings, how many custom
colours are set and the SG palette.
