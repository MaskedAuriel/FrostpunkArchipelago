# globaux.py <rva epic> ... : references EXACTES aux globaux, via fonctions
# identiques ou alignees (voisinage, alignement des mnemoniques).
import sys, os, difflib, collections
sys.argv_sauve = sys.argv[1:]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'base_apparier.py'), encoding='utf-8-sig').read())

def alignement(be, bs_):
    iE = insns(imgE, be, E[be][0]); iS = insns(imgS, bs_, S[bs_][0])
    if [x.mnemonic for x in iE] == [x.mnemonic for x in iS]:
        return list(zip(iE, iS))
    sm = difflib.SequenceMatcher(None, [x.mnemonic for x in iE], [x.mnemonic for x in iS], autojunk=False)
    out = []
    for a, b, n in sm.get_matching_blocks():
        out += [(iE[a + k], iS[b + k]) for k in range(n)]
    return out

cache = {}
for arg in sys.argv_sauve:
    g = int(arg, 16)
    votes = collections.Counter(); fonctions = collections.Counter()
    refs = refs_vers(g, g + 1)
    for (p, _) in refs[:300]:
        be = contenant(E, debE, p)
        if be is None: continue
        if be not in cache:
            bs_, meth, sc = apparier_fonction(be)
            cache[be] = (bs_, sc)
        bs_, sc = cache[be]
        if bs_ is None or sc < 0.85: continue
        for a, b in alignement(be, bs_):
            if cible_rip(a) == g:
                cs = cible_rip(b)
                if cs is not None:
                    votes[cs] += 1; fonctions[(be, bs_)] += 1
    if votes:
        tot = sum(votes.values())
        print('%07X : %s   (%d refs epic)' % (g, '  '.join('%07X x%d' % (v, n) for v, n in votes.most_common(4)), len(refs)))
    else:
        print('%07X : AUCUNE ref exacte exploitable (%d refs epic)' % (g, len(refs)))
