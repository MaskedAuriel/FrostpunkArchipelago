# Export des quetes par scenario vers Excel, depuis le journal du releve F9
# (lignes "[QXLS]", quetes.cpp). Python standard seulement (pas d'openpyxl).
#   python outils\quetes_excel.py [logs\console.log] [quetes_par_scenario.xlsx]
# Ecrit aussi known_quests.h (table du code) dans le dossier courant.
#
# Lignes lues (separees par des tabulations), cle = GUID de la quete (32 hex) :
#   Q  quete  repertoire  type  GUID  nom affiche  description
#   O  GUID  indice  nom interne  classe  obligatoire  texte affiche  description
#   S  scenario  ordre  GUID  sources (L loi, E evenement, D dilemme, Q quete)  detail
# Des noms internes existent dans plusieurs repertoires (ex. "[Sociotech
# Related] Care House" dans Default et Scenario Builders) : d'ou le GUID.
import html
import os
import re
import sys
import zipfile
from xml.sax.saxutils import escape

JOURNAL = sys.argv[1] if len(sys.argv) > 1 else r"logs\console.log"
SORTIE = sys.argv[2] if len(sys.argv) > 2 else "quetes_par_scenario.xlsx"

SCENARIOS = [
    ("DEFAULT", "A New Home"),
    ("TechHaven", "The Arks"),
    ("REFUGEES", "Refugees"),
    ("WINTERHOME", "Fall of Winterhome"),
    ("BUILDERS", "Last Autumn"),
    ("Reconquest", "On the Edge"),
]


def decoder(s):
    return re.sub(r"\\u([0-9A-Fa-f]{4})", lambda m: chr(int(m.group(1), 16)), s)


quetes = {}        # GUID -> {interne, rep, type, nom, desc}
objectifs = {}     # GUID -> [(indice, interne, classe, oblig, texte, desc)]
par_scenario = {}  # scenario -> [(ordre, GUID, genres, sources)]
# Sites d'exploration (lignes "[SXLS]", scooting.cpp) : scenario -> {site:
# [nom, dossier, 4 noms affiches, description, etat, contenu, dilemmes]}.
# Un F9 ne voit que les sites TIRES dans la partie (remarque de l'auteur du
# 2026-11-01) : les releves sont cumules d'une execution a l'autre dans
# outils\sites_releves.tsv (un site deja vu garde ses infos), et completes
# par les tables de tirage (outils\tables_tirage.tsv), qui donnent la liste
# complete des sites possibles de chaque scenario.
sites = {}
# Tables de tirage des sites (lignes "[TXLS]", scooting.cpp) : table -> [(site, chemin)].
tables_sites = {}
LARGEURS_SITES = [34, 14, 30, 30, 30, 30, 60, 30, 9, 6, 10, 12]
ENTETE_SITES = ["Site (nom interne)", "Dossier", "Nom affiche", "Nom non visite", "Nom explore",
                "Nom epuise", "Description", "Contenu (au releve)", "Dilemmes", "Etat", "Vu en partie",
                "Sur la carte (tirage fixe)"]
# Table de tirage de chaque scenario (noms relevees le 2026-11-01 ; la table
# contient TOUS les sites du scenario, fixes compris : verifie sur A New Home).
TABLE_DU_SCENARIO = {"DEFAULT": "Story Randomized", "TechHaven": "TechHaven", "REFUGEES": "Refugees",
                     "WINTERHOME": "Winterhome", "BUILDERS": "Builders", "Reconquest": "Reconquest"}
DOSSIER_OUTILS = os.path.dirname(os.path.abspath(__file__))
CACHE_SITES = os.path.join(DOSSIER_OUTILS, "sites_releves.tsv")
CACHE_TABLES = os.path.join(DOSSIER_OUTILS, "tables_tirage.tsv")
# Carte figee (tirage fixe de scooting.cpp, valide le 2026-11-01) : sites du
# dernier releve d'un scenario fait apres un "[SITES] tirage fixe" de sa
# table, = les sites qui existent dans TOUTE partie de ce scenario (ceux qui
# recevront un check). scenario -> set(site)
CACHE_CARTE = os.path.join(DOSSIER_OUTILS, "carte_fixe.tsv")
carte_fixe = {}


