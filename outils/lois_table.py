# Table des checks de lois par scenario (arbre AP, rang, lois concernees),
# depuis le releve F4 (lignes "[LOITAB]", lois.cpp).
#   python outils\lois_table.py [journal ...]
# Plusieurs journaux possibles : les noms affiches sont pris dans tous (le
# jeu ne traduit que les lois vues dans le livre pendant la session).
# Ecrit, depuis la racine du projet :
#   outils\lois_table.tsv        table pour l'apworld
#   .claude\chantiers\lois.md       tableau lisible (entre les marqueurs)
#
# Regles (decisions de l'auteur, 2026-10-30 / 2026-11-03) :
#   - arbres = ceux de lois_ap (Purpose = Order + Faith, Politics = Left +
#     Right, les autres nommes par leur premiere racine) ;
#   - le check de rang r part a la r-ieme signature dans l'arbre (poids
#     compris) : le rang ne designe pas une loi, les lois listees sont
#     l'ordre canonique (prerequis d'abord) ;
#   - loi sans exclusion = 1 check ; paire exclusive = 1 check ; lois
#     exclusives par leurs parents (meme profondeur, branches differentes
#     d'une paire) = 1 check ; Ceremonial Funerals (Adaptation) = 2 checks,
#     1 seul la ou Organic Fertiliser n'existe pas (On the Edge) ;
#   - Purpose / Politics : rang r = r-ieme loi signee dans la branche choisie ;
#   - lois de depart, desactivees, cachees, verrouillees : exclues.
import collections
import itertools
import os
import re
import sys

RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JOURNAUX = sys.argv[1:] or [os.path.join(RACINE, "logs", "console.log")]
SORTIE_TSV = os.path.join(RACINE, "outils", "lois_table.tsv")
DOC = os.path.join(RACINE, ".claude", "chantiers", "lois.md")
DEBUT_DOC = "<!-- TABLE LOIS (generee par outils/lois_table.py) -->"
FIN_DOC = "<!-- FIN TABLE LOIS -->"

SCENARIOS = [
    ("DEFAULT", "A New Home"),
    ("TechHaven", "The Arks"),
    ("REFUGEES", "Refugees"),
    ("WINTERHOME", "Fall of Winterhome"),
    ("BUILDERS", "Last Autumn"),
    ("Reconquest", "On the Edge"),
]

# Unlocks par scenario (releves F4 v5 a v8, .claude/chantiers/lois.md) :
# "depart" = actif des le debut, "plus tard" = donne par l'histoire.
# Un unlock absent de la table n'existe pas dans le scenario.
UNLOCKS = {
    "DEFAULT":    {"IdeologyTree": "plus tard"},
    "TechHaven":  {"[TechHeaven] Quest1": "depart"},
    "REFUGEES":   {"IdeologyTree": "depart"},
    "WINTERHOME": {"IdeologyTree": "depart"},
    "BUILDERS":   {"[Builders]Builders Scenario": "depart",
                   "[Builders] SOCIOTECH Labour tree - SAFETY": "plus tard",
                   "[Builders] SOCIOTECH Labour tree - STRIKE L": "plus tard",
                   "[Builders] SOCIOTECH Labour tree - STRIKE R": "plus tard",
                   "[Builders] Act 3 - The End": "plus tard"},
    "Reconquest": {},
}

# Politiques (enjeu=politique, sous-chaine) : valeur fixe, ou None = vraie
# avec la condition notee. Scenario -> note supplementaire.
POLITIQUES = [
    ("Sociotechs blocker=Avalible", True, {"WINTERHOME": "retiree en cours de partie (Winterhome)"}),
    ("Alternative food source avalible=Avalible", False, {}),
    ("IsEndlessBuilders=Yes", False, {}),
    ("IsEndless=Yes", False, {}),
    ("ManchesterQuestFinished", False, {}),
    ("Path chosen=", None, {}),   # choix Order / Faith : condition notee
]

