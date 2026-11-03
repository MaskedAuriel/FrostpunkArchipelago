# -*- coding: utf-8 -*-
"""Contenu reel des sites d'exploration -> outils/sites_contenu.tsv.

Source : lignes "[SCONT]" du releve F9 (sites_contenu.cpp), une partie neuve
par scenario. Un nouveau releve d'un scenario remplace l'ancien.

Usage : python outils/sites_contenu.py [journal]   (defaut : logs/console.log)

Colonnes : scenario | site (cle de SITE_PAYLOADS) | source | ressources |
survivants | technologies | detail
  source : "direct" (contenu du site, ce que l'expedition ramene),
           "bonus" (BonusResources du site),
           "dilemme <nom> choix N" (effet d'un choix du dilemme d'exploration).
  ressources : noms mod (resources.cpp, KNOWN), "nom:quantite" separes par ", "
  survivants : workers / engineers / children / automaton, "type:nombre"
  technologies : nombre
  detail : texte du choix (dilemmes)
Les quantites sont celles d'UNE partie : elles dependent probablement de la
difficulte et sont tirees au hasard (avis de l'auteur du 2026-11-03) ; seul
le type de contenu est fiable.
"""
import os
import sys
from collections import OrderedDict

ICI = os.path.dirname(os.path.abspath(__file__))
RACINE = os.path.dirname(ICI)

# Nom interne du jeu (definition +0x00) -> nom mod (resources.cpp, KNOWN).
# Last Autumn verifie par GUID le 2026-11-03 ([SRES]).
RESSOURCES = {
    "Coal": "coal", "Wood": "wood", "Steel": "steel", "Steam Cores": "steamcore",
    "Raw Food": "rawfood", "Food Rations": "ration", "Bodies": "corpse",
    "Steel Plates": "steelcomposite", "Wooden Materials": "structuralprofiles",
    "Steam Tools": "steamexchanger", "Augmentation parts": "prosthesis",
}
SURVIVANTS = {"Workers": "workers", "Specialists": "engineers", "Children": "children",
              "Automaton": "automaton"}


def cumul(d, k, n):
    d[k] = d.get(k, 0) + n


def texte(d):
    return ", ".join("%s:%d" % (k, v) for k, v in d.items()) if d else "-"


def lire(journal):
    scenarios = OrderedDict()   # scenario -> OrderedDict(site -> donnees)
    courant = None
    with open(journal, encoding="utf-8", errors="replace") as f:
        for ligne in f:
            ligne = ligne.rstrip("\r\n")
            if ligne.startswith("[SITES] scenario "):
                courant = ligne[len("[SITES] scenario "):].split(" :")[0]
                scenarios[courant] = OrderedDict()
                continue
            if not ligne.startswith("[SCONT]\t"):
                continue
            c = ligne.split("\t")
            genre, scn, site = c[1], c[2], c[3]
            if scn not in scenarios:
                scenarios[scn] = OrderedDict()
            sites = scenarios[scn]
            if genre == "S":
                sites[site] = {"sources": OrderedDict(), "choix": {}}
                continue
            s = sites.get(site)
            if s is None:
                continue

            def src(nom):
                return s["sources"].setdefault(nom, {"res": OrderedDict(), "surv": OrderedDict(), "tech": None})

            if genre == "R":
                source, nom, q = c[4], c[5], c[6]
                if q == "?":
                    continue                       # declaration aleatoire du gabarit
                cle = RESSOURCES.get(nom, nom)
                if source == "composant":
                    cumul(src("direct")["res"], cle, int(q))
                elif source == "composant bonus":
                    cumul(src("bonus")["res"], cle, int(q))
                elif source.startswith("dilemme "):
                    cumul(src(source.split(" / ")[0])["res"], cle, int(q))
            elif genre == "P" and c[4] == "composant":
                cumul(src("direct")["surv"], SURVIVANTS.get(c[5], c[5]), int(c[6]))
            elif genre == "X" and c[4] == "composant" and c[5] == "technologies":
                src("direct")["tech"] = c[6]
            elif genre == "C":
                s["choix"]["dilemme %s choix %s" % (c[4], c[5])] = c[6]
            # GiveRandomTechnologyToStationingExpeditionEffect (4 sites A New
            # Home, "Take technology") : ignore, ces sites ne donnent rien en
            # jeu (constat de l'auteur du 2026-11-03).
    return scenarios


def anciennes_lignes(sortie, remplaces):
    """Lignes de la table existante pour les scenarios absents du journal
    (cumulatif : un scenario releve remplace ses lignes)."""
    gardees = OrderedDict()
    if not os.path.exists(sortie):
        return gardees
    with open(sortie, encoding="utf-8") as f:
        next(f, None)
        for ligne in f:
            scn = ligne.split("\t", 1)[0]
            if scn and scn not in remplaces:
                gardees.setdefault(scn, []).append(ligne)
    return gardees


def main():
    journal = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RACINE, "logs", "console.log")
    scenarios = lire(journal)
    sortie = os.path.join(ICI, "sites_contenu.tsv")
    gardees = anciennes_lignes(sortie, set(scenarios))
    n = 0
    with open(sortie, "w", encoding="utf-8", newline="\n") as f:
        f.write("scenario\tsite\tsource\tressources\tsurvivants\ttechnologies\tdetail\n")
        for scn, lignes in gardees.items():
            f.writelines(lignes)
            n += len(lignes)
            print("  %-11s garde de la table precedente (%d lignes)" % (scn, len(lignes)))
        for scn, sites in scenarios.items():
            for site, s in sites.items():
                sources = s["sources"]
                if "direct" not in sources:
                    sources["direct"] = {"res": OrderedDict(), "surv": OrderedDict(), "tech": None}
                    sources.move_to_end("direct", last=False)
                for source, d in sources.items():
                    f.write("\t".join([scn, site, source, texte(d["res"]), texte(d["surv"]),
                                       d["tech"] or "-", s["choix"].get(source, "-")]) + "\n")
                    n += 1
    print("%d ligne(s), %d scenario(s) -> %s" % (n, len(scenarios), sortie))
    for scn, sites in scenarios.items():
        avec = sum(1 for s in sites.values() if s["sources"]["direct"]["surv"])
        noyaux = sum(1 for s in sites.values() if "steamcore" in s["sources"]["direct"]["res"])
        print("  %-11s %3d sites, %2d avec survivants, %2d avec noyaux" % (scn, len(sites), avec, noyaux))


if __name__ == "__main__":
    main()