def lire_cache_sites():
    if os.path.exists(CACHE_SITES):
        with open(CACHE_SITES, encoding="utf-8") as f:
            for l in f:
                c = l.rstrip("\n").split("\t")
                if len(c) == 11:
                    sites.setdefault(c[0], {})[c[1]] = c[1:]
        return
    # Amorcage : onglets "Sites - ..." d'un Excel deja produit.
    if not os.path.exists(SORTIE):
        return
    affiche_vers_interne = {a: i for i, a in SCENARIOS}
    z = zipfile.ZipFile(SORTIE)
    wb = z.read("xl/workbook.xml").decode("utf-8")
    for nom, num in re.findall(r'<sheet name="([^"]+)" sheetId="(\d+)"', wb):
        nom = html.unescape(nom)
        if not nom.startswith("Sites - ") or nom[8:] not in affiche_vers_interne:
            continue
        scn = affiche_vers_interne[nom[8:]]
        x = z.read("xl/worksheets/sheet%s.xml" % num).decode("utf-8")
        for r, contenu in re.findall(r'<row r="(\d+)">(.*?)</row>', x):
            if r == "1":
                continue
            d = {}
            for col_, a, b in re.findall(r'<c r="([A-Z]+)\d+"[^>]*>(?:<is><t[^>]*>(.*?)</t></is>|<v>(.*?)</v>)</c>', contenu):
                d[col_] = html.unescape(a or b)
            if d.get("A") and d.get("K", "oui") != "non":
                g = lambda k: d.get(k, "-")
                sites.setdefault(scn, {})[d["A"]] = [d["A"], g("B"), g("C"), g("D"), g("E"), g("F"), g("G"),
                                                     g("J"), g("H"), g("I")]


def lire_cache_tables():
    if os.path.exists(CACHE_TABLES):
        with open(CACHE_TABLES, encoding="utf-8") as f:
            for l in f:
                c = l.rstrip("\n").split("\t")
                if len(c) == 3:
                    tables_sites.setdefault(c[0], []).append((c[1], c[2]))


def lire_cache_carte():
    if os.path.exists(CACHE_CARTE):
        with open(CACHE_CARTE, encoding="utf-8") as f:
            for l in f:
                c = l.rstrip("\n").split("\t")
                if len(c) == 2:
                    carte_fixe.setdefault(c[0], set()).add(c[1])


lire_cache_sites()
lire_cache_tables()
lire_cache_carte()
tables_du_journal = {}
dernier_tirage_fixe = None   # table du dernier "[SITES] tirage fixe" du journal
releve_fixe = None           # scenario du releve en cours s'il suit un tirage fixe

with open(JOURNAL, "rb") as f:
    for brut in f:
        ligne = brut.decode("cp1252", errors="replace").rstrip("\r\n")
        m = re.match(r'\[SITES\] tirage fixe : table "([^"]*)"', ligne)
        if m:
            dernier_tirage_fixe = m.group(1)
            continue
        m = re.match(r"\[SITES\] scenario (\S+) :", ligne)
        if m:
            scn = m.group(1)
            releve_fixe = scn if TABLE_DU_SCENARIO.get(scn) == dernier_tirage_fixe else None
            if releve_fixe:
                carte_fixe[scn] = set()   # le dernier releve fixe remplace le precedent
            continue
        if ligne.startswith("[SXLS]\t") and releve_fixe:
            c = ligne.split("\t")
            if len(c) > 3 and c[1] == "S" and c[2] == releve_fixe:
                carte_fixe[releve_fixe].add(decoder(c[3]))
        if ligne.startswith("[TXLS]\t"):
            c = [decoder(x) for x in ligne.split("\t")[1:]]
            if c[0] == "T" and len(c) >= 2:
                tables_du_journal[c[1]] = []
            elif c[0] == "E" and len(c) >= 4:
                tables_du_journal.setdefault(c[1], []).append((c[2], c[3]))
            continue
        if ligne.startswith("[SXLS]\t"):
            c = [decoder(x) for x in ligne.split("\t")[1:]]
            if c[0] == "S" and len(c) >= 12:
                sites.setdefault(c[1], {})[c[2]] = c[2:]
            continue
        if not ligne.startswith("[QXLS]\t"):
            continue
        c = [decoder(x) for x in ligne.split("\t")[1:]]
        if c[0] == "Q" and len(c) >= 7:
            quetes[c[4]] = {"interne": c[1], "rep": c[2], "type": c[3], "nom": c[5], "desc": c[6]}
            objectifs[c[4]] = []
        elif c[0] == "O" and len(c) >= 8:
            objectifs.setdefault(c[1], []).append((int(c[2]), c[3], c[4], c[5] == "1", c[6], c[7]))
        elif c[0] == "S" and len(c) >= 6:
            # Plusieurs F9 dans un meme journal : une quete n'est gardee qu'une
            # fois par scenario (le dernier releve l'emporte).
            lst = par_scenario.setdefault(c[1], [])
            lst[:] = [x for x in lst if x[1] != c[3]]
            lst.append((int(c[2]), c[3], c[4], c[5]))