# Noms affiches de secours (.claude/chantiers/lois.md), cle = (categorie, nom
# interne sans prefixe [..]) ; le releve passe avant.
NOMS = {}
for cat, interne, aff in [
    ("Adaptation", "Kids work in light jobs", "Child Labour - Safe Jobs"),
    ("Adaptation", "Kids work in hard jobs", "Child Labour - All Jobs"),
    ("Adaptation", "Med Kids", "Medic Apprentices"),
    ("Adaptation", "EngineerKids", "Engineer Apprentices"),
    ("Adaptation", "Fightclub", "Fighting Arena"),
    ("Adaptation", "PublicHouses unlock", "Public House"),
    ("Adaptation", "Dueling Law", "Duelling Law"),
    ("Adaptation", "Emergancy shift", "Emergency Shift"),
    ("Adaptation", "Hard work rigor", "Extended Shift"),
    ("Adaptation", "Poor Soup", "Soup"),
    ("Adaptation", "Sawdust in food", "Food Additives"),
    ("Adaptation", "Put them in the snow", "Corpse Disposal"),
    ("Adaptation", "Burial Ceremony", "Ceremonial Funerals"),
    ("Adaptation", "Transplantation", "Organ Transplants"),
    ("Adaptation", "Life Sustain", "Sustain Life"),
    ("Adaptation", "Double Ration", "Extra Rations for the Ill"),
    ("Adaptation", "Poeple on the floor", "Overcrowding"),
    ("Adaptation", "RadicalTreatment", "Radical Treatment"),
    ("Adaptation", "Moonshine", "Moonshine"),
    ("ADMINISTRATION", "Life Sustain", "Sustain Life"),
    ("ADMINISTRATION", "Double Ration", "Extra Rations for the Ill"),
    ("ADMINISTRATION", "People on the floor", "Overcrowding"),
    ("Purpose", "Neighborhood watch", "Neighbourhood Watch"),
    ("Purpose", "Morning gathering", "Morning Gathering"),
    ("Purpose", "Guardpost unlock", "Guard Stations"),
    ("Purpose", "Forman", "Foreman"),
    ("Purpose", "Mechanical Agitator", "Agitator"),
    ("Purpose", "Shrine", "Shrines"),
    ("Purpose", "Temple", "The Temple"),
    ("Purpose", "Evening of Prayers", "Evening Prayers"),
    ("Purpose", "Faith keepers", "Faith Keepers"),
    ("Adaptation", "Corps Processing - fertilizer", "Organic Fertiliser"),
    ("Adaptation", "Augmentation", "Prosthetics"),
    ("Adaptation", "Prostitiutes", "House of Pleasure"),
    ("ADMINISTRATION", "Pub", "Public House"),
    ("ADMINISTRATION", "Prostitutes", "House of Pleasure"),
    ("ADMINISTRATION", "Evening of Prayers", "Evening Service"),
    ("ADMINISTRATION", "Cemetery", "On-Site Burials"),
    ("ADMINISTRATION", "Morgue", "Repatriation of Bodies"),
    ("ADMINISTRATION", "Steel coffins", "Company Funerals"),
    ("ADMINISTRATION", "Better food", "Hearty Meals"),
    ("ADMINISTRATION", "Bathhouse", "Bath House"),
    ("ADMINISTRATION", "Cocaine", "Cocaine Pills"),
    ("ADMINISTRATION", "Augmentation", "Prostheses Shipments"),
    ("Purpose", "Census Station", "Propaganda Centre"),
    ("Purpose", "Propaganda ability", "Pledge of Loyalty"),
    ("Purpose", "Prison beating", "Forceful Persuasion"),
    ("Purpose", "DESPOTIZM", "NEW ORDER"),
    ("Purpose", "Captains message", "Protector of the Truth"),
    ("Purpose", "Public conversion", "Public Penance"),
    ("Purpose", "FANATIZM", "NEW FAITH"),
]:
    NOMS[(cat, interne)] = aff


def sans_prefixe(nom):
    return re.sub(r"^(Builders\s*)?(\s*\[[^\]]*\]\s*)+", "", nom).strip()


