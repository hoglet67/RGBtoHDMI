#!/usr/bin/env python3
"""
mklib.py - W64LIB.BIN: machine code helpers for the CoCoVGA / RGBtoHDMI
64 column test (Dragon 32/64, DragonDOS), called from BASIC with USR.

Loaded at $7800-$7BFF (protect with CLEAR 500,&H77FF): below $7800 so that a
DOS or extension that keeps the top of RAM (e.g. DOSPLUS / DW4 reserve
$7EFF-$7FFF) still leaves it room. Entry points (EXEC; the argument is POKEd
to ARG = $7BFE first):
  $7800 REGSET  page(+128)    send a CoCoVGA register page: the 512 byte page
                              (arg = address / 512) is cleared (not with
                              +128: palette words POKEd before), the 9 bytes
                              at PARAM copied in, and shown for one frame
                              after CoCoVGA's combination lock on $FF22 (as
                              CoCoVGA's SG4PG2); the display is then restored
  $7803 W64ON   page(+128)    display a 2 KB buffer as CG2 (64 columns with
                              CoCoVGA); +128 = colour set 1 (buff / orange)
  $7806 TEXTON  0 / 128       BASIC's text screen ($0400), 128 = orange set
  $7809 PM4ON   page(+128)    display a 6 KB PMODE 4 page
  $780C SCROLL  page          scroll the 64x32 buffer up one line
  $780F STRESS  page          scroll + new line every frame (as fast as the
                              CPU can) until a key or FRAMES frames
  $7812 FULLST  page          rewrite all 2048 characters every update
                              until a key or FRAMES frames
  $7815 FILL64  page          fill the 2 KB buffer with PARAM+9
  $7818 CHARSET page          fill the 2 KB buffer with codes 0-255
  $781B PUTSTR  VARPTR(S$)    copy S$ to the address in DEST, ASCII -> 6847
                              codes (lower case -> $00-$1F); DEST advances
  $781E         RTS (the file's exec address), then "W64L" + version byte
PARAM ($7BF0): 9 register page bytes, +9 fill value, +10/11 FRAMES (0 = no
limit), +12/13 DEST. Interrupts are masked inside REGSET / STRESS / FULLST.
"""
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'xroar'))
from asm6809 import Asm

class A(Asm):
    def op(self, *b): self.emit(*b)
    def ora_i(self, v): self.emit(0x8A, v)
    def anda_i(self, v): self.emit(0x84, v)
    def ora_e(self, a): self.emit(0xBA); self.w(a)
    def bitb_i(self, v): self.emit(0xC5, v)
    def lsra(self): self.emit(0x44)
    def lsla(self): self.emit(0x48)
    def clra(self): self.emit(0x4F)
    def clrb(self): self.emit(0x5F)
    def incb(self): self.emit(0x5C)
    def tfr_ba(self): self.emit(0x1F, 0x98)
    def tfr_ab(self): self.emit(0x1F, 0x89)
    def tfr_dx(self): self.emit(0x1F, 0x01)
    def tfr_xy(self): self.emit(0x1F, 0x12)
    def tfr_yd(self): self.emit(0x1F, 0x20)
    def bcc(self, l): self._rel8(0x24, l)
    def sta_x(self, off=0): self.emit(0xA7, 0x84) if off == 0 else self.emit(0xA7, off & 0x1F)
    def leax(self, off): self.emit(0x30, off & 0x1F) if -16 <= off <= 15 else self.emit(0x30, 0x88, off & 0xFF)
    def leay_m1(self): self.emit(0x31, 0x3F)
    def pshs(self, m): self.emit(0x34, m)
    def puls(self, m): self.emit(0x35, m)
    def ldy_e(self, a): self.emit(0x10, 0xBE); self.w(a)
    def cmpy_e(self, a): self.emit(0x10, 0xBC); self.w(a)
    def cmpx_e(self, a): self.emit(0xBC); self.w(a)
    def addd_i(self, v): self.emit(0xC3); self.w(v)
    def ldb_ya(self): self.emit(0xE6, 0xA0)     # LDB ,Y+
    def lda_ya(self): self.emit(0xA6, 0xA0)     # LDA ,Y+
    def stb_xa(self): self.emit(0xE7, 0x80)     # STB ,X+
    def inc_x(self): self.emit(0x6C, 0x84)      # INC ,X
    def lda_xi(self): self.emit(0xA6, 0x84)     # LDA ,X
    def ldd_e(self, a): self.emit(0xFC); self.w(a)
    def std_e(self, a): self.emit(0xFD); self.w(a)
    def ldx_e(self, a): self.emit(0xBE); self.w(a)
    def stx_e(self, a): self.emit(0xBF); self.w(a)
    def sty_e(self, a): self.emit(0x10, 0xBF); self.w(a)
    def subd_i(self, v): self.emit(0x83); self.w(v)
    def ldx_lbl(self, l): self.ldx_l(l)
    def ldy_lbl(self, l):
        self.emit(0x10, 0x8E, 0, 0)
        self.fix.append((len(self.out) - 2, 'abs16', l, self.pc))

