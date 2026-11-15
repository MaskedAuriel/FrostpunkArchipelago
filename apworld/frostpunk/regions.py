# regions.py -- Regions : Menu -> <Scenario> - Start -> une region par goulot.

from BaseClasses import ItemClassification, LocationProgressType, Region
from worlds.generic.Rules import add_rule

from .data import LOCATIONS
from .items import FrostpunkItem
from .locations import FrostpunkLocation, regions_montees
from .rules import regle_location


def creer_regions(world, scenarios, difficulte, exclure_purpose_politics):
    mw, player = world.multiworld, world.player
    menu = Region("Menu", player, mw)
    mw.regions.append(menu)
    victoires = []

    for s in scenarios:
        regions = [Region(nom, player, mw) for nom in s["regions"]]
        mw.regions.extend(regions)

        # Entrees
        unlock = "Scenario Unlock %s" % s["nom"]
        menu.connect(regions[0], "Play %s" % s["nom"],
                     lambda state, u=unlock: state.has(u, player))
        for k in range(1, len(regions)):
            goulot = s["goulots"][k - 1]
            entree = regions[k - 1].connect(
                regions[k], "%s -> %s" % (regions[k - 1].name, regions[k].name),
                lambda state, g=goulot: state.can_reach_location(g, player))
            mw.register_indirect_condition(regions[k - 1], entree)

        # Montees de temperature (decision de l'auteur, 2026-11-14) : les items
        # Temperature Rise vont dans le pool commun ; le goulot de la region k
        # exige les montees reparties sur les regions 0..k (regions_montees,
        # Q4.5 a), la victoire les exige toutes. Garantit un minimum cumule
        # par region, pas un plafond (une regle AP ne peut pas en poser).
        n_montees = s["montees"][difficulte]
        repartition = regions_montees(n_montees, len(regions))
        montee = "%s - Temperature Rise" % s["nom"]

        # Locations (hors temperature)
        for info in LOCATIONS:
            if info["scn"] != s["cle"] or info["genre"] == "temperature":
                continue
            region = regions[info["region"]]
            loc = FrostpunkLocation(player, info["nom"], info["id"], region)
            if exclure_purpose_politics and info["exclue_option"]:
                loc.progress_type = LocationProgressType.EXCLUDED
            regle_location(loc, info, s, player)
            if info["nom"] in s["goulots"]:
                requis = sum(1 for r in repartition if r <= info["region"])
                if requis:
                    add_rule(loc, lambda state, m=montee, n=requis: state.has(m, player, n))
            region.locations.append(loc)

        # Victoire : evenement dans la derniere region (Q4.5 b), toutes les
        # montees du scenario recues (2026-11-14).
        nom_victoire = "%s - Victory" % s["nom"]
        victoire = FrostpunkLocation(player, nom_victoire, None, regions[-1])
        victoire.place_locked_item(FrostpunkItem(nom_victoire, ItemClassification.progression, None, player))
        if n_montees:
            add_rule(victoire, lambda state, m=montee, n=n_montees: state.has(m, player, n))
        regions[-1].locations.append(victoire)
        victoires.append(nom_victoire)

    return victoires
