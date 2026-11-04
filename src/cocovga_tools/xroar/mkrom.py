#!/usr/bin/env python3
"""
mkrom.py - build a 16 KB Dragon / CoCo test ROM that drives the MC6847 the
way NitrOS-9's CoVGA driver does (plus stage 2 test screens), and write
phases.json describing what each phase puts on the screen.

Small two-pass assembler for the handful of 6809 instructions used.

Memory map (RAM, all visible to the VDG via the SAM):
  $0400  text page, BASIC-like screen
  $0600  CoCoVGA register page 0 (NitrOS-9 VGASetup)
  $0800  text page with inverse codes (stage 2 lower case)
  $0A00  register page: force lower case on
  $0C00  register page: lower case off
  $1000  CG2 2 KB: 64x32 text codes
  $2000  RG6 6 KB: PMODE 4 artifact patterns
"""
import json, sys

ROM_BASE = 0x8000          # 16 KB ROM at $8000-$BFFF; vectors come from $BFF0-$BFFF

class Asm:
    def __init__(self, org):
        self.org = org
        self.code = []          # list of (addr, bytes or callable)
        self.labels = {}
        self.pc = org
        self.fix = []           # (offset in out, kind, label)
        self.out = bytearray()

    def label(self, name):
        assert name not in self.labels, name
        self.labels[name] = self.pc

    def emit(self, *bs):
        for b in bs:
            self.out.append(b & 0xFF)
        self.pc += len(bs)

    def w(self, v):
        self.emit(v >> 8, v)

    # --- instructions (only what we use) -------------------------------
    def lda_i(self, v): self.emit(0x86, v)
    def ldb_i(self, v): self.emit(0xC6, v)
    def ldd_i(self, v): self.emit(0xCC); self.w(v)
    def ldx_i(self, v): self.emit(0x8E); self.w(v)
    def ldy_i(self, v): self.emit(0x10, 0x8E); self.w(v)
    def ldu_i(self, v): self.emit(0xCE); self.w(v)
    def lds_i(self, v): self.emit(0x10, 0xCE); self.w(v)
    def sta(self, a): self.emit(0xB7); self.w(a)
    def stb(self, a): self.emit(0xF7); self.w(a)
    def lda(self, a): self.emit(0xB6); self.w(a)
    def ldb(self, a): self.emit(0xF6); self.w(a)
    def std(self, a): self.emit(0xFD); self.w(a)
    def ldd(self, a): self.emit(0xFC); self.w(a)
    def stx(self, a): self.emit(0xBF); self.w(a)
    def ldx(self, a): self.emit(0xBE); self.w(a)
    def clr(self, a): self.emit(0x7F); self.w(a)
    def inc(self, a): self.emit(0x7C); self.w(a)
    def lda_xp(self): self.emit(0xA6, 0x80)      # LDA ,X+
    def sta_yp(self): self.emit(0xA7, 0xA0)      # STA ,Y+
    def sta_xp(self): self.emit(0xA7, 0x80)      # STA ,X+
    def ldd_xpp(self): self.emit(0xEC, 0x81)     # LDD ,X++
    def std_ypp(self): self.emit(0xED, 0xA1)     # STD ,Y++
    def cmpx_i(self, v): self.emit(0x8C); self.w(v)
    def cmpy_i(self, v): self.emit(0x10, 0x8C); self.w(v)
    def deca(self): self.emit(0x4A)
    def decb(self): self.emit(0x5A)
    def inca(self): self.emit(0x4C)
    def tstb(self): self.emit(0x5D)
    def orcc(self, v): self.emit(0x1A, v)
    def rts(self): self.emit(0x39)
    def nop(self): self.emit(0x12)

    def _rel8(self, op, label):
        self.emit(op, 0)
        self.fix.append((len(self.out) - 1, 'rel8', label, self.pc))
    def bra(self, l): self._rel8(0x20, l)
    def bne(self, l): self._rel8(0x26, l)
    def beq(self, l): self._rel8(0x27, l)
    def bpl(self, l): self._rel8(0x2A, l)
    def bsr(self, l): self._rel8(0x8D, l)
    def jsr(self, l):
        self.emit(0xBD, 0, 0)
        self.fix.append((len(self.out) - 2, 'abs16', l, self.pc))
    def jmp(self, l):
        self.emit(0x7E, 0, 0)
        self.fix.append((len(self.out) - 2, 'abs16', l, self.pc))
    def ldx_l(self, l):              # LDX #label
        self.emit(0x8E, 0, 0)
        self.fix.append((len(self.out) - 2, 'abs16', l, self.pc))

    def data(self, bs):
        for b in bs:
            self.emit(b)

    def link(self):
        for off, kind, label, after in self.fix:
            t = self.labels[label]
            if kind == 'rel8':
                d = t - after
                assert -128 <= d <= 127, (label, d)
                self.out[off] = d & 0xFF
            else:
                self.out[off] = t >> 8
                self.out[off + 1] = t & 0xFF
        return self.out

