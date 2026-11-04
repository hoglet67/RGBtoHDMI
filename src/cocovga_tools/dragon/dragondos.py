"""Minimal DragonDOS VDK reader / writer (40 track single sided, 18 x 256)."""
SPT = 18

def lsn_of(track, sector, sides=1, side=0):
    return (track * sides + side) * SPT + (sector - 1)

class VDK:
    def __init__(self, data):
        self.hlen = data[2] | (data[3] << 8)
        self.hdr = data[:self.hlen]
        self.tracks, self.sides = data[8], data[9]
        self.img = bytearray(data[self.hlen:])
    def sector(self, lsn):
        return bytes(self.img[lsn * 256:(lsn + 1) * 256])
    def entries(self):
        out = []
        for s in range(3, 19):
            b = self.sector(lsn_of(20, s, self.sides))
            for e in range(10):
                ent = b[e * 25:(e + 1) * 25]
                if ent[0] & 0x08: return out
                out.append(ent)
        return out
    def files(self):
        ents = self.entries()
        res = []
        for idx, ent in enumerate(ents):
            if ent[0] & 0xA0: continue          # deleted or continuation
            name = ent[1:9].rstrip(b'\0 ').decode('latin1') + '.' + ent[9:12].rstrip(b'\0 ').decode('latin1')
            data = bytearray()
            e = ent
            while True:
                allocs = [e[12 + 3 * i:15 + 3 * i] for i in range(4 if e[0] & 0x20 else 4)]
                for a in allocs:
                    lsn = (a[0] << 8) | a[1]; n = a[2]
                    for k in range(n): data += self.sector(lsn + k)
                if e[0] & 0x01:                 # continued in another entry
                    e = ents[e[24]]
                    continue
                last = e[24]
                break
            # trim last sector
            if last: data = data[:len(data) - 256 + last]
            res.append((name, bytes(data)))
        return res