def lire_journaux():
    CAT, L, REG, SCN, AFF = {}, {}, {}, {}, {}
    for j in JOURNAUX:
        for l in open(j, encoding="utf-8", errors="replace"):
            if "[LOITAB]" not in l:
                continue
            corps = l.split("[LOITAB]", 1)[1].strip()
            p = corps.split(" | ")
            if p[0] == "CAT" and len(p) >= 3:
                CAT[p[1]] = " | ".join(p[2:])
            elif p[0] == "L" and len(p) >= 13:
                p = corps.split(" | ", 12)
                d = dict(guid=p[1], nom=p[2], categorie=p[4], active=p[5] == "1",
                         exclusion="" if p[6] == "-" else p[6], arbre="" if p[7] == "-" else p[7],
                         poids=int(p[8]), x=int(p[9]), y=int(p[10]), req=p[11], voir=p[12])
                L[p[1]] = d
                if p[3] != "-":
                    AFF[p[1]] = p[3]
            elif p[0] == "REG" and len(p) >= 4:
                REG[p[1]] = tuple([] if x in ("(aucune)", "") else x.split(", ") for x in p[2:4])
            elif p[0] == "SCN" and len(p) >= 3:
                SCN[p[1]] = [] if p[2] == "-" else p[2].split(", ")
    if not L:
        sys.exit(f"aucune ligne [LOITAB] dans {JOURNAUX} : faire F4 en partie")
    return CAT, L, REG, SCN, AFF


# --- Expressions de prerequis (format de DecrireExigence, lois.cpp) ---------------
def decouper(s, sep):
    out, prof, cur, i = [], 0, "", 0
    while i < len(s):
        if s.startswith(sep, i) and prof == 0:
            out.append(cur); cur = ""; i += len(sep); continue
        c = s[i]
        prof += c == "("
        prof -= c == ")"
        cur += c
        i += 1
    out.append(cur)
    return [x.strip() for x in out if x.strip()]


def analyser(s):
    # -> ("et"|"ou"|"non", [noeuds]) ou ("feuille", texte)
    m = re.match(r"^op(\d)\((.*)\)$", s)
    if m:
        op = {"0": "et", "1": "ou", "2": "non"}.get(m.group(1), "et")
        return (op, [analyser(x) for x in decouper(m.group(2), ", ")])
    return ("feuille", s)


def analyser_liste(s):
    if s in ("(aucun)", ""):
        return ("et", [])
    return ("et", [analyser(x) for x in decouper(s, " ; ")])


INCONNUS = set()


def evaluer(n, scn):
    # -> (possible, [conditions]) ; prerequis de loi / d'exclusion : vrais ici.
    t, v = n
    if t == "feuille":
        if v.startswith("loi ") or v.startswith("exclusion "):
            return True, []
        if v.startswith("unlock "):
            u = v[7:]
            etat = UNLOCKS[scn].get(u)
            if etat is None:
                if not any(u in d for d in UNLOCKS.values()):
                    INCONNUS.add(v)
                return False, []
            return True, ([] if etat == "depart" else [v])
        if v.startswith("politique "):
            for cle, val, notes in POLITIQUES:
                if cle in v:
                    if val is None:
                        return True, [v]
                    return val, ([notes[scn]] if val and scn in notes else [])
            INCONNUS.add(v)
            return True, [v]
        m = re.match(r"^(PAS )?scenario (.*)$", v)
        if m:
            return (m.group(2) == scn) != bool(m.group(1)), []
        if v.startswith("techno "):
            return True, [v]
        INCONNUS.add(v)
        return True, [v]
    res = [evaluer(x, scn) for x in v]
    if t == "et":
        return all(r[0] for r in res), [c for r in res for c in r[1]]
    if t == "ou":
        ok = [r for r in res if r[0]]
        if not ok:
            return False, []
        if any(not r[1] for r in ok):
            return True, []
        return True, ["(" + " ou ".join("+".join(r[1]) for r in ok) + ")"]
    # non : NON(ET de la liste)
    poss, cond = all(r[0] for r in res), [c for r in res for c in r[1]]
    if not poss:
        return True, []
    if not cond:
        return False, []
    return True, ["avant " + " + ".join(cond)]


def lois_requises(n):
    # Structure des prerequis de loi : ("et"|"ou", [noms ou sous-structures]).
    t, v = n
    if t == "feuille":
        return v[4:] if v.startswith("loi ") else None
    if t == "non":
        return None
    sous = [x for x in (lois_requises(y) for y in v) if x is not None]
    return (t, sous) if sous else None


