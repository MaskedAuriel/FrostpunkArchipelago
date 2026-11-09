# atlas_steam.py <sortie.gz> : ap_buildings.gz pour Steam (#3055) = atlas Steam
# + blocs DXT que le mod modifie dans l'atlas Epic (#3037 -> ap_buildings.gz).
import sys, struct, zlib, gzip
E = r"C:\Program Files\Epic Games\Frostpunk"
S = r"C:\Program Files (x86)\Steam\steamapps\common\Frostpunk"
def ent(d, i):
    b = open(d + r"\textures-s3.idx", 'rb').read()
    o = 11 + i * 29
    return struct.unpack_from('<I', b, o)[0], struct.unpack_from('<I', b, o + 4)[0], struct.unpack_from('<Q', b, o + 20)[0]
def blob(d, i):
    h, comp, off = ent(d, i)
    with open(d + r"\textures-s3.dat", 'rb') as f:
        f.seek(off); raw = f.read(comp + (1 << 20))
    d = zlib.decompressobj(16 + zlib.MAX_WBITS)
    out = d.decompress(raw)
    print('  entree %d : %d octets lus avant la fin du flux (index : %d), eof=%s' % (i, len(raw) - len(d.unused_data), comp, d.eof))
    return out, comp
epic, _ = blob(E, 3037)
steam, compS = blob(S, 3055)
ap = gzip.decompress(open(E + r"\mods\ap_buildings.gz", 'rb').read())
print('tailles epic %d steam %d ap %d' % (len(epic), len(steam), len(ap)))
assert len(epic) == len(steam) == len(ap) and epic[:144] == steam[:144] == ap[:144]
blocs = [k for k in range(144, len(ap), 16) if ap[k:k + 16] != epic[k:k + 16]]
conflits = [k for k in blocs if steam[k:k + 16] != epic[k:k + 16]]
print('blocs modifies par le mod : %d ; dont deja changes par Steam : %d' % (len(blocs), len(conflits)))
out = bytearray(steam)
for k in blocs:
    out[k:k + 16] = ap[k:k + 16]
z = gzip.compress(bytes(out), compresslevel=9)
print('compresse %d octets, place Steam %d -> %s' % (len(z), compS, 'OK' if len(z) <= compS else 'TROP GROS'))
open(sys.argv[1], 'wb').write(z)

