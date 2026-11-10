# generer.py -- Genere les donnees de l'apworld Frostpunk et la table des ids
# pour le mod.
#
# Sources (dossier du mod) :
#   outils\technos_table.tsv      technos par scenario (arbre, rang, palier...)
#   outils\lois_table.tsv         checks de lois par scenario (un par rang)
#   outils\sites_apparition.tsv   sites sur la carte et region d'apparition
#   outils\sites_contenu.tsv      contenu vanilla des sites (survivants compris)
#   outils\technos_classification.tsv  arbres progression / useful
#   known_quests.h                quetes (nom interne -> nom affiche)
#
# Sorties :
#   apworld\frostpunk\data.py     tables lues par l'apworld (NE PAS EDITER)
#   apworld\ids.tsv               ids attribues : STABLES A VIE, on n'y
#                                 retire jamais rien ; une cle nouvelle prend
#                                 le prochain id libre de sa plage
#   known_ap_ids.h                table des ids pour le mod (NE PAS EDITER)
#
# Les decisions de logique sont dans docs\chantiers\apworld.md (journal) ;
# celles qui ne sont dans aucune table sont codees ICI, en tete de fichier.
#
# Usage : python apworld\outils\generer.py

import csv
import os
import re
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
APWORLD = os.path.dirname(ICI)
MOD = os.path.dirname(APWORLD)

# ---------------------------------------------------------------------------
# Decisions de l'auteur (journal de docs\chantiers\apworld.md)
# ---------------------------------------------------------------------------

# Ordre fixe : il sert aussi d'index de scenario dans les ids.
SCENARIOS = [
    ("DEFAULT",    "A New Home"),
    ("TechHaven",  "The Arks"),
    ("REFUGEES",   "The Refugees"),
    ("WINTERHOME", "Fall of Winterhome"),
    ("BUILDERS",   "The Last Autumn"),
    ("Reconquest", "On the Edge"),
]

# Goulots, dans l'ordre (nom interne de la quete). Region 0 = Start ; le
# goulot k fini ouvre la region k ; la victoire suit le dernier.
GOULOTS = {
    "DEFAULT":    ["[Main Narrative Line] 004 FIND OTHER CITIES",
                   "[Main Narrative Line] 005 LONDONERS",
                   "[Main Narrative Line] 006 REFUGEES"],
    "TechHaven":  ["[TechHeaven] 001 FIND FACTORY PLANS",
                   "[TechHeaven] 004 PREPARE FOR THE STORM",
                   "[TechHeaven] 008 NEW MANCHESTER - COAL"],
    "REFUGEES":   ["[Refugees] 001 SAVE YOUR PEOPLE",
                   "[Refugees] 003 FACE THE LORDS",
                   "[Refugees] 004a NORMALIZE THE SITUATION"],
    "WINTERHOME": ["[Winterhome] 001 RESTORE HOPE",
                   "[Winterhome] 002 BROKEN GENERATOR",
                   "[Winterhome] 003 PREPARE EVACUATION",
                   "[Winterhome] 004A FINAL HOURS"],
    "BUILDERS":   ["[Builders] 001 BASIC FACILITIES EXTENDED",
                   "[Builders] 003 GENERATOR CONSTRUCTION SITE",
                   "[Builders] 006 GENERATOR FINALIZATION"],
    "Reconquest": ["[1] Send first batch",
                   "[3] Food settlement",
                   "[7] Make Decision"],
}

