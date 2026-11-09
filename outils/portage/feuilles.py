# feuilles.py <rva epic> ... : fonctions sans .pdata, par motif d'octets masque
# cherche autour de la position predite par l'ancre exacte la plus proche.
import sys, pickle, os, bisect, re
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_REG_RIP, X86_OP_IMM
ICI = os.path.dirname(os.path.abspath(__file__))
imgE = pefile.PE(r"C:\Program Files\Epic Games\Frostpunk\Frostpunk.exe", fast_load=True).get_memory_mapped_image()
imgS = pefile.PE(r"C:\Program Files (x86)\Steam\steamapps\common\Frostpunk\Frostpunk.exe", fast_load=True).get_memory_mapped_image()
E = pickle.load(open(os.path.join(ICI, 'epic.pkl'), 'rb'))['fonctions']
S = pickle.load(open(os.path.join(ICI, 'steam.pkl'), 'rb'))['fonctions']
import collections
def cles(F):
    d = collections.defaultdict(list)
    for b, (f, seq) in F.items():
        if len(seq) >= 6: d[hash(seq)].append(b)
    return d
kE, kS = cles(E), cles(S)
anc = sorted((l[0], kS[h][0]) for h, l in kE.items() if len(l) == 1 and len(kS.get(h, ())) == 1)
ancE = [a for a, _ in anc]
md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True
for arg in sys.argv[1:]:
    a = int(arg, 16)
    motif, masque = bytearray(), bytearray()
    for ins in md.disasm(imgE[a:a + 96], 0x140000000 + a):
        b = bytearray(ins.bytes); m = bytearray(b'\xff' * len(b))
        rel = ins.mnemonic.startswith(('call', 'j')) and ins.operands and ins.operands[0].type == X86_OP_IMM
        rip = any(o.type == X86_OP_MEM and o.mem.base == X86_REG_RIP for o in ins.operands)
        if rel or rip:
            off = ins.disp_offset if rip else ins.imm_offset
            for k in range(off, min(off + 4, len(b))): m[k] = 0
        motif += b; masque += m
        if ins.mnemonic in ('ret', 'jmp', 'int3') or len(motif) >= 48: break
    i = bisect.bisect_right(ancE, a) - 1
    pred = anc[i][1] + (a - anc[i][0])
    trouves = []
    for c in range(pred - 0x20000, pred + 0x20000):
        ok = True
        for k in range(len(motif)):
            if masque[k] and imgS[c + k] != motif[k]: ok = False; break
        if ok: trouves.append(c)
    print('%07X  predit %07X  (%d octets) -> %s' % (a, pred, len(motif), ' '.join('%07X' % t for t in trouves[:8]) or 'rien'))