# ---------------------------------------------------------------------
# screen contents
# ---------------------------------------------------------------------
def covdg(ch):
    """NitrOS-9 CoVDG translation of an ASCII character to a 6847 code"""
    a = ord(ch)
    if a == ord('^'): return 0x00
    if a == ord('_'): return 0x1F
    if a == ord('`'): return 0x67
    if a >= 0x60: return a - 0x60          # lower case -> inverse codes
    if a >= 0x40: return (a - 0x40) ^ 0x40
    return a ^ 0x40

def basic_code(ch):
    """Dragon BASIC: normal (non-inverse) upper case"""
    a = ord(ch.upper())
    if a >= 0x40: return (a - 0x40) | 0x40
    return a | 0x40

def put(buf, row, col, s, cols, fn):
    for i, ch in enumerate(s):
        if col + i < cols:
            buf[row * cols + col + i] = fn(ch)

text_basic = bytearray([0x60] * 512)
put(text_basic, 0, 0, "(C) 1983 TANO CORPORATION", 32, basic_code)
put(text_basic, 1, 0, "BASIC INTERPRETER 1.0", 32, basic_code)
put(text_basic, 3, 0, "OK", 32, basic_code)

regs_nitros9 = bytearray([0x60] * 512)
regs_nitros9[0:9] = bytes([0x00, 0x81, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x02])
regs_lower_on = bytearray([0x60] * 512)
regs_lower_on[0:9] = bytes([0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00])
regs_lower_off = bytearray([0x60] * 512)
regs_lower_off[0:9] = bytes([0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])

text_inverse = bytearray([0x60] * 512)
put(text_inverse, 0, 0, "LOWER CASE TEST", 32, basic_code)
for i in range(64):
    text_inverse[64 + i] = i                       # rows 2-3: every inverse code
for i in range(32):
    text_inverse[160 + i] = 0x80 | ((i & 7) << 4) | (i & 15)   # row 5: semigraphics
msg = "the quick brown fox jumps over"
for i, ch in enumerate(msg):
    text_inverse[224 + i] = (ord(ch) - 0x60) if ch != ' ' else 0x60   # row 7: lower case words

w64 = bytearray([0x60] * 2048)
put(w64, 0, 0, "NitrOS-9/6809 Level 1 V3.3.0", 64, covdg)
put(w64, 1, 0, "Tano Dragon 64 (NTSC) with CoCoVGA", 64, covdg)
put(w64, 3, 0, "{Term|02}/DD:dir -e", 64, covdg)
for r in range(4, 30):
    put(w64, r, 0, "line %02d: The quick brown fox jumps over the lazy dog 0123456789" % r, 64, covdg)

typed = "echo Hello, World!"
TYPE_ROW = 31