# Quetes d'histoire qui sont des checks : nom interne -> region.
# Une liste = quetes paralleles, UN SEUL check (le premier nom est la cle).
# Valide par l'auteur le 2026-11-03 (sauf les LondonerArc, retirees).
QUETES = {
    "DEFAULT": [
        ("[Main Narrative Line] 001 ACTIVATE GENERATOR", 0),
        ("[Main Narrative Line] 002 MEANS FOR SURVIVAL", 0),
        ("[Main Narrative Line] 003 RESCUE SURVIVORS", 0),
        ("[Main Narrative Line] 004 FIND OTHER CITIES", 0),
        ("[Main Narrative Line] 005 LONDONERS", 1),
        ("[Main Narrative Line] 006 REFUGEES", 2),
        ("[ENDGAME] 001_Research tech", 3),
        ("[ENDGAME] 002 Prepare for the storm - main quest", 3),
        ("[ENDGAME] 002a_Prepare for the storm FOOD", 3),
        ("[ENDGAME] 002b_Prepare for the storm SURVIVORS", 3),
        ("[ENDGAME] 002c_Prepare for the storm OUTPOSTS", 3),
        ("[ENDGAME] 002d_Prepare for the storm TEMPERATURE", 3),
        ("[ENDGAME] 003_Final preparation", 3),
        ("[ENDGAME] Gather wood", 3),
        ("[ENDGAME] Build Coal Thumpers", 3),
    ],
    "TechHaven": [
        ("[TechHeaven] 001 FIND FACTORY PLANS", 0),
        ("[TechHeaven] 002 FACTORY", 1),
        ("[TechHeaven] 003 AUTONOMOUS CITY", 1),
        ("[TechHeaven] 003 EXPLORE THE FROSTLAND", 1),
        ("[TechHeaven] 004 PREPARE FOR THE STORM", 1),
        ("[TechHeaven] 005 NEW MANCHESTER - FINDING THE WAY", 2),
        ("[TechHeaven] 006 NEW MANCHESTER - SHELTER", 2),
        ("[TechHeaven] 007 NEW MANCHESTER - FOOD", 2),
        ("[TechHeaven] 008 NEW MANCHESTER - COAL", 2),
        ("[TechHeaven] 009 AUTONOMOUS CITY READY", 2),
    ],
    "REFUGEES": [
        ("[Refugees] 001 SAVE YOUR PEOPLE", 0),
        ("[Refugees] 003 FACE THE LORDS", 1),
        ("[Refugees] 003a FEED THE LORDS - all", 1),
        ("[Refugees] 003a FEED THE LORDS - infrastructure", 1),
        (["[Refugees] 004a NORMALIZE THE SITUATION",
          "[Refugees] 004b BUILD THE WALL"], 2),
    ],
    "WINTERHOME": [
        ("[Winterhome] 001 RESTORE HOPE", 0),
        ("[Winterhome] 002 BROKEN GENERATOR", 1),
        ("[Winterhome] 003 PREPARE EVACUATION", 2),
        ("[Winterhome] 004A FINAL HOURS", 3),
    ],
    "BUILDERS": [
        ("[Builders] 001 BASIC FACILITIES EXTENDED", 0),
        ("[Builders] 002 DOCK INFRASTRUCTURE", 0),
        ("[Builders] 003 GENERATOR CONSTRUCTION SITE", 1),
        ("[Builders] 004 GENERATOR FOUNDATIONS", 2),
        ("[Builders] 005 GENERATOR HEART", 2),
        ("[Builders] 006 GENERATOR FINALIZATION", 2),
        ("[Builders] 006a Upgrade the Generator", 3),
    ],
    "Reconquest": [
        (["[0] Establish Outpost", "[0] Establish Outpost [timeout]"], 0),
        (["[1] Send first batch", "[1] Send first batch [timeout]"], 0),
        ("[2] Send recurent batch", 1),
        ("[3] Food settlement", 1),
        ("[TradeRoutes] Ark Springs", 2),
        ("[TradeRoutes] Children's Mine", 2),
        ("[TradeRoutes] Creve-Neige", 2),
        ("[7] Make Decision", 2),
        (["[8] Helping NL", "[8] Taking NL population"], 3),
    ],
}

# Refugies de l'apres-005 (A New Home, decision de l'auteur du 2026-11-05) :
# chaque groupe qui arrive a la porte ("[Refugees] Refugees at the door k")
# envoie un check, et ses gens deviennent un item du pool (livre par
# convoi). Region : celle de 006 REFUGEES (apres 005 LONDONERS).
# Effectifs releves en jeu le 2026-11-07 (journal [GRP] groupe du jeu :
# StoryModeRefugees1/2/3 ; classes : 35F253D1 = ouvriers, AFD04DDA =
# enfants, 61F13397 = specialistes, presume d'apres les autres classes).
REFUGIES = {
    "DEFAULT": {"region": 2, "groupes": [
        [("workers", 27), ("engineers", 21), ("children", 22)],
        [("workers", 42), ("children", 28)],
        [("workers", 38), ("engineers", 22)],
    ]},
}

# Noms des checks de quete trop vagues avec le seul nom affiche du jeu
# ("Food", "Survivors"... : sous-quetes de Prepare For The Storm).
NOMS_QUETES = {
    ("DEFAULT", "[ENDGAME] 002a_Prepare for the storm FOOD"): "Prepare For The Storm - Food",
    ("DEFAULT", "[ENDGAME] 002b_Prepare for the storm SURVIVORS"): "Prepare For The Storm - Survivors",
    ("DEFAULT", "[ENDGAME] 002c_Prepare for the storm OUTPOSTS"): "Prepare For The Storm - Outposts",
    ("DEFAULT", "[ENDGAME] 002d_Prepare for the storm TEMPERATURE"): "Prepare For The Storm - Cold",
    ("DEFAULT", "[ENDGAME] 002 Prepare for the storm - main quest"): "Prepare For The Storm",
    ("TechHaven", "[TechHeaven] 002 FACTORY"): "Factory",
    ("BUILDERS", "[Builders] 001 BASIC FACILITIES EXTENDED"): "Finish Basic Facilities",
    ("BUILDERS", "[Builders] 006a Upgrade the Generator"): "(Optional) Upgrade The Generator",
}


def titre_quete(texte):
    """Majuscule en tete de chaque mot (separateur : espace seulement ;
    str.title() donnait "Children'S")."""
    return " ".join(m[:1].upper() + m[1:].lower() for m in texte.split(" "))


