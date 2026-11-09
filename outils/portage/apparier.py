# apparier.py : adresses Epic -> Steam.
# Entrees : epic.pkl, steam.pkl (empreintes.py), adresses_epic.tsv (extraire.py).
# Sortie : correspondances.tsv  (epic  steam  methode  score  detail)
import pickle, sys, os, difflib, bisect, collections
import numpy as np
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_REG_RIP

ICI = os.path.dirname(os.path.abspath(__file__))
EXE_E = r"C:\Program Files\Epic Games\Frostpunk\Frostpunk.exe"
EXE_S = r"C:\Program Files (x86)\Steam\steamapps\common\Frostpunk\Frostpunk.exe"
IB = 0x140000000

E = pickle.load(open(os.path.join(ICI, 'epic.pkl'), 'rb'))['fonctions']
S = pickle.load(open(os.path.join(ICI, 'steam.pkl'), 'rb'))['fonctions']
imgE = pefile.PE(EXE_E, fast_load=True).get_memory_mapped_image()
imgS = pefile.PE(EXE_S, fast_load=True).get_memory_mapped_image()

# --- 1. correspondances exactes uniques -------------------------------------
def cles(F):
    d = collections.defaultdict(list)
    for b, (f, seq) in F.items():
        if len(seq) >= 6:
            d[hash(seq)].append(b)
    return d
kE, kS = cles(E), cles(S)
paires = []
for h, lb in kE.items():
    if len(lb) == 1 and len(kS.get(h, ())) == 1:
        paires.append((lb[0], kS[h][0]))
paires.sort()
# plus longue sous-suite croissante (ordre des fonctions conserve par le lieur)
def lis(p):
    import bisect as bs
    queues, idx, prec = [], [], [-1] * len(p)
    for i, (_, s) in enumerate(p):
        j = bs.bisect_left(queues, s)
        if j == len(queues): queues.append(s); idx.append(i)
        else: queues[j] = s; idx[j] = i
        prec[i] = idx[j - 1] if j > 0 else -1
    res, k = [], idx[-1] if idx else -1
    while k >= 0: res.append(p[k]); k = prec[k]
    return res[::-1]
ancres = lis(paires)
mapE = dict(ancres)
ancE = [a for a, _ in ancres]
print('paires exactes %d, ancres ordonnees %d' % (len(paires), len(ancres)), flush=True)

debE, debS = sorted(E), sorted(S)
def contenant(F, deb, rva):
    i = bisect.bisect_right(deb, rva) - 1
    while i >= 0:
        b = deb[i]
        if F[b][0] > rva: return b
        if rva - b > 0x40000: return None
        i -= 1
    return None

md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True

def insns(img, b, f):
    return list(md.disasm(img[b:f], IB + b))

def apparier_fonction(be):
    """Fonction Steam correspondant a la fonction Epic qui commence en be."""
    if be in mapE: return mapE[be], 'exact', 1.0
    i = bisect.bisect_left(ancE, be)
    gauche = ancres[i - 1] if i > 0 else (0, 0)
    droite = ancres[i] if i < len(ancres) else (1 << 40, 1 << 40)
    # fenetre Steam entre les deux ancres
    lo = bisect.bisect_right(debS, gauche[1]); hi = bisect.bisect_left(debS, droite[1])
    cand = debS[lo:hi]
    seqE = E[be][1]
    meilleur, score = None, 0.0
    for bs_ in cand[:400]:
        sm = difflib.SequenceMatcher(None, seqE, S[bs_][1], autojunk=False)
        if sm.real_quick_ratio() < score: continue
        if sm.quick_ratio() < score: continue
        r = sm.ratio()
        if r > score: meilleur, score = bs_, r
    return meilleur, 'voisinage(%d cand)' % len(cand), score

