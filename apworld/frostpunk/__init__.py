# Frostpunk -- apworld (logique dictee par l'auteur : docs/chantiers/apworld.md).
# Les tables (technos, lois, sites, quetes, ids) sont dans data.py, GENERE par
# apworld/outils/generer.py depuis les tables du mod.

from BaseClasses import LocationProgressType
from worlds.AutoWorld import WebWorld, World

from .data import DIFFICULTIES, SCENARIOS
from .items import FILLER_GENERIQUE, PIEGES, ITEM_PAR_NOM, creer_item, item_name_to_id, noms_pool
from .locations import location_name_to_id, regions_montees
from .options import NOMS_SCENARIOS, FrostpunkOptions
from .regions import creer_regions


class FrostpunkWeb(WebWorld):
    theme = "ice"


class FrostpunkWorld(World):
    """Frostpunk: a society survival game. Research technologies, sign laws,
    send scouts and survive the cold across six scenarios."""

    game = "Frostpunk"
    web = FrostpunkWeb()
    options_dataclass = FrostpunkOptions
    options: FrostpunkOptions
    item_name_to_id = item_name_to_id
    location_name_to_id = location_name_to_id

    def generate_early(self):
        noms = set(self.options.scenarios.value)
        if self.options.starting_scenario == self.options.starting_scenario.option_random_included:
            depart = self.random.choice(sorted(noms)) if noms else "A New Home"
        else:
            depart = NOMS_SCENARIOS[self.options.starting_scenario.value]
        noms.add(depart)
        self.depart = depart
        self.scenarios = [s for s in SCENARIOS if s["nom"] in noms]
        self.difficulte = self.options.goal_difficulty.value

    def create_regions(self):
        self.victoires = creer_regions(self, self.scenarios, self.difficulte,
                                       bool(self.options.exclude_purpose_politics))

    def create_item(self, name):
        return creer_item(name, self.player)

    def get_filler_item_name(self):
        return self.random.choice(FILLER_GENERIQUE)

    def placer_montees(self, noms):
        """Repartition equitable des Temperature Rise (Q4.5 a, decision de
        l'auteur du 2026-11-10) : dans chaque scenario, la k-ieme montee va
        dans une location libre et non exclue, tiree au hasard, de la region
        regions_montees(n, nombre de regions)[k] (le reste aux premieres
        regions). Items useful : aucun effet sur la logique. Les items places
        sont retires de 'noms'."""
        for s in self.scenarios:
            nom = "%s - Temperature Rise" % s["nom"]
            n = noms.count(nom)
            if not n:
                continue
            regions = [self.multiworld.get_region(r, self.player) for r in s["regions"]]
            for r in regions_montees(n, len(regions)):
                # Region visee, a defaut la plus proche avant elle, puis apres.
                ordre = list(range(r, -1, -1)) + list(range(r + 1, len(regions)))
                for k in ordre:
                    libres = [l for l in regions[k].locations
                              if l.item is None and l.address is not None
                              and l.progress_type != LocationProgressType.EXCLUDED]
                    if libres:
                        self.random.choice(libres).place_locked_item(self.create_item(nom))
                        noms.remove(nom)
                        break

    def create_items(self):
        cles = {s["cle"] for s in self.scenarios}
        montees = {s["cle"]: s["montees"][self.difficulte] for s in self.scenarios}
        noms = noms_pool(cles, montees)

        # Scenario de depart : recu d'office
        unlock = "Scenario Unlock %s" % self.depart
        noms.remove(unlock)
        self.multiworld.push_precollected(self.create_item(unlock))

        # Temperature Rise : places d'office, repartis entre les regions
        self.placer_montees(noms)

        places = len(self.multiworld.get_unfilled_locations(self.player))
        # Trop d'items : on retire d'abord le contenu des sites (filler, puis useful)
        for classe in ("filler", "useful"):
            for nom in [n for n in noms if ITEM_PAR_NOM[n]["genre"] == "site" and ITEM_PAR_NOM[n]["classe"] == classe]:
                if len(noms) <= places:
                    break
                noms.remove(nom)
        if len(noms) > places:
            raise Exception("Frostpunk : %d items pour %d locations" % (len(noms), places))
        # Pas assez : filler de complement
        while len(noms) < places:
            noms.append(self.get_filler_item_name())

        # Pieges : une part du filler (Q6 c)
        fillers = [i for i, n in enumerate(noms) if ITEM_PAR_NOM[n]["classe"] == "filler"]
        n_pieges = len(fillers) * self.options.trap_percentage.value // 100
        for i in self.random.sample(fillers, n_pieges):
            noms[i] = self.random.choice(PIEGES)

        self.multiworld.itempool += [self.create_item(n) for n in noms]

    def set_rules(self):
        victoires = self.victoires
        self.multiworld.completion_condition[self.player] = \
            lambda state: all(state.has(v, self.player) for v in victoires)

    def fill_slot_data(self):
        return {
            "scenarios": [s["cle"] for s in self.scenarios],
            "starting_scenario": next(s["cle"] for s in self.scenarios if s["nom"] == self.depart),
            "goal_difficulty": DIFFICULTIES[self.difficulte],
            "exclude_purpose_politics": bool(self.options.exclude_purpose_politics),
            "delivery_hours": self.options.delivery_hours.value,   # ApReglerHeuresLivraison
        }