# Conditions d'affichage des technos -> region (Q4.1).
CONDITIONS_TECHNO = {
    ("DEFAULT", "unlock StormResearch"): 3,                          # apres 006 REFUGEES
    ("TechHaven", "unlock [TechHeaven] Quest3"): 1,                  # apres 001 FIND FACTORY PLANS
    ("WINTERHOME", "unlock [Winterhome] Repair Station"): 2,         # apres 002 BROKEN GENERATOR
    ("WINTERHOME", "unlock [Winterhome] Repair Station Upgrades"): 2,
    ("WINTERHOME", "unlock [Winterhome] EvacuationUpgradeDescription"): 3,  # apres 003 PREPARE EVACUATION
    ("BUILDERS", "unlock [Builder] HeatTab"): 3,                     # apres 006 GENERATOR FINALIZATION
    ("BUILDERS", "unlock [Builder] Generator Upgrades"): 3,
    ("Reconquest", "unlock [Reconquest] SteamCores"): 1,             # apres [1] Send first batch
}

# Region d'ouverture des arbres de lois (Q4.2) ; tout autre arbre : Start.
OUVERTURE_LOIS = {
    ("DEFAULT", "Purpose"): 2,      # apres 005 LONDONERS
    ("BUILDERS", "Labour"): 1,      # apres 001 BASIC FACILITIES EXTENDED (arbres "[Labour")
    ("BUILDERS", "Politics"): 2,    # apres 003 GENERATOR CONSTRUCTION SITE
}

# Arbre du Beacon (envoyer des eclaireurs, Q4.3) ; None = techno de depart.
ARBRE_ECLAIREURS = {
    "DEFAULT": 7, "TechHaven": 7, "REFUGEES": 7,
    "WINTERHOME": None, "Reconquest": None,
    "BUILDERS": 54,                 # Forager Quarters
}

ARBRE_PALIERS = 36
PALIERS_DE_DEPART = {"WINTERHOME": 3}

# Montees de temperature par scenario et difficulte (Q8 d), en NIVEAUX
# (decision de l'auteur, 2026-11-03) : une montee de N niveaux = N checks
# "Temperature Rise k" consecutifs et N items (+1 niveau chacun). Calcule
# depuis docs\chantiers\temperature.md (montees de fin de partie exclues ;
# A New Home : +1 de l'acte Storm preparation).
DIFFICULTES = ["easy", "medium", "hard", "very_hard"]
MONTEES = {
    "DEFAULT":    [6, 7, 7, 7],
    "TechHaven":  [2, 5, 0, 0],
    "REFUGEES":   [1, 4, 3, 3],
    "WINTERHOME": [4, 6, 5, 5],
    "BUILDERS":   [0, 0, 0, 0],
    "Reconquest": [9, 9, 9, 9],
}

# Contenu des sites (outils\sites_contenu.tsv, valide le 2026-11-03) : noms
# de ressources du mod (ReceiveResource) et types de survivants.
RESSOURCES_MOD = ("coal", "wood", "steel", "rawfood", "ration", "steamcore",
                  "structuralprofiles", "steelcomposite", "steamexchanger")
SURVIVANTS = ("workers", "engineers", "children", "automaton")

# Regle des goulots (decision de l'auteur, 2026-11-04) : la quete goulot de
# la region k exige d'avoir recu ce pourcentage des items Progressive Tree
# "progression" de la region k (seulement k). Les lois ne comptent plus
# (elles restent progression), les autres quetes n'ont plus d'exigence.
# Avant : 80 %, technos et lois, toutes les quetes de la region.
QUETE_POURCENT_PROGRESSIFS = 100

# Filler de complement (places restantes) et pieges (Q6).
# Nom affiche de chaque arbre de technos (decision de l'auteur, 2026-11-10 :
# table ecrite a la main, plus le nom interne de la racine type "Coal mine
# upgrade 1" / "GU GeneratorUpgrade1"). Cle = numero d'arbre
# (outils\technos_arbres.tsv) ; les ids ne dependent pas du nom. Texte en
# anglais, sans numero. Un arbre absent = erreur de generation.
NOMS_ARBRES = {
    0: "Steam Hub", 1: "Generator Power", 2: "Generator Coal Efficiency",
    3: "Generator Range", 4: "Generator Overdrive", 5: "Boilers",
    6: "Boiler Coal Efficiency", 7: "Beacon", 8: "Expedition Center",
    9: "Scout Limit", 10: "Scout Speed", 11: "Factory", 12: "Augmentation Parts",
    13: "Automaton Efficiency", 14: "Automaton Medical", 15: "Gathering Upgrade",
    16: "Resource Depot", 17: "Gathering Efficiency", 18: "Coal Thumper",
    19: "Coal Mine", 20: "Coal Mine Upgrade", 21: "Charcoal Kiln", 22: "Sawmill",
    23: "Sawmill Upgrade", 24: "Wall Drill", 25: "Steel Mine", 26: "Hothouse",
    27: "Hothouse Insulation", 28: "Hunters", 29: "Hunters Workforce",
    30: "Healthcare", 31: "Healthcare Heating", 32: "Infirmary Workforce",
    33: "Housing", 34: "Cookhouse Heating", 35: "Cheaper Houses", 36: "Tier",
    37: "Repair Station", 38: "Repair Station Ability", 39: "Repair Station Workforce",
    40: "Brazier", 41: "Brazier Range", 42: "Brazier Coal Efficiency", 43: "Heater",
    44: "Healthcare Insulation", 45: "Scaffolding Manufacture", 46: "Protective Structure",
    47: "Steel Plate Forge", 48: "Cheaper Forge", 49: "Steam Mill", 50: "Supercharge",
    51: "Air Scrubbers", 52: "Air Scrubber Optimization", 53: "Generator Upgrade",
    54: "Foragers", 55: "Forager Capacity", 56: "Forager Speed", 57: "Fishing Harbor",
    58: "Fishing Harbor Efficiency", 59: "Healthcare", 60: "Infirmary Upgrade",
    61: "Docks", 62: "URS", 63: "URS Steam", 64: "Docks Reload Speed",
    65: "Gathering Upgrade", 66: "Resource Depot", 67: "Sawmill", 68: "Sawmill Upgrade",
    69: "Charcoal Kiln", 70: "Brazier", 71: "Brazier Range", 72: "Brazier Coal Efficiency",
    73: "Vault Efficiency", 74: "Vault Upgrade", 75: "Vault Multitasking",
    76: "Vault Insulation", 77: "Vault Steam Cores", 79: "Automaton Engineers",
    80: "Storm Research", 81: "URS Advanced",
}

