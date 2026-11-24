# Frostpunk Archipelago

Version 1.0.0 (mod and apworld).

A mod that allow **Frostpunk** to be used in an [Archipelago](https://archipelago.gg)
multiworld randomizer game. Technologies, laws, quests and exploration sites
become checks; the items they unlock (technology tiers, laws, scenarios,
resources, survivor convoys, traps) come from the multiworld.

> Unofficial fan project, not affiliated with 11 bit studios. The texture
> atlases in `mods\` are modified versions of the game's own assets.

## Use of AI

> **This project was built with the help of an AI: Claude (Anthropic), used
> through Claude Code.**

- **What Claude did:** a large part of the code (the C++ mod, the Python
  apworld and the tools in `outils\`), reverse engineering of `Frostpunk.exe`
  (with Ghidra), builds and release packaging, and the work notes.
- **What the author did:** the project idea and its direction, every design
  and game-logic decision (items, checks, rules, options), and all in-game
  testing. Nothing ships without having been played and approved by the author.
- **Where to see it in the repository:** everything specific to Claude lives
  in the [`.claude/`](.claude/) folder: the working instructions
  (`CLAUDE.md`), the per-feature work notes (`chantiers\`, in French) and the
  Claude Code settings.

## Requirements

- **Frostpunk, Epic Games Store or Steam version**, up to date. The mod
  patches the game's code at known addresses and checks which executable it
  is loaded into. Supported builds: Epic (August 2021 build) and Steam
  (October 2025 build). On any other build (GOG, Game Pass, a future update)
  the mod stays **inactive** and the game runs unmodified; the log says so.
- *The Last Autumn* and *On the Edge* are paid DLCs: only include them in your
  YAML if you own them.
- Archipelago **0.6.4 or newer** to generate and host the multiworld.

## Contents of this archive

| Path | What it is |
|---|---|
| `game\version.dll` | The mod (loaded by the game at startup) |
| `game\mods\*.gz` | Replacement icon atlases (Archipelago icons in the tech tree, laws book, map and dilemmas); `ap_buildings_steam.gz` is the Steam variant |
| `archipelago\frostpunk.apworld` | The Archipelago world |
| `archipelago\Frostpunk.yaml` | Example player settings |
| `licenses\` | License of this project (MIT) and of MinHook and nlohmann/json |

## Installation

1. Find the game folder (the folder that contains `Frostpunk.exe`). By default:
   - Epic: `C:\Program Files\Epic Games\Frostpunk\`
   - Steam: `C:\Program Files (x86)\Steam\steamapps\common\Frostpunk\`
     (Steam library > Frostpunk > Manage > Browse local files)
2. Copy **the content of `game\`** into it, so that you get:
   ```
   Frostpunk\Frostpunk.exe
   Frostpunk\version.dll
   Frostpunk\mods\ap_buildings.gz
   Frostpunk\mods\ap_buildings_steam.gz
   Frostpunk\mods\ap_icons_bulles.gz
   Frostpunk\mods\ap_icons_recherche.gz
   Frostpunk\mods\ap_icons_ressources.gz
   ```
   The same files work for both versions; the mod picks the right atlas.
3. The mod writes its own files in `Frostpunk\mods\` (session data, logs).
   If Windows refuses (folder under `Program Files` without write access),
   give your user write permission on the `Frostpunk` folder, or install the
   game elsewhere.

**Uninstall:** delete `version.dll`. You may also delete `mods\`.

## Generating a multiworld

1. Double-click `frostpunk.apworld` (or copy it into
   `Archipelago\custom_worlds\`).
2. Copy `Frostpunk.yaml` into `Archipelago\Players\`, set your `name` and
   options (see below), and generate as usual (`ArchipelagoGenerate`).
3. Host the game (archipelago.gg or `ArchipelagoServer`).

### YAML options

| Option | Values | Effect |
|---|---|---|
| `scenarios` | list among `A New Home`, `The Arks`, `The Refugees`, `Fall of Winterhome`, `The Last Autumn`, `On the Edge` | Scenarios in the multiworld. Empty (default): only the starting scenario. |
| `starting_scenario` | `a_new_home` … `on_the_edge`, `random_included` | Scenario unlocked from the start. Added to `scenarios` if missing (with a warning at generation) |
| `scenarios_to_win` | 1–6 (default 1) | **Goal: win this many scenarios of the slot** (any of them, starting scenario included). Generation fails if it is larger than the number of scenarios of the slot |
| `goal_difficulty` | `easy`, `medium`, `hard`, `very_hard` | Difficulty forced by the mod in every scenario |
| `trap_percentage` | 0–100 | Share of filler items replaced by traps |
| `exclude_purpose_politics` | `true` / `false` | Laws of the Purpose (Faith / Order) and Politics trees only hold filler or traps |
| `delivery_hours` | 1–24 | Travel time (game hours) of the survivor convoy that delivers site contents and filler items |

## Playing

1. Start Frostpunk from the Epic launcher or from Steam.
2. On the **main menu**, fill in the Archipelago panel:
   - **Server**: `archipelago.gg:12345` (or `host:port`).
   - **Slot name**: the `name` of your YAML.
   - **Password**: if the room has one.
   Then click **Connect**. The connection status is shown under the button.
   The scenario, continue, load and new game buttons stay disabled until you
   are connected. Your settings are remembered (`mods\ap_connexion.txt`).
3. Pick an unlocked scenario. Locked scenarios unlock when you receive their
   `Scenario Unlock` item. The difficulty is set by your YAML and cannot be
   changed.

### Checks (locations)

- **Technologies**: researching a technology sends its check. The tech tree
  shows Archipelago icons; the description tells which item is behind it.
- **Laws**: signing a law sends the check of its rank in its tree. The law
  takes effect only once you received the matching `Progressive Law` item.
- **Quests**: completing a story quest of the scenario.
- **Exploration sites**: sites on the world map hold checks (map markers show
  an Archipelago icon). Scouts carry them back to the city.

### Items

- `Progressive <tree>`: next technology tier of that tree. Technologies
  of a tree can only be researched up to the tiers you received.
- `Progressive Law <tree>`: effects of the laws of that tree.
- `Scenario Unlock <scenario>`.
- Site contents and resource crates (wood, coal, steel, raw food): delivered
  by a survivor convoy that walks to your city on the world map.
- `Temperature Rise`: the scenario's temperature drops are removed from the
  game; each one comes back as an item.
- Traps (`Wood / Coal / Steel Trap`): a dilemma bubble costing resources.

A message pops up on the left of the screen for every item received
("from <player>").

### Saves

While the mod is installed, the load menu only shows the saves made **in the
current Archipelago session** (same seed and slot). Your other saves are not
deleted: they come back if you remove the mod. Archipelago progress is kept in
`mods\ap_<seed>_<slot>\` and in `mods\ap_*.txt` files; do not delete them
during a run.

## Known limitations

- Epic (2021 build) and Steam (2025 build) only. A game update changes the
  executable: the mod then disables itself until it is updated.
- Endless mode and The Rifts are not supported.
- The game must be **unpaused** for received items, messages and convoys to
  be applied.

## Bug reports

Every game start writes its own log file in `Frostpunk\mods\logs\`, named
after the date and time the game was started:
`session_YYYY-MM-DD_HH-MM-SS.log`. The 20 most recent ones are kept.

Send the log of the session where the problem happened (the most recent one
if you have just restarted the game once) along with a description of what
happened.

## License

The code of this project (the mod, the apworld, the tools and scripts) is
released under the [MIT License](LICENSE).

Not covered by this license:
- the texture atlases (`*.gz` files in `game\mods\` and `textures\`): they are
  modified versions of Frostpunk's own assets, which belong to 11 bit studios;
- the third-party libraries, under their own licenses: MinHook (BSD 2-Clause,
  `minhook\LICENSE.txt`) and JSON for Modern C++ (MIT, header of
  `nlohmann\json.hpp`).

## Credits

- Mod and apworld: Simon Corté (Masked), with Claude as coding assistant (see
  [Use of AI](#use-of-ai)).
- [MinHook](https://github.com/TsudaKageyu/minhook) — Tsuda Kageyu (BSD 2-Clause).
- [JSON for Modern C++](https://github.com/nlohmann/json) — Niels Lohmann (MIT).
