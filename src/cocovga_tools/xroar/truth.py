#!/usr/bin/env python3
"""
truth.py - independent ground truth for the CoCoVGA verification.

Reads frame buffers produced by xroar2fb.py (640x480 LumaCode codes, the
6847 active area at ox, oy with 2x2 pixels per dot) and decodes what the 6847
was displaying, using XRoar's own 6847 font (not the CoCoVGA decoder):

  text  -> 512 character codes (exact glyph match, SG4 by colour/quadrants)
  CG2   -> 2048 bytes (lossless: 4 colours, 2x3 dot pixels)
  RG6   -> 6144 bytes (PMODE 4, 1 bit per dot)
  CG6   -> 6144 bytes (PMODE 3, lossless: 4 colours, 2x1 dot pixels)

Output: per frame 1 byte type (0 unknown, 1 text, 2 CG2, 3 RG6, 4 CG6), 1 byte css,
6144 bytes data.

  truth.py frames.fb truth.bin --ox 64 --oy 48 --xroar-src DIR
"""
import sys, re, argparse
import numpy as np

W, H = 640, 480
GREEN, YELLOW, BLUE, RED, BUFF, CYAN, MAGENTA, ORANGE = 12, 13, 6, 2, 7, 10, 9, 8
BLACK, DKGREEN, DKORANGE, LORANGE = 1, 5, 4, 3
SGC = [GREEN, YELLOW, BLUE, RED, BUFF, CYAN, MAGENTA, ORANGE]
CG = [[GREEN, YELLOW, BLUE, RED], [BUFF, CYAN, MAGENTA, ORANGE]]

def carray(path, name):
    s = open(path).read()
    i = s.index(name); j = s.index('{', i); k = s.index('};', j)
    return [int(x, 16) for x in re.findall(r'0x[0-9a-fA-F]+', s[j:k])]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('fb'); ap.add_argument('out')
    ap.add_argument('--ox', type=int, default=64); ap.add_argument('--oy', type=int, default=48)
    ap.add_argument('--xroar-src', default='/home/claude/proj/emu/src/xroar-main/src')
    a = ap.parse_args()
    font = carray(a.xroar_src + '/mc6847/font-6847.c', 'font_6847[')
    glyph = {}
    for i in range(64):
        bits = tuple(font[i * 12:i * 12 + 12])
        glyph[bits] = i
    raw = np.fromfile(a.fb, dtype=np.uint8)
    n = raw.size // (W * H)
    frames = raw[:n * W * H].reshape(n, H, W) & 15
    out = open(a.out, 'wb')
    counts = {}
    for f in range(n):
        dots = frames[f, a.oy:a.oy + 384:2, a.ox:a.ox + 512:2]   # 192 x 256 dots
        # border colour at mid height: a mode change made just after field
        # sync can land in the top border (seen on XRoar's PAL Dragon)
        border = int(frames[f, a.oy + 192, 4])
        typ, css, data = 0, 0, bytes(6144)
        if border == BLACK:
            typ, css, data = decode_text(dots, glyph)
        elif border in (GREEN, BUFF):
            css = 1 if border == BUFF else 0
            if (dots == BLACK).any() or (dots == DKGREEN).any():
                typ, data = decode_rg6(dots, css)
            else:
                typ, data = decode_cg2(dots, css)
                if typ == 0:
                    typ, data = decode_cg6(dots, css)
        out.write(bytes([typ, css]) + data.ljust(6144, b'\0'))
        counts[typ] = counts.get(typ, 0) + 1
    print('frames: %d  types: %s' % (n, {['unknown', 'text', 'cg2', 'rg6', 'cg6'][k]: v for k, v in sorted(counts.items())}))

