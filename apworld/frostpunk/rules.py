# rules.py -- Regles de logique (journal de .claude/chantiers/apworld.md).
#
# Ce que l'apworld exige (le reste est porte par la region de la location) :
#   - techno : Progressive Tree Tier x tiers ; tiers et region sont deja
#     cumules sur le rang precedent et le parent par generer.py (Q4.1) ;
#   - site : Beacon / Forager Quarters recu (Q4.3) ;
#   - quete GOULOT de la region k : tous les items Progressive Tree
#     "progression" de la region k recus (seuil et items calcules par
#     generer.py ; decision de l'auteur du 2026-11-04) ;
#   - quete GOULOT de la region k, en plus : les Temperature Rise reparties
#     sur les regions 0..k (regions_montees) ; victoire : toutes les montees
#     (posees dans regions.py, decision de l'auteur du 2026-11-14) ;
#   - autre quete, loi : rien (Q4.2, Q4.4).
# Entrees : scenario = Scenario Unlock ; region k = goulot k fini.

from worlds.generic.Rules import set_rule


def progressifs_de_region(state, player, seuil, items):
    """Items progressifs de la region recus : pour chaque item, ceux qui
    depassent les checks des regions precedentes, plafonnes a la region."""
    total = 0
    for nom, avant, n in items:
        total += min(max(state.count(nom, player) - avant, 0), n)
        if total >= seuil:
            return True
    return False


def regle_location(location, info, scenario, player):
    exigences = []
    if info["tiers"] > 0:
        exigences.append((scenario["paliers"], info["tiers"]))
    if info["eclaireurs"] and scenario["eclaireurs"]:
        exigences.append((scenario["eclaireurs"], 1))
    progressifs = None
    if info["genre"] == "quete" and info["nom"] in scenario["goulots"] \
            and info["region"] in scenario["quetes_progressifs"]:
        progressifs = scenario["quetes_progressifs"][info["region"]]
    if exigences or progressifs:
        set_rule(location, lambda state, e=tuple(exigences), p=progressifs:
                 all(state.has(nom, player, n) for nom, n in e)
                 and (p is None or progressifs_de_region(state, player, p[0], p[1])))
