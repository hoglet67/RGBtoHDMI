#!/usr/bin/env python3
"""
mkhebrew.py - HEBREW.BIN: a Hebrew character set for CoCoVGA's character RAM
(256 characters x 12 rows, top row first, bit 7 = left dot; 3072 bytes).

The 27 Hebrew letters (22 + 5 final forms) take the place of lower case, in
the order of the Israeli 7 bit code SI-960 (ASCII $60-$7A): on the 6847 that
is codes $00-$1A, which W64LIB's PUTSTR makes of lower case ASCII, so in a
BASIC string ` a b c ... z  =  alef bet gimel ... tav.
  $00-$1A  Hebrew letters (on the 6847's base line, row 9; lamed rises to
           row 1, the final forms and qof go down to row 11)
  $1B-$3F  the 6847's inverse characters, as on a plain 6847
  $40-$7F  the 6847's characters
  $80-$FF  SG4 (2 x 2 blocks), as on a plain 6847
For $00-$7F a set bit is a dark dot on the light background, for $80-$FF a
dot in the semigraphics colour on black.

  mkhebrew.py ../../cocovga_font.c HEBREW.raw [preview.png]
"""
import re, sys

# alef .. tav in SI-960 order; '#' = dark dot. Each glyph lists rows from
# row FIRST down; unlisted rows are empty.
LETTERS = [
 ('alef', 3, ['.#...#..',
              '..#..#..',
              '...##...',
              '...#....',
              '..#.#...',
              '.#...#..',
              '.#....#.']),
 ('bet', 3, ['.####...',
             '.....#..',
             '.....#..',
             '.....#..',
             '.....#..',
             '.....#..',
             '.######.']),
 ('gimel', 3, ['..##....',
               '....#...',
               '....#...',
               '....#...',
               '...##...',
               '..#.#...',
               '.#..#...']),
 ('dalet', 3, ['.######.',
               '.....#..',
               '.....#..',
               '.....#..',
               '.....#..',
               '.....#..',
               '.....#..']),
 ('he', 3, ['.#####..',
            '.....#..',
            '.....#..',
            '.#...#..',
            '.#...#..',
            '.#...#..',
            '.#...#..']),
 ('vav', 3, ['..##....',
             '...#....',
             '...#....',
             '...#....',
             '...#....',
             '...#....',
             '...#....']),
 ('zayin', 3, ['.#####..',
               '...#....',
               '...#....',
               '...#....',
               '...#....',
               '...#....',
               '...#....']),
 ('het', 3, ['.#####..',
             '.#...#..',
             '.#...#..',
             '.#...#..',
             '.#...#..',
             '.#...#..',
             '.#...#..']),
 ('tet', 3, ['.#..##..',
             '.#.#..#.',
             '.#....#.',
             '.#....#.',
             '.#....#.',
             '.#....#.',
             '..####..']),
 ('yod', 3, ['..##....',
             '...#....',
             '...#....']),
 ('final kaf', 3, ['.#####..',
                   '.....#..',
                   '.....#..',
                   '.....#..',
                   '.....#..',
                   '.....#..',
                   '.....#..',
                   '.....#..',
                   '.....#..']),
 ('kaf', 3, ['.####...',
             '.....#..',
             '.....#..',
             '.....#..',
             '.....#..',
             '.....#..',
             '.####...']),
 ('lamed', 1, ['.#......',
               '.#......',
               '.#####..',
               '.....#..',
               '.....#..',
               '....#...',
               '...#....',
               '..#.....',
               '..#.....']),
 ('final mem', 3, ['.#####..',
                   '.#...#..',
                   '.#...#..',
                   '.#...#..',
                   '.#...#..',
                   '.#...#..',
                   '.#####..']),
 ('mem', 3, ['.#.##...',
             '..#..#..',
             '.#...#..',
             '.#...#..',
             '.#...#..',
             '.#...#..',
             '.#.###..']),
 ('final nun', 3, ['..##....',
                   '...#....',
                   '...#....',
                   '...#....',
                   '...#....',
                   '...#....',
                   '...#....',
                   '...#....',
                   '...#....']),
 ('nun', 3, ['..##....',
             '...#....',
             '...#....',
             '...#....',
             '...#....',
             '...#....',
             '.###....']),
 ('samekh', 3, ['.#####..',
                '.#...#..',
                '.#...#..',
                '.#...#..',
                '.#...#..',
                '..#.#...',
                '...#....']),
 ('ayin', 3, ['.#...#..',
              '.#...#..',
              '..#..#..',
              '..#..#..',
              '...#.#..',
              '....#...',
              '.###....']),
 ('final pe', 3, ['.#####..',
                  '.#...#..',
                  '.#...#..',
                  '.....#..',
                  '.....#..',
                  '.....#..',
                  '.....#..',
                  '.....#..',
                  '.....#..']),
 ('pe', 3, ['.#####..',
            '.#...#..',
            '.##..#..',
            '.....#..',
            '.....#..',
            '.....#..',
            '.#####..']),
 ('final tsadi', 3, ['.#...#..',
                     '..#..#..',
                     '...##...',
                     '...#....',
                     '...#....',
                     '...#....',
                     '...#....',
                     '...#....',
                     '...#....']),
 ('tsadi', 3, ['.#...#..',
               '..#..#..',
               '...#.#..',
               '....#...',
               '.....#..',
               '......#.',
               '.######.']),
 ('qof', 3, ['.#####..',
             '.....#..',
             '.....#..',
             '.#...#..',
             '.#..#...',
             '.#......',
             '.#......',
             '.#......',
             '.#......']),
 ('resh', 3, ['.####...',
              '.....#..',
              '.....#..',
              '.....#..',
              '.....#..',
              '.....#..',
              '.....#..']),
 ('shin', 3, ['.#.#.#..',
              '.#.#.#..',
              '.#.#.#..',
              '.#..##..',
              '.#...#..',
              '.#..#...',
              '..##....']),
 ('tav', 3, ['.#####..',
             '..#..#..',
             '..#..#..',
             '..#..#..',
             '..#..#..',
             '..#..#..',
             '##...#..']),
]

