# Table d'apparition des sites d'exploration (session apworld, 2026-11-03).
#   python outils\sites_apparition.py [logs\console.log]
# Lit les lignes [SREV] V / R du journal (releve F9, scooting.cpp) et les
# fusionne dans le cache outils\sites_revelations.tsv (un scenario releve
# remplace ses anciennes lignes), puis ecrit outils\sites_apparition.tsv :
#   scenario | site (cle de SITE_PAYLOADS) | nom affiche | condition | region
# Region : "Start" = visible au depart ou atteint par les listes Reveals
# (exploration seule) ; sinon la region du site "racine" qui ouvre sa chaine
# (RACINES, decisions de l'auteur du 2026-11-03).
# Cache : D = visible au depart, C = cache et non atteint par les Reveals,
# R = lien Reveals (parent -> enfant).
import os, re, sys, collections

RACINE_PROJET = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JOURNAL = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RACINE_PROJET, 'logs', 'console.log')
CACHE = os.path.join(RACINE_PROJET, 'outils', 'sites_revelations.tsv')
ENTETE = os.path.join(RACINE_PROJET, 'known_sites.h')
SORTIE = os.path.join(RACINE_PROJET, 'outils', 'sites_apparition.tsv')

# Site racine -> (source lue au releve [SREV] C, region). "A VALIDER" = a trancher.
RACINES = {
    ('DEFAULT', 'Site_049_A0'): ('[ENDGAME] 002b Prepare for the storm SURVIVORS (debut) ou U: Refugees in the City3', 'apres 006 REFUGEES'),
    ('DEFAULT', 'Site_053_B0'): ('[ENDGAME] 002b Prepare for the storm SURVIVORS (debut) ou U: Refugees in the City3', 'apres 006 REFUGEES'),
    ('DEFAULT', 'Site_057_C0'): ('[ENDGAME] 002b Prepare for the storm SURVIVORS (debut) ou U: Refugees in the City3', 'apres 006 REFUGEES'),
    ('TechHaven', '[quest]SouthPass'): ('[TechHeaven] 003 EXPLORE THE FROSTLAND (debut)', 'apres 001 FIND FACTORY PLANS'),
    ('TechHaven', '[quest]MistyValley'): ('[TechHeaven] 003 EXPLORE THE FROSTLAND (debut)', 'apres 001 FIND FACTORY PLANS'),
    ('TechHaven', '[NM]Expedition01'): ('[TechHeaven] 005 NEW MANCHESTER - FINDING THE WAY (debut)', 'apres 001 FIND FACTORY PLANS'),
    ('WINTERHOME', '[lore05]_Trail_South'): ('evenement E: Trail South Discovered (conditions d\'unlock)', 'apres 004A FINAL HOURS'),
    ('WINTERHOME', '[t-plain][landmark]site08'): ('aucune source trouvee au releve (Broken Tree) ; derniere section selon l\'auteur', 'apres 004A FINAL HOURS'),
    ('BUILDERS', '42. Natures Sculpture'): ('[Builders] 003 GENERATOR CONSTRUCTION SITE (succes) ou dilemme [Telegram] 2', 'apres 003 GENERATOR CONSTRUCTION SITE'),
    ('BUILDERS', '61. Workers Camp'): ('[Builders] New Allerton (debut, apres 004 GENERATOR FOUNDATIONS) ou dilemme [Telegram] 4a', 'apres 006 GENERATOR FINALIZATION'),
    ('BUILDERS', '66. Polar Bear Cave'): ('dilemme [Act III] Missing lookout II', 'apres 006 GENERATOR FINALIZATION'),
    ('Reconquest', '29. Natural Terrace'): ('[3] Food settlement (debut) : ajoute aux revelations de 04. Skull Cave', 'apres [3] Food settlement'),
    ('Reconquest', '08. Tower Ruins'): ('[5] Prepare for confrontation (debut) : ajoute aux revelations de 07. Stone Wall / 18. Observatory Ruins / 22. Fortified Hut', 'apres [3] Food settlement'),
    ('Reconquest', '20. Transshipment Station 01'): ('[5] Prepare for confrontation (debut) : idem', 'apres [3] Food settlement'),
    ('Reconquest', '23. Ice Caverns'): ('[5] Prepare for confrontation (debut) : idem', 'apres [3] Food settlement'),
    ('REFUGEES', '[ref]Radio_Mast'): ('dilemme [3][Lords] S: Radio Mast', 'apres 003 FACE THE LORDS'),
    ('REFUGEES', '[ref]Trail_Bend'): ('dilemme [1][OurPeople] Survivors04 - MissingChildren ; PAIRE EXCLUSIVE avec [ref]Trail_Bend-dead : un seul check', 'apres 001 SAVE YOUR PEOPLE'),
    ('REFUGEES', '[ref]Trail_Bend-dead'): ('[OurPeople] S: Our People - timeout ; PAIRE EXCLUSIVE avec [ref]Trail_Bend : un seul check', 'apres 001 SAVE YOUR PEOPLE'),
    ('REFUGEES', '[ref]Landing_Place'): ('dilemme [1][OurPeople] Survivors06 - UnknowShip', 'apres 001 SAVE YOUR PEOPLE'),
    ('REFUGEES', '[ref]White_Pass'): ('dilemme [1][OurPeople] Survivors07 - LostRefugees ; PAIRE EXCLUSIVE avec [ref]White_Pass-dead : un seul check', 'apres 001 SAVE YOUR PEOPLE'),
    ('REFUGEES', '[ref]White_Pass-dead'): ('[OurPeople] S: Our People - timeout ; PAIRE EXCLUSIVE avec [ref]White_Pass : un seul check', 'apres 001 SAVE YOUR PEOPLE'),
    ('REFUGEES', '[ref]Black_Spire'): ('dilemme [1][OurPeople] Survivors09 - BlackSpire ; PAIRE EXCLUSIVE avec [ref]Black_Spire-dead : un seul check', 'apres 001 SAVE YOUR PEOPLE'),
    ('REFUGEES', '[ref]Black_Spire-dead'): ('[OurPeople] S: Our People - timeout ; PAIRE EXCLUSIVE avec [ref]Black_Spire : un seul check', 'apres 001 SAVE YOUR PEOPLE'),
    ('REFUGEES', '[lordCamp] Camp_A'): ('evenements Lords01 / Lord Established Camp1 (003 FACE THE LORDS)', 'apres 003 FACE THE LORDS'),
    ('REFUGEES', '[lordCamp] Camp_B'): ('evenements Lords02 / Lord Established Camp2 (003 FACE THE LORDS)', 'apres 003 FACE THE LORDS'),
    ('REFUGEES', '[lordCamp] Camp_C'): ('evenement Lord Established Camp3 (003 FACE THE LORDS)', 'apres 003 FACE THE LORDS'),
}
for i in range(1, 7):
    RACINES[('Reconquest', f'TG{i}. Generic')] = ('quete optionnelle [TradeRoutes] (amelioration de route commerciale)', 'apres [3] Food settlement')