def decode_text(dots, glyph):
    codes = bytearray(512)
    # colour set of the alphanumeric cells: the dark background colour tells
    # (a solid orange cell is a space in the orange set but a semigraphics
    # block in the green set)
    css = 1 if (dots == DKORANGE).any() and not (dots == DKGREEN).any() else 0
    alpha = [{GREEN, DKGREEN}, {LORANGE, ORANGE, DKORANGE}][css]
    for cy in range(16):
        for cx in range(32):
            cell = dots[cy * 12:cy * 12 + 12, cx * 8:cx * 8 + 8]
            cols = set(np.unique(cell).tolist())
            dark = DKGREEN if css == 0 else DKORANGE
            if cols <= alpha and (dark in cols or cols == {GREEN} or cols == {LORANGE}):
                if dark not in cols:
                    codes[cy * 32 + cx] = 0x60          # normal space (= solid light)
                    continue
                normal = tuple(int(sum((1 << (7 - x)) for x in range(8) if cell[y, x] == dark)) for y in range(12))
                inverse = tuple(int(sum((1 << (7 - x)) for x in range(8) if cell[y, x] != dark)) for y in range(12))
                if normal in glyph: codes[cy * 32 + cx] = 0x40 | glyph[normal]
                elif inverse in glyph: codes[cy * 32 + cx] = glyph[inverse]
                else: return 0, 0, bytes(6144)
            else:
                lit = cols - {BLACK}
                if len(lit) > 1: return 0, 0, bytes(6144)
                colour = SGC.index(lit.pop()) if lit else 0
                q = 0
                for k, (y0, x0) in enumerate([(0, 0), (0, 4), (6, 0), (6, 4)]):
                    blk = cell[y0:y0 + 6, x0:x0 + 4]
                    if (blk != BLACK).all(): q |= 8 >> k
                    elif not (blk == BLACK).all(): return 0, 0, bytes(6144)
                codes[cy * 32 + cx] = 0x80 | (colour << 4) | q
    return 1, css, bytes(codes)

def decode_cg2(dots, css):
    lut = {c: i for i, c in enumerate(CG[css])}
    pix = dots[1:192:3, 0:256:2]                      # 64 x 128, middle line of each row
    blocks = dots.reshape(64, 3, 128, 2)
    if not (blocks[:, :, :, 0] == blocks[:, :, :, 1]).all():
        return 0, bytes(6144)                         # pixels must be 2 dots wide
    # the CPU may write to the screen while the 6847 draws a row: the row's 3
    # lines then differ. Accept up to 2 such rows (their middle line is what
    # the 6847 showed there, and what RGBtoHDMI reads)
    torn = (~(blocks[:, :, :, 0] == pix[:, None, :]).all(axis=(1, 2))).sum()
    if torn > 2:
        return 0, bytes(6144)
    vals = np.vectorize(lambda c: lut.get(int(c), -1))(pix)
    if (vals < 0).any():
        return 0, bytes(6144)
    vals = vals.reshape(64, 32, 4)
    data = (vals[:, :, 0] << 6) | (vals[:, :, 1] << 4) | (vals[:, :, 2] << 2) | vals[:, :, 3]
    return 2, data.astype(np.uint8).tobytes()

def decode_cg6(dots, css):
    # PMODE 3: 128x192, pixels 2 dots wide, 1 line high, 32 bytes per row
    lut = {c: i for i, c in enumerate(CG[css])}
    pairs = dots.reshape(192, 128, 2)
    if not (pairs[:, :, 0] == pairs[:, :, 1]).all():
        return 0, bytes(6144)
    vals = np.vectorize(lambda c: lut.get(int(c), -1))(pairs[:, :, 0])
    if (vals < 0).any():
        return 0, bytes(6144)
    vals = vals.reshape(192, 32, 4)
    data = (vals[:, :, 0] << 6) | (vals[:, :, 1] << 4) | (vals[:, :, 2] << 2) | vals[:, :, 3]
    return 4, data.astype(np.uint8).tobytes()

def decode_rg6(dots, css):
    lit = BUFF if css else GREEN
    # XRoar draws the green set's "black" as dark green
    if not np.isin(dots, [BLACK, DKGREEN, lit]).all():
        return 0, bytes(6144)
    bits = (dots == lit).reshape(192, 32, 8)
    data = np.zeros((192, 32), dtype=np.uint8)
    for b in range(8):
        data |= (bits[:, :, b].astype(np.uint8) << (7 - b))
    return 3, data.tobytes()

if __name__ == '__main__':
    main()
