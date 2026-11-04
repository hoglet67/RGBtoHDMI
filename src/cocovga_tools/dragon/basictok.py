"""Dragon BASIC (+ DragonDOS) tokenizer, keyword tables from the ROMs."""
import json, os
T = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tokens.json')))
CMDS = [(k, 0x80 + i) for i, k in enumerate(T['cmds'])] + [(k, 0x80 + len(T['cmds']) + i) for i, k in enumerate(T['dcmds'])]
FUNCS = [(k, 0x80 + i) for i, k in enumerate(T['funcs'])]
REM_TOKS = (dict(CMDS)['REM'], dict(CMDS)["'"])
DATA_TOK = dict(CMDS)['DATA']

def tokenize_line(text):
    out = bytearray(); i = 0; quote = False; data = False
    while i < len(text):
        c = text[i]
        if quote:
            out.append(ord(c)); i += 1
            if c == '"': quote = False
            continue
        if c == '"':
            quote = True; out.append(ord(c)); i += 1; continue
        if data:
            if c == ':': data = False
            out.append(ord(c)); i += 1; continue
        hit = None
        for kw, tok in CMDS:
            if text.startswith(kw, i): hit = (kw, bytes([tok])); break
        if not hit:
            for kw, tok in FUNCS:
                if text.startswith(kw, i): hit = (kw, bytes([0xFF, tok])); break
        if hit:
            out += hit[1]; i += len(hit[0])
            if hit[1][0] in REM_TOKS:
                out += text[i:].encode('latin1'); break
            if hit[1][0] == DATA_TOK: data = True
            continue
        out.append(ord(c)); i += 1
    return bytes(out)

def tokenize(lines, base=0x2401):
    """lines: list of (number, text) -> program bytes (with links), as saved"""
    body = bytearray(); addr = base
    for num, text in lines:
        tl = tokenize_line(text)
        nxt = addr + 4 + len(tl) + 1
        body += bytes([nxt >> 8, nxt & 0xFF, num >> 8, num & 0xFF]) + tl + b'\0'
        addr = nxt
    body += b'\0\0'
    return bytes(body)
