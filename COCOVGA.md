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
| Extras: inverse-video text | yes (64 columns; 32 columns see "Text colours") |
| Reset / edit-mask handling of register page 0 | yes |
| Leaving W64 (any non-CG2 picture), returning to 32 columns | yes |
| 32-column lower case (font register "force lower case", or the menu) | yes |
| MC6847 NTSC artifact colours in PMODE 4 | yes |
| Register page palettes: semigraphics (SG) palette, artifact colours | yes (see "Register palettes") |
| Artifact register: colour on/off, swap | yes |
| Artifact styles fat bits, smArtifact, MESS | drawn as standard |
| Extras: 9 / 10 colour palette | yes |
| Extras: text colours from the extra palette (text foreground / background) | yes (see "Text colours") |
| Extras: scanlines, extra palette scanline colour | yes (see "Scanlines") |
| Extras: border modes black and custom (extra palette border colour) | yes (see "Border modes") |
| Extras: border mode 6847T1 | drawn as 6847 (the mode pins can't be seen) |
| VG6 128x96 16-colour mode (enhanced bit 0), default and custom palette | yes (see "VG6") |
| Extras bit 4: lower case in place of SG6 (rows drawn with INT/EXT, as a 6847T1) | yes (see "INT/EXT rows") |
| Uploaded character sets (character RAM, page $18): 64 columns, INT/EXT rows, font bit 2 | yes (see "Character RAM") |

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
* **Extra palette** (text foreground/background, border, scanline): text
  foreground / background are used with extras bit 7 (see "Text colours"),
  the scanline colour with extras bit 2 (see "Scanlines"), the border
  colour with border mode custom (see "Border modes").
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
counts these ("colours unreadable"). Most colour values are readable. The
same goes for the extras register: $80-$F0 with the low 4 bits 0 reads as
$80: bit 6 (10 colours) then keeps its previous value and bits 5-4 are
taken as 0, so CoCoVGA's "text palette" value $C0 comes out right, also
after inverse video was on; $E0 (text palette with inverse video) reads as
$C0.

Timing: only the text rows holding the groups being loaded are read (SG:
row 1 columns 0-21, artifact: rows 2-3, extra: row 4 columns 0-7). Reading
the frame buffer is the slow part (on a Pi Zero W a page with all groups
took about 450 us when read in one frame), so these lines are copied to
cached memory line by line within the per-frame work limit, starting in
the register page frame; meanwhile the page's buffer is kept from being
captured over (`cv_frame` picks the next capture buffer). Then the
palette words are decoded, also within the limit, and applied together
when all are read. It takes a few frames (the benchmark row "Reg page
palettes" shows the longest frame and how many frames it took). Other
registers in the page apply at once.

Colour changes rebuild the 256-entry colour table once
(`osd_update_palette`, about 280 us on a Pi Zero W). It waits for a frame
whose hook took less than 90 us (at most 30 frames, e.g. on a busy 64
column screen), so it does not add to a busy frame. Info → CoCoVGA Live
Timing shows how many times it ran and the longest time taken, and for
dropped frames whether the frame before rebuilt the colour table or else
the longest hook time just before a drop.

The hook does not run while the menu is shown, so register pages sent
then are missed: close the menu before running a program that sets
colours.

Test: key B on the Dragon BASIC test disk (`src/cocovga_tools/dragon`);
`verify.sh` runs it on XRoar and checks every palette / artifact register
change, also with the work limit.

**Info → CoCoVGA Registers** shows the font, artifact, extras and enhanced
registers, artifact style, border/scanline settings, the text colour
settings, how many custom colours are set, the SG palette and the text
foreground / background colours.

## Text colours and inverse video (extras bits 7 and 5)

Extras bit 7 draws alphanumeric characters in the extra palette's text
colours instead of the SG palette's (slot 48 "text fg" for the dots that
are dark in normal text - the letters of normal text, the background of
inverse ones - and slot 49 "text bg" for the light ones). Extras bit 5
(inverse video) swaps the two. Semigraphics keep their SG palette colours.

The frame buffer holds colour codes only, and text green and semigraphics
green are the same code, so text dots carry a tag: frame buffer value
`0x70 | code` (CV_TEXT_TAG). In 64 columns the renderer draws them so. In
32 columns the capture does it: the MC6847 capture routine maps every pair
of dots through its pair table (as for the artifact colours), and for a
frame after a text frame the table also tags text dots (dark green, light
and dark orange, and green next to green or dark green). The colour table
then gives tagged dots the text colours, swapped for inverse video. No
picture is read or rewritten, so it costs no time per frame. With the
CoCoVGA emulation on, the capture always uses that routine (with artifacts
off its table leaves every dot as it is).

The table for a frame is chosen after the frame before it, so the first
frame after a change between text and graphics is captured with the wrong
one. Such a frame is not shown: the picture on screen stays for that one
frame (Live Timing: "Held at text/graphics change"), and 64 columns and VG6
are not built over it. Without the pair table (a profile without double
width) 32 column text keeps its normal colours and the Registers page says so.

**Limit:** a semigraphics dot pair that is all green looks exactly like
the background of text (a space and a solid green block are the same
picture), so with text colours or inverse video on, green semigraphics
blocks take the text background colour. The other semigraphics colours
are not affected.

Test: key T on the Dragon BASIC test disk: text palette (white on blue),
inverse video in green, text palette with inverse video, lower case in the
text palette, scanlines (next section), the orange colour set, then 64
columns, reset.

## Scanlines (extras bit 2)

CoCoVGA draws a line of black, or of the extra palette's scanline colour
(slot 51), between the lines of the picture. RGBtoHDMI has the same thing
built in for double height: with its Scanlines setting on, the capture
writes every second frame buffer line with bit 7 set, and the colour table
shows those entries dimmed. While extras bit 2 is set, the hook switches
those scan lines on for the following frames (whatever the Scanlines
setting) and the colour table shows them in CoCoVGA's scanline colour
instead of dimmed. When the bit is cleared the Scanlines setting decides
again. It needs the profile's double height (the MC6847 profiles have it);
the Registers page says so otherwise.

32 column lower case and VG6 keep the scan lines of the picture they
redraw. 64 columns has none in its text area (each 64 column text row is
only 6 lines of the 6847 picture; CoCoVGA's own tools switch scanlines off
for 64 columns), only in its border.

## Border modes (extras bits 1-0)

| Mode | CoCoVGA | Here |
|---|---|---|
| 00 6847 | the 6847's border: black in text, green / buff in graphics | as captured |
| 01 6847T1 | the T1's border: green / orange in text when GM2 is set | as 6847: the mode pins are not in the picture |
| 10 black | black in every mode | yes |
| 11 custom | the extra palette's border colour (slot 50) in every mode | yes (slot 50 not set: as 6847) |

The border is part of the captured picture and the same colour codes as
the picture's (text black, graphics green or buff), so it has to be told
apart by where it is. Repainting it in the hook would cost about 1 ms per
frame, so the capture does it: while border mode black or custom is set,
the hook makes rgb_to_fb use a second MC6847 capture routine
(`capture_line_c64lc6847_border_double_8bpp`, through `capture_address`)
that sends the border's dot pairs through a second pair table. That table
keeps every dot's colour code (the decoders, which look at the border to
tell modes apart, see the same picture) and marks the border colours with
tag / code pairs nothing else uses (0x71 black, 0x1C green, 0x77 buff); the
colour table shows those in the border colour. Per line the routine knows
from rgb_to_fb's line count (r5) whether the line is in the picture; on the
picture's lines it counts dot pairs and swaps the two tables where the
picture starts and 128 pairs later. Where the picture is comes from the
calibration (`cv_border_params`). The routine is used only while one of the
two border modes is set; otherwise nothing changes. It needs double width
(dot pairs of 4 pixels); the Registers page says when the border can't be
drawn. 64 columns and VG6 keep the border of the frame they were built
over, so they are only built over a frame the border routine captured
(its preload tells the hook which routine ran; rgb_to_fb starts again with
the plain routine after any re-init): such a frame is held instead (up to
CV_BASE_HOLDS frames), and a 64 column picture built over a frame without
it, or with text tags in its border, is built again over the next clean
frame.

