# empreintes.py <exe> <sortie.pkl>
# Fonctions d'un exe x64 d'apres .pdata ; pour chacune, sequence d'instructions
# normalisee (adresses absolues / rip-relatives masquees, petits immediats gardes).
import sys, re, pickle, struct
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

exe, sortie = sys.argv[1], sys.argv[2]
pe = pefile.PE(exe, fast_load=True)
pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']])
data = pe.get_memory_mapped_image()
fonctions = {}
for e in pe.DIRECTORY_ENTRY_EXCEPTION:
    b, f = e.struct.BeginAddress, e.struct.EndAddress
    # un meme debut peut avoir plusieurs entrees (fragments) : garder la plus longue
    if b not in fonctions or fonctions[b] < f:
        fonctions[b] = f
debuts = sorted(fonctions)
md = Cs(CS_ARCH_X86, CS_MODE_64)
grand = re.compile(r'0x[0-9a-f]{5,}')
rip = re.compile(r'rip \+ 0x[0-9a-f]+|rip - 0x[0-9a-f]+')
res = {}
for n, b in enumerate(debuts):
    f = fonctions[b]
    if f - b > 200000:
        continue
    code = data[b:f]
    seq = []
    for ins in md.disasm_lite(code, 0x140000000 + b):
        op = rip.sub('rip+*', ins[3])
        op = grand.sub('*', op)
        seq.append(ins[2] + ' ' + op)
    res[b] = (f, tuple(seq))
    if n % 20000 == 0:
        print(n, len(debuts), flush=True)
pickle.dump({'fonctions': res}, open(sortie, 'wb'))
print('fini', len(res))
