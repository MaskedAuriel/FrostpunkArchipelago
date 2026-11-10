import sys, re
from collections import OrderedDict

TACHES = ["Retrieve base resources", "Retrieve bonus resources", "Retrieve both resources",
          "Retrieve technologies", "Retrieve artifacts", "Retrieve survivors", "Reveal",
          "Build outpost", "Build settlement", "Build settlement road", "Send survivors",
          "Escort survivors", "Abandon survivors", "Dismantle outpost", "Hide in shelter"]
SCN = {"DEFAULT": "A New Home", "TechHaven": "The Arks", "REFUGEES": "The Refugees",
       "WINTERHOME": "Fall of Winterhome", "BUILDERS": "The Last Autumn", "Reconquest": "On the Edge"}

lignes = [l.rstrip("\n") for l in open(sys.argv[1], encoding="utf-8", errors="replace") if l.startswith("[SCONT]\t")]

# dilemmes[(scn, site, dnom)] = {titre, choix: {n: texte}, effets: {source: [..]}}
dil = OrderedDict()
site_dil = {}
for l in lignes:
    f = l.split("\t")
    if len(f) < 7: continue
    t = f[1]
    if t == "D":
        scn, site, dnom, n, titre = f[2], f[3], f[5], int(f[6]), f[7]
        k = (scn, site)
        site_dil.setdefault(k, [])
        if dnom not in site_dil[k]: site_dil[k].append(dnom)
        if n >= 2: dil.setdefault((scn, site, dnom), {"titre": titre, "choix": OrderedDict(), "eff": OrderedDict()})
    elif t == "C":
        scn, site, dnom, n, texte = f[2], f[3], f[4], int(f[5]), f[6]
        key = (scn, site, dnom)
        if key in dil: dil[key]["choix"][n] = texte
    elif t == "E":
        scn, site, src, prof, cls = f[2], f[3], f[4], f[5], f[6]
        det = f[7] if len(f) > 7 else ""
        m = re.match(r"dilemme (.+?) choix (\d+)(?: resultat (\d+))?(?: > (.*))?$", src)
        if not m: continue
        key = (scn, site, m.group(1))
        if key not in dil: continue
        cls = cls.replace("Effect", "").replace("Expression", "?")
        if cls == "OrderStationingExpeditionToPerformTask":
            mm = re.search(r"\+18=(-?\d+)", det)
            v = int(mm.group(1)) if mm else 0
            cls = "Tache[%s]" % (TACHES[v] if 0 <= v < len(TACHES) else v)
        elif cls == "DestroyStationingExpedition":
            cls = "ECLAIREURS TUES (neutralise)"
        sous = "resultat %s" % m.group(3) if m.group(3) else "-"
        if m.group(4): sous += " > " + m.group(4)
        lst = dil[key]["eff"].setdefault((int(m.group(2)), sous), [])
        if prof != "0" or cls not in lst: lst.append(cls)

ordre = list(SCN.keys())
for s in ordre:
    print("\n### %s\n" % SCN[s])
    for (scn, site, dnom), d in dil.items():
        if scn != s: continue
        print("- **%s** - `%s` (%s)" % (site, dnom, d["titre"]))
        for n, texte in d["choix"].items():
            branches = [(k[1], v) for k, v in d["eff"].items() if k[0] == n]
            if not branches:
                print("  %d. %s : *aucun effet*" % (n, texte)); continue
            base = [v for b, v in branches if b == "-"]
            alt = [(b, v) for b, v in branches if b != "-"]
            s1 = ", ".join(base[0]) if base else ""
            s2 = " ; ".join("%s : %s" % (b, ", ".join(v)) for b, v in alt)
            print("  %d. %s : %s%s" % (n, texte, s1, (" | " if s1 and s2 else "") + s2))
print("\n%d dilemmes" % len(dil))