With the capture an odd number of dots off the 6847's dot pairs, the
picture's last dot column shares a dot pair with the border and can take
the border colour (the NTSC artifact colours need even pairs anyway).

Test: key T on the Dragon BASIC test disk: a red custom border in text,
then 64 columns with the red border and with a black one.

## INT/EXT rows (extras bit 4)

On a Dragon or CoCo the 6847's INT/EXT pin is GM0 ($FF22 bit 4), so a program
can set it in text mode, even for part of the screen. A plain 6847 then draws
each alphanumeric byte as its own dot pattern on all 12 lines of the cell
(inverted when bit 6 is set) and each semigraphics byte as SG6 (3 x 2 blocks,
two colours of the CG colour set). A 6847T1, as in the CoCo 2B, shows lower
case instead: $00-$1F lower case, $20-$3F the inverse characters, $40-$7F the
normal ones, and SG4 for semigraphics. CoCoVGA does the same with extras bit
4.

With extras bit 4 set the 32 column text fix-up (the one that does the lower
case, spread over frames within the work limit and shown when complete) also
looks at every text row: lines 1, 5 and 9 of each cell (line 1 is empty in
every 6847 glyph; the three lines are in the three SG6 block rows). A row in
which every cell has three identical lines in text colours, or is SG6, and at
least one cell is not an empty line, is an INT/EXT row: every byte is read
back from its pattern and the row is redrawn as a 6847T1 shows it (with the
lower case style of the 32 Col Lower Case setting, text colours and scan
lines kept). Other rows are left as they are. Only an SG6 cell with no block
lit hides its colour, and the 6847T1 shows those as black SG4 anyway.
Live Timing counts these frames under "Fix-up 32".

