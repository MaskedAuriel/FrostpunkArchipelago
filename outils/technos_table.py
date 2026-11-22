# Table des technologies par scenario (palier, arbre progressif, rang,
# parent), depuis le releve F4 (lignes "[TECHNO]", lois.cpp).
#   python outils\technos_table.py [logs\console.log]
# Ecrit, depuis la racine du projet :
#   known_techs.h                    table du mod (un groupe par scenario AP)
#   outils\technos_table.tsv         table pour l'apworld
#   .claude\chantiers\technos.md        tableau lisible (entre les marqueurs)
# Source de verite des numeros d'arbres : outils\technos_arbres.tsv
# (hash -> arbre). Cree au premier lancement depuis l'ancien known_techs.h,
# puis edite a la main : un numero d'arbre ne change plus (ReceiveProgressive).
#
# Regles (decisions de l'auteur, 2026-11-03) :
#   - palier = prerequis "0N_TierUnlockN" de la techno (0 si aucun) ;
#   - parent = l'unique autre prerequis "techno X" (le jeu n'en a jamais deux) ;
#   - rang = ordre dans l'arbre DANS LE SCENARIO, technos de depart retirees ;
#     dans un arbre, un parent passe avant ses enfants, puis par palier ;
#   - technos de depart (reglage du scenario) : ni check ni item, exclues ;
#   - technos desactivees : exclues ; technos cachees derriere un deblocage
#     de l'histoire : checks normaux, la condition est notee.
import collections
import os
import re
import sys

RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JOURNAL = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RACINE, "logs", "console.log")
ARBRES = os.path.join(RACINE, "outils", "technos_arbres.tsv")
SORTIE_TSV = os.path.join(RACINE, "outils", "technos_table.tsv")
SORTIE_H = os.path.join(RACINE, "known_techs.h")
DOC = os.path.join(RACINE, ".claude", "chantiers", "technos.md")
DEBUT_DOC = "<!-- TABLE TECHNOS (generee par outils/technos_table.py) -->"
FIN_DOC = "<!-- FIN TABLE TECHNOS -->"

# Scenario AP -> (nom affiche, nom du reglage de technos du scenario).
SCENARIOS = [
    ("DEFAULT", "A New Home"),
    ("TechHaven", "The Arks"),
    ("REFUGEES", "Refugees"),
    ("WINTERHOME", "Fall of Winterhome"),
    ("BUILDERS", "Last Autumn"),
    ("Reconquest", "On the Edge"),
]
# Technos de l'ensemble jamais visibles dans ce scenario (prerequis
# d'affichage positif sur un deblocage d'un autre scenario).
EXCLUES = {
    ("DEFAULT", "E 5_ExpeditionUpgradeWinterhome"),   # visible seulement avec [Winterhome] EvacuationUpgradeDescription
}
# Premier lancement seulement (technos_arbres.tsv absent) : arbres a creer ou
# a corriger par rapport a l'ancien known_techs.h.
ARBRES_FORCES = {
    "E X_Storm_research": 80,               # absente de l'ancien known_techs.h
    "[Builders] RU 5_URS_Advanced": 81,     # branche de URS_Steam, comme URS_Faster gathering
    "E 5_Automaton Engineers": 79,          # 79 ou 15 selon les anciens groupes (15 = Gathering)
    "E 5_ExpeditionUpgradeWinterhome": 8,
}


def lire_journal():
    T, REQ, VOIR, SETS, REG, SCN = {}, {}, {}, collections.defaultdict(list), {}, {}
    for l in open(JOURNAL, encoding="utf-8", errors="replace"):
        if "[TECHNO]" not in l:
            continue
        p = [x.strip() for x in l.split("[TECHNO]", 1)[1].rstrip("\n").split("|")]
        # T | hash | nom | onglet | colonne | ligne | type | categorie d'affichage | active
        if p[0] == "T" and len(p) >= 9:
            T[p[1]] = dict(nom=p[2], onglet=p[3], col=int(p[4]), ligne=int(p[5]), type=p[6], active=p[8] == "1")
        elif p[0] == "REQ":
            REQ[p[1]] = p[2]
        elif p[0] == "VOIR":
            VOIR[p[1]] = p[2]
        elif p[0] == "SETT":
            if p[2] not in SETS[p[1]]:
                SETS[p[1]].append(p[2])
        elif p[0] == "REG":
            REG[p[1]] = (p[2], [] if p[3] in ("(aucune)", "") else [x.strip() for x in p[3].split(", ")])
        elif p[0] == "SCN":
            SCN[p[1]] = [x.strip() for x in p[2].split(", ")] if p[2] != "-" else []
    if not T:
        sys.exit(f"aucune ligne [TECHNO] dans {JOURNAL} : faire F4 en partie")
    return T, REQ, VOIR, SETS, REG, SCN


