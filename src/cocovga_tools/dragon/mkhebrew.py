#!/usr/bin/env python3
"""
mkhebrew.py - HEBREW.BIN: a Hebrew character set for CoCoVGA's character RAM
(256 characters x 12 rows, top row first, bit 7 = left dot; 3072 bytes).

The 27 Hebrew letters (22 + 5 final forms) take the place of lower case, in
the order of the Israeli 7 bit code SI-960 (ASCII $60-$7A): on the 6847 that
is codes $00-$1A, which W64LIB's PUTSTR makes of lower case ASCII, so in a
BASIC string ` a b c ... z  =  alef bet gimel ... tav.
  $00-$1A  Hebrew letters from the Xanmono font (SIL OFL 1.1, see XANMONO
           below): 6 x 6 dots on the 6847's base line (row 9), lamed to
           row 3, the final forms and qof down to row 10
  $1B-$3F  the 6847's inverse characters, as on a plain 6847
  $40-$7F  the 6847's characters
  $80-$FF  SG4 (2 x 2 blocks), as on a plain 6847
For $00-$7F a set bit is a dark dot on the light background, for $80-$FF a
dot in the semigraphics colour on black.

  mkhebrew.py [--otf Xanmono-Regular.otf] ../../cocovga_font.c HEBREW.raw [preview.png]
"""
import re, sys

# alef .. tav in SI-960 order, as 12 rows of 8 dots ('#' = dark dot).
# The letters are taken from Xanmono by GGBotNet (c) 2024, licensed under
# the SIL Open Font License 1.1 (https://scripts.sil.org/OFL): rasterised
# from its outlines on their own grid (64 units = 1 dot, 6 x 6 letters,
# lamed one dot higher, final forms and qof one lower), baseline on the
# 6847's (row 9). This table is a derivative of Xanmono and so is under the
# OFL as well. `mkhebrew.py --otf Xanmono-Regular.otf` extracts it again.
XANMONO = [
 ('alef', ['........', '........', '........', '........', '.#....#.', '..#...#.', '..##.##.', '.##.##..', '.#...#..', '.#....#.', '........', '........']),
 ('bet', ['........', '........', '........', '........', '.####...', '....##..', '.....#..', '.....#..', '.....#..', '.######.', '........', '........']),
 ('gimel', ['........', '........', '........', '........', '..##....', '...##...', '...##...', '..###...', '..#..#..', '..#..#..', '........', '........']),
 ('dalet', ['........', '........', '........', '........', '.######.', '.....#..', '.....#..', '.....#..', '.....#..', '.....#..', '........', '........']),
 ('he', ['........', '........', '........', '........', '.#####..', '.....##.', '.#....#.', '.#....#.', '.#....#.', '.#....#.', '........', '........']),
 ('vav', ['........', '........', '........', '........', '...##...', '....#...', '....#...', '....#...', '....#...', '....#...', '........', '........']),
 ('zayin', ['........', '........', '........', '........', '..####..', '...##...', '...#....', '...#....', '...#....', '...#....', '........', '........']),
 ('het', ['........', '........', '........', '........', '.#####..', '.#...##.', '.#....#.', '.#....#.', '.#....#.', '.#....#.', '........', '........']),
 ('tet', ['........', '........', '........', '........', '.#..##..', '.#....#.', '.#....#.', '.#....#.', '.##..##.', '..####..', '........', '........']),
 ('yod', ['........', '........', '........', '........', '...##...', '....#...', '....#...', '........', '........', '........', '........', '........']),
 ('final kaf', ['........', '........', '........', '........', '.#####..', '.....##.', '......#.', '......#.', '......#.', '......#.', '......#.', '........']),
 ('kaf', ['........', '........', '........', '........', '.#####..', '.....##.', '......#.', '......#.', '.....##.', '.#####..', '........', '........']),
 ('lamed', ['........', '........', '........', '.#......', '.#####..', '......#.', '......#.', '.....##.', '...###..', '..#.....', '........', '........']),
 ('final mem', ['........', '........', '........', '........', '.#####..', '.#...##.', '.#....#.', '.#....#.', '.#....#.', '.######.', '........', '........']),
 ('mem', ['........', '........', '........', '........', '.#.####.', '..##..#.', '..#...#.', '..#...#.', '.#....#.', '.#.####.', '........', '........']),
 ('final nun', ['........', '........', '........', '........', '..###...', '....#...', '....#...', '....#...', '....#...', '....#...', '....#...', '........']),
 ('nun', ['........', '........', '........', '........', '..###...', '....##..', '.....#..', '.....#..', '.....#..', '..####..', '........', '........']),
 ('samekh', ['........', '........', '........', '........', '######..', '.##...#.', '.#....#.', '.#....#.', '.##..##.', '..####..', '........', '........']),
 ('ayin', ['........', '........', '........', '........', '..#...#.', '..#...#.', '...#..#.', '...##.#.', '....###.', '.#####..', '........', '........']),
 ('final pe', ['........', '........', '........', '........', '.#####..', '.#...##.', '.###..#.', '.###..#.', '......#.', '......#.', '......#.', '........']),
 ('pe', ['........', '........', '........', '........', '.#####..', '.#...##.', '.###..#.', '.###..#.', '.....##.', '.#####..', '........', '........']),
 ('final tsadi', ['........', '........', '........', '........', '.###..#.', '...##.#.', '....###.', '....##..', '....#...', '....#...', '....#...', '........']),
 ('tsadi', ['........', '........', '........', '........', '.#....#.', '..#...#.', '...#.##.', '....##..', '.....#..', '.######.', '........', '........']),
 ('qof', ['........', '........', '........', '........', '.#####..', '......#.', '.#....#.', '.#...##.', '.#.###..', '.#......', '.#......', '........']),
 ('resh', ['........', '........', '........', '........', '.#####..', '.....##.', '......#.', '......#.', '......#.', '......#.', '........', '........']),
 ('shin', ['........', '........', '........', '........', '.#.##.#.', '.#.#..#.', '.#.#..#.', '.##...#.', '.##..##.', '..####..', '........', '........']),
 ('tav', ['........', '........', '........', '........', '.#####..', '..#..##.', '..#...#.', '..#...#.', '..#...#.', '.#....#.', '........', '........']),
]