def glyph(first, rows):
    g = [0] * 12
    for i, r in enumerate(rows):
        assert len(r) == 8, r
        g[first + i] = sum(0x80 >> x for x, c in enumerate(r) if c == '#')
    return g

def c_font(src, name, n):
    t = open(src).read()
    body = t[t.index('const uint8_t %s[' % name):]
    body = body[body.index('{') + 1:]
    vals = [int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{2})', body[:body.index('};')])]
    assert len(vals) == n * 12, (name, len(vals))
    return [vals[i * 12:(i + 1) * 12] for i in range(n)]

def build(src):
    f6847 = c_font(src, 'mc6847_font', 64)
    font = []
    for c in range(64):
        if c < len(LETTERS):
            font.append(glyph(LETTERS[c][1], LETTERS[c][2]))
        elif c < 64:
            font.append([~b & 0xFF for b in f6847[c]])      # inverse, as the 6847
    for c in range(64):
        font.append(list(f6847[c]))
    for c in range(128):                                     # SG4: bit 3 top left .. bit 0 bottom right
        q = c & 15
        top = (0xF0 if q & 8 else 0) | (0x0F if q & 4 else 0)
        bot = (0xF0 if q & 2 else 0) | (0x0F if q & 1 else 0)
        font.append([top] * 6 + [bot] * 6)
    assert len(LETTERS) == 27
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
    src, out = sys.argv[1:3]
    data = build(src)
    open(out, 'wb').write(data)
    if len(sys.argv) > 3:
        preview(data, sys.argv[3])
    print('HEBREW: %d bytes' % len(data))

if __name__ == '__main__':
    main()
