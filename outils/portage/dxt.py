# dxt.py <entree.bin> <sortie.png> [reduction] : plus grand mip DXT5 d'un blob
# de textures-s3 (en-tete 144 octets, mips du plus petit au plus grand) -> PNG.
import sys, struct
import numpy as np
from PIL import Image
d = open(sys.argv[1], 'rb').read()
w, h = struct.unpack_from('<II', d, 0)
bw, bh = w // 4, h // 4
blocs = np.frombuffer(d[len(d) - bw * bh * 16:], dtype=np.uint8).reshape(bh, bw, 16)
# couleur (octets 8..15) : c0, c1 RGB565 + 32 bits d'indices
c0 = blocs[..., 8].astype(np.uint32) | (blocs[..., 9].astype(np.uint32) << 8)
c1 = blocs[..., 10].astype(np.uint32) | (blocs[..., 11].astype(np.uint32) << 8)
def rgb(c):
    return np.stack([((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31], -1)
p0, p1 = rgb(c0), rgb(c1)
pal = np.stack([p0, p1, (2 * p0 + p1) // 3, (p0 + 2 * p1) // 3], 2)   # bh, bw, 4, 3
idx = blocs[..., 12].astype(np.uint32) | (blocs[..., 13].astype(np.uint32) << 8) | (blocs[..., 14].astype(np.uint32) << 16) | (blocs[..., 15].astype(np.uint32) << 24)
# alpha (octets 0..7)
a0 = blocs[..., 0].astype(np.int32); a1 = blocs[..., 1].astype(np.int32)
abits = np.zeros(blocs.shape[:2], dtype=np.uint64)
for k in range(6): abits |= blocs[..., 2 + k].astype(np.uint64) << np.uint64(8 * k)
img = np.zeros((h, w, 4), dtype=np.uint8)
for py in range(4):
    for px in range(4):
        i = py * 4 + px
        ci = (idx >> (2 * i)) & 3
        col = np.take_along_axis(pal, ci[..., None, None].astype(np.int64).repeat(3, -1), 2)[:, :, 0, :]
        ai = ((abits >> np.uint64(3 * i)) & np.uint64(7)).astype(np.int32)
        alt8 = np.where(ai == 0, a0, np.where(ai == 1, a1, ((8 - ai) * a0 + (ai - 1) * a1) // 7))
        alt6 = np.where(ai == 0, a0, np.where(ai == 1, a1, np.where(ai == 6, 0, np.where(ai == 7, 255, ((6 - ai) * a0 + (ai - 1) * a1) // 5))))
        al = np.where(a0 > a1, alt8, alt6)
        img[py::4, px::4, :3] = col
        img[py::4, px::4, 3] = al
im = Image.fromarray(img, 'RGBA')
fond = Image.new('RGBA', im.size, (60, 60, 60, 255)); fond.alpha_composite(im)
r = int(sys.argv[3]) if len(sys.argv) > 3 else 4
fond.convert('RGB').resize((w // r, h // r)).save(sys.argv[2])
print('ok', w, h)
