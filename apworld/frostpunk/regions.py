# regions.py -- Regions : Menu -> <Scenario> - Start -> une region par goulot.

from BaseClasses import ItemClassification, LocationProgressType, Region

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

        # Locations (hors temperature)
        for info in LOCATIONS:
            if info["scn"] != s["cle"] or info["genre"] == "temperature":
                continue
            region = regions[info["region"]]
            loc = FrostpunkLocation(player, info["nom"], info["id"], region)
            if exclure_purpose_politics and info["exclue_option"]:
                loc.progress_type = LocationProgressType.EXCLUDED
            regle_location(loc, info, s, player)
            region.locations.append(loc)

        # Montees de temperature (nombre selon la difficulte)
        n = s["montees"][difficulte]
        for k, r in enumerate(regions_montees(n, len(regions))):
            nom = s["temperature"][k]
            info = next(l for l in LOCATIONS if l["nom"] == nom)
            regions[r].locations.append(FrostpunkLocation(player, nom, info["id"], regions[r]))

        # Victoire : evenement dans la derniere region (rien de plus, Q4.5 b)
        nom_victoire = "%s - Victory" % s["nom"]
        victoire = FrostpunkLocation(player, nom_victoire, None, regions[-1])
        victoire.place_locked_item(FrostpunkItem(nom_victoire, ItemClassification.progression, None, player))
        regions[-1].locations.append(victoire)
        victoires.append(nom_victoire)

    return victoires