# Caches des sites : tables du journal prioritaires, releves fusionnes.
tables_sites.update(tables_du_journal)
with open(CACHE_TABLES, "w", encoding="utf-8") as f:
    for table in sorted(tables_sites):
        for site, chemin in tables_sites[table]:
            f.write("%s\t%s\t%s\n" % (table, site, chemin))
with open(CACHE_SITES, "w", encoding="utf-8") as f:
    for scn in sorted(sites):
        for nom in sorted(sites[scn]):
            f.write("\t".join([scn] + list(sites[scn][nom])) + "\n")
with open(CACHE_CARTE, "w", encoding="utf-8") as f:
    for scn in sorted(carte_fixe):
        for nom in sorted(carte_fixe[scn]):
            f.write("%s\t%s\n" % (scn, nom))


def sites_complets(interne):
    """Liste complete d'un scenario : sites releves + sites de sa table de
    tirage jamais vus en partie. -> [(nom, ligne ou None, chemin)]"""
    chemins = dict(tables_sites.get(TABLE_DU_SCENARIO.get(interne, ""), []))
    vus = sites.get(interne, {})
    return [(nom, vus.get(nom), chemins.get(nom, "")) for nom in sorted(set(vus) | set(chemins))]


def dossier_du_chemin(chemin):
    m = re.search(r"ExpeditionSites/([^/]+)/", chemin)
    return m.group(1) if m else "?"


if not quetes:
    sys.exit("Aucune ligne [QXLS] dans " + JOURNAL + " : faire F9 en partie (jeu hors pause).")
if any(len(g) != 32 for g in quetes):
    sys.exit("Releve F9 d'une ancienne version (sans GUID sur les lignes O / S) : refaire F9.")

CAT_HISTOIRE = "Histoire / scenario"
CAT_DILEMME = "Histoire (lancee par un dilemme)"
CAT_REPERTOIRE = "Deduit du repertoire (source non trouvee)"
CAT_LOI = "Lancee par une loi"


# Tri manuel de l'auteur (2026-11-01, feuille Last Autumn) : nom interne ->
# categorie, prioritaire sur les regles automatiques.
CAT_SECONDAIRE = "Secondaire (hors quete principale)"
CLASSEMENT_MANUEL = {
    "[Builders] Pass First Safety Law": CAT_LOI,
    "[Builders] Open House of Pleasure": CAT_LOI,
    "[Builders] No more soup": CAT_LOI,
    "[Builders] Open Bath House": CAT_LOI,
    "[Builders] Open Public House": CAT_LOI,
    "[Builders] Open Care House": CAT_LOI,
    "[Builders] Dismantle guard posts": CAT_LOI,
    "[Builders] Better food": CAT_LOI,
    "[Builders] [Crisis] Resolve strike": CAT_SECONDAIRE,
    "[Builders] Heat construction site (no research)": CAT_SECONDAIRE,
    "[Builders] Heat construction site (research)": CAT_SECONDAIRE,
    "[Builders][Discontent] General Strike": CAT_SECONDAIRE,
    "[Builders] Find escaped convicts": CAT_SECONDAIRE,
    "[Builders] Safe Construction Site": CAT_SECONDAIRE,
}


