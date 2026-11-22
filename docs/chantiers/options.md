# Chantier : options du YAML issues des retours des joueurs

Historique. Le résumé est dans CLAUDE.md (section 9).

## Goal partiel (`scenarios_to_win`)

### Origine

Retours des joueurs : pouvoir choisir X scénarios dans le YAML et n'en gagner
que Y. Conception proposée le 2026-11-14 par la session « Options du YAML :
goal partiel et DeathLink », restée sans décision de l'auteur et sans code.
Reprise le 2026-11-21 (session « historique, légende, goal partiel »).

### Décisions de l'auteur (2026-11-21)

- Option `scenarios_to_win` (« Scenarios To Win »), `Range` **1 à 6**,
  **défaut 1** (préféré à un défaut à 6, qui ferait échouer tout YAML de
  moins de 6 scénarios ; conséquence assumée : un YAML sans l'option ne
  demande plus qu'une victoire).
- Y plus grand que le nombre de scénarios du slot (scénario de départ
  compris) : **erreur de génération** (`OptionError`), pas de correction.
- Scénarios qui comptent : **n'importe lesquels** du slot.

### apworld

- `options.py` : `ScenariosToWin`, ajoutée à `FrostpunkOptions` après
  `starting_scenario`.
- `__init__.py` : `generate_early` lève `OptionError` si
  `scenarios_to_win > len(self.scenarios)` ; `completion_condition` = au
  moins Y événements `<scénario> - Victory` ; `slot_data["scenarios_to_win"]`.
- Logique inchangée par ailleurs : tous les scénarios du slot restent dans
  le monde (locations et items), seuls Y victoires sont exigées.
- `release\Frostpunk.yaml` et `release\README.md` : option documentée
  (anglais).

### Mod

- `ApSlotData.scenariosToWin` (lu par `ReseauAP::AppliquerSlotData`,
  absent = 0 : graine d'avant l'option).
- `ApReglerScenariosGoal(scenarios, n, aGagner)` : `g_nAGagner` ; 0 ou plus
  que le slot = tous (compatibilité des anciennes graines).
- `SendScenarioVictory` : goal quand `gagnes >= g_nAGagner` ; message
  `"<scénario> (k/Y scenarios needed)"`, puis `Goal reached` («All scenarios
  of this slot are completed.» si Y = tous, sinon «Y of the N scenarios of
  this slot are completed.»). `ApGoalAtteint` (renvoi du goal à la
  connexion) suit la même règle.
- Journal : `[AP GOAL] k scenario(s) a gagner sur N (scenarios_to_win=…)`,
  `[AP NET] slot_data : … , k a gagner, …`.

### Essais

- 2026-11-21 : DLL compilée. Générations avec l'Archipelago 0.6.7 de
  l'auteur (apworld reconstruit dans `custom_worlds`) : sans l'option (défaut
  1), 6 scénarios / 3, 6 / 6, deux joueurs 2 / 2 : 12/12 OK (3 graines
  chacune), spoiler `Scenarios To Win` conforme ; 2 scénarios / 3 :
  `OptionError` attendue. **En jeu : non testé.**
