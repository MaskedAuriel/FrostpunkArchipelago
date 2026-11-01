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

with open(JOURNAL, "rb") as f:
    for brut in f:
        ligne = brut.decode("cp1252", errors="replace").rstrip("\r\n")
        if not ligne.startswith("[QXLS]\t"):
            continue
        c = [decoder(x) for x in ligne.split("\t")[1:]]
        if c[0] == "Q" and len(c) >= 7:
            quetes[c[4]] = {"interne": c[1], "rep": c[2], "type": c[3], "nom": c[5], "desc": c[6]}
            objectifs[c[4]] = []
        elif c[0] == "O" and len(c) >= 8:
            objectifs.setdefault(c[1], []).append((int(c[2]), c[3], c[4], c[5] == "1", c[6], c[7]))
        elif c[0] == "S" and len(c) >= 6:
            par_scenario.setdefault(c[1], []).append((int(c[2]), c[3], c[4], c[5]))

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
    x = ['<?xml version="1.0" encoding="UTF-8" standalone="yes"?>',
         '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">',
         '<sheetViews><sheetView workbookViewId="0"><pane ySplit="1" topLeftCell="A2" '
         'activePane="bottomLeft" state="frozen"/></sheetView></sheetViews><cols>']
    for i, w in enumerate(LARGEURS):
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
        x.append('<autoFilter ref="A1:%s%d"/>' % (col(len(ENTETE) - 1), len(donnees)))
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

compte = {}
for scn, cat, _, _ in table_code:
    compte.setdefault(scn, {}).setdefault(cat, 0)
    compte[scn][cat] += 1
print("%s : %d quetes, %d sans scenario" % (SORTIE, len(quetes), len(quetes) - len(vues)))
for i, a in SCENARIOS:
    print("  %-20s %s" % (a, ", ".join("%s %d" % (c, n) for c, n in sorted(compte.get(i, {}).items()))))