def rg6_pattern():
    """PMODE 4 artifact test card: 8 vertical bands"""
    pats = [0xAA, 0x55, 0xCC, 0x33, 0xFF, 0x00, 0xF0, 0x0F]
    buf = bytearray(6144)
    for y in range(192):
        for x in range(32):
            buf[y * 32 + x] = pats[x // 4] if y < 160 else (0xAA if (x & 1) else 0x55)
    return buf

rg6 = rg6_pattern()

# ---------------------------------------------------------------------
# program
# ---------------------------------------------------------------------
a = Asm(ROM_BASE)
SAM = 0xFFC0
FF02, FF03, FF22, FF23 = 0xFF02, 0xFF03, 0xFF22, 0xFF23
COUNT = 0x0100            # frame counter scratch

def sam_mode(v):          # V2 V1 V0
    for i in range(3):
        a.sta(SAM + 2 * i + ((v >> i) & 1))

def sam_offset(addr):     # F0..F6 = addr / 512
    f = addr // 512
    for i in range(7):
        a.sta(SAM + 6 + 2 * i + ((f >> i) & 1))

def vdg(mode):            # $FF22 bits 7..3
    a.lda_i(mode)
    a.sta(FF22)

def wait(frames):
    a.lda_i(frames)
    a.sta(COUNT)
    a.jsr('wait_frames')

def show(addr, sam, vdgmode, frames):
    a.jsr('vsync')
    vdg(vdgmode)
    sam_mode(sam)
    sam_offset(addr)
    if frames > 1:
        wait(frames - 1)

def copy(src_label, dst, n):
    a.ldx_l(src_label)
    a.ldy_i(dst)
    a.label('cp%x' % dst)
    a.ldd_xpp()
    a.std_ypp()
    a.cmpy_i(dst + n)
    a.bne('cp%x' % dst)

TEXT, CG2, RG6 = 0x00, 0xA0, 0xF0
CSS = 0x08
phases = []

def phase(name, **kw):
    kw['name'] = name
    phases.append(kw)

a.label('reset')
a.orcc(0x50)
a.lds_i(0x0200)
# SAM memory size: 64K dynamic RAM (M1=1, M0=0); the reset default is 4K,
# where everything above $0FFF wraps around
a.sta(0xFFDA)            # M0 = 0
a.sta(0xFFDD)            # M1 = 1
# PIA setup: PIA0 CB1 (field sync) flag, PIA1 port B outputs for the VDG
a.clr(FF03); a.clr(FF02); a.lda_i(0x34); a.sta(FF03)
a.clr(FF23); a.lda_i(0xF8); a.sta(FF22); a.lda_i(0x34); a.sta(FF23)
# copy all screens to RAM
copy('d_text_basic', 0x0400, 512)
copy('d_regs_nitros9', 0x0600, 512)
copy('d_text_inverse', 0x0800, 512)
copy('d_regs_lower_on', 0x0A00, 512)
copy('d_regs_lower_off', 0x0C00, 512)
copy('d_w64', 0x1000, 2048)
copy('d_rg6', 0x2000, 6144)

a.label('main')
# 1: BASIC screen
show(0x0400, 0, TEXT, 60);         phase('basic', frames=60, page=0x0400)
# 2: register page, exactly one frame, then CG2 64 column screen
show(0x0600, 0, TEXT, 1);          phase('register_nitros9', frames=1)
show(0x1000, 2, CG2, 60);          phase('w64_start', frames=60, vram='w64_0')
# 3: typing on the bottom line, one character every 2 frames
a.ldx_l('d_typed')
a.ldy_i(0x1000 + TYPE_ROW * 64)
a.label('type_next')
a.lda_xp()
a.beq('type_done')
a.sta_yp()
wait(2)
a.bra('type_next')
a.label('type_done')
phase('w64_typed', frames=2 * len(typed), vram='w64_1')
wait(30);                            phase('w64_typed_hold', frames=30, vram='w64_1')
# 4: scroll the 64x32 screen up 8 lines, one line every 4 frames
a.lda_i(8); a.sta(COUNT + 1)
a.label('scroll')
a.ldx_i(0x1000 + 64)
a.ldy_i(0x1000)
a.label('scroll_cp')
a.ldd_xpp(); a.std_ypp()
a.cmpy_i(0x1000 + 2048 - 64)
a.bne('scroll_cp')
a.ldd_i(0x6060)
a.label('scroll_clr')
a.std_ypp()
a.cmpy_i(0x1000 + 2048)
a.bne('scroll_clr')
wait(4)
a.lda(COUNT + 1); a.deca(); a.sta(COUNT + 1)
a.bne('scroll')
phase('w64_scrolled', frames=32, vram='w64_2')
wait(30);                            phase('w64_scrolled_hold', frames=30, vram='w64_2')
# 5: back to the 32 column BASIC screen (W64 must end)
show(0x0400, 0, TEXT, 60);          phase('basic_again', frames=60, page=0x0400)
# 6 (stage 2): force lower case on, inverse text screen, then off again
show(0x0A00, 0, TEXT, 1);           phase('register_lower_on', frames=1)
show(0x0800, 0, TEXT, 60);          phase('lower32', frames=60, page=0x0800, lower=1)
show(0x0800, 0, TEXT | CSS, 60);    phase('lower32_css1', frames=60, page=0x0800, lower=1, css=1)
show(0x0C00, 0, TEXT, 1);           phase('register_lower_off', frames=1)
show(0x0800, 0, TEXT, 60);          phase('inverse32', frames=60, page=0x0800, lower=0)
# 7 (stage 2): PMODE 4 artifacts, green and buff colour sets
show(0x2000, 6, RG6, 60);           phase('pmode4_css0', frames=60, rg6=1, css=0)
show(0x2000, 6, RG6 | CSS, 60);     phase('pmode4_css1', frames=60, rg6=1, css=1)
a.jmp('main')

a.label('vsync')                    # wait for the start of field sync
a.lda(FF02)
a.label('vs1')
a.lda(FF03)
a.bpl('vs1')
a.lda(FF02)
a.rts()

a.label('wait_frames')
a.label('wf1')
a.jsr('vsync')
a.lda(COUNT); a.deca(); a.sta(COUNT)
a.bne('wf1')
a.rts()

for name, buf in [('d_text_basic', text_basic), ('d_regs_nitros9', regs_nitros9),
                  ('d_text_inverse', text_inverse), ('d_regs_lower_on', regs_lower_on),
                  ('d_regs_lower_off', regs_lower_off), ('d_w64', w64), ('d_rg6', rg6)]:
    a.label(name); a.data(buf)
a.label('d_typed'); a.data([covdg(c) for c in typed] + [0])

rom = a.link()
assert len(rom) <= 0x3FF0, len(rom)
image = bytearray([0xFF] * 0x4000)
image[:len(rom)] = rom
reset = a.labels['reset']
for v in range(0x3FF0, 0x4000, 2):          # every vector -> reset
    image[v] = reset >> 8; image[v + 1] = reset & 0xFF

# expected VRAM states (Python model of the program)
w64_1 = bytearray(w64)
for i, c in enumerate(typed):
    w64_1[TYPE_ROW * 64 + i] = covdg(c)
w64_2 = bytearray(w64_1)
for _ in range(8):
    w64_2 = w64_2[64:] + bytearray([0x60] * 64)

out = sys.argv[1] if len(sys.argv) > 1 else '.'
open(out + '/covga_test.rom', 'wb').write(image)
json.dump({
    'phases': phases,
    'vram': {'w64_0': list(w64), 'w64_1': list(w64_1), 'w64_2': list(w64_2)},
    'pages': {'0x0400': list(text_basic), '0x0800': list(text_inverse)},
    'rg6': list(rg6),
}, open(out + '/phases.json', 'w'))
print('ROM %d bytes code+data, reset at $%04X' % (len(rom), reset))
