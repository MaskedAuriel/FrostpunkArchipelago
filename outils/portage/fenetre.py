# fenetre.py <lo> <hi> : cibles rip-relatives Epic dans [lo, hi) vues depuis des
# fonctions IDENTIQUES, avec la cible Steam alignee. Une ligne par cible distincte.
import sys, os, collections
args = sys.argv[1:]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'base_apparier.py'), encoding='utf-8-sig').read())
lo, hi = int(args[0], 16), int(args[1], 16)
res = collections.defaultdict(collections.Counter)
vus = set()
for (p, _) in refs_vers(lo, hi):
    be = contenant(E, debE, p)
    if be is None or be not in mapE or be in vus: continue
    vus.add(be)
    bs_ = mapE[be]
    iE = insns(imgE, be, E[be][0]); iS = insns(imgS, bs_, S[bs_][0])
    if len(iE) != len(iS): continue
    for a, b in zip(iE, iS):
        ce = cible_rip(a)
        if ce is not None and lo <= ce < hi:
            cs = cible_rip(b)
            if cs is not None: res[ce][cs] += 1
for ce in sorted(res):
    print('%07X -> %s' % (ce, '  '.join('%07X x%d (d=-0x%X)' % (v, n, ce - v) for v, n in res[ce].most_common(3))))