def categorie(genres, rep, nom, affiche=""):
    if nom in CLASSEMENT_MANUEL:
        return CLASSEMENT_MANUEL[nom]
    # Decision de l'auteur (2026-11-01, Burials / Snow Pit) : une quete lancee
    # par une loi est une quete de loi, meme si un evenement ou un dilemme
    # (aide "Sociotech Hints", premier mort...) la lance aussi ; de meme pour
    # toute "[Sociotech Related]" ("Burials again" : relancee par evenement),
    # et pour les quetes "Pass "<loi>"" de Last Autumn (signer une loi) et
    # "[Builders] ... Build <batiment de loi>" (People's Militia, Security
    # Outposts ; [ENDGAME] Build Coal Thumpers n'en est pas une).
    if "L" in genres or nom.startswith("[Sociotech Related]") or affiche.startswith("Pass "):
        return CAT_LOI
    if re.match(r"^\[Builders\](\s*\[[^\]]*\])*\s*Build ", nom):
        return CAT_LOI
    if rep in ("Simple Snops", "MiniSnops"):
        return "Secondaire (" + rep + ")"
    if genres and set(genres) <= {"D", "Q"} and "D" in genres:
        return CAT_DILEMME
    return CAT_HISTOIRE


ORDRE_CAT = {CAT_HISTOIRE: 0, CAT_DILEMME: 1, CAT_REPERTOIRE: 2, "Secondaire (hors quete principale)": 3,
             "Secondaire (Simple Snops)": 4, "Secondaire (MiniSnops)": 5, CAT_LOI: 6}

# Lois absentes d'un scenario (releve F4 v5 du 2026-10-30, verifie par
# l'auteur) : le releve F9 ne filtre les lois que par les etiquettes Builders
# / Endless. Une quete lancee seulement par des lois absentes est ecartee.
LOIS_ABSENTES = {
    "TechHaven": ["[Order - level", "[Faith - level", "Child shelters", "Kids work", "Med Kids", "EngineerKids"],
    "Reconquest": ["[Order - level", "[Faith - level", "Corps Processing"],
}


# Quetes reservees a un scenario par leur nom, en plus des etiquettes du
# releve F9 : les [LondonerArc] demandent des lois de Purpose, absent des
# Arks (repertoire "Londoners" actif aussi dans The Arks).
RESERVEES = {"[LondonerArc]": "DEFAULT"}


def reservee_ailleurs(nom, scenario):
    return any(nom.startswith(e) and s != scenario for e, s in RESERVEES.items())


def loi_presente(scenario, sources):
    lois = re.findall(r'"([^"]*)" \(Lois\)', sources)
    absentes = LOIS_ABSENTES.get(scenario, [])
    return not lois or any(not any(a in loi for a in absentes) for loi in lois)


# Repli pour une quete qu'aucune source ne rattache (demande de l'auteur du
# 2026-11-01) : repertoire de la quete propre a un scenario, ou etiquette.
def scenario_du_repertoire(q):
    nom, rep = q["interne"], q["rep"]
    if nom.startswith("CUT ") or nom.startswith("[Endless]"):
        return None
    if rep == "Reconquest":
        return "Reconquest"
    if rep == "Scenario Builders" or nom.startswith("[Builders]"):
        return "BUILDERS"
    if rep == "Scenarios":
        for etiquette, scn in (("[Refugees", "REFUGEES"), ("[Winterhome", "WINTERHOME"),
                               ("[TechHeaven", "TechHaven"), ("[TH", "TechHaven")):
            if nom.startswith(etiquette):
                return scn
    return None


ENTETE = ["Categorie", "Ordre", "Quete (nom interne)", "Quete (nom affiche)",
          "Description de la quete", "Repertoire", "Type", "Tache n", "Tache (nom interne)",
          "Tache (classe)", "Obligatoire", "Tache (texte affiche)", "Tache (description)",
          "Lancee par"]


def lignes_quete(cat, ordre, guid, sources):
    q = quetes.get(guid, {"interne": "?", "rep": "?", "type": "?", "nom": "?", "desc": "?"})
    objs = sorted(objectifs.get(guid, []))
    base = [cat, ordre, q["interne"], q["nom"], q["desc"], q["rep"], q["type"]]
    if not objs:
        return [base + ["", "", "", "", "", "", sources]]
    sortie = []
    for k, (i, interne, cls, oblig, texte, desc) in enumerate(objs):
        tete = base if k == 0 else ["", "", "", "", "", "", ""]
        sortie.append(tete + [i, interne, cls, "oui" if oblig else "non", texte, desc,
                              sources if k == 0 else ""])
    return sortie


