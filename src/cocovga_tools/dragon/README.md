# W64TEST.VDK: CoCoVGA 64 column test disk for Dragon BASIC

A DragonDOS disk (40 track, single sided) that tests the RGBtoHDMI CoCoVGA
emulation from Dragon BASIC, without NitrOS-9. It runs on a stock 32K
Dragon 32 or a Dragon 64 (in 32K mode) with DragonDOS.

    RUN"W64TEST"

The program loads `W64LIB.BIN` (machine code at $7800-$7BFF, protected with
`CLEAR 500,&H77FF`; below $7C00 so a DOS or extension that keeps the top of
RAM, such as DOSPLUS with the DW4 / SDC extender at $7EFF-$7FFF, leaves it
room). It sends CoCoVGA register pages the same way CoCoVGA's
own SG4PG2 / WIDTH64 do (the $FF22 combination lock, then the 512 byte page
shown for one frame), and draws the 64x32 screen in a 2 KB CG2 buffer at
$0C00.

| Key | Test | Stage |
|-----|------|-------|
| 1 | 64 column text, MC6847T1 font + lower case, then the 6847 font (same screen, register change only) | 1 |
| 2 | all 256 character codes in 64 columns, both fonts | 1 |
| 3 | typing in 64 columns: ENTER new line, left arrow deletes, up arrow switches case, CLEAR exits. Lines scroll the whole screen | 1 |
| 4 | scroll stress: the whole screen scrolls with a counter line, every 2 frames, about 30 s (any key stops it) | 1 |
| 5 | full screen stress: all 2048 characters rewritten every update, about 30 s | 1 |
| 6 | 64 columns in the orange colour set | 1 |
| 7 | 64 columns with inverse video (extras register $60) | 1 |
| 8 | 32 column lower case (font register 3 in the normal text mode), green and orange, then the 6847 font | 2 |
| 9 | PMODE 4 artifact colour patterns (buff, then green) | 2 |
| B | custom colours (stage 3): a CoCoVGA semigraphics palette (text turns dark blue on cream, the colour bars change), then the 9 colour palette (extras $00: dark text shows black), a reset; then PMODE 4: default artifacts, custom artifact colours (yellow / purple), swapped, artifacts off, reset. Each key goes to the next step; the PMODE 4 steps have no prompt | 3 |
| A | runs 1-9 one after the other, 5 s per screen, shorter stress tests (about 2 minutes) | |
| 0 | back to BASIC | |

Key C runs B automatically (5 s per step; used by `../xroar/verify.sh`).
B needs **Palette → 6847 Artifacts** on for the PMODE 4 steps.

Any key ends a screen. Use the menu's 0 rather than BREAK; after a BREAK
the 64 column picture stays on: type `RUN` (blind) to get the menu back.

Watch the RGBtoHDMI Info pages (CoCoVGA status / Live Timing) while the
stress tests run.

## Rebuilding

    python3 mklib.py w64lib.raw
    python3 mkdisk.py w64test.bas w64lib.raw W64TEST.VDK

`w64test.bas` is plain text; `basictok.py` tokenises it with the Dragon
BASIC + DragonDOS keyword tables in `tokens.json` (taken from the ROMs).
`mklib.py` assembles W64LIB with `../xroar/asm6809.py`.

## Checked in XRoar

`../xroar/verify.sh` with `D32_ROM` / `DDOS_ROM` set runs the disk (menu
key A) on XRoar's PAL Dragon 32 and checks every frame against what both
stages must show.