def glyph(rows):
    assert len(rows) == 12
    return [sum(0x80 >> x for x, c in enumerate(r) if c == '#') for r in rows]

def from_otf(path):
    """the 27 letters from an outline font with Hebrew on a 64 unit grid
    (Xanmono): rendered 8 times oversize, a dot is set when half covered"""
    from PIL import Image, ImageDraw, ImageFont
    font = ImageFont.truetype(path, 64)            # 512 units -> 64 px: 1 dot = 8 px
    out = []
    for i, c in enumerate(range(0x5D0, 0x5EB)):
        im = Image.new('L', (64, 96), 0)
        ImageDraw.Draw(im).text((0, 80), chr(c), font=font, fill=255, anchor='ls')   # baseline: bottom of row 9
        rows = [''.join('#' if sum(im.getpixel((x * 8 + i2, r * 8 + j)) for i2 in range(8) for j in range(8))
                        >= 0.5 * 64 * 255 else '.' for x in range(8)) for r in range(12)]
        out.append((XANMONO[i][0], rows))
    return out

def c_font(src, name, n):
    t = open(src).read()
    body = t[t.index('const uint8_t %s[' % name):]
    body = body[body.index('{') + 1:]
    vals = [int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{2})', body[:body.index('};')])]
    assert len(vals) == n * 12, (name, len(vals))
    return [vals[i * 12:(i + 1) * 12] for i in range(n)]

def build(src, letters):
    f6847 = c_font(src, 'mc6847_font', 64)
    font = []
    for c in range(64):
        if c < len(letters):
            font.append(glyph(letters[c][1]))
        elif c < 64:
            font.append([~b & 0xFF for b in f6847[c]])      # inverse, as the 6847
    for c in range(64):
        font.append(list(f6847[c]))
    for c in range(128):                                     # SG4: bit 3 top left .. bit 0 bottom right
        q = c & 15
        top = (0xF0 if q & 8 else 0) | (0x0F if q & 4 else 0)
        bot = (0xF0 if q & 2 else 0) | (0x0F if q & 1 else 0)
        font.append([top] * 6 + [bot] * 6)
    assert len(letters) == 27
    gl = [tuple(font[c]) for c in range(27)]
    assert len(set(gl)) == 27, 'two Hebrew letters are the same'
    return bytes(b for g in font for b in g)

def preview(data, path):
    from PIL import Image
    green, dark = (0x2E, 0xC8, 0x3E), (0x0E, 0x3A, 0x0D)
    im = Image.new('RGB', (27 * 10 * 4, 14 * 4), dark)
    px = im.load()
    for i in range(27):
        c = 26 - i                       # alef on the right, as Hebrew reads
        for y in range(12):
            b = data[c * 12 + y]
            for x in range(10):
                on = 1 <= x <= 8 and (b >> (8 - x)) & 1
                for dy in range(4):
                    for dx in range(4):
                        px[(i * 10 + x) * 4 + dx, (y + 1) * 4 + dy] = dark if on else green
    im.save(path)

def main():
    args = sys.argv[1:]
    letters = XANMONO
    if args[:1] == ['--otf']:
        letters = from_otf(args[1])
        if letters != XANMONO:
            print('note: the letters from %s differ from the table in this script' % args[1])
        args = args[2:]
    src, out = args[:2]
    data = build(src, letters)
    open(out, 'wb').write(data)
    if len(args) > 2:
        preview(data, args[2])
    print('HEBREW: %d bytes' % len(data))

if __name__ == '__main__':
    main()
