# extraire.py <dossier du projet> : adresses Epic (RVA) presentes dans le code.
import sys, re, os, glob
racine = sys.argv[1]
TEXT = (0x1000, 0x1000 + 0x1D7A64A)
RDATA = (0x1D7C000, 0x1D7C000 + 0xBAB8AE)
DATA = (0x2928000, 0x2928000 + 0x178BE58)
EXCLUS = {0x1000000, 0x2000000, 0x4000000, 0x40000000}
lit = re.compile(r'\b0x([0-9A-Fa-f]{6,9})\b')
trouve = {}
for f in sorted(glob.glob(os.path.join(racine, '*.cpp')) + glob.glob(os.path.join(racine, '*.h'))):
    nom = os.path.basename(f)
    if nom.startswith('known_'):
        continue
    dans_bloc = False
    for n, ligne in enumerate(open(f, encoding='utf-8', errors='replace'), 1):
        l = ligne
        if dans_bloc:
            if '*/' in l: l = l.split('*/', 1)[1]; dans_bloc = False
            else: continue
        if '/*' in l and '*/' not in l.split('/*', 1)[1]:
            l = l.split('/*', 1)[0]; dans_bloc = True
        l = l.split('//', 1)[0]
        for m in lit.finditer(l):
            v = int(m.group(1), 16)
            if v >= 0x140000000: v -= 0x140000000
            if v in EXCLUS: continue
            sec = 'text' if TEXT[0] <= v < TEXT[1] else 'rdata' if RDATA[0] <= v < RDATA[1] else 'data' if DATA[0] <= v < DATA[1] else None
            if not sec or v < 0xD00000: continue
            trouve.setdefault(v, (sec, set()))[1].add('%s:%d' % (nom, n))
for v in sorted(trouve):
    sec, ou = trouve[v]
    print('%07X\t%s\t%s' % (v, sec, ' '.join(sorted(ou))))
