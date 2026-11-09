# convertir.py <projet> : enveloppe les adresses Epic du code dans Adr(...).
import os, re, sys, glob
racine = sys.argv[1]
ICI = os.path.dirname(os.path.abspath(__file__))
adresses = set()
for l in open(os.path.join(ICI, 'adresses_epic.tsv'), encoding='utf-8-sig'):
    adresses.add(int(l.split('\t')[0], 16))
lit = re.compile(r'(?<!Adr\()(?<!\w)0x([0-9A-Fa-f]{6,9})\b')
modifs = {}
for f in sorted(glob.glob(os.path.join(racine, '*.cpp')) + glob.glob(os.path.join(racine, '*.h'))):
    nom = os.path.basename(f)
    if nom.startswith('known_') or nom.startswith('versions_jeu'): continue
    brut = open(f, 'rb').read()
    bom = brut.startswith(b'\xef\xbb\xbf')
    try: texte = brut.decode('utf-8-sig'); enc = 'utf-8'
    except UnicodeDecodeError: texte = brut.decode('cp1252'); enc = 'cp1252'
    brutes = texte.splitlines(keepends=True)
    fins = [l[len(l.rstrip('\r\n')):] for l in brutes]
    lignes = [l.rstrip('\r\n') for l in brutes]
    dans_bloc = False; n = 0
    for i, ligne in enumerate(lignes):
        # partie code (hors commentaire // et blocs /* */)
        code, reste = ligne, ''
        if dans_bloc:
            if '*/' in ligne: dans_bloc = False
            continue
        if '/*' in code:
            avant, apres = code.split('/*', 1)
            if '*/' not in apres:
                dans_bloc = True; code, reste = avant, '/*' + apres
        if '//' in code:
            k = code.index('//'); code, reste = code[:k], code[k:] + reste
        def rempl(m):
            v = int(m.group(1), 16)
            if v in adresses: return 'Adr(%s)' % m.group(0)
            return m.group(0)
        nouveau = lit.sub(rempl, code)
        if nouveau != code:
            if 'constexpr uintptr_t' in nouveau:
                nouveau = nouveau.replace('constexpr uintptr_t', 'inline const uintptr_t' if nom.endswith('.h') else 'static const uintptr_t')
            lignes[i] = nouveau + reste; n += 1
    if n:
        sortie = ''.join(l + e for l, e in zip(lignes, fins))
        open(f, 'wb').write((b'\xef\xbb\xbf' if bom else b'') + sortie.encode(enc))
        modifs[nom] = n
for k, v in modifs.items(): print('%-22s %d ligne(s)' % (k, v))
print('total', sum(modifs.values()))


