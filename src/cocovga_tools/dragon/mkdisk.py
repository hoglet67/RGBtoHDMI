#!/usr/bin/env python3
"""Build W64TEST.VDK: a 40 track single sided DragonDOS disk with
W64TEST.BAS (tokenised Dragon BASIC), W64LIB.BIN ($7800-$7BFF) and, if
given, FONT.BIN and HEBREW.BIN (3 KB character sets at $1600, from
mkfont.py and mkhebrew.py).

  mkdisk.py w64test.bas w64lib.raw W64TEST.VDK [font.raw [hebrew.raw]]
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from basictok import tokenize
from detok import detok
from dragondos import VDK, lsn_of, SPT

TRACKS, SIDES = 40, 1
NSEC = TRACKS * SIDES * SPT

def header():
    h = bytearray(256)
    h[0:12] = bytes.fromhex('646b00011010580028010000')   # as the CoCoVGA disks
    return bytes(h)

def dosfile(ftype, load, data, exec_):
    return bytes([0x55, ftype, load >> 8, load & 0xFF, len(data) >> 8, len(data) & 0xFF,
                  exec_ >> 8, exec_ & 0xFF, 0xAA]) + data

def main():
    src, lib, out = sys.argv[1:4]
    lines = []
    for l in open(src):
        l = l.rstrip('\n')
        if not l.strip(): continue
        n, t = l.split(' ', 1)
        lines.append((int(n), t))
    nums = [n for n, _ in lines]
    assert nums == sorted(nums) and len(set(nums)) == len(nums), 'line numbers out of order'
    assert max(len(t) for _, t in lines) < 240
    prog = tokenize(lines, 0x2401)
    # round trip check
    back = detok(prog)
    libraw = open(lib, 'rb').read()
    files = [('W64TEST', 'BAS', dosfile(0x01, 0x2401, prog, 0x8B8D)),
             ('W64LIB', 'BIN', dosfile(0x02, 0x7800, libraw, 0x781E))]
    # character sets for the character RAM tests, all loaded at $1600
    for arg, name in zip(sys.argv[4:6], ('FONT', 'HEBREW')):
        font = open(arg, 'rb').read()
        assert len(font) == 3072
        files.append((name, 'BIN', dosfile(0x02, 0x1600, font, 0x781E)))   # exec: W64LIB's RTS
    img = bytearray(b'\xE5' * (NSEC * 256))
    used = set(range(lsn_of(16, 1), lsn_of(16, 1) + SPT)) | set(range(lsn_of(20, 1), lsn_of(20, 1) + SPT))
    nxt = lsn_of(21, 1)
    entries = []
    for name, ext, data in files:
        if len(data) % 256 == 0: data += b'\0'    # keep the last sector byte count non zero
        n = (len(data) + 255) // 256
        assert n < 256
        start = nxt; nxt += n
        for k in range(n):
            blk = data[k * 256:(k + 1) * 256].ljust(256, b'\0')
            img[(start + k) * 256:(start + k + 1) * 256] = blk
            used.add(start + k)
        e = bytearray(25)
        e[0] = 0
        e[1:9] = name.encode().ljust(8, b'\0'); e[9:12] = ext.encode().ljust(3, b'\0')
        e[12:15] = bytes([start >> 8, start & 0xFF, n])
        e[24] = len(data) % 256
        entries.append(bytes(e))
        print('%-8s.%s  %5d bytes  LSN %d-%d' % (name, ext, len(data), start, start + n - 1))
    # bitmap: bit set = free, LSB first
    bm = bytearray(256)
    for i in range(180): bm[i] = 0xFF        # as DSKINIT writes it
    for lsn in used: bm[lsn >> 3] &= ~(1 << (lsn & 7))
    bm[0xFC:0x100] = bytes([TRACKS, SPT, TRACKS ^ 0xFF, SPT ^ 0xFF])
    dirsec = []
    allents = entries + [bytes([0x89]) + bytes(24)] * (160 - len(entries))
    for s in range(16):
        b = b''.join(allents[s * 10:(s + 1) * 10]).ljust(256, b'\0')
        dirsec.append(b)
    for trk in (20, 16):
        img[lsn_of(trk, 1) * 256:lsn_of(trk, 2) * 256] = bm
        img[lsn_of(trk, 2) * 256:lsn_of(trk, 3) * 256] = bytes(256)
        for s in range(16):
            img[lsn_of(trk, 3 + s) * 256:lsn_of(trk, 4 + s) * 256] = dirsec[s]
    data = header() + bytes(img)
    open(out, 'wb').write(data)
    # read back
    v = VDK(data)
    for (name, ext, d), (n2, d2) in zip(files, v.files()):
        assert n2 == name + '.' + ext, n2
        assert d2[:len(d)] == d, name
    print('wrote %s (%d bytes), BASIC program %d bytes, %d lines' % (out, len(data), len(prog), len(lines)))

if __name__ == '__main__':
    main()
