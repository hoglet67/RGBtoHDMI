#!/usr/bin/env python3
"""
cpucheck.py - run covga_test.rom on an independent 6809 core (PyPI MC6809)
with a minimal Dragon model (RAM, ROM, PIA0 field sync, PIA1 VDG mode
register, SAM mode/offset) and check the frame-by-frame display state and
screen memory against phases.json.
"""
import json, sys, logging
logging.disable(logging.CRITICAL)
from MC6809.components.cpu6809 import CPU
from MC6809.components.memory import Memory
from MC6809.core.configs import BaseConfig

class Cfg(BaseConfig):
    RAM_START, RAM_END, ROM_START, ROM_END = 0x0000, 0x7FFF, 0x8000, 0xFFFF

d = sys.argv[1] if len(sys.argv) > 1 else '.'
rom = open(d + '/covga_test.rom', 'rb').read()
ph = json.load(open(d + '/phases.json'))

cfg = Cfg({'verbosity': None, 'trace': None})
mem = Memory(cfg)
cpu = CPU(mem, cfg)
mem.load(0x8000, rom)
mem.load(0xFFF0, rom[0x3FF0:])

st = {'fs': 0, 'ff22': 0, 'ff23': 0, 'sam': 0}
def r_io(cycles, op, addr):
    if addr == 0xFF03: return 0x34 | (0x80 if st['fs'] else 0)
    if addr == 0xFF02: st['fs'] = 0; return 0
    return 0
def w_io(cycles, op, addr, value):
    if addr == 0xFF22 and (st['ff23'] & 4): st['ff22'] = value
    if addr == 0xFF23: st['ff23'] = value
def r_sam(cycles, op, addr): return 0
def w_sam(cycles, op, addr, value):
    n = addr - 0xFFC0
    bit = n >> 1
    if n & 1: st['sam'] |= 1 << bit
    else: st['sam'] &= ~(1 << bit)
mem.add_read_byte_callback(r_io, 0xFF00, 0xFF3F)
mem.add_write_byte_callback(w_io, 0xFF00, 0xFF3F)
mem.add_read_byte_callback(r_sam, 0xFFC0, 0xFFDF)
mem.add_write_byte_callback(w_sam, 0xFFC0, 0xFFDF)

cpu.reset()
FRAME = 30000                # bus cycles per frame (long enough for a scroll)
frames = []
next_fs = FRAME
total = sum(p['frames'] for p in ph['phases']) + 40
while len(frames) < total:
    cpu.get_and_call_next_op()
    if cpu.cycles >= next_fs:
        next_fs += FRAME
        # what the VDG shows in the frame starting now (state set during the
        # previous field sync is in effect)
        v, s = st['ff22'], st['sam']
        off = ((s >> 3) & 0x7F) * 512
        frames.append((v & 0xF8, s & 7, off,
                       bytes(mem._mem[a] for a in range(0x1000, 0x1800)),
                       bytes(mem._mem.get(a, 0) if hasattr(mem._mem, 'get') else mem._mem[a] for a in range(off, off + 6144))))
        st['fs'] = 1

# runs of identical display state
runs = []
for f in frames[1:]:
    key = f[:3]
    if runs and runs[-1][0] == key: runs[-1][1] += 1
    else: runs.append([key, 1])
names = {(0x00, 0, 0x0400): 'TEXT $0400', (0x00, 0, 0x0600): 'TEXT $0600 (regs NitrOS-9)',
         (0xA0, 2, 0x1000): 'CG2 $1000', (0x00, 0, 0x0A00): 'TEXT $0A00 (regs lower on)',
         (0x00, 0, 0x0800): 'TEXT $0800 (inverse page)', (0x00, 0, 0x0C00): 'TEXT $0C00 (regs lower off)',
         (0xF0, 6, 0x2000): 'RG6 css0 $2000', (0xF8, 6, 0x2000): 'RG6 css1 $2000'}
for key, n in runs:
    print('%4d frames  %s' % (n, names.get(key, 'UNEXPECTED %r' % (key,))))

ok = True
expect = [('TEXT $0400', 60), ('TEXT $0600 (regs NitrOS-9)', 1), ('CG2 $1000', None),
          ('TEXT $0400', 60), ('TEXT $0A00 (regs lower on)', 1), ('TEXT $0800 (inverse page)', 60),
          ('TEXT $0C00 (regs lower off)', 1), ('TEXT $0800 (inverse page)', 60),
          ('RG6 css0 $2000', 60), ('RG6 css1 $2000', 60)]
got = [(names.get(k, '?'), n) for k, n in runs]
if got and got[0][0] == '?': got = got[1:]          # start-up (copying screens)
for i, (name, n) in enumerate(expect):
    if i >= len(got) or got[i][0] != name or (n is not None and got[i][1] != n and i < len(expect) - 1):
        print('MISMATCH at run %d: expected %s x%s, got %s' % (i, name, n, got[i] if i < len(got) else None))
        ok = False
# register pages are on screen for exactly one frame
# CG2 screen memory goes through w64_0 -> w64_1 -> w64_2 in that order
cg2 = [f[3] for f in frames if f[:3] == (0xA0, 2, 0x1000)]
order = []
for name in ('w64_0', 'w64_1', 'w64_2'):
    want = bytes(ph['vram'][name])
    idx = [i for i, m in enumerate(cg2) if m == want]
    print('%s: first shown at CG2 frame %s, %d frames' % (name, idx[0] if idx else None, len(idx)))
    if not idx: ok = False
    order.append(idx[0] if idx else -1)
if order != sorted(order): ok = False; print('w64 states out of order')
# RAM pages copied correctly
for page, want in ph['pages'].items():
    a = int(page, 16)
    if bytes(mem._mem[a + i] for i in range(512)) != bytes(want):
        ok = False; print('page %s wrong' % page)
if bytes(mem._mem[0x2000 + i] for i in range(6144)) != bytes(ph['rg6']):
    ok = False; print('RG6 page wrong')
# per-frame display state for the frame renderers: vdg, sam, offset hi/lo, 6144 bytes
with open(d + '/frames_state.bin', 'wb') as f:
    for fr in frames[1:]:
        f.write(bytes([fr[0], fr[1], fr[2] >> 8, fr[2] & 0xFF]) + fr[4])
print('wrote %d frame states' % (len(frames) - 1))
print('CPU CHECK', 'PASS' if ok else 'FAIL')
sys.exit(0 if ok else 1)