def covdg(ch):
    a = ord(ch)
    if a >= 0x60: return a - 0x60
    if a >= 0x40: return a
    return a ^ 0x40

ORG = 0x7800
TOP = 0x7C00             # first byte after the library
PARAM = TOP - 0x10
FILLV = PARAM + 9
FRAMES = PARAM + 10
ARG = PARAM + 14         # call argument (byte, or 16 bit for PUTSTR)
EXEC = ORG + 0x1E
LIBVER = 5               # 4: moved from $7C00 to $7800; 5: REGSET +128 keeps the page
DEST = PARAM + 12
V = TOP - 0x20           # variables
CURPAGE, CURV, CURVDG, TMPPAGE = V, V + 1, V + 2, V + 3
TMPEND, TMPEND2, COUNT, FCNT = V + 4, V + 6, V + 8, V + 10
FSTIMEOUT = V + 12      # times field sync did not come (PIA0 CB1 flag)
FF00, FF02, FF03, FF22 = 0xFF00, 0xFF02, 0xFF03, 0xFF22
INTCNV = 0x8B2D         # Dragon BASIC: USR argument -> D

a = A(ORG)
for name in ('regset', 'w64on', 'texton', 'pm4on', 'scroll', 'stress', 'fullst', 'fill64', 'charset', 'putstr'):
    a.jmp(name)
# ORG+$1E: the file's exec address (a DOS that runs a binary after LOAD just
# returns), then "W64L" + version at ORG+$1F for the BASIC program to check
assert a.pc == EXEC, hex(a.pc)
a.rts()
a.data(list(b'W64L') + [LIBVER])

# --- helpers -----------------------------------------------------------
a.label('getarg')               # A = argument byte (POKEd to ARG; no ROM call,
a.lda(ARG)                      # so it works with every Dragon BASIC version)
a.rts()

a.label('pageaddr')             # A = page -> X = page * 512 (D clobbered)
a.anda_i(0x7F); a.lsla(); a.clrb(); a.tfr_dx(); a.rts()

a.label('vsync')                # wait for the start of field sync (IRQs masked)
a.pshs(0x10)                    # X
a.lda(FF02)
a.ldx_i(0x4000)                 # give up after about 0.3 s (never hang)
a.label('vs1'); a.lda(FF03); a.bmi('vs_ok')
a.emit(0x30, 0x1F); a.bne('vs1')    # LEAX -1,X
a.inc(FSTIMEOUT)
a.label('vs_ok'); a.lda(FF02)
a.puls(0x10); a.rts()

def samloop(label, base, n):
    a.label(label)              # A = value
    a.pshs(0x14)                # B, X
    a.ldx_i(base); a.ldb_i(n)
    a.label(label + '_l')
    a.lsra(); a.bcc(label + '_z')
    a.sta_x(1); a.bra(label + '_n')
    a.label(label + '_z'); a.sta_x(0)
    a.label(label + '_n'); a.leax(2); a.decb(); a.bne(label + '_l')
    a.puls(0x14); a.rts()
samloop('samoff', 0xFFC6, 7)
samloop('samv', 0xFFC0, 3)

a.label('apply')                # show CURPAGE / CURV / CURVDG from the next frame
a.pshs(0x01); a.orcc(0x50)
a.jsr('vsync')
a.lda(CURV); a.jsr('samv')
a.lda(CURPAGE); a.jsr('samoff')
a.lda(FF22); a.anda_i(0x07); a.ora_e(CURVDG); a.sta(FF22)
a.puls(0x01); a.rts()

a.label('anykey')               # Z = 1 when no key is pressed
a.clr(FF02); a.lda(FF00); a.ldb_i(0xFF); a.stb(FF02)
a.ora_i(0x80); a.emit(0x81, 0xFF)   # CMPA #$FF
a.rts()
a.label('waitrel')
a.jsr('anykey'); a.bne('waitrel'); a.rts()

a.label('frames_done')          # Z = 0 when FRAMES (non zero) frames have passed
a.ldd_e(FRAMES); a.beq('fd_no')
a.ldd_e(FCNT); a.emit(0xC3); a.w(1); a.std_e(FCNT)   # ADDD #1
a.emit(0x10, 0xB3); a.w(FRAMES)                       # CMPD FRAMES
a.bne('fd_no')
a.lda_i(1); a.rts()             # Z = 0
a.label('fd_no'); a.clra(); a.rts()   # Z = 1

