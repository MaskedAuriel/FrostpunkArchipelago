# Frostpunk -- apworld (logique dictee par l'auteur : .claude/chantiers/apworld.md).
# Les tables (technos, lois, sites, quetes, ids) sont dans data.py, GENERE par
# apworld/outils/generer.py depuis les tables du mod.

import logging

from Options import OptionError
from worlds.AutoWorld import WebWorld, World

from .data import DIFFICULTIES, SCENARIOS
from .items import FILLER_GENERIQUE, PIEGES, ITEM_PAR_NOM, creer_item, item_name_to_id, noms_pool
from .locations import location_name_to_id
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
        depart_au_hasard = (self.options.starting_scenario ==
                            self.options.starting_scenario.option_random_included)
        depart = None if depart_au_hasard else NOMS_SCENARIOS[self.options.starting_scenario.value]
        # Depart hors d'une liste non vide : ajoute, avec un avertissement
        # (2026-11-15 : le slot contient alors un scenario de plus a gagner).
        if depart and noms and depart not in noms:
            logging.warning(
                f"Frostpunk ({self.player_name}): starting scenario '{depart}' is not in "
                f"scenarios {sorted(noms)}; it is added to the slot.")

        # Scenarios tires au hasard (2026-11-25) : random_scenarios = N > 0 ->
        # N scenarios tires dans « scenarios » (les six si vide) ; un depart
        # fixe est toujours dans le slot et compte dans N.
        n_hasard = self.options.random_scenarios.value
        if n_hasard:
            reserve = [n for n in NOMS_SCENARIOS if not noms or n in noms]
            tires = {depart} if depart else set()
            candidats = [n for n in reserve if n not in tires]
            manque = n_hasard - len(tires)
            if manque > len(candidats):
                raise OptionError(
                    f"Frostpunk ({self.player_name}): random_scenarios is {n_hasard} but there "
                    f"are only {len(candidats) + len(tires)} scenario(s) to draw from "
                    f"({', '.join(sorted(set(candidats) | tires))}). Add scenarios to "
                    f"'scenarios' (or leave it empty to draw from all six), or lower "
                    f"random_scenarios.")
            tires.update(self.random.sample(candidats, manque))
            noms = tires
            logging.info(f"Frostpunk ({self.player_name}): random scenarios: {sorted(noms)}")

        if depart_au_hasard:
            depart = self.random.choice(sorted(noms) if noms else NOMS_SCENARIOS)
        noms.add(depart)
        self.depart = depart
        self.scenarios = [s for s in SCENARIOS if s["nom"] in noms]
        self.difficulte = self.options.goal_difficulty.value
        # Goal partiel (decision de l'auteur, 2026-11-21) : gagner Y scenarios
        # quelconques du slot ; Y plus grand que le slot = erreur de generation.
        self.a_gagner = self.options.scenarios_to_win.value
        if self.a_gagner > len(self.scenarios):
            raise OptionError(
                f"Frostpunk ({self.player_name}): scenarios_to_win is {self.a_gagner} but the "
                f"slot only has {len(self.scenarios)} scenario(s) "
                f"({', '.join(s['nom'] for s in self.scenarios)}). List more scenarios in "
                f"'scenarios', or set random_scenarios to draw them at random. Weighting "
                f"starting_scenario only picks ONE scenario.")

    def write_spoiler_header(self, spoiler_handle):
        # Scenarios reellement retenus (tirage de random_scenarios / random_included)
        spoiler_handle.write(f"Slot Scenarios:                  "
                             f"{', '.join(s['nom'] for s in self.scenarios)}\n")
        spoiler_handle.write(f"Slot Starting Scenario:          {self.depart}\n")

    def create_regions(self):
        self.victoires = creer_regions(self, self.scenarios, self.difficulte,
                                       bool(self.options.exclude_purpose_politics))

    def create_item(self, name):
        return creer_item(name, self.player)

    def get_filler_item_name(self):
        return self.random.choice(FILLER_GENERIQUE)

    def create_items(self):
        cles = {s["cle"] for s in self.scenarios}
        montees = {s["cle"]: s["montees"][self.difficulte] for s in self.scenarios}
        noms = noms_pool(cles, montees)

        # Scenario de depart : recu d'office
        unlock = "Scenario Unlock %s" % self.depart
        noms.remove(unlock)
        self.multiworld.push_precollected(self.create_item(unlock))

        # Temperature Rise : dans le pool commun ; leur repartition par region
        # vient des regles des goulots (regions.py, 2026-11-14). Le placement
        # d'office dans le monde du joueur (placer_montees, 2026-11-10) est
        # retire : il les verrouillait tous chez lui.

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
        a_gagner = self.a_gagner
        self.multiworld.completion_condition[self.player] = \
            lambda state: sum(1 for v in victoires if state.has(v, self.player)) >= a_gagner

    def fill_slot_data(self):
        return {
            "scenarios": [s["cle"] for s in self.scenarios],
            "starting_scenario": next(s["cle"] for s in self.scenarios if s["nom"] == self.depart),
            "scenarios_to_win": self.a_gagner,                     # ApReglerScenariosAGagner
            "goal_difficulty": DIFFICULTIES[self.difficulte],
            "exclude_purpose_politics": bool(self.options.exclude_purpose_politics),
            "delivery_hours": self.options.delivery_hours.value,   # ApReglerHeuresLivraison
        }