# 1) Rattachements trouves par le releve.
rangees_par_scn = {i: [] for i, _ in SCENARIOS}
vues = set()
for interne, _ in SCENARIOS:
    for ordre, guid, genres, sources in par_scenario.get(interne, []):
        if guid not in quetes:
            continue    # succes Steam / Endless : pas de ligne Q (exclus du releve)
        if reservee_ailleurs(quetes[guid]["interne"], interne):
            continue
        cat = categorie(genres, quetes[guid]["rep"], quetes[guid]["interne"], quetes[guid]["nom"])
        if cat == CAT_LOI and not loi_presente(interne, sources):
            continue
        vues.add(guid)
        rangees_par_scn[interne].append((ORDRE_CAT.get(cat, 9), ordre, cat, guid, sources))

# 2) Repli par repertoire pour les quetes restees seules.
for guid, q in quetes.items():
    if guid in vues:
        continue
    scn = scenario_du_repertoire(q)
    if scn:
        vues.add(guid)
        cat = categorie("", q["rep"], q["interne"], q["nom"])
        if cat not in (CAT_LOI, CAT_SECONDAIRE, "Secondaire (Simple Snops)", "Secondaire (MiniSnops)"):
            cat = CAT_REPERTOIRE
        rangees_par_scn[scn].append((ORDRE_CAT[cat], 9999, cat, guid,
                                     "repertoire de quetes \"%s\" (source non trouvee)" % q["rep"]))

# 3) Quetes presentes dans TOUS les scenarios (hors lois) : "sans scenario"
#    (decision de l'auteur du 2026-11-01, "[GAME ENDING] Last chance" : c'est
#    present partout). Simple Snops / MiniSnops communes, Augmentation...
communes = {}      # GUID -> (categorie d'origine, sources du 1er scenario)
for guid in quetes:
    presences = [[r for r in rangees_par_scn[i] if r[3] == guid] for i, _ in SCENARIOS]
    if all(presences) and presences[0][0][2] != CAT_LOI:
        communes[guid] = (presences[0][0][2], presences[0][0][4])
for i, _ in SCENARIOS:
    rangees_par_scn[i] = [r for r in rangees_par_scn[i] if r[3] not in communes]

feuilles = []
table_code = []    # (scenario interne, categorie, ordre, GUID)
lois = {}          # GUID -> (scenarios ou elle existe, sources)
secondaires = {}   # GUID -> (scenarios, categorie, sources)
for interne, affiche in SCENARIOS:
    rangees = sorted(rangees_par_scn[interne], key=lambda r: (r[0], r[1], quetes[r[3]]["interne"]))
    donnees = [ENTETE]
    for _, ordre, cat, guid, sources in rangees:
        table_code.append((interne, cat, ordre, guid))
        if cat == CAT_LOI:
            # Onglet a part (demande de l'auteur).
            lois.setdefault(guid, ([], sources))[0].append(affiche)
            continue
        if cat.startswith("Secondaire"):
            # Les feuilles de scenario ne gardent que l'histoire (demande de
            # l'auteur du 2026-11-01) : secondaires dans leur onglet.
            secondaires.setdefault(guid, ([], cat, sources))[0].append(affiche)
            continue
        donnees += lignes_quete(cat, ordre if ordre != 9999 else "", guid, sources)
    feuilles.append((affiche, donnees))

# Quetes lancees par une loi : un seul onglet, colonne A = scenarios.
donnees = [["Scenarios"] + ENTETE[1:]]
for guid in sorted(lois, key=lambda g: quetes[g]["interne"]):
    scn, sources = lois[guid]
    donnees += lignes_quete(", ".join(scn), "", guid, sources)
feuilles.append(("Lois", donnees))

# Quetes secondaires propres a certains scenarios (les communes a tous sont
# dans "Sans scenario") : colonne A = categorie et scenarios.
donnees = [["Categorie / scenarios"] + ENTETE[1:]]
for guid in sorted(secondaires, key=lambda g: (ORDRE_CAT.get(secondaires[g][1], 9), quetes[g]["interne"])):
    scn, cat, sources = secondaires[guid]
    donnees += lignes_quete("%s - %s" % (cat, ", ".join(scn)), "", guid, sources)
feuilles.append(("Secondaires", donnees))

# Sans scenario : d'abord les communes a tous, puis celles qu'aucune source
# ne rattache (hors Endless / succes).
donnees = [ENTETE]
for guid in sorted(communes, key=lambda g: (ORDRE_CAT.get(communes[g][0], 9), quetes[g]["interne"])):
    cat, sources = communes[guid]
    donnees += lignes_quete("Commune a tous les scenarios (" + cat + ")", "", guid, sources)
    table_code.append(("*", cat, -1, guid))