def segment(interne):
    # Comme InternalToSeg (tech_state.cpp) : nom court affiche par le mod.
    p = re.sub(r"^\[[^\]]*\]\s*", "", interne)
    if "_" in p:
        return p.split("_", 1)[1]
    m = re.match(r"^[A-Za-z]+ (\d+ )?(.*)$", p)
    return m.group(2) if m else p


def condition(voir):
    # Prerequis d'affichage utiles : deblocages positifs, hors mode Endless.
    if voir == "(aucun)" or "IsEndless" in voir or voir.startswith("op2(") or voir.startswith("op1(op2("):
        return ""
    return " ; ".join(x for x in voir.split(" ; ") if x.startswith("unlock ") and "LockTechnology" not in x)


def amorcer_arbres(T):
    # Ancien known_techs.h -> hash -> arbre (premier groupe rencontre).
    src = open(SORTIE_H, encoding="utf-8").read()
    arbres, noms = {}, {}
    for h, n, a in re.findall(r"0x([0-9A-F]{16})ULL,\s*\"([^\"]*)\",\s*(\d+)", src):
        arbres.setdefault(h, int(a))
        noms.setdefault(h, n)
    par_nom = {v["nom"]: h for h, v in T.items()}
    for nom, a in ARBRES_FORCES.items():
        if nom in par_nom:
            arbres[par_nom[nom]] = a
    with open(ARBRES, "w", encoding="utf-8") as f:
        f.write("# hash\tarbre\tnom court (affiche par le mod)\tnom interne -- source de verite des arbres\n")
        for h in sorted(arbres, key=lambda h: (arbres[h], T.get(h, {}).get("ligne", 0))):
            f.write(f"{h}\t{arbres[h]}\t{noms.get(h) or segment(T[h]['nom'])}\t{T.get(h, {}).get('nom', '?')}\n")
    print(f"{ARBRES} cree ({len(arbres)} technos)")


def lire_arbres():
    arbres, noms = {}, {}
    for l in open(ARBRES, encoding="utf-8"):
        if l.startswith("#") or not l.strip():
            continue
        c = l.rstrip("\n").split("\t")
        arbres[c[0]] = int(c[1])
        noms[c[0]] = c[2]
    return arbres, noms