# --- display modes ------------------------------------------------------
a.label('w64on')
a.jsr('getarg'); a.tfr_ab(); a.anda_i(0x7F); a.sta(CURPAGE)
a.lda_i(2); a.sta(CURV)
a.lda_i(0xA0); a.bitb_i(0x80); a.beq('w64_c0'); a.ora_i(0x08)
a.label('w64_c0'); a.sta(CURVDG); a.jmp('apply')

a.label('texton')
a.jsr('getarg'); a.tfr_ab()
a.lda_i(2); a.sta(CURPAGE); a.clr(CURV)
a.clra(); a.bitb_i(0x80); a.beq('tx_c0'); a.lda_i(0x08)
a.label('tx_c0'); a.sta(CURVDG); a.jmp('apply')

a.label('pm4on')
a.jsr('getarg'); a.tfr_ab(); a.anda_i(0x7F); a.sta(CURPAGE)
a.lda_i(6); a.sta(CURV)
a.lda_i(0xF0); a.bitb_i(0x80); a.beq('pm_c0'); a.ora_i(0x08)
a.label('pm_c0'); a.sta(CURVDG); a.jmp('apply')

# --- CoCoVGA register page ---------------------------------------------
def mark(ch):                   # progress marker for a hang report
    a.pshs(0x12)                # A, X
    a.lda_i(covdg(ch)); a.sta(0x05FF)
    a.ldx_e(TMPEND); a.emit(0xA7, 0x89); a.w(0x1FF)   # STA $1FF,X (page)
    a.puls(0x12)
a.label('regset')
a.jsr('getarg'); a.pshs(0x02); a.anda_i(0x7F); a.sta(TMPPAGE)
a.jsr('pageaddr'); a.stx_e(TMPEND)
mark('1')
a.puls(0x02); a.emit(0x4D)      # TSTA: +128 = keep the page (palette words POKEd)
a.bmi('rs_keep')
a.ldx_e(TMPEND)
a.ldd_i(0); a.ldy_i(256)
a.label('rs_clr')
a.emit(0xED, 0x81)              # STD ,X++
a.leay_m1(); a.bne('rs_clr')
a.label('rs_keep')
a.lda(TMPPAGE); a.jsr('pageaddr')
a.ldy_i(PARAM); a.ldb_i(9)
a.label('rs_cp'); a.lda_ya(); a.sta_xp(); a.decb(); a.bne('rs_cp')
mark('2')
a.pshs(0x01); a.orcc(0x50)
a.jsr('vsync')
mark('3')
a.clra(); a.jsr('samv')         # text mode: the page is 512 bytes
a.lda(TMPPAGE); a.jsr('samoff')
# CoCoVGA combination lock (CoCoVGA's SG4PG2), then register bank 0
a.lda(FF22)
for v in (0x90, 0x48, 0xA0, 0xF8, 0x00):
    a.anda_i(0x07); a.ora_i(v); a.sta(FF22)
mark('4')
a.jsr('vsync')                  # the page was on screen for one frame
a.lda(CURV); a.jsr('samv')
a.lda(CURPAGE); a.jsr('samoff')
a.lda(FF22); a.anda_i(0x07); a.ora_e(CURVDG); a.sta(FF22)
mark('5')
a.puls(0x01); a.rts()

# --- 64x32 buffer helpers ----------------------------------------------
a.label('scrcore')              # A = page: scroll up one line, clear last
a.jsr('pageaddr')
a.tfr_xy()
a.tfr_yd(); a.addd_i(1984); a.std_e(TMPEND); a.addd_i(64); a.std_e(TMPEND2)
a.leax(64)
a.label('sc_l'); a.ldd_xpp(); a.std_ypp(); a.cmpy_e(TMPEND); a.bne('sc_l')
a.ldd_i(0x6060)
a.label('sc_c'); a.std_ypp(); a.cmpy_e(TMPEND2); a.bne('sc_c')
a.rts()

a.label('scroll')
a.jsr('getarg'); a.jmp('scrcore')

TEMPLATE_TEXT = ("STRESS LINE 00000  The quick brown fox jumps over the lazy dog!" + " " * 64)[:64]
assert len(TEMPLATE_TEXT) == 64
DIG = TEMPLATE_TEXT.index('00000')

