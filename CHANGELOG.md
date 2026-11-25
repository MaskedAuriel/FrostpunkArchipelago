# Changelog

## 1.0.1 — 2026-11-25

### Added
- **New YAML option `random_scenarios`** (0–6, default 0). Set it to N to
  get N scenarios **drawn at random** for your slot, from the scenarios listed
  in `scenarios` (from all six if the list is empty). A fixed
  `starting_scenario` is always included and counts in N. With 0, nothing
  changes: the slot holds the scenarios listed in `scenarios`.
  If you don't own *The Last Autumn* and *On the Edge*, list the four base
  scenarios in `scenarios`: an empty list draws from all six.
- The spoiler log now shows the scenarios of each slot (`Slot Scenarios`) and
  its starting scenario (`Slot Starting Scenario`).

### Changed
- Clearer error at generation when `scenarios_to_win` is larger than the
  number of scenarios in the slot: it tells you to list more scenarios or to
  use `random_scenarios`.
- Clearer help for `starting_scenario`: it picks **one** scenario. Weighting
  several values in the YAML picks one of them; it does not add the others to
  the slot.

### Compatibility
- Only the apworld changed: `version.dll` and the icon atlases are the same as
  in 1.0.0. A player still on the 1.0.0 mod can play a seed generated with
  the 1.0.1 apworld.

## 1.0.0 — 2026-11-22

First public release.

- Checks: technologies, laws, story quests and exploration sites in 6
  scenarios (A New Home, The Arks, The Refugees, Fall of Winterhome, The Last
  Autumn, On the Edge).
- Items: progressive technology tiers and laws, scenario unlocks, resources
  and site contents delivered by survivor convoys, temperature rises, traps.
- Goal: win `scenarios_to_win` scenarios of the slot at the difficulty forced
  by the YAML.
- Archipelago connection panel on the main menu, session history panel in the
  HUD, Archipelago icons in the tech tree, the book of laws and on the map.
- Supported game builds: Steam (October 2025) and Epic Games Store (August
  2021).