Test: key E on the Dragon BASIC test disk: INT/EXT on with extras bit 4 off
(the plain 6847's patterns), then with bit 4 (lower case), then off.

## Character RAM (uploaded character sets)

CoCoVGA 0.89 and later hold a character set of 256 characters x 12 rows in
RAM (3072 bytes, top row first, bit 7 the left dot). A program uploads it
with the combination lock and page select $18: the next frame is the 3 KB
shown as CG3 (PMODE 1, $FF22 = $C0, SAM V = 4), which CoCoVGA reads in and
does not show. Enhanced bit 2 then uses it: in 64 columns for every
character, in 32 columns for the rows drawn with INT/EXT, and with font bit
2 as well for every row. Characters $00-$7F: a set bit is a dark dot on the
light background (green, or orange with CSS); $80-$FF: a dot in the
semigraphics colour of bits 6-4 on black.

The page select can't be seen in the picture, so a CG3 frame that comes
right after text, a register page or 64 columns is held back (the previous
picture stays on screen for that frame; with Num Buffers = 0 a text screen
is captured straight onto the screen, so the upload frame shows once, and
the next frame is captured elsewhere to keep it for reading). If the next frame is text, a
register page or 64 columns again, it was an upload: its 96 rows of 32 bytes
are read back from the 4 CG colours (lossless), within the per-frame work
limit and over several frames if need be, into the emulation's character
RAM. Anything else (a program going to PMODE 1 graphics) and the frame was
graphics after all, shown one frame late. The frame is only taken for CG3
with a green or buff border beside the picture and 128 sampled dot pairs of
one CG colour set, both lines of each pixel row the same. The upload's mode
change comes at the start of field sync, which is the start of the bottom
border, so the text frame before it already has a green bottom border; a
text frame at the calibrated place with black borders above and beside the
picture still counts as text then (and a CG3 frame one frame after text,
with one torn frame between, is also taken). Line 2r of the picture always
shows CG3 row r; its second line may be line 2r+1 or 2r-1, depending on
where the SAM's row divider started, so either is accepted. Live Timing's
"CG3 no: N why R a/b/c bef TT" names the reason the last graphics frame
was not taken for an upload (1 border, 2 pixel pair, 3 colour, 4 lines,
5 colour sets, 6 the frames before: TT = the two frame types before it).

In 32 columns the fix-up of the INT/EXT rows reads each byte back as in
"INT/EXT rows" and redraws it from character RAM. With font bit 2 the other
rows are read back too: 6847 glyphs (normal, inverse, either colour set) and
SG4. A cell that is solid green is a space in colour set 0, green SG4 $8F in
set 1; a black SG4 cell reads as $80. 64 columns with enhanced bit 2 draw
the decoded CG2 bytes from character RAM directly (and rebuild the picture
when the character set changes). Live Timing counts the upload frame under
"Char RAM" and shows "Char RAM loads N of M" (M = CG3 frames held back as a
possible upload; ", in use" with enhanced bit 2).

Test: key G on the Dragon BASIC test disk (FONT.BIN, made by mkfont.py: T1
lower case at $00-$1F, bold 6847 glyphs at $20-$7F, 16 patterns at $80-$FF):
upload (no change on screen), enhanced bit 2 (no change), INT/EXT on (every
row in the uploaded set), font bit 2, the orange set, then 64 columns with
all 256 characters, and 64 columns in the 6847 font to compare.

## VG6

With enhanced register bit 0 set, CoCoVGA shows the 6 KB of a CG6 (PMODE 3)
picture as 128x96 pixels of 16 colours: 64 bytes per row, 2 pixels per byte,
left pixel in the high nibble. A VG6 row r is CG6 rows 2r and 2r+1, and the
6847 draws those on exactly the lines the VG6 row covers (pixels 2 dots wide
and 2 lines high). So a captured CG6 frame is read back (the 4 CG6 colours
are 2 bits each, lossless) and redrawn in place, one VG6 row at a time,
within the per-frame work limit, off screen, and shown when complete (as the
32 column lower case). Newer captures are skipped meanwhile: a moving VG6
picture is shown at a lower frame rate (the benchmark row "VG6 128x96" shows
how many frames one picture takes).

A frame is taken for VG6 when the border above the picture is green or buff
and 128 sampled dot pairs are pairs of one CG colour set. CoCoVGA itself
looks at the mode pins, which the picture does not show, so with VG6 on the
other 4 colour modes (CG1, CG3, and CG2 unless W64 is on too, which takes
CG2 first) are also drawn as VG6. With Num Buffers = 0 the first VG6 frame
is captured straight onto the screen and shows as CG6 once. A CG6 frame
whose mode changed after its top border began (the frame right after a
register page) is not used: the VG6 picture stays on screen for it, as
W64 does.

Colours: frame buffer values `0x50 | n` (colour set 1) and `0x60 | n`
(colour set 0) are VG6 pixel n. The colour table gives them the artifact /
VG6 palette slot (16 + n, or 32 + n) when a register page has set it, else
CoCoVGA's defaults (the MESS artifact colours for colour set 1, shades of
green for colour set 0; Artifact Emulation p.1-3). Note that VG6 palettes
written in CoCoVGA's 9 bit (AMC2) format, as the ModeDemo VG6 program does,
often have a first byte with low nibble 0 (green below 2 of 7), which the
register page cannot show (see "Register palettes"): those colours keep
their previous value.

Test: key V on the Dragon BASIC test disk (16 colour bars over diagonal
stripes, colour set 1, colour set 0, a custom palette, reset); `verify.sh`
runs it on XRoar (key W) and checks every VG6 frame and the palette, also
with the work limit.