for guid in sorted(quetes, key=lambda g: (quetes[g]["rep"], quetes[g]["interne"])):
    if guid not in vues:
        donnees += lignes_quete("Rattachement non trouve", "", guid, "")
feuilles.append(("Sans scenario", donnees))


# Sites d'exploration : un onglet par scenario (releves + table de tirage).
for interne, affiche in SCENARIOS:
    liste = sites_complets(interne)
    if not liste:
        continue
    donnees = [ENTETE_SITES]
    carte = carte_fixe.get(interne)
    sur_carte = lambda n: "?" if carte is None else ("oui" if n in carte else "non")
    for nom, s, chemin in liste:
        if s:   # nom, dossier, 4 noms, description, etat, contenu, dilemmes
            donnees.append([s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[8],
                            int(s[9]) if s[9].isdigit() else s[9],
                            int(s[7]) if s[7].lstrip("-").isdigit() else s[7], "oui", sur_carte(nom)])
        else:
            donnees.append([nom, dossier_du_chemin(chemin), "(jamais tire en partie)", "", "", "", "", "", "", "",
                            "non", sur_carte(nom)])
    feuilles.append(("Sites - " + affiche, donnees))


# Tables de tirage : tous les sites possibles, par table (tous scenarios).
if tables_sites:
    donnees = [["Table de tirage", "Site (nom interne)", "Chemin du gabarit"]]
    for table in sorted(tables_sites):
        for site, chemin in tables_sites[table]:
            donnees.append([table, site, chemin])
    feuilles.append(("Tables de tirage", donnees))


# --- Ecriture xlsx minimale -------------------------------------------------
def col(n):
    s = ""
    n += 1
    while n:
        n, r = divmod(n - 1, 26)
        s = chr(65 + r) + s
    return s


LARGEURS = [22, 6, 45, 32, 50, 14, 6, 7, 40, 34, 10, 45, 50, 60]


def xml_feuille(donnees):
    largeurs = (LARGEURS_SITES if donnees and donnees[0] == ENTETE_SITES else
                [40, 40, 90] if donnees and len(donnees[0]) == 3 else LARGEURS)
    x = ['<?xml version="1.0" encoding="UTF-8" standalone="yes"?>',
         '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">',
         '<sheetViews><sheetView workbookViewId="0"><pane ySplit="1" topLeftCell="A2" '
         'activePane="bottomLeft" state="frozen"/></sheetView></sheetViews><cols>']
    for i, w in enumerate(largeurs):
        x.append('<col min="%d" max="%d" width="%d" customWidth="1"/>' % (i + 1, i + 1, w))
    x.append("</cols><sheetData>")
    for r, ligne in enumerate(donnees):
        x.append('<row r="%d">' % (r + 1))
        for c, v in enumerate(ligne):
            ref = "%s%d" % (col(c), r + 1)
            style = ' s="1"' if r == 0 else ' s="2"'
            if isinstance(v, int):
                x.append('<c r="%s"%s><v>%d</v></c>' % (ref, style, v))
            elif v != "":
                x.append('<c r="%s"%s t="inlineStr"><is><t xml:space="preserve">%s</t></is></c>'
                         % (ref, style, escape(str(v))))
        x.append("</row>")
    x.append("</sheetData>")
    if len(donnees) > 1:
        x.append('<autoFilter ref="A1:%s%d"/>' % (col(len(donnees[0]) - 1), len(donnees)))
    x.append("</worksheet>")
    return "".join(x)


STYLES = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
          '<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
          '<fonts count="2"><font><sz val="11"/><name val="Calibri"/></font>'
          '<font><b/><sz val="11"/><name val="Calibri"/></font></fonts>'
          '<fills count="3"><fill><patternFill patternType="none"/></fill>'
          '<fill><patternFill patternType="gray125"/></fill>'
          '<fill><patternFill patternType="solid"><fgColor rgb="FFD9E1F2"/></patternFill></fill></fills>'
          '<borders count="1"><border/></borders>'
          '<cellStyleXfs count="1"><xf/></cellStyleXfs>'
          '<cellXfs count="3"><xf/>'
          '<xf fontId="1" fillId="2" applyFont="1" applyFill="1"/>'
          '<xf applyAlignment="1"><alignment vertical="top" wrapText="1"/></xf></cellXfs>'
          '</styleSheet>')

