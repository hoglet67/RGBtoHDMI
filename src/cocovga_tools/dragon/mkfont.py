#!/usr/bin/env python3
"""
mkfont.py - FONT.BIN: a test character set for CoCoVGA's character RAM
(256 characters x 12 rows, top row first, bit 7 = left dot; 3072 bytes).

Chosen so that every part is easy to tell from the 6847's own font:
  $00-$1F  6847T1 lower case (a-z etc.), on the 6847's base line
  $20-$3F  6847 glyphs $20-$3F (digits, punctuation) in bold, underlined
  $40-$7F  6847 glyphs $40-$7F (capitals) in bold
  $80-$FF  16 patterns (bits 3-0) in the semigraphics colour (bits 6-4)
For characters $00-$7F a set bit is a dark dot on the light background, for
$80-$FF a coloured dot on black.

  mkfont.py ../../cocovga_font.c FONT.raw
"""
import re, sys

def c_font(src, name, n):
    t = open(src).read()
    body = t[t.index('const uint8_t %s[' % name):]
    body = body[body.index('{') + 1:]
    vals = [int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{2})', body[:body.index('};')])]
    assert len(vals) == n * 12, (name, len(vals))
    return [vals[i * 12:(i + 1) * 12] for i in range(n)]

def bold(g):
    return [(r | (r >> 1)) & 0xFF for r in g]

def patterns():
    p = []
    p.append([0xFF] * 12)                                   # 0 solid
    p.append([0xAA if y & 1 else 0x55 for y in range(12)])  # 1 checker
    p.append([0xFF if y & 2 else 0x00 for y in range(12)])  # 2 horizontal stripes
    p.append([0xCC] * 12)                                   # 3 vertical stripes
    p.append([(0x80 >> (y % 8)) | (0x80 >> ((y + 4) % 8)) for y in range(12)])   # 4 diagonal
    p.append([(0x01 << (y % 8)) | (0x01 << ((y + 4) % 8)) for y in range(12)])   # 5 other diagonal
    p.append([0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF])   # 6 box
    p.append([0x18, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF, 0x18, 0x18, 0x18, 0x18, 0x18])   # 7 cross
    p.append([0x00, 0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00])   # 8 circle
    p.append([0x18, 0x18, 0x3C, 0x3C, 0x7E, 0x7E, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00])   # 9 triangle
    p.append([0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81, 0x81, 0x42, 0x24, 0x18])   # A X
    p.append([0xF0] * 6 + [0x0F] * 6)                       # B diagonal halves
    p.append([0x00] * 6 + [0xFF] * 6)                       # C bottom half
    p.append([0xF0] * 12)                                   # D left half
    p.append([0x00, 0x00, 0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00, 0x00, 0x00])   # E heart
    p.append([0x80 >> (x % 8) for x in range(12)])          # F thin diagonal
    return p

def main():
    src, out = sys.argv[1:3]
    f6847 = c_font(src, 'mc6847_font', 64)
    t1 = c_font(src, 'mc6847t1_font', 96)
    font = []
    for c in range(32):                                     # lower case on the 6847 base line
        g = t1[64 + c]
        font.append([0, 0] + g[:10])
    for c in range(32, 64):
        g = bold(f6847[c]); g[11] = 0x7E
        font.append(g)
    for c in range(64):
        font.append(bold(f6847[c]))
    pat = patterns()
    for c in range(128):
        font.append(pat[c & 15])
    data = bytes(b for g in font for b in g)
    assert len(data) == 3072
    open(out, 'wb').write(data)
    print('FONT: %d bytes' % len(data))

if __name__ == '__main__':
    main()