a.label('putline')              # TMPPAGE: copy the template to the last line
a.lda(TMPPAGE); a.jsr('pageaddr'); a.leax(127); a.leax(127); a.leax(127)
a.emit(0x30, 0x89); a.w(1984 - 381)   # LEAX 1984-381,X (16 bit offset)
a.ldy_lbl('template'); a.ldb_i(64)
a.label('pl_l'); a.lda_ya(); a.sta_xp(); a.decb(); a.bne('pl_l')
# count up the 5 digits
a.ldx_lbl('digits_end'); a.ldb_i(5)
a.label('pl_d'); a.inc_x(); a.lda_xi(); a.emit(0x81, 0x7A); a.bne('pl_ok')   # CMPA #'9'+1
a.lda_i(0x70); a.sta_x(0); a.leax(-1); a.decb(); a.bne('pl_d')
a.label('pl_ok'); a.rts()

a.label('stress')
a.jsr('getarg'); a.anda_i(0x7F); a.sta(TMPPAGE)
a.ldd_i(0); a.std_e(FCNT)
a.pshs(0x01); a.orcc(0x50)
a.jsr('waitrel')
a.label('st_l')
a.jsr('vsync')
a.lda(TMPPAGE); a.jsr('scrcore')
a.jsr('putline')
a.jsr('frames_done'); a.bne('st_x')
a.jsr('anykey'); a.beq('st_l')
a.jsr('waitrel')
a.label('st_x')
a.puls(0x01); a.rts()

a.label('fullst')
a.jsr('getarg'); a.anda_i(0x7F); a.sta(TMPPAGE)
a.ldd_i(0); a.std_e(FCNT); a.clr(COUNT)
a.pshs(0x01); a.orcc(0x50)
a.jsr('waitrel')
a.label('fs_l')
a.jsr('vsync')
a.lda(TMPPAGE); a.jsr('pageaddr')
a.tfr_xy(); a.tfr_yd(); a.addd_i(2048); a.std_e(TMPEND)
a.ldb(COUNT)
a.label('fs_w'); a.tfr_ba(); a.anda_i(0x3F); a.ora_i(0x40); a.sta_xp(); a.incb()
a.cmpx_e(TMPEND); a.bne('fs_w')
a.inc(COUNT)
a.jsr('frames_done'); a.bne('fs_x')
a.jsr('anykey'); a.beq('fs_l')
a.jsr('waitrel')
a.label('fs_x')
a.puls(0x01); a.rts()

a.label('fill64')
a.jsr('getarg'); a.jsr('pageaddr')
a.lda(FILLV); a.tfr_ab(); a.ldy_i(1024)
a.label('f6_l'); a.emit(0xED, 0x81); a.leay_m1(); a.bne('f6_l')
a.rts()

a.label('charset')
a.jsr('getarg'); a.jsr('pageaddr')
a.clrb(); a.ldy_i(2048)
a.label('cs_l'); a.stb_xa(); a.incb(); a.leay_m1(); a.bne('cs_l')
a.rts()

a.label('putstr')
a.ldx_e(ARG)                    # ARG/ARG+1 = VARPTR(S$): the string descriptor
a.emit(0xE6, 0x84)              # LDB ,X   length
a.beq('ps_x')
a.emit(0x10, 0xAE, 0x02)        # LDY 2,X  string address
a.ldx_e(DEST)
a.label('ps_l'); a.lda_ya()
a.emit(0x81, 0x40); a.bcc('ps_hi')          # CMPA #$40 / BHS
a.emit(0x8B, 0x40); a.bra('ps_st')          # ADDA #$40: $20-$3F -> $60-$7F
a.label('ps_hi'); a.emit(0x81, 0x60); a._rel8(0x25, 'ps_st')   # CMPA #$60 / BLO
a.emit(0x80, 0x60)                          # SUBA #$60: lower case -> $00-$1F
a.label('ps_st'); a.sta_xp(); a.decb(); a.bne('ps_l')
a.stx_e(DEST)
a.label('ps_x'); a.rts()

a.label('template'); a.data([covdg(c) for c in TEMPLATE_TEXT])
a.labels['digits_end'] = a.labels['template'] + DIG + 4

code = a.link()
end = ORG + len(code)
assert end <= V, hex(end)
# variables: CURPAGE=2 (text), CURV=0, CURVDG=0, rest 0; PARAM: 9 zero bytes,
# fill $60, FRAMES 0
img = bytearray(code) + bytearray(V - end)
img += bytes([2, 0, 0, 0]) + bytes(12)                  # V .. V+15
img += bytes(9) + bytes([0x60]) + bytes([0, 0]) + bytes(4)   # PARAM .. PARAM+15
assert ORG + len(img) == TOP
open(sys.argv[1] if len(sys.argv) > 1 else 'w64lib.raw', 'wb').write(img)
print('W64LIB: %d bytes of code, $%04X-$%04X' % (len(code), ORG, TOP - 1))