with zipfile.ZipFile(SORTIE, "w", zipfile.ZIP_DEFLATED) as z:
    n = len(feuilles)
    z.writestr("[Content_Types].xml",
               '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
               '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
               '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
               '<Default Extension="xml" ContentType="application/xml"/>'
               '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
               '<Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>'
               + "".join('<Override PartName="/xl/worksheets/sheet%d.xml" ContentType="application/'
                         'vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>' % (i + 1)
                         for i in range(n))
               + "</Types>")
    z.writestr("_rels/.rels",
               '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
               '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
               '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/'
               'relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>')
    z.writestr("xl/workbook.xml",
               '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
               '<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" '
               'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>'
               + "".join('<sheet name="%s" sheetId="%d" r:id="rId%d"/>' % (escape(nom[:31]), i + 1, i + 1)
                         for i, (nom, _) in enumerate(feuilles))
               + "</sheets></workbook>")
    z.writestr("xl/_rels/workbook.xml.rels",
               '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
               '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
               + "".join('<Relationship Id="rId%d" Type="http://schemas.openxmlformats.org/officeDocument/'
                         '2006/relationships/worksheet" Target="worksheets/sheet%d.xml"/>' % (i + 1, i + 1)
                         for i in range(n))
               + '<Relationship Id="rId%d" Type="http://schemas.openxmlformats.org/officeDocument/2006/'
                 'relationships/styles" Target="styles.xml"/></Relationships>' % (n + 1))
    z.writestr("xl/styles.xml", STYLES)
    for i, (_, donnees) in enumerate(feuilles):
        z.writestr("xl/worksheets/sheet%d.xml" % (i + 1), xml_feuille(donnees))


# --- known_quests.h : la meme table dans le code (futurs checks de quete) ---
def c_texte(s):
    s = re.sub(r"[^\x20-\x7E]", "?", s)
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


CAT_C = {CAT_HISTOIRE: "QueteCat::Histoire", CAT_DILEMME: "QueteCat::Dilemme",
         CAT_REPERTOIRE: "QueteCat::Repertoire", CAT_SECONDAIRE: "QueteCat::Secondaire",
         "Secondaire (Simple Snops)": "QueteCat::SimpleSnops",
         "Secondaire (MiniSnops)": "QueteCat::MiniSnops", CAT_LOI: "QueteCat::Loi"}
h = ["// known_quests.h  --  Table des quetes par scenario (GENERE par",
     "// outils\\quetes_excel.py depuis le releve F9 [QXLS] ; ne pas editer a la main).",
     "// Cle stable = GUID 128 bits de la FrostpunkQuestEntry (quete +0x08), comme",
     "// le hash des technos. Une quete peut figurer dans plusieurs scenarios.",
     "// nomAffiche / nbTaches : langue du jeu au moment du releve.",
     "// Repertoire = rattachee par le repertoire de la quete (aucune source trouvee).",
     "", "#pragma once", "#include <cstdint>", "",
     "enum class QueteCat : uint8_t { Histoire, Dilemme, Repertoire, Secondaire, SimpleSnops, MiniSnops, Loi };", "",
     "struct QueteInfo {",
     "    const char* scenario;     // nom interne (DEFAULT, TechHaven, REFUGEES, WINTERHOME, BUILDERS, Reconquest) ; \"*\" = commune a tous",
     "    uint64_t    guidA, guidB; // quete +0x08 / +0x10",
     "    const char* nom;          // nom interne (quete +0x00)",
     "    const char* nomAffiche;",
     "    QueteCat    categorie;",
     "    int         ordre;        // ordre de decouverte dans le releve (pas un ordre de jeu ; -1 : repli)",
     "    int         nbTaches;",
     "};", "", "static const QueteInfo KNOWN_QUESTS[] = {"]
for scn, cat, ordre, guid in table_code:
    q = quetes[guid]
    h.append("    { %s, 0x%sULL, 0x%sULL, %s, %s, %s, %d, %d }," % (
        c_texte(scn), guid[:16], guid[16:], c_texte(q["interne"]), c_texte(q["nom"]),
        CAT_C.get(cat, "QueteCat::Histoire"), ordre if ordre != 9999 else -1, len(objectifs.get(guid, []))))
