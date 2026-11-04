# End-to-end verification with XRoar

Runs the CoCoVGA emulation (`src/cocovga.c` of this branch) on frames from
XRoar's MC6847 emulation instead of the harness's own 6847 model, and checks
every frame against what must be on the screen, decoded independently with
XRoar's own character ROMs.

```
XROAR_SRC=/path/to/xroar [D32_ROM=d32.rom DDOS_ROM=ddos11c.rom] ./verify.sh [nitros9_coco1_vga.dsk]
```

With `D32_ROM` / `DDOS_ROM` (Dragon 32 BASIC and DragonDOS ROM images, not
included) it also runs the Dragon BASIC test disk (`../dragon`, menu key A)
on XRoar's PAL Dragon 32 and checks it like the others, also with a work
limit (128 characters / 8 CG2 rows per frame).

## Pieces

* `xroar-verify.patch` - XRoar additions (against xroar main, Oct 2026):
  * `XROAR_VDG_DUMP=file`, `XROAR_VDG_DUMP_FRAMES=n`: write every frame as
    262 x 912 raw VDG colour indices, quit after n frames.
  * `XROAR_SHOT_DIR`, `XROAR_SHOT_FRAMES=a,b,..`, `XROAR_PHASE`: with
    `-ui null`, render video for real (e.g. `-ccr simulated` NTSC) and save
    PNG screenshots of those frames (used for the artifact colours).
  * `rom.c`: two more Dragon BASIC ROM CRCs, so `-type` works with them.
* `mkrom.py` - a 16 KB test ROM (no Microsoft code) that drives the
  6847 like NitrOS-9's CoVGA driver: BASIC-like screen, register page for
  one frame, CG2 64x32 text with typing and scrolling, back to 32 columns,
  register page "force lower case" with inverse text (green and orange),
  lower case off, PMODE 4 artifact test card in both colour sets.
  `cpucheck.py` runs it on an independent 6809 core (PyPI `MC6809`).
* `mkboot.py` - an 8 KB CoCo boot ROM (no Microsoft code) that does what
  Disk BASIC's `DOS` command does, so a NitrOS-9 disk boots in XRoar's
  `cocous` with the RS-DOS controller.
* `xroar2fb.py` - raw frames -> RGBtoHDMI frame buffers, as the
  MC6847digitizer + MC6847 LumaCode profile store them (LumaCode codes,
  320x240 doubled to 640x480; `--dx` shifts the capture by whole dots).
* `truth.py` - what the 6847 showed in each frame (text codes, CG2 bytes,
  RG6 bytes), decoded with XRoar's font tables.
* `runner.c` - captures each frame into the 4 frame buffers the way
  `rgb_to_fb.S` does (Num Buffers, CoCoVGA redirect / no-flip, stage 2
  artifact table switched per frame like `mc6847_artifact_frame()`), runs
  `cv_frame()` and checks the picture on screen pixel by pixel:
  64x32 screens against XRoar's 6847T1 glyphs (font 0: CoCoVGA's own 6847
  table, which differs from the 6847 ROM in one row of E, F and 3), 32
  column text and graphics unchanged, lower case cells against XRoar's
  6847T1 lower case, register pages hidden (or shown for one frame with Num
  Buffers 0). With a work limit (`-maxchars`, `-decoderows`) a cell may lag
  (it must match one of the last 64 frames, or the look before the last
  font / colour change while that is redrawn). Allowed ride-over frames:
  the first frame after leaving W64, the frame after a register page, and
  up to `CV_ENTRY_HOLDS` CG2 frames on entering W64 that keep the previous
  picture.
  `RUNNER_FAILS=1` lists failing frames, `RUNNER_CELLS=n` the wrong cells of
  frame n, `RUNNER_LOG=n` the decoder state from frame n.
