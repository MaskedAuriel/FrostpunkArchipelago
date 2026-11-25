# options.py -- Options du YAML (Q8 du journal de .claude/chantiers/apworld.md).

from dataclasses import dataclass

from Options import Choice, OptionSet, PerGameCommonOptions, Range, Toggle

from .data import SCENARIOS

NOMS_SCENARIOS = [s["nom"] for s in SCENARIOS]


class Scenarios(OptionSet):
    """Scenarios included in the multiworld. The goal is to win Scenarios To Win
    of them. Empty (default): only the starting scenario.
    If Random Scenarios is not 0, this list is instead the pool the random
    scenarios are drawn from (empty: all six)."""
    display_name = "Scenarios"
    valid_keys = NOMS_SCENARIOS
    # Vide par defaut (decision de l'auteur, 2026-11-15) : l'ancien defaut
    # {"A New Home"} s'ajoutait au scenario de depart (YAML avec seulement
    # starting_scenario: the_last_autumn -> slot A New Home + Last Autumn).
    default = frozenset()


class StartingScenario(Choice):
    """Scenario unlocked from the start. Only ONE scenario: weighting several
    values picks one of them, it does not add the others to the slot (use
    Scenarios or Random Scenarios for that). If it is not in Scenarios, it is
    added (a warning is printed at generation).
    random_included: a random scenario among the scenarios of the slot."""
    display_name = "Starting Scenario"
    option_a_new_home = 0
    option_the_arks = 1
    option_the_refugees = 2
    option_fall_of_winterhome = 3
    option_the_last_autumn = 4
    option_on_the_edge = 5
    option_random_included = 6   # "random" est reserve par Archipelago
    default = 0


class ScenariosToWin(Range):
    """Number of scenarios of the slot (starting scenario included) to win for
    the goal. Any of them count. Generation fails if it is larger than the
    number of scenarios of the slot."""
    display_name = "Scenarios To Win"
    # Decision de l'auteur (2026-11-21) : 1 a 6, defaut 1, erreur si trop grand.
    range_start = 1
    range_end = 6
    default = 1


class RandomScenarios(Range):
    """0 (default): the slot contains the scenarios listed in Scenarios.
    1-6: the slot contains this many scenarios drawn at random from Scenarios
    (from all six if Scenarios is empty). A starting scenario that is not
    random_included is always in the slot and counts in this number.
    Generation fails if there are not enough scenarios to draw from."""
    display_name = "Random Scenarios"
    # Demande de l'auteur (2026-11-25) apres le retour d'un joueur qui voulait
    # « 2 scenarios au hasard » : aucun YAML ne le permettait.
    range_start = 0
    range_end = 6
    default = 0


class GoalDifficulty(Choice):
    """Difficulty forced by the mod for every scenario. Also sets the number of
    Temperature Rise checks."""
    display_name = "Goal Difficulty"
    option_easy = 0
    option_medium = 1
    option_hard = 2
    option_very_hard = 3
    default = 1


class TrapPercentage(Range):
    """Percentage of filler items replaced by traps."""
    display_name = "Trap Percentage"
    range_start = 0
    range_end = 100
    default = 0


class ExcludePurposePolitics(Toggle):
    """Laws of the Purpose (Faith / Order) and Politics trees only hold filler
    or traps."""
    display_name = "Exclude Purpose and Politics"


class DeliveryHours(Range):
    """Travel time, in game hours, of the survivor group that delivers a site
    content or filler item to the city."""
    display_name = "Delivery Hours"
    range_start = 1
    range_end = 24
    default = 24


@dataclass
class FrostpunkOptions(PerGameCommonOptions):
    scenarios: Scenarios
    random_scenarios: RandomScenarios
    starting_scenario: StartingScenario
    scenarios_to_win: ScenariosToWin
    goal_difficulty: GoalDifficulty
    trap_percentage: TrapPercentage
    exclude_purpose_politics: ExcludePurposePolitics
    delivery_hours: DeliveryHours