FILLER_GENERIQUE = [
    ("Wood Crate", [("wood", 30)]),
    ("Coal Crate", [("coal", 30)]),
    ("Steel Crate", [("steel", 15)]),
    ("Raw Food Crate", [("rawfood", 15)]),
]
PIEGES = [
    ("Wood Trap", "trap_wood"),
    ("Coal Trap", "trap_coal"),
    ("Steel Trap", "trap_steel"),
]

# Plages d'ids : BASE + categorie * 100000 + index.
BASE = 7_747_000_000
PLAGES = {
    # locations
    "loc_techno": 1, "loc_loi": 2, "loc_site": 3, "loc_quete": 4, "loc_temp": 5, "loc_refugies": 6,
    # items
    "item_arbre": 11, "item_loi": 12, "item_temp": 13, "item_scenario": 14,
    "item_site": 15, "item_filler": 16, "item_piege": 17,
}

# ---------------------------------------------------------------------------

def lire_tsv(chemin):
    with open(chemin, encoding="utf-8-sig", newline="") as f:
        lignes = [l for l in f if l.strip() and not l.startswith("#")]
    return list(csv.DictReader(lignes, delimiter="\t"))


def lire_quetes():
    motif = re.compile(r'\{ "([^"]+)", 0x[0-9A-F]+ULL, 0x[0-9A-F]+ULL, "((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)", QueteCat::(\w+)')
    quetes = {}
    with open(os.path.join(MOD, "known_quests.h"), encoding="utf-8") as f:
        for l in f:
            m = motif.search(l)
            if m:
                quetes[(m.group(1), m.group(2).replace('\\"', '"'))] = m.group(3).replace('\\"', '"')
    return quetes


class Ids:
    """ids.tsv : cle -> id, jamais rien de retire."""

    def __init__(self, chemin):
        self.chemin = chemin
        self.ids = {}
        if os.path.exists(chemin):
            with open(chemin, encoding="utf-8") as f:
                for l in f:
                    if l.strip() and not l.startswith("#"):
                        cle, id_ = l.rstrip("\n").split("\t")
                        self.ids[cle] = int(id_)
        self.vus = set()

    def __call__(self, plage, cle):
        cle = plage + "|" + cle
        self.vus.add(cle)
        if cle not in self.ids:
            debut = BASE + PLAGES[plage] * 100000
            pris = [v for v in self.ids.values() if debut <= v < debut + 100000]
            self.ids[cle] = max(pris) + 1 if pris else debut + 1
        return self.ids[cle]

    def sauver(self):
        with open(self.chemin, "w", encoding="utf-8", newline="\n") as f:
            f.write("# ids Archipelago de Frostpunk : STABLES A VIE, ne jamais retirer ni renumeroter\n")
            for cle, id_ in sorted(self.ids.items(), key=lambda kv: kv[1]):
                f.write("%s\t%d\n" % (cle, id_))
        orphelins = sorted(set(self.ids) - self.vus)
        if orphelins:
            print("ATTENTION : %d cles d'ids.tsv ne sont plus generees (gardees) :" % len(orphelins))
            for c in orphelins:
                print("  " + c)


def unique(nom, deja):
    base, n = nom, 2
    while nom in deja:
        nom = "%s (%d)" % (base, n)
        n += 1
    deja.add(nom)
    return nom


def nom_arbre_loi(arbre):
    if arbre in ("Purpose", "Politics"):
        return arbre
    return re.sub(r"^(Builders )?\[[^\]]*\]\s*", "", arbre).strip()


def region_palier(scn, palier, n_regions):
    if scn in PALIERS_DE_DEPART:
        # Winterhome : paliers de depart a Start, puis un palier par region
        # en partant de la fin (N - 2, N - 1 ; N = Finish).
        depart = PALIERS_DE_DEPART[scn]
        if palier <= depart:
            return 0
        return max(0, n_regions - 1 - (5 - palier))
    return min(max(palier - 1, 0), n_regions - 1)