def main():
    CAT, L, REG, SCN, AFF = lire_journaux()
    par_nom = {}
    for g, l in L.items():
        par_nom.setdefault(l["nom"], g)
    for g, l in L.items():
        l["affiche"] = AFF.get(g) or NOMS.get((l["categorie"], sans_prefixe(l["nom"]))) or sans_prefixe(l["nom"])
        l["arbre_req"] = lois_requises(analyser_liste(l["req"]))

    lignes, resume, erreurs = [], [], []
    for scn, affiche in SCENARIOS:
        reglages = SCN.get(scn, [])
        depart, verr = set(), set()
        for r in reglages:
            d, v = REG.get(r, ([], []))
            depart |= set(d)
            verr |= set(v)
        presentes, exclues = {}, collections.defaultdict(list)
        cond_arbre, deja = {}, set()
        for g, l in L.items():
            if not l["active"] or not l["arbre"]:
                continue
            okc, cc = evaluer(analyser_liste(CAT.get(l["categorie"], "(aucun)")), scn)
            if not okc:
                continue
            if l["nom"] in depart:
                exclues["de depart"].append(l["nom"]); deja.add(l["nom"]); continue
            if l["nom"] in verr:
                exclues["verrouillees"].append(l["nom"]); continue
            okv, cv = evaluer(analyser_liste(l["voir"]), scn)
            if not okv:
                exclues["cachees"].append(l["nom"]); continue
            okr, cr = evaluer(analyser_liste(l["req"]), scn)
            if not okr:
                exclues["bloquees (unlock absent)"].append(l["nom"]); continue
            presentes[l["nom"]] = l
            l.setdefault("cond", {})[scn] = sorted(set(cv + cr))
            cond_arbre.setdefault(l["arbre"], sorted(set(cc)))
        # Lois exclusives d'une loi de depart (meme groupe, meme categorie) :
        # jamais signables.
        groupes_depart = {(L[par_nom[n]]["categorie"], L[par_nom[n]]["exclusion"]) for n in deja if L[par_nom[n]]["exclusion"]}
        for nom, l in list(presentes.items()):
            if (l["categorie"], l["exclusion"]) in groupes_depart:
                del presentes[nom]
                exclues["exclusives d'une loi de depart"].append(nom)
        # Lois dont une loi requise est absente (sans alternative) : exclues.
        # Les lois de depart sont deja signees.
        change = True
        while change:
            change = False
            for nom, l in list(presentes.items()):
                if not satisfaisable(l["arbre_req"], set(presentes) | deja):
                    del presentes[nom]
                    exclues["loi requise absente"].append(nom)
                    change = True
        # Ceremonial Funerals (poids 2) ne pese que 1 la ou Organic Fertiliser
        # n'existe pas (decision de l'auteur, 2026-11-03 ; Poids, lois_ap.cpp).
        fertiliser = any("Corps Processing" in n for n in presentes)
        for l in presentes.values():
            l["poids_scn"] = l["poids"] if fertiliser else min(l["poids"], 1)
        par_arbre = collections.defaultdict(list)
        for l in presentes.values():
            par_arbre[l["arbre"]].append(l)
        for arbre in sorted(par_arbre):
            membres = par_arbre[arbre]
            n_max = signatures_max(arbre, membres, deja)
            rangs = rangs_canoniques(arbre, membres, presentes)
            if len(rangs) != n_max:
                erreurs.append(f"{scn} / {arbre} : {len(rangs)} rangs canoniques, mais {n_max} signatures possibles au plus")
            for k, (lois, req) in enumerate(rangs, 1):
                conds = sorted({c for nom in lois for c in presentes[sans_suffixe(nom)]["cond"][scn]})
                lignes.append(dict(scn=scn, affiche_scn=affiche, arbre=arbre, rang=k,
                                   lois=lois, aff=[affiche_de(n, presentes) for n in lois], req=req,
                                   cond_arbre=" ; ".join(cond_arbre.get(arbre, [])), cond=" ; ".join(conds)))
        resume.append(dict(scn=scn, affiche=affiche, exclues=exclues,
                           arbres={a: sum(1 for x in lignes if x["scn"] == scn and x["arbre"] == a) for a in par_arbre}))
    if INCONNUS:
        print("A VERIFIER (prerequis non reconnus, tenus pour vrais ou unlock absent) :")
        for x in sorted(INCONNUS):
            print("   ", x)
    for e in erreurs:
        print("ERREUR", e)
    ecrire_tsv(lignes, resume)
    ecrire_doc(lignes, resume)
    for r in resume:
        print(f"{r['scn']:<11} {sum(r['arbres'].values()):3d} checks, {len(r['arbres']):2d} arbres")
    if erreurs:
        sys.exit(1)


def satisfaisable(st, noms):
    if st is None:
        return True
    if isinstance(st, str):
        return st in noms
    t, v = st
    r = [satisfaisable(x, noms) for x in v]
    return any(r) if t == "ou" else all(r)


