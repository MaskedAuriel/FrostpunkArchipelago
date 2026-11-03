# Difficulte imposee aux parties Archipelago

> Historique detaille du chantier. Le resume a jour est dans CLAUDE.md ; les
> nouveaux resultats de test s'ajoutent ici.

- **Objectif (2026-11-03)** : une partie neuve d'un scénario AP se lance à la
  **difficulté générale du YAML** (décision Q2 de `apworld.md` : le goal
  compte la victoire à cette difficulté ; le nombre d'items `Temperature
  Rise` en dépend aussi, `temperature.md`).
- **Décisions de l'auteur (2026-11-03)** :
  1. difficulté **imposée**, curseurs **figés** dans l'interface ;
  2. personnalisation (Customize) : **toutes les lignes au niveau cible** ;
  3. les **6 scénarios AP** seulement (`DEFAULT`, `TechHaven`, `REFUGEES`,
     `WINTERHOME`, `BUILDERS`, `Reconquest`) ; **ironman libre** (non
     touché).
- **Décompilés (2026-11-03)**, `FrostpunkUIMainMenuScenariosPanel` :
  - `FUN_141b2bc20` (init) : `+0x140` poignée du curseur principal
    (`MAIN_DIFFICULTY_SLIDER` > `SLIDER`, plage 0..3, écouteur de valeur
    `FUN_141b2d810` qui recopie la valeur sur toutes les lignes) ; une ligne
    par curseur de `FrostpunkUIConfig +0xDC0` (`DAT_144062380`, 0xB8) créée
    par `FUN_141b2ac50` → paire `{curseur, widget}` triée dans `+0x1C8`
    (compte `+0x1D0`). Boutons : `START_BUTTON` → `FUN_141b2dcc0`,
    `CUSTOMIZE_BUTTON` (`+0x120`) → `LAB_141b2d590`, `DEFAULTS_BUTTON` →
    `LAB_141b2d900`, `IRON_MAN_MODE_BUTTON` (`+0x150`) → `FUN_141b2d7b0`.
  - États (`FUN_141b2b1b0`) : 0 liste, 1 détails, 2 personnalisation
    (`CUSTOMISATION`). Scénario affiché : `+0x198[+0x1B0]`.
  - Widget curseur : **`+0x314` = niveau** (0 Easy, 1 Medium, 2 Hard,
    3 Very Hard), `+0x31C` max. `FUN_141518b70(widget, niveau, forcer)` pose
    le niveau (poignée + écouteur).
  - `FUN_141b2e410` (fin de `FUN_141b2e5d0`, changement de scénario) : tous
    les curseurs à Medium, visibilité des lignes selon le scénario, puis
    `FUN_141b2e8c0(panneau, 0)`.
  - `FUN_141b2e8c0(panneau, ironman)` : `+0x1E0 = ironman`, Customize actif
    si pas ironman, masque flou. Appelée aussi par le bouton ironman.
  - `FUN_141b2a9c0` (ApplyConfig, via `FUN_141b2ea10` au lancement) : sans
    ironman, chaque ligne → `FUN_1412f9b20(DAT_142c3cb58, réglage, niveau)`
    selon `+0x314` ; avec ironman, la difficulté prédéfinie
    `DAT_144062398`.
  - `FUN_14105a3a0(élément, actif, 1)` : active / désactive un élément.
- **Implémentation (2026-11-03, `scenarios_ap.cpp`)** :
  - hook de `FUN_141b2e8c0` (après l'original) : scénario AP affiché, pas
    ironman → curseur principal et toutes les lignes posés au niveau cible
    (`FUN_141518b70`, forcé) puis **désactivés** ; sinon, s'ils avaient été
    figés, réactivés. Log `[DIFF] "<scn>" : N curseurs figes a <niveau>` /
    `curseurs liberes`.
  - hook de `FUN_141b2a9c0` (avant l'original) : **filet** — chaque `+0x314`
    réécrit au niveau cible. Log `[DIFF] lancement de "<scn>" : difficulte
    imposee <niveau> (N curseurs, M corriges)` ou `difficulte libre
    (ironman | hors Archipelago)`.
  - Cible : `DIFFICULTE_TEST = 2` (Hard), **TEMPORAIRE** (viendra de
    `slot_data`).
  - Non testé. À vérifier : curseurs à Hard et grisés (détails et
    Customize), Start → partie en Hard (`[FIN]` / F9 `[TEMP]`), passage à
    un scénario non AP (Endless) → curseurs libérés, ironman → libre puis
    re-figé en le retirant, bouton Defaults.
- **1er test (2026-11-03, Refugees)** : curseurs figés à Hard,
  **immobiles mais pas grisés** (accepté par l'auteur : « pas grave ») ;
  ironman → `curseurs liberes`, retiré → `7 curseurs figes a Hard`. Mais
  `[!] hook ApplyConfig KO` : `scenario.cpp` avait déjà un hook sur
  `FUN_141b2a9c0` (capture de `g_scenarioSelf`), que le nouveau, posé
  avant, faisait échouer. → plus de second hook :
  `DifficulteImposerAuLancement(panneau)` (scenarios_ap) appelée par
  `hkApplyConfig` (scenario.cpp) avant l'original. Aucune ligne
  `[DIFF] lancement` dans ce journal.
- **Ironman (décision de l'auteur, 2026-11-03)** : laissé libre au menu,
  mais une **victoire en ironman compte au moins au niveau cible**, même si
  le joueur a annoncé un niveau plus bas dans le YAML. `victoire.cpp` :
  `PartieIronman()` (`DifficultySystem +0x48` « IronmanMode ») → chaque
  ligne relevée sous la cible est comptée à la cible (le niveau général
  suit). Log `[FIN]   partie en ironman : …`. Non testé.
- **2e test (2026-11-03, Refugees en ironman, Maj+F9)** : `[DIFF]
  lancement de "REFUGEES" : difficulte libre (ironman)`, puis à la
  victoire `partie en ironman …` et **les 4 lignes lues à Very Hard** (le
  préréglage ironman du jeu = Very Hard partout) → niveau général Very
  Hard, au-dessus de la cible. **Ironman VALIDÉ.** Le lancement hors
  ironman (`difficulte imposee Hard`) n'a pas encore été vu.
- **Décision de l'auteur (2026-11-03)** : les sous-difficultés n'importent
  plus, **seule la difficulté principale compte**. Mode de victoire
  **Detail abandonné** : commenté dans `victoire.h` / `victoire.cpp` (pas
  supprimé) ; `MODE_TEST = General`. Les lignes restent lues, puisque le
  niveau général en est déduit (le plus faible). Non testé.
- **3e test (2026-11-03, Refugees sans ironman, Maj+F9) : VALIDÉ.**
  `[DIFF] "REFUGEES" : 7 curseurs figes a Hard`, `lancement … difficulte
  imposee Hard (7 curseurs, 0 corriges)` (le filet n'a rien eu à
  corriger : le verrou de l'interface suffit), victoire → 4 lignes à Hard,
  niveau général Hard, mode General → checks `Refugees - Victory (Easy)`,
  `(Medium)`, `(Hard)`. **Chantier terminé** côté mod ; reste à brancher
  la cible sur `slot_data` avec le réseau.