def main():
    ids = Ids(os.path.join(APWORLD, "ids.tsv"))
    quetes_jeu = lire_quetes()
    technos = [t for t in lire_tsv(os.path.join(MOD, "outils", "technos_table.tsv"))]
    lois = lire_tsv(os.path.join(MOD, "outils", "lois_table.tsv"))
    sites = lire_tsv(os.path.join(MOD, "outils", "sites_apparition.tsv"))
    classification = {(c["scenario"], int(c["arbre"])): c["classification"]
                      for c in lire_tsv(os.path.join(MOD, "outils", "technos_classification.tsv"))}
    contenus = {}
    for c in lire_tsv(os.path.join(MOD, "outils", "sites_contenu.tsv")):
        contenus.setdefault((c["scenario"], c["site"]), []).append(c)

    scenarios_out, locations, items = [], [], []
    lois_rangs = []                 # (scenario, arbre, loi interne, rang) pour le mod
    noms_loc, noms_item = set(), set()
    erreurs = []

    for scn, affiche in SCENARIOS:
        n_regions = len(GOULOTS[scn]) + 1
        regions = ["%s - Start" % affiche]
        for g in GOULOTS[scn]:
            regions.append("%s - After %s" % (affiche, quetes_jeu.get((scn, g), g)))
        # nom des locations des goulots (remplis plus bas)
        goulots_loc = {}

        # --- Technos ------------------------------------------------------
        ts = [t for t in technos if t["scenario"] == scn]
        par_hash = {t["hash"]: t for t in ts}
        arbres = {}
        for t in ts:
            arbres.setdefault(int(t["arbre"]), []).append(t)
        noms_arbres = {}
        for a, lst in arbres.items():
            if a not in NOMS_ARBRES:
                erreurs.append("arbre sans nom affiche (NOMS_ARBRES) : %s / %d" % (scn, a))
            noms_arbres[a] = NOMS_ARBRES.get(a, "Tree %d" % a)
        depart = PALIERS_DE_DEPART.get(scn, 0)

        # region / paliers requis effectifs (fermeture sur rang precedent et parent)
        memo = {}

        def exigence(t):
            h = t["hash"]
            if h in memo:
                return memo[h]
            a, rang, palier = int(t["arbre"]), int(t["rang"]), int(t["palier"])
            if a == ARBRE_PALIERS:
                # TierUnlock N (rang N - 1 hors depart) : region du palier N - 1
                region = region_palier(scn, depart + rang, n_regions)
            else:
                region = region_palier(scn, palier, n_regions)
            cond = t["condition d'affichage"]
            if cond:
                if (scn, cond) not in CONDITIONS_TECHNO:
                    erreurs.append("condition d'affichage inconnue : %s / %s" % (scn, cond))
                else:
                    region = max(region, CONDITIONS_TECHNO[(scn, cond)])
            tiers = max(0, palier - depart)
            liens = []
            if rang > 0:
                prec = [x for x in arbres[a] if int(x["rang"]) == rang - 1]
                liens += prec
            if t["parent (hash)"] and t["parent de depart"] != "1" and t["parent (hash)"] in par_hash:
                liens.append(par_hash[t["parent (hash)"]])
            for x in liens:
                r2, t2 = exigence(x)
                region, tiers = max(region, r2), max(tiers, t2)
            memo[h] = (region, tiers)
            return memo[h]

        for t in sorted(ts, key=lambda t: (int(t["arbre"]), int(t["rang"]))):
            region, tiers = exigence(t)
            nom = unique("%s - %s" % (affiche, t["nom court"]), noms_loc)
            locations.append(dict(id=ids("loc_techno", scn + "|" + t["hash"]), nom=nom, scn=scn,
                                  region=region, genre="techno", tiers=tiers, eclaireurs=False,
                                  exclue_option=False, cle=t["hash"]))
        # Classification des arbres (outils\technos_classification.tsv, regle
        # de l'auteur du 2026-11-04) : progression = arbre principal (ne depend
        # d'aucun autre arbre), Tier, eclaireurs ; useful = depend d'un autre
        # arbre ("important" en jeu).
        noms_items_arbres = {}
        for a, lst in sorted(arbres.items()):
            # "Progressive <arbre>" sans "Tree" (decision de l'auteur, 2026-11-04 ;
            # ids inchanges : la cle est le numero d'arbre)
            nom = unique("%s - Progressive %s" % (affiche, noms_arbres[a]), noms_item)
            noms_items_arbres[a] = nom
            if (scn, a) not in classification:
                erreurs.append("arbre sans classification : %s / %d" % (scn, a))
            items.append(dict(id=ids("item_arbre", "%s|%d" % (scn, a)), nom=nom, scn=scn, genre="arbre",
                              classe=classification.get((scn, a), "progression"), nombre=len(lst), cle=str(a)))

        # Regle des quetes : regions des checks progressifs, par item et dans
        # l'ordre des rangs (arbres de technos "progression" ici, lois plus
        # bas). Un item useful n'est pas compte : Archipelago ne le suit pas.
        progressifs = []
        for a, lst in sorted(arbres.items()):
            if classification.get((scn, a)) != "progression":
                continue
            progressifs.append((noms_items_arbres[a],
                                [exigence(t)[0] for t in sorted(lst, key=lambda t: int(t["rang"]))]))

        # --- Lois ---------------------------------------------------------
        ls = [l for l in lois if l["scenario"] == scn]
        arbres_lois = {}
        for l in ls:
            arbres_lois.setdefault(l["arbre"], []).append(l)
        for arbre, lst in sorted(arbres_lois.items()):
            court = nom_arbre_loi(arbre)
            ouverture = OUVERTURE_LOIS.get((scn, arbre))
            if ouverture is None and "[Labour" in arbre:
                ouverture = OUVERTURE_LOIS.get((scn, "Labour"))
            ouverture = ouverture or 0
            regs = []
            for l in sorted(lst, key=lambda l: int(l["rang"])):
                r = int(l["rang"])
                # Rang FIXE de chaque loi (decision de l'auteur, 2026-11-04 :
                # seules les lois exclusives partagent un check) ; une loi de
                # poids 2 (Ceremonial Funerals) a deux lignes.
                for loi in l["lois (nom interne)"].split(" | "):
                    lois_rangs.append((scn, arbre, loi.replace(" (2e check)", "").strip(), r))
                regs.append(min(ouverture + r - 1, n_regions - 1))
                nom = unique("%s - %s Law %d" % (affiche, court, r), noms_loc)
                locations.append(dict(id=ids("loc_loi", "%s|%s|%d" % (scn, arbre, r)), nom=nom, scn=scn,
                                      region=regs[-1], genre="loi", tiers=0,
                                      eclaireurs=False, exclue_option=arbre in ("Purpose", "Politics"),
                                      cle="%s|%d" % (arbre, r)))
            # progression (decision de l'auteur, 2026-11-04 : garde meme si la
            # regle des goulots ne compte plus les lois)
            nom = unique("%s - Progressive Law %s" % (affiche, court), noms_item)
            items.append(dict(id=ids("item_loi", "%s|%s" % (scn, arbre)), nom=nom, scn=scn, genre="loi",
                              classe="progression", nombre=len(lst), cle=arbre))

        # Regle des goulots : par region k, pour chaque arbre de technos
        # "progression" : (item, nombre de ses checks dans les regions < k,
        # nombre dans la region k). Les regions croissent avec le rang (cumul
        # sur le rang precedent).
        quetes_progressifs = {}
        for k in range(n_regions):
            items_k, total = [], 0
            for nom, regs in progressifs:
                assert regs == sorted(regs), (scn, nom, regs)
                avant, n = sum(1 for r in regs if r < k), sum(1 for r in regs if r == k)
                if n:
                    items_k.append((nom, avant, n))
                    total += n
            if total:
                quetes_progressifs[k] = (-(-total * QUETE_POURCENT_PROGRESSIFS // 100), items_k)

        # --- Sites ----------------------------------------------------------
        for s in [s for s in sites if s["scenario"] == scn]:
            if s["site"].endswith("-dead"):
                continue                      # paire exclusive : un seul check
            reg = s["region"]
            if reg == "Start":
                region = 0
            else:
                g = reg[len("apres "):]
                cands = [i for i, q in enumerate(GOULOTS[scn]) if quetes_jeu.get((scn, q), "") and (q.endswith(g) or q == g)]
                if not cands:
                    erreurs.append("region de site inconnue : %s / %s / %s" % (scn, s["site"], reg))
                    continue
                region = cands[0] + 1
            nom = unique("%s - %s" % (affiche, s["nom_affiche"]), noms_loc)
            locations.append(dict(id=ids("loc_site", scn + "|" + s["site"]), nom=nom, scn=scn, region=region,
                                  genre="site", tiers=0, eclaireurs=True, exclue_option=False, cle=s["site"]))
            # contenu vanilla (sites_contenu.tsv : direct + bonus ; les choix
            # de dilemme sont ignores) -> item : survivants / noyaux / technos
            # = useful, ressources seules = filler (Q6 d)
            res = []
            for c in contenus.get((scn, s["site"]), []):
                if c["source"] not in ("direct", "bonus"):
                    continue
                for col in ("ressources", "survivants"):
                    for morceau in c[col].split(", "):
                        if morceau == "-" or morceau.startswith("AP_Item"):
                            continue
                        nom_r, qte = morceau.split(":")
                        if col == "ressources" and nom_r not in RESSOURCES_MOD:
                            erreurs.append("ressource inconnue : %s (%s)" % (nom_r, s["site"]))
                        res.append((nom_r, int(qte)))
                if c["technologies"] != "-":
                    res.append(("technology", int(c["technologies"])))
            # Option B (decision de l'auteur, 2026-11-03) : l'item appartient
            # au scenario de son site et n'est livre que dans ce scenario.
            if res:
                precieux = ("steamcore", "technology") + SURVIVANTS
                classe = "useful" if any(r in precieux for r, _ in res) else "filler"
                nomItem = unique("%s - %s Supplies" % (affiche, s["nom_affiche"]), noms_item)
                items.append(dict(id=ids("item_site", scn + "|" + s["site"]), nom=nomItem, scn=scn,
                                  genre="site", classe=classe, nombre=1, cle=s["site"],
                                  ressources=res))

        # --- Quetes ---------------------------------------------------------
        for q, region in QUETES[scn]:
            groupe = q if isinstance(q, list) else [q]
            for x in groupe:
                if (scn, x) not in quetes_jeu:
                    erreurs.append("quete inconnue : %s / %s" % (scn, x))
            # Nom du check (demande de l'auteur, 2026-11-04 : "A New Home - Food"
            # n'etait pas clair) : "<Scenario> - Quest: <nom affiche>", nom
            # precise pour les sous-quetes trop vagues (NOMS_QUETES).
            titre = NOMS_QUETES.get((scn, groupe[0])) or titre_quete(quetes_jeu.get((scn, groupe[0]), groupe[0]))
            nom = unique("%s - Quest: %s" % (affiche, titre), noms_loc)
            for x in groupe:
                if x in GOULOTS[scn]:
                    goulots_loc[GOULOTS[scn].index(x)] = nom
            locations.append(dict(id=ids("loc_quete", scn + "|" + groupe[0]), nom=nom, scn=scn, region=region,
                                  genre="quete", tiers=0, eclaireurs=False, exclue_option=False,
                                  cle="|".join(groupe)))

        # --- Refugies (arrivees a la porte) --------------------------------
        if scn in REFUGIES:
            ref = REFUGIES[scn]
            for k, contenu in enumerate(ref["groupes"], 1):
                nom = unique("%s - Refugees Arrival %d" % (affiche, k), noms_loc)
                locations.append(dict(id=ids("loc_refugies", "%s|%d" % (scn, k)), nom=nom, scn=scn,
                                      region=ref["region"], genre="refugies", tiers=0, eclaireurs=False,
                                      exclue_option=False, cle=str(k)))
                nomItem = unique("%s - Refugees (Group %d)" % (affiche, k), noms_item)
                items.append(dict(id=ids("item_site", "%s|refugies_%d" % (scn, k)), nom=nomItem, scn=scn,
                                  genre="site", classe="useful", nombre=1, cle="refugies_%d" % k,
                                  ressources=list(contenu)))

        # --- Temperature ----------------------------------------------------
        # Plus de LOCATIONS de montee (decision de l'auteur, 2026-11-04 : la
        # frise etant masquee, le joueur ne peut plus situer ces checks). Les
        # ids "loc_temp|..." restent dans ids.tsv (jamais retires). Les ITEMS
        # Temperature Rise restent : le mod retire toujours les montees du
        # jeu, chaque item rend un niveau.
        maxi = max(MONTEES[scn])
        temp_noms = []
        if maxi:
            items.append(dict(id=ids("item_temp", scn), nom="%s - Temperature Rise" % affiche, scn=scn,
                              genre="temperature", classe="useful", nombre=0, cle=scn))

        items.append(dict(id=ids("item_scenario", scn), nom="Scenario Unlock %s" % affiche, scn=scn,
                          genre="scenario", classe="progression", nombre=1, cle=scn))

        if len(goulots_loc) != len(GOULOTS[scn]):
            erreurs.append("goulot sans location : %s" % scn)
        scenarios_out.append(dict(cle=scn, nom=affiche, regions=regions,
                                  goulots=[goulots_loc.get(i) for i in range(len(GOULOTS[scn]))],
                                  montees=MONTEES[scn], temperature=temp_noms,
                                  quetes_progressifs=quetes_progressifs,
                                  eclaireurs=noms_items_arbres[ARBRE_ECLAIREURS[scn]]
                                  if ARBRE_ECLAIREURS[scn] is not None else None,
                                  paliers=noms_items_arbres[ARBRE_PALIERS] if ARBRE_PALIERS in arbres else None))

    for nom, res in FILLER_GENERIQUE:
        items.append(dict(id=ids("item_filler", nom), nom=unique(nom, noms_item), scn=None, genre="filler",
                          classe="filler", nombre=0, cle=nom, ressources=res))
    for nom, dilemme in PIEGES:
        items.append(dict(id=ids("item_piege", dilemme), nom=unique(nom, noms_item), scn=None, genre="piege",
                          classe="trap", nombre=0, cle=dilemme))

    if erreurs:
        for e in erreurs:
            print("ERREUR : " + e)
        sys.exit(1)

    ids.sauver()
    ecrire_data(scenarios_out, locations, items)
    ecrire_h(locations, items, lois_rangs)
    for scn, affiche in SCENARIOS:
        nl = sum(1 for l in locations if l["scn"] == scn and l["genre"] != "temperature")
        print("%-12s %4d locations (hors temperature)" % (scn, nl))
    print("%d locations, %d items" % (len(locations), len(items)))


def ecrire_data(scenarios, locations, items):
    chemin = os.path.join(APWORLD, "frostpunk", "data.py")
    with open(chemin, "w", encoding="utf-8", newline="\n") as f:
        f.write("# data.py -- GENERE par apworld/outils/generer.py ; ne pas editer a la main.\n\n")
        f.write("DIFFICULTIES = %r\n\n" % DIFFICULTES)
        f.write("SCENARIOS = [\n")
        for s in scenarios:
            f.write("    %r,\n" % s)
        f.write("]\n\n")
        f.write("# genre : techno, loi, site, quete, temperature ; region None = calculee\n")
        f.write("# (temperature) ; tiers = Progressive Tier requis ; eclaireurs =\n")
        f.write("# Beacon / Forager Quarters requis ; exclue_option = Purpose / Politics.\n")
        f.write("LOCATIONS = [\n")
        for l in locations:
            f.write("    %r,\n" % {k: l[k] for k in ("id", "nom", "scn", "region", "genre", "tiers", "eclaireurs", "exclue_option")})
        f.write("]\n\n")
        f.write("# nombre = exemplaires par scenario (0 = selon les options) ; scn None =\n")
        f.write("# item commun (filler, pieges : applique au scenario en cours) ; origine =\n")
        f.write("# scenario dont l'item fait partie du pool (= scn), None = tous.\n")
        f.write("ITEMS = [\n")
        for i in items:
            d = {k: i[k] for k in ("id", "nom", "scn", "genre", "classe", "nombre")}
            d["origine"] = i.get("origine", i["scn"])
            f.write("    %r,\n" % d)
        f.write("]\n")


def ecrire_h(locations, items, lois_rangs):
    chemin = os.path.join(MOD, "known_ap_ids.h")

    def c(s):
        return s.replace("\\", "\\\\").replace('"', '\\"')

    with open(chemin, "w", encoding="utf-8", newline="\n") as f:
        f.write("// known_ap_ids.h  --  Ids Archipelago de Frostpunk (GENERE par\n")
        f.write("// apworld\\outils\\generer.py ; ne pas editer a la main).\n")
        f.write("// Les ids sont stables a vie (apworld\\ids.tsv). Noms = noms des\n")
        f.write("// locations / items cote serveur (affiches en jeu, en anglais).\n\n")
        f.write("#pragma once\n#include <cstdint>\n\n")
        f.write("enum class ApLocGenre : uint8_t { Techno, Loi, Site, Quete, Temperature, Refugies };\n")
        f.write("enum class ApItemGenre : uint8_t { Arbre, Loi, Temperature, Scenario, Site, Filler, Piege };\n\n")
        f.write("// cle : techno = hash (hex) ; loi = \"<arbre>|<rang>\" ; site = nom interne\n")
        f.write("// (SITE_PAYLOADS) ; quete = noms internes (\"|\" = paralleles) ; temperature = k.\n")
        f.write("struct ApLocationInfo { int64_t id; ApLocGenre genre; const char* scenario; const char* cle; const char* nom; };\n\n")
        f.write("static const ApLocationInfo AP_LOCATIONS[] = {\n")
        for l in locations:
            f.write('    { %dLL, ApLocGenre::%s, "%s", "%s", "%s" },\n' % (
                l["id"], {"techno": "Techno", "loi": "Loi", "site": "Site", "quete": "Quete", "temperature": "Temperature",
                 "refugies": "Refugies"}[l["genre"]],
                l["scn"], c(l["cle"]), c(l["nom"])))
        f.write("};\n\n")
        f.write("// cle : arbre = numero d'arbre (ReceiveProgressive) ; loi = nom de l'arbre\n")
        f.write("// (ReceiveLawProgressive) ; scenario / temperature = nom interne du scenario ;\n")
        f.write("// site / filler = ressources \"wood 30;coal 10\" (ReceiveResource ; site :\n")
        f.write("// scenario du site seulement, filler : scenario courant) ; piege = id de\n")
        f.write("// DILEMMES_AP. scenario nullptr = item commun (filler, pieges).\n")
        f.write("struct ApItemInfo { int64_t id; ApItemGenre genre; const char* scenario; const char* cle; const char* nom; };\n\n")
        f.write("static const ApItemInfo AP_ITEMS[] = {\n")
        genres = {"arbre": "Arbre", "loi": "Loi", "temperature": "Temperature", "scenario": "Scenario",
                  "site": "Site", "filler": "Filler", "piege": "Piege"}
        for i in items:
            cle = ";".join("%s %d" % r for r in i["ressources"]) if "ressources" in i else i["cle"]
            scn = '"%s"' % i["scn"] if i["scn"] else "nullptr"
            f.write('    { %dLL, ApItemGenre::%s, %s, "%s", "%s" },\n' % (i["id"], genres[i["genre"]], scn, c(cle), c(i["nom"])))
        f.write("};\n\n")
        f.write("// Rang du check de chaque loi (outils\\lois_table.tsv) : le check d'une loi\n")
        f.write("// ne depend pas de l'ordre des signatures ; seules les lois exclusives\n")
        f.write("// partagent un rang. Une loi de poids 2 a deux lignes (deux checks).\n")
        f.write("struct ApLoiRang { const char* scenario; const char* arbre; const char* loi; int rang; };\n\n")
        f.write("// Site d'origine des items de contenu de site (nom interne ; \"refugies_k\"\n")
        f.write("// pour les refugies) : le mod y prend la classe et l'etat de sante des\n")
        f.write("// survivants livres (2026-11-06).\n")
        f.write("struct ApItemSite { int64_t id; const char* site; };\n\n")
        f.write("static const ApItemSite AP_ITEMS_SITES[] = {\n")
        for i in items:
            if i["genre"] == "site":
                f.write('    { %dLL, "%s" },\n' % (i["id"], c(i["cle"])))
        f.write("};\n\n")
        f.write("static const ApLoiRang AP_LOIS_RANGS[] = {\n")
        for scn, arbre, loi, r in lois_rangs:
            f.write('    { "%s", "%s", "%s", %d },\n' % (scn, c(arbre), c(loi), r))
        f.write("};\n")


if __name__ == "__main__":
    main()