def main():
    T, REQ, VOIR, SETS, REG, SCN = lire_journal()
    par_nom = {v["nom"]: h for h, v in T.items()}
    if not os.path.exists(ARBRES):
        amorcer_arbres(T)
    arbres, noms = lire_arbres()

    def palier(h):
        t = [int(x) for x in re.findall(r"techno 0(\d)_TierUnlock", REQ.get(h, ""))]
        return max(t) if t else 0

    def parent(h):
        # Pour une TierUnlockN, le parent est la TierUnlock precedente.
        tier = re.match(r"0(\d)_TierUnlock", T[h]["nom"]) is not None
        ps = [x[7:] for x in REQ.get(h, "").split(" ; ") if x.startswith("techno ") and (tier or "TierUnlock" not in x)]
        if len(ps) > 1:
            print(f"ATTENTION : {T[h]['nom']} a {len(ps)} parents : {ps}")
        return par_nom.get(ps[0]) if ps else None

    # TierUnlockN : chaine 1 -> 5, palier = N - 1 (parent seulement).
    def palier_reel(h):
        m = re.match(r"0(\d)_TierUnlock", T[h]["nom"])
        return 0 if m else palier(h)

    lignes, erreurs = [], []
    for scn, affiche in SCENARIOS:
        reglages = SCN.get(scn, [])
        if not reglages:
            erreurs.append(f"{scn} : aucun reglage de technos dans le releve")
            continue
        ensemble, depart_noms = REG[reglages[0]]
        depart = {par_nom[n] for n in depart_noms if n in par_nom}
        technos = [h for h in SETS[ensemble] if T[h]["active"] and (scn, T[h]["nom"]) not in EXCLUES and h not in depart]
        sans_arbre = [T[h]["nom"] for h in technos if h not in arbres]
        if sans_arbre:
            erreurs.append(f"{scn} : sans arbre dans technos_arbres.tsv : {sans_arbre}")
            continue
        par_arbre = collections.defaultdict(list)
        for h in technos:
            par_arbre[arbres[h]].append(h)
        for a, membres in sorted(par_arbre.items()):
            # Ordre : parent avant enfant (dans l'arbre), puis palier, puis ligne/colonne.
            restants, ordre = list(membres), []
            while restants:
                prets = [h for h in restants if parent(h) not in restants]
                prets.sort(key=lambda h: (palier_reel(h), T[h]["ligne"], T[h]["col"], T[h]["nom"]))
                ordre.append(prets[0])
                restants.remove(prets[0])
            for k, h in enumerate(ordre):
                p = parent(h)
                if k > 0 and p in membres and p != ordre[k - 1]:
                    erreurs.append(f"{scn} arbre {a} : {T[h]['nom']} a pour parent {T[p]['nom']}, pas le rang precedent {T[ordre[k-1]]['nom']} (branche a separer)")
                lignes.append(dict(scn=scn, affiche=affiche, h=h, nom=T[h]["nom"], court=noms.get(h) or segment(T[h]["nom"]),
                                   onglet=T[h]["onglet"], arbre=a, rang=k, palier=palier_reel(h), parent=p or "",
                                   parent_nom=T[p]["nom"] if p else "", parent_depart=1 if p in depart else 0,
                                   condition=condition(VOIR.get(h, "(aucun)"))))
        lignes.append(dict(scn=scn, depart=[T[h]["nom"] for h in sorted(depart, key=lambda h: T[h]["nom"])]))
    if erreurs:
        print("\n".join("ERREUR " + e for e in erreurs))
        sys.exit(1)
    ecrire_tsv(lignes)
    ecrire_h(lignes)
    ecrire_doc(lignes)
    for scn, _ in SCENARIOS:
        n = sum(1 for l in lignes if l.get("scn") == scn and "h" in l)
        print(f"{scn:<11} {n:3d} technos")


def ecrire_tsv(lignes):
    with open(SORTIE_TSV, "w", encoding="utf-8") as f:
        f.write("scenario\thash\tnom interne\tnom court\tonglet\tarbre\trang\tpalier\tparent (hash)\tparent (nom)\tparent de depart\tcondition d'affichage\n")
        for l in lignes:
            if "h" in l:
                f.write(f"{l['scn']}\t{l['h']}\t{l['nom']}\t{l['court']}\t{l['onglet']}\t{l['arbre']}\t{l['rang']}\t{l['palier']}\t{l['parent']}\t{l['parent_nom']}\t{l['parent_depart']}\t{l['condition']}\n")
        for l in lignes:
            if "depart" in l:
                f.write(f"# depart {l['scn']} : {', '.join(l['depart']) or '(aucune)'}\n")


