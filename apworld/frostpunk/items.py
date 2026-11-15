# items.py -- Items de Frostpunk et construction du pool.

from BaseClasses import Item, ItemClassification

from .data import ITEMS

GAME = "Frostpunk"

CLASSES = {
    "progression": ItemClassification.progression,
    # Temperature Rise (2026-11-14) : requis par les goulots, mais exclus de
    # l'equilibrage de progression (sinon tires vers les premieres spheres).
    "progression_skip_balancing": ItemClassification.progression_skip_balancing,
    "useful": ItemClassification.useful,
    "filler": ItemClassification.filler,
    "trap": ItemClassification.trap,
}

ITEM_PAR_NOM = {i["nom"]: i for i in ITEMS}
item_name_to_id = {i["nom"]: i["id"] for i in ITEMS}


class FrostpunkItem(Item):
    game = GAME


def creer_item(nom, player):
    i = ITEM_PAR_NOM[nom]
    return FrostpunkItem(nom, CLASSES[i["classe"]], i["id"], player)


def noms_pool(scenarios, montees_par_scenario):
    """Items des scenarios choisis (hors filler de complement et pieges).
    montees_par_scenario : cle -> nombre de Temperature Rise."""
    noms = []
    for i in ITEMS:
        if i["genre"] in ("filler", "piege"):
            continue
        if i["origine"] not in scenarios:
            continue
        if i["genre"] == "temperature":
            n = montees_par_scenario[i["scn"]]
        else:
            n = i["nombre"]
        noms += [i["nom"]] * n
    return noms


FILLER_GENERIQUE = [i["nom"] for i in ITEMS if i["genre"] == "filler"]
PIEGES = [i["nom"] for i in ITEMS if i["genre"] == "piege"]
