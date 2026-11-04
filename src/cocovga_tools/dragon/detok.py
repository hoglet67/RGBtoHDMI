import json, os, sys
T = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tokens.json')))
cmds, funcs, dcmds = T['cmds'], T['funcs'], T['dcmds']
DOSF = ['LOF', 'FREE', 'ERL', 'ERR', 'HIMEM', 'FRE$']   # guess, printed with ? if used
def detok(data, base=None):
    d = data[9:] if data[0] == 0x55 else data
    i = 0; out = []
    while i + 4 <= len(d):
        nxt = (d[i] << 8) | d[i + 1]
        if nxt == 0: break
        ln = (d[i + 2] << 8) | d[i + 3]
        i += 4; s = ''; q = False
        while i < len(d) and d[i] != 0:
            c = d[i]
            if c == 0x22: q = not q
            if q or c < 0x80: s += chr(c) if c >= 0x20 else '\\x%02x' % c
            elif c == 0xFF:
                i += 1; k = d[i] - 0x80
                s += funcs[k] if k < len(funcs) else (DOSF[k - len(funcs)] + '?' if k - len(funcs) < len(DOSF) else '<FF%02X>' % d[i])
            else:
                k = c - 0x80
                s += cmds[k] if k < len(cmds) else (dcmds[k - len(cmds)] if k - len(cmds) < len(dcmds) else '<%02X>' % c)
            i += 1
        i += 1
        out.append('%d %s' % (ln, s))
    return '\n'.join(out)
if __name__ == '__main__':
    print(detok(open(sys.argv[1], 'rb').read()))