SUFFIXE_2E = " (2e check)"


def sans_suffixe(nom):
    return nom[:-len(SUFFIXE_2E)] if nom.endswith(SUFFIXE_2E) else nom


def affiche_de(nom, presentes):
    l = presentes[sans_suffixe(nom)]
    return l["affiche"] + (SUFFIXE_2E if nom.endswith(SUFFIXE_2E) else "")


def branche(l):
    m = re.search(r"\[(Order|Faith|Left|Right)", l["nom"])
    return m.group(1) if m else ""


def signatures_max(arbre, membres, deja):
    # Nombre maximal de checks d'une partie : meilleur choix dans chaque
    # groupe d'exclusion (et de branche pour Purpose / Politics).
    branches = sorted({branche(l) for l in membres}) if arbre in ("Purpose", "Politics") else [""]
    meilleur = 0
    for b in branches:
        ls = [l for l in membres if not b or branche(l) == b]
        groupes = collections.defaultdict(list)
        for l in ls:
            if l["exclusion"]:
                groupes[l["exclusion"]].append(l["nom"])
        cles = sorted(groupes)
        for choix in itertools.product(*[groupes[c] + [None] for c in cles]):
            permises = {l["nom"] for l in ls if not l["exclusion"]} | {c for c in choix if c}
            signees, change = set(deja), True
            while change:
                change = False
                for l in ls:
                    if l["nom"] in permises and l["nom"] not in signees and satisfaisable(l["arbre_req"], signees):
                        signees.add(l["nom"]); change = True
            meilleur = max(meilleur, sum(L_poids(l) for l in ls if l["nom"] in signees))
    return meilleur


def L_poids(l):
    return max(1, l.get("poids_scn", l["poids"]))


def texte_req(st):
    if st is None:
        return ""
    if isinstance(st, str):
        return st
    t, v = st
    return (" ou " if t == "ou" else " + ").join(("(" + texte_req(x) + ")") if not isinstance(x, str) and len(x[1]) > 1 else texte_req(x) for x in v)


def rangs_canoniques(arbre, membres, presentes):
    noms = {l["nom"] for l in membres}

    def prof(st, memo={}):
        if st is None:
            return 0
        if isinstance(st, str):
            return profondeur(presentes[st]) if st in noms else 0
        t, v = st
        r = [prof(x) for x in v]
        return (min(r) if t == "ou" else max(r)) if r else 0

    def profondeur(l):
        if "prof" not in l:
            l["prof"] = 1 + prof(l["arbre_req"])
        return l["prof"]

    def cote(st):
        # Ensemble {(groupe, loi)} des choix exclusifs dont depend la loi.
        if st is None:
            return frozenset()
        if isinstance(st, str):
            return cote_loi(presentes[st]) if st in noms else frozenset()
        t, v = st
        r = [cote(x) for x in v]
        if not r:
            return frozenset()
        return frozenset.intersection(*r) if t == "ou" else frozenset().union(*r)

    def cote_loi(l):
        if "cote" not in l:
            l["cote"] = cote(l["arbre_req"]) | ({(l["exclusion"], l["nom"])} if l["exclusion"] else frozenset())
        return l["cote"]

    def conflit(a, b):
        return any(g1 == g2 and x != y for g1, x in cote_loi(a) for g2, y in cote_loi(b))

    for l in membres:
        l.pop("prof", None); l.pop("cote", None)
    cle = lambda l: (profondeur(l), l["y"], l["x"], l["nom"])

    if arbre in ("Purpose", "Politics"):
        par_b = collections.defaultdict(list)
        for l in sorted(membres, key=cle):
            par_b[branche(l)].append(l)
        bs = sorted(par_b)
        n = max(len(v) for v in par_b.values())
        rangs = []
        for k in range(n):
            ls = [par_b[b][k] for b in bs if k < len(par_b[b])]
            rangs.append(([l["nom"] for l in ls], " | ".join(texte_req(l["arbre_req"]) for l in ls)))
        return rangs

    # Unites : groupes d'exclusion, puis fusion des unites de meme profondeur
    # en conflit (lois exclusives par leurs parents).
    unites = []
    groupes = {}
    for l in sorted(membres, key=cle):
        if l["exclusion"]:
            if l["exclusion"] not in groupes:
                groupes[l["exclusion"]] = []
                unites.append(groupes[l["exclusion"]])
            groupes[l["exclusion"]].append(l)
        else:
            unites.append([l])
    fusion = True
    while fusion:
        fusion = False
        for i, j in itertools.combinations(range(len(unites)), 2):
            a, b = unites[i], unites[j]
            if min(map(profondeur, a)) == min(map(profondeur, b)) and all(conflit(x, y) for x in a for y in b):
                unites[i] = sorted(a + b, key=cle)
                del unites[j]
                fusion = True
                break
    unites.sort(key=lambda u: min(cle(l) for l in u))

    # Poids > 1 : les checks suivants rejoignent les unites suivantes dont
    # toutes les lois sont en conflit avec elle (Ceremonial Funerals |
    # Organic Fertiliser), sinon un rang a part juste apres.
    rangs = [[[l["nom"] for l in u], [l for l in u]] for u in unites]
    k = 0
    while k < len(rangs):
        for l in list(rangs[k][1]):
            for _ in range(L_poids(l) - 1):
                cible = next((r for r in rangs[k + 1:] if l["nom"] + SUFFIXE_2E not in r[0]
                              and all(conflit(l, x) for x in r[1]) and not any(n.endswith(SUFFIXE_2E) for n in r[0])), None)
                if cible is None:
                    rangs.insert(k + 1, [[l["nom"] + SUFFIXE_2E], []])
                else:
                    cible[0].insert(0, l["nom"] + SUFFIXE_2E)
        k += 1
    return [(r[0], " | ".join(texte_req(presentes[sans_suffixe(n)]["arbre_req"]) for n in r[0])) for r in rangs]


