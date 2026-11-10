# locations.py -- Locations de Frostpunk.

from BaseClasses import Location

from .data import LOCATIONS

GAME = "Frostpunk"

location_name_to_id = {l["nom"]: l["id"] for l in LOCATIONS}


class FrostpunkLocation(Location):
    game = GAME


def regions_montees(n, n_regions):
    """Q4.5 a : N montees reparties equitablement, dans l'ordre, entre les
    regions ; le reste va aux premieres regions. Sert au placement des items
    Temperature Rise (FrostpunkWorld.placer_montees, 2026-11-10)."""
    base, reste = divmod(n, n_regions)
    regions = []
    for r in range(n_regions):
        regions += [r] * (base + (1 if r < reste else 0))
    return regions
