"""Tiny two-pass 6809 assembler (subset) used by the test ROM builders."""

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

    def bita_i(self, v): self.emit(0x85, v)
    def bitb_i(self, v): self.emit(0xC5, v)
    def stb_xp(self): self.emit(0xE7, 0x80)      # STB ,X+
    def cmpa_i(self, v): self.emit(0x81, v)
    def ldd_x(self, off=0): self.emit(0xEC, 0x88, off & 0xFF)   # LDD n,X
    def tfr(self, pb): self.emit(0x1F, pb)
    def lda_xo(self, off): self.emit(0xA6, 0x88, off & 0xFF)   # LDA n,X
    def bmi(self, l): self._rel8(0x2B, l)

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