def ecrire_tsv(lignes, resume):
    with open(SORTIE_TSV, "w", encoding="utf-8") as f:
        f.write("scenario\tarbre\trang\tlois (nom interne)\tlois (nom affiche)\tlois requises par le jeu\t"
                "condition d'ouverture de l'arbre\tcondition des lois\n")
        for l in lignes:
            f.write(f"{l['scn']}\t{l['arbre']}\t{l['rang']}\t{' | '.join(l['lois'])}\t{' | '.join(l['aff'])}\t"
                    f"{l['req']}\t{l['cond_arbre']}\t{l['cond']}\n")
        for r in resume:
            for motif, noms in sorted(r["exclues"].items()):
                f.write(f"# exclues {r['scn']} ({motif}) : {', '.join(sorted(noms))}\n")


def ecrire_doc(lignes, resume):
    md = [DEBUT_DOC, ""]
    md.append("Table générée par `outils\\lois_table.py` (relevé F4, lignes `[LOITAB]`). "
              "Le check de rang r part à la r-ième signature dans l'arbre (poids compris) ; "
              "les lois d'un rang sont l'ordre canonique (prérequis d'abord), séparées par `|` "
              "quand elles sont exclusives (un seul check). Purpose / Politics : une colonne par branche.")
    md.append("")
    for r in resume:
        sel = [l for l in lignes if l["scn"] == r["scn"]]
        md.append(f"### {r['affiche']} (`{r['scn']}`) — {len(sel)} checks")
        md.append("")
        for motif, noms in sorted(r["exclues"].items()):
            md.append(f"Exclues ({motif}) : {', '.join(sorted(noms))}.")
            md.append("")
        if not sel:
            continue
        md.append("| Arbre | Rang | Lois (affiché) | Lois (interne) | Lois requises | Ouverture de l'arbre | Condition |")
        md.append("|---|---:|---|---|---|---|---|")
        barre = " \\| "
        for l in sel:
            req = l["req"].replace("|", "\\|")
            md.append(f"| {l['arbre']} | {l['rang']} | {barre.join(l['aff'])} | {barre.join(l['lois'])} | "
                      f"{req} | {l['cond_arbre']} | {l['cond']} |")
        md.append("")
    md.append(FIN_DOC)
    texte = open(DOC, encoding="utf-8").read()
    bloc = "\n".join(md)
    if DEBUT_DOC in texte:
        texte = texte[:texte.index(DEBUT_DOC)] + bloc + texte[texte.index(FIN_DOC) + len(FIN_DOC):]
    else:
        texte = texte.rstrip("\n") + "\n\n## Table des checks par scénario\n\n" + bloc + "\n"
    with open(DOC, "w", encoding="utf-8") as f:
        f.write(texte)


if __name__ == "__main__":
    main()
