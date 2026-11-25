# Chantier : options du YAML issues des retours des joueurs

Historique. Le résumé est dans .claude/CLAUDE.md (section 9).

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

## Scénarios tirés au hasard (`random_scenarios`)

### Origine (2026-11-25)

Un joueur voulait « gagner 2 scénarios au hasard ». Son YAML (modèle
généré par Archipelago) : `scenarios: []`, `starting_scenario` pondéré à 50
sur les six, `scenarios_to_win: 2` → `OptionError` à chaque génération. Il
pensait que pondérer `starting_scenario` ajoutait plusieurs scénarios : le
tirage n'en retient qu'**un**, et la liste vide ne garde que le départ.
Aucun YAML ne permettait un tirage de N scénarios exactement (essais avec
l'exe 0.6.7) : `scenarios: random` = sous-ensemble de taille aléatoire (3
échecs sur 12 graines avec `scenarios_to_win: 2`) ; `["random"]`,
`random-2` et paires pondérées (`"A New Home, The Arks": 1`, l'OptionSet lit
le dictionnaire comme une liste de clés) = YAML invalide.

### Décision de l'auteur (2026-11-25)

Ajouter l'option, sans conflit avec la sélection manuelle.

### apworld

- `RandomScenarios` (`random_scenarios`, `Range` 0 à 6, défaut 0 = liste
  telle quelle). N > 0 : slot = N scénarios tirés dans `scenarios` (les six
  si vide) ; `scenarios` devient la **réserve** du tirage. Départ fixe :
  toujours dans le slot, compte dans N (ajouté avec l'avertissement s'il
  est hors de la liste). `random_included` : départ tiré parmi les
  scénarios retenus. Réserve trop courte = `OptionError`.
- Message d'erreur de `scenarios_to_win` complété (lister plus de
  scénarios, ou `random_scenarios` ; pondérer le départ n'en prend qu'un).
- Docstrings de `Scenarios` et `StartingScenario` (modèle de YAML généré)
  précisées. Spoiler : lignes `Slot Scenarios` et `Slot Starting Scenario`
  (`write_spoiler_header`), les options seules ne montrant pas le tirage.
- `slot_data` inchangé (liste des scénarios retenus) : **rien côté DLL**.
- `release\Frostpunk.yaml` et `README.md` : option documentée (anglais).
