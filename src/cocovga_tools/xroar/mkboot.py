#!/usr/bin/env python3
"""
mkboot.py - 8 KB boot ROM for XRoar's CoCo ("-bas") that does what Disk
BASIC's DOS command does, without any Microsoft code: initialise SAM / PIAs
like Color BASIC, read track 34 (18 sectors, side 0) of drive 0 through the
RS-DOS controller's WD1793 into $2600, check for "OS" and jump to $2602.
"""
import sys
from asm6809 import Asm

a = Asm(0xA000)
DSKREG, FDC_CMD, FDC_TRK, FDC_SEC, FDC_DAT = 0xFF40, 0xFF48, 0xFF49, 0xFF4A, 0xFF4B
a.label('reset')
a.orcc(0x50)
a.lds_i(0x0300)
a.sta(0xFFDA); a.sta(0xFFDD)                     # SAM: 64K dynamic RAM
# RAM interrupt vector table, as Color BASIC sets it up (NitrOS-9 patches it)
a.ldx_i(0x0100); a.lda_i(0x3B)                   # RTI in every slot
a.label('vec'); a.sta_xp(); a.cmpx_i(0x0112); a.bne('vec')
# PIA0: port A inputs (keyboard rows), port B outputs (columns), CB1 = FS
a.clr(0xFF01); a.clr(0xFF00); a.lda_i(0x34); a.sta(0xFF01)
a.clr(0xFF03); a.lda_i(0xFF); a.sta(0xFF02); a.lda_i(0x34); a.sta(0xFF03)
# PIA1: port A DAC outputs, port B VDG mode outputs
a.clr(0xFF21); a.lda_i(0xFE); a.sta(0xFF20); a.lda_i(0x34); a.sta(0xFF21)
a.clr(0xFF23); a.lda_i(0xF8); a.sta(0xFF22); a.lda_i(0x34); a.sta(0xFF23)
a.clr(0xFF22)                                    # VDG: text
for i in range(3): a.sta(0xFFC0 + 2 * i)         # SAM V = 0
for i in range(7): a.sta(0xFFC6 + 2 * i + ((2 >> i) & 1))   # SAM F = 2 ($0400)
# clear the text screen
a.ldx_i(0x0400); a.lda_i(0x60)
a.label('cls'); a.sta_xp(); a.cmpx_i(0x0600); a.bne('cls')
# drive 0, motor on, double density
a.lda_i(0x29); a.sta(DSKREG)
a.ldx_i(0); a.label('spin'); a.ldd_xpp(); a.cmpx_i(0x4000); a.bne('spin')   # motor spin-up delay
a.lda_i(0x00); a.sta(FDC_CMD); a.jsr('wait')      # RESTORE
a.lda_i(0x52); a.sta(0x0400)   # R
a.lda_i(34); a.sta(FDC_DAT)
a.lda_i(0x10); a.sta(FDC_CMD); a.jsr('wait')      # SEEK track 34
a.lda_i(0x53); a.sta(0x0401)   # S
a.lda(FDC_TRK); a.sta(0x0402)  # track register as a character
a.ldx_i(0x2600)
a.lda_i(1); a.sta(0x0200)                         # sector number
a.label('sector')
a.lda(0x0200); a.sta(FDC_SEC)
a.lda_i(0x80); a.sta(FDC_CMD)                     # READ SECTOR
a.nop(); a.nop(); a.nop(); a.nop()
a.label('rd')
a.lda(FDC_CMD)
a.bita_i(0x02); a.bne('byte')
a.bita_i(0x01); a.bne('rd')
a.lda(0x0200); a.sta(0x0410); a.ldb(FDC_CMD); a.stb(0x0411)   # last sector, status
a.lda(0x0200); a.inca(); a.sta(0x0200)
a.cmpa_i(19); a.bne('sector')
a.ldd(0x2600); a.emit(0x10, 0x83); a.w(0x4F53)   # CMPD #"OS"
a.bne('fail')
a.lda_i(0x4A); a.sta(0x0403)   # J
a.jmp('boot')
a.label('byte')
a.ldb(FDC_DAT); a.stb_xp(); a.bra('rd')
a.label('fail')
a.lda_i(0x46); a.sta(0x0400)                     # "F" top left: boot failed
a.label('hang'); a.bra('hang')
a.label('wait')
a.nop(); a.nop(); a.nop(); a.nop()
a.label('w1'); a.lda(FDC_CMD); a.bita_i(0x01); a.bne('w1'); a.rts()
a.label('boot'); a.emit(0x7E); a.w(0x2602)       # JMP $2602
a.label('nmi'); a.emit(0x3B)                    # RTI: the controller's INTRQ drives NMI
rom = a.link()
img = bytearray([0xFF] * 0x2000)
img[:len(rom)] = rom
r = a.labels['reset']
for v in range(0x1FF0, 0x2000, 2): img[v] = r >> 8; img[v + 1] = r & 0xFF
# CPU vectors through the RAM table, as on a real CoCo
for off, ram in ((0x1FF2, 0x0100), (0x1FF4, 0x0103), (0x1FF6, 0x010F), (0x1FF8, 0x010C), (0x1FFA, 0x0106), (0x1FFC, 0x0109)):
    img[off] = ram >> 8; img[off + 1] = ram & 0xFF
open(sys.argv[1] if len(sys.argv) > 1 else 'boot.rom', 'wb').write(img)
open('dummy_cart.rom', 'wb').write(bytes([0xFF] * 0x2000))
print('boot ROM %d bytes' % len(rom))
