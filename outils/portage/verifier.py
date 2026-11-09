import os, re, glob, sys
racine = sys.argv[1]; ICI = sys.argv[2]
adr = {int(l.split('\t')[0], 16) for l in open(os.path.join(ICI, 'adresses_epic.tsv'), encoding='utf-8-sig')}
lit = re.compile(r'(Adr\()?0x([0-9A-Fa-f]{6,9})\b')
nus = 0; vus = set()
for f in sorted(glob.glob(os.path.join(racine, '*.cpp')) + glob.glob(os.path.join(racine, '*.h'))):
    nom = os.path.basename(f)
    if nom.startswith('known_') or nom.startswith('versions_jeu'): continue
    b = open(f, 'rb').read()
    try: t = b.decode('utf-8-sig')
    except UnicodeDecodeError: t = b.decode('cp1252')
    bloc = False
    for n, l in enumerate(t.splitlines(), 1):
        if 'Adr(Adr(' in l: print('DOUBLE', nom, n)
        if bloc:
            if '*/' in l: bloc = False
            continue
        if '/*' in l and '*/' not in l.split('/*', 1)[1]: l = l.split('/*', 1)[0]; bloc = True
        code = l.split('//', 1)[0]
        for m in lit.finditer(code):
            v = int(m.group(2), 16)
            if v in adr:
                vus.add(v)
                if not m.group(1): print('NU', nom, n, code.strip()[:100]); nus += 1
print('nus', nus, '; adresses couvertes', len(vus), '/', len(adr))
