# Classification Archipelago des items "<Scenario> - Progressive Tree <arbre>"
# (progression / useful = "important" cote mod), depuis outils\technos_table.tsv.
#   python outils\technos_classification.py
# Ecrit, depuis la racine du projet :
#   outils\technos_classification.tsv   table pour l'apworld
#   docs\chantiers\technos.md           tableau lisible (entre les marqueurs)
#
# Regle (decisions de l'auteur, 2026-11-03) :
#   - un arbre est "progression" si une de ses technos est le PARENT d'une
#     techno d'un AUTRE arbre du meme scenario (d'autres arbres dependent de
#     lui) ; sinon "useful" (ex. : Coal Mine progression, Coal mine upgrade 1
#     useful) ;
#   - par scenario : un parent qui est une techno de depart ne compte pas
#     (il n'est pas dans la table) ;
#   - la condition d'affichage (deblocage par l'histoire) ne compte pas ;
#   - forces en progression : Tier (arbre 36, paliers) et l'arbre des
#     eclaireurs (Beacon = 7, Forager Quarters = 54 dans Last Autumn),
#     exiges par la logique de l'apworld.
import collections
import os

RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLE = os.path.join(RACINE, "outils", "technos_table.tsv")
SORTIE_TSV = os.path.join(RACINE, "outils", "technos_classification.tsv")
DOC = os.path.join(RACINE, "docs", "chantiers", "technos.md")
DEBUT_DOC = "<!-- CLASSIFICATION ARBRES (generee par outils/technos_classification.py) -->"
FIN_DOC = "<!-- FIN CLASSIFICATION ARBRES -->"

# Memes noms et memes constantes que apworld\outils\generer.py.
SCENARIOS = [
    ("DEFAULT", "A New Home"),
    ("TechHaven", "The Arks"),
    ("REFUGEES", "Refugees"),
    ("WINTERHOME", "Fall of Winterhome"),
    ("BUILDERS", "The Last Autumn"),
    ("Reconquest", "On the Edge"),
]
ARBRE_PALIERS = 36
ARBRE_ECLAIREURS = {"DEFAULT": 7, "TechHaven": 7, "REFUGEES": 7, "BUILDERS": 54}


def lire_table():
    lignes = [l.rstrip("\n").split("\t") for l in open(TABLE, encoding="utf-8") if not l.startswith("#")]
    entete = lignes[0]
    return [dict(zip(entete, c)) for c in lignes[1:] if len(c) == len(entete)]


def classer(technos):
    resultat = []
    for scn, affiche in SCENARIOS:
        ts = [t for t in technos if t["scenario"] == scn]
        arbre_de = {t["hash"]: int(t["arbre"]) for t in ts}
        arbres = collections.defaultdict(list)
        for t in ts:
            arbres[int(t["arbre"])].append(t)
        for a in sorted(arbres):
            membres = sorted(arbres[a], key=lambda t: int(t["rang"]))
            nom = "Tier" if a == ARBRE_PALIERS else membres[0]["nom court"]
            enfants = sorted({int(t["arbre"]) for t in ts
                              if int(t["arbre"]) != a and arbre_de.get(t["parent (hash)"]) == a})
            parents = sorted({arbre_de[t["parent (hash)"]] for t in membres
                              if t["parent (hash)"] in arbre_de and arbre_de[t["parent (hash)"]] != a})
            if a == ARBRE_PALIERS:
                classe, raison = "progression", "force : paliers"
            elif ARBRE_ECLAIREURS.get(scn) == a:
                classe, raison = "progression", "force : eclaireurs"
            elif enfants:
                classe, raison = "progression", "parent d'un autre arbre"
            else:
                classe, raison = "useful", "aucun arbre ne depend de lui"
            resultat.append(dict(scn=scn, affiche=affiche, arbre=a, nom=nom,
                                 item="%s - Progressive Tree %s" % (affiche, nom), nombre=len(membres),
                                 classe=classe, raison=raison, enfants=enfants, parents=parents,
                                 noms={x: ("Tier" if x == ARBRE_PALIERS else min(arbres[x], key=lambda t: int(t["rang"]))["nom court"])
                                       for x in enfants + parents}))
    return resultat


def ecrire_tsv(res):
    with open(SORTIE_TSV, "w", encoding="utf-8") as f:
        f.write("scenario\tarbre\titem\tnombre\tclassification\traison\tarbres enfants\tarbres parents\n")
        for r in res:
            f.write("%s\t%d\t%s\t%d\t%s\t%s\t%s\t%s\n" % (
                r["scn"], r["arbre"], r["item"], r["nombre"], r["classe"], r["raison"],
                ", ".join(str(x) for x in r["enfants"]), ", ".join(str(x) for x in r["parents"])))


def ecrire_doc(res):
    md = [DEBUT_DOC, ""]
    md.append("Table générée par `outils\\technos_classification.py` depuis `technos_table.tsv`. "
              "**progression** = un autre arbre du scénario dépend de lui (parent d'une de ses technos), "
              "ou forcé (Tier, éclaireurs) ; **useful** = « important » côté mod. "
              "Parent de départ et condition d'affichage ne comptent pas.")
    md.append("")
    for scn, affiche in SCENARIOS:
        sel = [r for r in res if r["scn"] == scn]
        n = sum(1 for r in sel if r["classe"] == "progression")
        md.append("### %s (`%s`) — %d arbres : %d progression, %d useful" % (affiche, scn, len(sel), n, len(sel) - n))
        md.append("")
        md.append("| Arbre | Item | Technos | Classification | Raison | Arbres enfants | Arbres parents |")
        md.append("|---:|---|---:|---|---|---|---|")
        for r in sel:
            enf = ", ".join("%d %s" % (x, r["noms"][x]) for x in r["enfants"])
            par = ", ".join("%d %s" % (x, r["noms"][x]) for x in r["parents"])
            cl = "**progression**" if r["classe"] == "progression" else "useful"
            md.append("| %d | %s | %d | %s | %s | %s | %s |" % (r["arbre"], r["nom"], r["nombre"], cl, r["raison"], enf, par))
        md.append("")
    md.append(FIN_DOC)
    texte = open(DOC, encoding="utf-8").read()
    bloc = "\n".join(md)
    if DEBUT_DOC in texte:
        texte = texte[:texte.index(DEBUT_DOC)] + bloc + texte[texte.index(FIN_DOC) + len(FIN_DOC):]
    else:
        texte = texte.rstrip("\n") + "\n\n## Classification Archipelago des arbres\n\n" + bloc + "\n"
    with open(DOC, "w", encoding="utf-8") as f:
        f.write(texte)


def main():
    res = classer(lire_table())
    ecrire_tsv(res)
    ecrire_doc(res)
    for scn, _ in SCENARIOS:
        sel = [r for r in res if r["scn"] == scn]
        n = sum(1 for r in sel if r["classe"] == "progression")
        print("%-11s %2d arbres : %2d progression, %2d useful" % (scn, len(sel), n, len(sel) - n))


if __name__ == "__main__":
    main()