h += ["};", "", "static const int KNOWN_QUESTS_COUNT = (int)(sizeof(KNOWN_QUESTS) / sizeof(KNOWN_QUESTS[0]));", ""]
with open("known_quests.h", "w", encoding="ascii", newline="\r\n") as f:
    f.write("\n".join(h))

# --- known_sites.h : les sites d'exploration dans le code (assignation des
#     checks ; meme cle que SITE_PAYLOADS de scooting.cpp : scenario + nom
#     interne du site) -----------------------------------------------------
hs = ["// known_sites.h  --  Sites d'exploration par scenario (GENERE par",
      "// outils\\quetes_excel.py depuis le releve F9 [SXLS] ; ne pas editer a la main).",
      "// Cle = scenario + nom interne du site (segment ExpeditionSites/<dossier>/<nom>",
      "// de la cle du gabarit), comme SITE_PAYLOADS dans scooting.cpp.",
      "// nomAffiche / contenu : langue du jeu et partie au moment du releve",
      "// (contenu vanilla si le site n'etait ni explore ni configure ; \"AP\" = porteur).",
      "// Liste complete = table de tirage du scenario (FrostpunkScenariosConfig +0x368)",
      "// + sites releves ; vuEnPartie = 0 : site possible jamais tire dans un releve",
      "// (nomAffiche et contenu vides).",
      "// surCarte = 1 : site present dans TOUTE partie du scenario (tirage fixe de",
      "// scooting.cpp, releve F9 sur partie neuve) : ce sont eux qui portent un check.",
      "", "#pragma once", "",
      "struct SiteInfo {",
      "    const char* scenario;     // nom interne (DEFAULT, TechHaven, REFUGEES, WINTERHOME, BUILDERS, Reconquest)",
      "    const char* nom;          // nom interne du site (cle de SITE_PAYLOADS)",
      "    const char* dossier;      // Story, Refugees, _SHARED...",
      "    const char* nomAffiche;   // gabarit +0x270 (Display name)",
      "    const char* contenu;      // ressources du site au releve, ex. \"Coal 110, Steel 86\"",
      "    int         nbDilemmes;   // dilemmes d'exploration (gabarit +0x3D0)",
      "    bool        vuEnPartie;   // false : connu seulement par la table de tirage",
      "    bool        surCarte;     // true : sur la carte figee du scenario (porte un check)",
      "};", "", "static const SiteInfo KNOWN_SITES[] = {"]
nb_sites, par_scn = 0, []
for interne, affiche in SCENARIOS:
    liste = sites_complets(interne)
    carte = carte_fixe.get(interne, set())
    for nom, s, chemin in liste:
        sc = "true" if nom in carte else "false"
        if s:
            hs.append("    { %s, %s, %s, %s, %s, %d, true, %s }," % (c_texte(interne), c_texte(s[0]), c_texte(s[1]),
                      c_texte(s[2]), c_texte(s[8]), int(s[9]) if s[9].isdigit() else 0, sc))
        else:
            hs.append("    { %s, %s, %s, \"\", \"\", 0, false, %s }," % (c_texte(interne), c_texte(nom),
                      c_texte(dossier_du_chemin(chemin)), sc))
        nb_sites += 1
    if liste:
        par_scn.append("%s %d (%d vus, %d sur la carte)" % (affiche, len(liste), sum(1 for x in liste if x[1]),
                                                            sum(1 for x in liste if x[0] in carte)))
hs += ["};", "", "static const int KNOWN_SITES_COUNT = (int)(sizeof(KNOWN_SITES) / sizeof(KNOWN_SITES[0]));", ""]
if nb_sites:
    with open("known_sites.h", "w", encoding="ascii", newline="\r\n") as f:
        f.write("\n".join(hs))
    print("known_sites.h : %d sites : %s" % (nb_sites, ", ".join(par_scn)))

compte = {}
for scn, cat, _, _ in table_code:
    compte.setdefault(scn, {}).setdefault(cat, 0)
    compte[scn][cat] += 1
print("%s : %d quetes, %d sans scenario" % (SORTIE, len(quetes), len(quetes) - len(vues)))
for i, a in SCENARIOS:
    print("  %-20s %s" % (a, ", ".join("%s %d" % (c, n) for c, n in sorted(compte.get(i, {}).items()))))