def ecrire_h(lignes):
    out = []
    out.append("// known_techs.h  --  Table des technologies de Frostpunk\n")
    out.append("// ---------------------------------------------------------------------------\n")
    out.append("// GENERE par outils\\technos_table.py depuis le releve F4 ([TECHNO]) et\n")
    out.append("// outils\\technos_arbres.tsv (numeros d'arbres). Ne pas editer a la main.\n")
    out.append("// Un groupe par scenario AP (label = nom interne du scenario, cf.\n")
    out.append("// ScenarioUsesGroup). Technos de depart et desactivees absentes.\n")
    out.append("// hash   = 8 premiers octets du GUID (techno +0x20), identifiant stable\n")
    out.append("// name   = nom court affiche par le mod (AP_NomDuCheck, TEMPORAIRE)\n")
    out.append("// progressiveTree / progressiveTreePosition = arbre et rang dans le scenario\n")
    out.append("// tier   = palier (prerequis TierUnlockN), 0 si aucun\n")
    out.append("// parent = hash de la techno exigee par le jeu (0 si aucune)\n")
    out.append("// ---------------------------------------------------------------------------\n\n")
    out.append("#pragma once\n#include <cstdint>\n#include <cstring>\n\n")
    out.append("struct TechInfo {\n    uint64_t    hash;         // identifiant STABLE -- cle de recherche\n    const char* name;\n")
    out.append("    int progressiveTree;\n    int progressiveTreePosition;\n    int tier;\n    uint64_t parent;\n};\n\n")
    out.append("struct TechGroup { const TechInfo* items; int count; const char* label; };\n\n")
    for scn, affiche in SCENARIOS:
        out.append(f"// {affiche}\nstatic const TechInfo KNOWN_TECHS_{scn.upper()}[] = {{\n")
        for l in sorted((l for l in lignes if l.get("scn") == scn and "h" in l), key=lambda l: (l["arbre"], l["rang"])):
            nom = '"' + l["court"].replace('"', '\\"') + '",'
            par = f"0x{l['parent']}ULL" if l["parent"] else "0"
            out.append(f"{{ 0x{l['h']}ULL, {nom:<42} {l['arbre']:2d}, {l['rang']}, {l['palier']}, {par} }},\n")
        out.append("};\n\n")
    out.append("#define TECH_GROUP(arr, lbl) { arr, (int)(sizeof(arr) / sizeof((arr)[0])), lbl }\n\n")
    out.append("static const TechGroup KNOWN_TECH_GROUPS[] = {\n")
    for scn, _ in SCENARIOS:
        out.append(f"    TECH_GROUP(KNOWN_TECHS_{scn.upper()}, \"{scn}\"),\n")
    out.append("};\n\nstatic const int KNOWN_TECH_GROUP_COUNT =\n(int)(sizeof(KNOWN_TECH_GROUPS) / sizeof(KNOWN_TECH_GROUPS[0]));\n\n")
    out.append("static int KnownTechsTotal()\n{\n    int n = 0;\n    for (int g = 0; g < KNOWN_TECH_GROUP_COUNT; ++g) n += KNOWN_TECH_GROUPS[g].count;\n    return n;\n}\n\n")
    out.append("// Recherche par hash dans TOUS les groupes (premier trouve : le nom et le\n// palier ne dependent pas du scenario).\n")
    out.append("static const TechInfo* TechByHash(uint64_t hash)\n{\n    for (int g = 0; g < KNOWN_TECH_GROUP_COUNT; ++g) {\n")
    out.append("        const TechGroup& grp = KNOWN_TECH_GROUPS[g];\n        for (int i = 0; i < grp.count; ++i)\n")
    out.append("            if (grp.items[i].hash == hash) return &grp.items[i];\n    }\n    return nullptr;\n}\n")
    with open(SORTIE_H, "w", encoding="utf-8") as f:
        f.write("".join(out))


def ecrire_doc(lignes):
    md = [DEBUT_DOC, ""]
    md.append("Table générée par `outils\\technos_table.py` (relevé F4). Palier = TierUnlock exigé ; "
              "parent = techno exigée par le jeu (*italique* = techno de départ, déjà acquise) ; "
              "rang = ordre de l'item progressif dans l'arbre, dans ce scénario.")
    md.append("")
    for scn, affiche in SCENARIOS:
        sel = sorted((l for l in lignes if l.get("scn") == scn and "h" in l), key=lambda l: (l["arbre"], l["rang"]))
        dep = next(l["depart"] for l in lignes if l.get("scn") == scn and "depart" in l)
        md.append(f"### {affiche} (`{scn}`) — {len(sel)} technos")
        md.append("")
        md.append(f"Technos de départ (exclues) : {', '.join(dep) if dep else 'aucune'}.")
        md.append("")
        md.append("| Arbre | Rang | Techno | Palier | Parent | Onglet | Condition d'affichage | Hash |")
        md.append("|---:|---:|---|---:|---|---|---|---|")
        for l in sel:
            par = l["parent_nom"]
            if par and l["parent_depart"]:
                par = f"*{par}*"
            md.append(f"| {l['arbre']} | {l['rang']} | {l['nom']} | {l['palier']} | {par} | {l['onglet']} | {l['condition']} | `{l['h']}` |")
        md.append("")
    md.append(FIN_DOC)
    texte = open(DOC, encoding="utf-8").read()
    bloc = "\n".join(md)
    if DEBUT_DOC in texte:
        texte = texte[:texte.index(DEBUT_DOC)] + bloc + texte[texte.index(FIN_DOC) + len(FIN_DOC):]
    else:
        texte = texte.rstrip("\n") + "\n\n## Table par scénario\n\n" + bloc + "\n"
    with open(DOC, "w", encoding="utf-8") as f:
        f.write(texte)


if __name__ == "__main__":
    main()