for i in range(1, 4):
    RACINES[('Reconquest', f'TD{i}. Dilemma')] = ('quete optionnelle [TradeRoutes] (amelioration de route commerciale)', 'apres [3] Food settlement')

# --- cache ---
D = collections.defaultdict(set); C = collections.defaultdict(set); R = collections.defaultdict(set)
if os.path.exists(CACHE):
    for l in open(CACHE, encoding='utf-8').read().splitlines()[1:]:
        t, s, n, lien = (l.split('\t') + [''])[:4]
        if t == 'D': D[s].add(n)
        elif t == 'C': C[s].add(n)
        elif t == 'R': R[s].add((n, lien))

# --- journal : le dernier releve de chaque scenario remplace le cache ---
V = collections.defaultdict(dict); RJ = collections.defaultdict(set)
if os.path.exists(JOURNAL):
    for l in open(JOURNAL, encoding='utf-8', errors='replace').read().splitlines():
        f = l.split('\t')
        if l.startswith('[SREV]\tV\t'): V[f[2]][f[3]] = f[5] == '1'
        elif l.startswith('[SREV]\tR\t'): RJ[f[2]].add((f[3], f[4]))
for s in V:
    depart = {n for n, vis in V[s].items() if vis}
    atteints = set(depart); file = sorted(depart)
    while file:
        n = file.pop(0)
        for p, e in sorted(RJ[s]):
            if p == n and e not in atteints: atteints.add(e); file.append(e)
    D[s] = depart
    C[s] = set(V[s]) - atteints
    R[s] = set(RJ[s])
    print(f'releve du journal : {s} ({len(V[s])} sites, {len(depart)} visibles, {len(C[s])} caches)')
with open(CACHE, 'w', encoding='utf-8', newline='') as o:
    o.write('type\tscenario\tsite\tlien\n')
    for s in sorted(set(D) | set(C) | set(R)):
        for n in sorted(D[s]): o.write(f'D\t{s}\t{n}\t\n')
        for n in sorted(C[s]): o.write(f'C\t{s}\t{n}\t\n')
        for p, e in sorted(R[s]): o.write(f'R\t{s}\t{p}\t{e}\n')

# --- table ---
sites = []
for m in re.finditer(r'\{ "([^"]*)", "([^"]*)", "[^"]*", "([^"]*)", "[^"]*", \d+, (true|false), (true|false) \}',
                     open(ENTETE, encoding='utf-8').read()):
    if m.group(5) == 'true': sites.append((m.group(1), m.group(2), m.group(3)))

def parents(s, n): return sorted(p for p, e in R[s] if e == n)

def racines(s, n, vus=()):
    if (s, n) in RACINES: return [n]
    out = []
    for p in parents(s, n):
        if p not in vus and p in C[s]: out += racines(s, p, vus + (n,))
    return sorted(set(out))

nb = collections.Counter()
with open(SORTIE, 'w', encoding='utf-8', newline='') as o:
    o.write('scenario\tsite\tnom_affiche\tcondition\tregion\n')
    for s, n, aff in sites:
        if s not in D and s not in C:
            cond, reg = 'RELEVE MANQUANT', 'A VALIDER'
        elif n in D[s]:
            cond, reg = '', 'Start'
        elif n not in C[s]:
            p = [x for x in parents(s, n) if x not in C[s]]
            cond = 'exploration de ' + ' ou '.join(p) if p else 'exploration (chaine Reveals depuis le depart)'
            reg = 'Start'
        elif (s, n) in RACINES:
            cond, reg = RACINES[(s, n)]
        else:
            rac = racines(s, n)
            regs = sorted({RACINES[(s, r)][1] for r in rac if (s, r) in RACINES})
            cond = 'exploration de ' + ' ou '.join(parents(s, n)) + ' (chaine depuis ' + ', '.join(rac) + ')'
            reg = regs[0] if len(regs) == 1 else ('A VALIDER : ' + ' / '.join(regs) if regs else 'A VALIDER')
        nb[(s, reg if not reg.startswith('A VALIDER') else 'A VALIDER')] += 1
        o.write(f'{s}\t{n}\t{aff}\t{cond}\t{reg}\n')
for k in sorted(nb): print(f'{k[0]:12} {k[1]:45} {nb[k]}')
