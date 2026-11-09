# cases.py : differences par case (plus grand mip) entre atlas Epic, Steam et mod.
import struct, sys, os
ICI = os.path.dirname(os.path.abspath(__file__))
e = open(os.path.join(ICI, 'epic3037.bin'), 'rb').read()
s = open(os.path.join(ICI, 'steam3055.bin'), 'rb').read()
a = open(os.path.join(ICI, 'ap3037.bin'), 'rb').read()
w, h = struct.unpack_from('<II', e, 0); nm = struct.unpack_from('<I', e, 12)[0]
print('atlas %dx%d, %d mips, fourcc %s' % (w, h, nm, e[8:12]))
# le plus grand mip est en dernier
bw, bh = w // 4, h // 4
debut = len(e) - bw * bh * 16
COLS, ROWS = 16, 8    # cases de 256 px si 4096x2048
cw, ch = (w // COLS) // 4, (h // ROWS) // 4
def bloc(d, x, y): k = debut + (y * bw + x) * 16; return d[k:k + 16]
def stats(d1, d2, c, r):
    n = 0
    for y in range(r * ch, (r + 1) * ch):
        for x in range(c * cw, (c + 1) * cw):
            if bloc(d1, x, y) != bloc(d2, x, y): n += 1
    return n
tot = cw * ch
print('cases %dx%d px, %d blocs par case' % (cw * 4, ch * 4, tot))
print('legende : E/S = %% de blocs differents Epic/Steam ; M = case modifiee par le mod')
for r in range(ROWS):
    l = []
    for c in range(COLS):
        es = stats(e, s, c, r) * 100 // tot
        m = 'M' if stats(e, a, c, r) else ' '
        l.append('%3d%s' % (es, m))
    print('ligne %d : %s' % (r, ' '.join(l)))