def decalage_dans(be, bs_, off):
    """Decalage dans la fonction Steam de l'instruction Epic en be+off."""
    if off == 0: return 0
    iE = insns(imgE, be, E[be][0]); iS = insns(imgS, bs_, S[bs_][0])
    adrE = [x.address - IB - be for x in iE]
    if off not in adrE: return None
    k = adrE.index(off)
    nE = [x.mnemonic for x in iE]; nS = [x.mnemonic for x in iS]
    sm = difflib.SequenceMatcher(None, nE, nS, autojunk=False)
    for a, b, n in sm.get_matching_blocks():
        if a <= k < a + n: return iS[b + (k - a)].address - IB - bs_
    return None

# --- 2. references aux globaux -----------------------------------------------
textE = np.frombuffer(imgE[0x1000:0x1000 + 0x1D7A64A], dtype=np.uint8)
disp = textE[:-4].astype(np.int64) | (textE[1:-3].astype(np.int64) << 8) | (textE[2:-2].astype(np.int64) << 16) | (textE[3:-1].astype(np.int64) << 24)
disp = np.where(disp >= 1 << 31, disp - (1 << 32), disp)
pos = np.arange(len(disp), dtype=np.int64) + 0x1000

def refs_vers(lo, hi):
    """Positions (rva du deplacement) dont la cible rip-relative tombe dans [lo, hi)."""
    out = []
    for k in (0, 1, 2, 4):
        cib = pos + 4 + k + disp
        idx = np.nonzero((cib >= lo) & (cib < hi))[0]
        out += [(int(pos[i]), int(cib[i])) for i in idx]
    return out

def cible_rip(ins):
    for op in ins.operands:
        if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
            return ins.address + ins.size + op.mem.disp - IB
    return None

def global_steam(g, fenetre=0x800):
    votes = collections.Counter(); exemples = {}
    for (p, _) in refs_vers(g - fenetre, g + 1)[:4000]:
        be = contenant(E, debE, p)
        if be is None or be not in mapE: continue      # seulement via fonctions identiques
        bs_ = mapE[be]
        iE = insns(imgE, be, E[be][0]); iS = insns(imgS, bs_, S[bs_][0])
        if len(iE) != len(iS): continue
        for a, b in zip(iE, iS):
            ce = cible_rip(a)
            if ce is None or not (g - fenetre <= ce <= g): continue
            cs = cible_rip(b)
            if cs is None: continue
            v = cs + (g - ce)
            votes[(v, g - ce)] += 1
            exemples[(v, g - ce)] = be
        if sum(votes.values()) > 60: break
    if not votes: return None, 'aucune ref', 0
    # preferer la reference exacte (ecart 0), puis le plus de votes
    tot = collections.Counter()
    for (v, ec), n in votes.items(): tot[v] += n
    v, n = tot.most_common(1)[0]
    exact = any(ec == 0 and vv == v for (vv, ec) in votes)
    return v, ('ref exacte' if exact else 'ref voisine'), n / sum(tot.values())

# --- 3. traitement ------------------------------------------------------------
lignes = []
for l in open(os.path.join(ICI, 'adresses_epic.tsv'), encoding='utf-8-sig'):
    a, sec, ou = l.rstrip('\n').split('\t')
    a = int(a, 16)
    if sec == 'text':
        be = contenant(E, debE, a)
        if be is None:
            lignes.append((a, None, 'hors fonction', 0, ou)); continue
        bs_, meth, sc = apparier_fonction(be)
        if bs_ is None:
            lignes.append((a, None, meth, sc, ou)); continue
        d = decalage_dans(be, bs_, a - be)
        if d is None:
            lignes.append((a, None, meth + ' decalage?', sc, ou)); continue
        lignes.append((a, bs_ + d, meth + ('' if a == be else ' +0x%X' % (a - be)), sc, ou))
    else:
        v, meth, sc = global_steam(a)
        lignes.append((a, v, meth, sc, ou))
    print('%07X -> %s  %s %.2f' % (a, ('%07X' % lignes[-1][1]) if lignes[-1][1] else '?', lignes[-1][2], lignes[-1][3]), flush=True)

with open(os.path.join(ICI, 'correspondances.tsv'), 'w') as f:
    for a, v, m, sc, ou in lignes:
        f.write('%07X\t%s\t%s\t%.3f\t%s\n' % (a, ('%07X' % v) if v else '-', m, sc, ou))

