#!/usr/bin/env python3
"""
xroar2fb.py - convert XRoar raw VDG frames (262 lines x 912 samples of enum
vdg_colour, 2 samples per 6847 dot) into RGBtoHDMI frame buffers the way the
MC6847digitizer + RGBtoHDMI MC6847 LumaCode profile store them: 8bpp
LumaCode colour codes, 320x240 dots doubled to 640x480.

  xroar2fb.py in.raw out.fb [--dx N] [--dy N] [--png-every N --png-dir D]

--dx/--dy shift the captured window (dots / lines) to test other
capture offsets.
"""
import sys, argparse
import numpy as np

LINE, LINES = 912, 262
ACTIVE_X = 64 + 70 + 120          # VDG_tWHS + VDG_tBP + VDG_tLB (samples from HS fall)
ACTIVE_Y = 13 + 25                # first active scanline
FB_W, FB_H = 640, 480
BORDER_DOTS, BORDER_LINES = 32, 24   # 320x240 capture around 256x192

# enum vdg_colour -> LumaCode colour code (MC6847digitizer)
#   GREEN YELLOW BLUE RED WHITE CYAN MAGENTA ORANGE BLACK DKGREEN DKORANGE BRIGHT_ORANGE
VDG2LC = np.array([12, 13, 6, 2, 7, 10, 9, 8, 1, 5, 4, 3] + [0] * 244, dtype=np.uint8)

# MC6847_LC palette (RGB) for previews
PAL = {0: (0, 0, 0), 1: (0, 0, 0), 2: (0x9a, 0x32, 0x36), 3: (0xff, 0xb7, 0), 4: (0x6b, 0x27, 0),
       5: (0x0d, 0x3a, 0x06), 6: (0x4c, 0x3a, 0xb4), 7: (0xbf, 0xc8, 0xad), 8: (0xd4, 0x7f, 0),
       9: (0xc8, 0x4e, 0xf0), 10: (0x41, 0xaf, 0x71), 12: (0x30, 0xd2, 0), 13: (0xc1, 0xe5, 0)}

def convert(frame, dx=0, dy=0):
    """frame: (262, 912) uint8 -> (480, 640) uint8 LumaCode"""
    x0 = ACTIVE_X - 2 * BORDER_DOTS + 2 * dx
    y0 = ACTIVE_Y - BORDER_LINES + dy
    win = frame[y0:y0 + FB_H // 2, x0:x0 + FB_W]          # one sample = one fb pixel
    lc = VDG2LC[win]
    return np.repeat(lc, 2, axis=0)                        # double height

def palette_image(fb):
    lut = np.zeros((256, 3), dtype=np.uint8)
    for k, v in PAL.items():
        lut[k] = v
    return lut[fb & 15]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('inp'); ap.add_argument('out')
    ap.add_argument('--dx', type=int, default=0); ap.add_argument('--dy', type=int, default=0)
    ap.add_argument('--png-every', type=int, default=0); ap.add_argument('--png-dir', default='.')
    a = ap.parse_args()
    raw = np.fromfile(a.inp, dtype=np.uint8)
    n = raw.size // (LINE * LINES)
    frames = raw[:n * LINE * LINES].reshape(n, LINES, LINE)
    with open(a.out, 'wb') as f:
        for i in range(n):
            fb = convert(frames[i], a.dx, a.dy)
            f.write(fb.tobytes())
            if a.png_every and i % a.png_every == 0:
                from PIL import Image
                Image.fromarray(palette_image(fb)).save('%s/xroar_%04d.png' % (a.png_dir, i))
    print('%d frames -> %s' % (n, a.out))

if __name__ == '__main__':
    main()
