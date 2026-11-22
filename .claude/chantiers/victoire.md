# Difficulte, victoire / defaite et checks de victoire

> Historique detaille, copie mot pour mot depuis .claude/CLAUDE.md le 2026-11-02.
> Le resume a jour est dans .claude/CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

- **Revue de la détection de la difficulté (2026-11-01)** : réflexions
  `FUN_1412f9360` (`FrostpunkDifficultySystem`, 0x80 : `+0x30`
  « Difficulty » = Map `{EntryPtr réglage, EntryPtr difficulté}` au pas de
  0x10, compte `+0x38` ; `+0x48` IronmanMode), `FUN_1413dfe30`
  (`FrostpunkDifficultySettingEntry`, 0x38 : `+0x00` nom, `+0x08` GUID,
  `+0x18` Difficulties, compte `+0x20`), `FUN_1413dfbe0`
  (`FrostpunkDifficultyEntry`, 0x38 : `+0x00` nom, `+0x08` GUID, `+0x18`
  Default, `+0x20` Modifications) → `CaptureRunState` /
  `LireDifficultesRuntime` **conformes**. `g_run.diff` n'est lu par
  personne (seul le relevé F9 de la température lit le système). Menu :
  `FUN_141b2dcc0` (« SelectScenario ») = bouton **Start** du panneau des
  scénarios ; `FUN_141b2a9c0` (« ApplyConfig ») n'applique les curseurs
  (`obj +0x314`) que si `panneau +0x1E0 == 0`, sinon la difficulté
  prédéfinie `DAT_144062398` (mode ironman présumé). `LireDifficultes()` :
  code mort (ancien balayage, mauvaise disposition).
  **Bug corrigé** : `ScenarioUsesGroup` (groupes de technos du scénario,
  utilisé par `TechAtTreePosition` à la réception d'une progressive) et
  `ScenarioVictoryId` lisaient `g_scenario` (menu : « Start » ou
  `SetScenario`), vide après « Continue » / « Load » et périmé après le
  chargement d'une sauvegarde d'un autre scénario → lisent désormais
  `g_run.scenario` (système de scénario), repli sur `g_scenario` hors
  partie. Non testé.
- **Victoire / défaite et checks de victoire (2026-11-01, non testé)**.
  Constat : `CheckScenarioVictory` / `ScenarioJustWon` (scenario.cpp)
  n'étaient appelées nulle part et lisaient `ScenarioSystem +0x78` =
  `Ignored_LastScenarioInSessionVictory` (réflexion `FUN_14131eeb0` :
  souvenir de session, pas un drapeau en direct) → **retirées**.
  Réflexion de `FrostpunkScenarioSystem` (0x130) : `+0x68` CurrentScenario,
  `+0x80` CurrentCustomScenario, `+0x88..+0x108` réglages de la partie,
  `+0x110` CurrentGameId, `+0x120` CurrentGameAct, `+0x128` début de l'acte.
  Décompilés :
  - **`FUN_141301490(état, fin, cinématique)` =
    `FrostpunkGameStateGameplay::EndGame`**, seul appelant
    `FrostpunkEndGameEffect` (`FUN_14168dea0`, fin = lien `+0x18`,
    cinématique `+0x28`) ; `état = DAT_142c30158` : `+0x50` phase (3 =
    GAMEPLAY), `+0x58` fin en attente (une 2e demande est ignorée).
  - `FrostpunkGameEndingEntry` (0x20, `FUN_1412b02d0`) : `+0x00` nom,
    `+0x08` GUID, **`+0x18` Victory**.
  - Curseurs de difficulté : `FrostpunkUIConfig` (`base + 0x40615C0`)
    **`+0xDC0` « DifficultySliders »** (objets intégrés de 0xB8, compte
    `+0xDC8`), `+0xDD8` « Ironman Difficulty Entries » (= `DAT_144062398`
    d'`ApplyConfig` : `panneau +0x1E0` est donc le **mode ironman**),
    `+0xDF0` descriptions du curseur principal. Curseur (`FUN_141977ee0`) :
    `+0x00` DifficultyName (clé), `+0x08` 4 niveaux de 0x20
    (`eSliderDifficultyLevel` : Easy, Medium, Hard, Very Hard ; `+0x00` lien
    GUID vers un `FrostpunkDifficultyEntry`, `+0x18` Description), `+0x88`
    « Visible in scenarios » (liens 0x10, vide = tous), `+0xA0` « Invisible
    in scenarios ».
  **Système voulu par l'auteur** : 3 modes — **Complete** (un check par
  scénario gagné), **General** (un check par niveau général), **Detail**
  (un check par sous-difficulté et par niveau, 4 par ligne). Décisions du
  2026-11-01 : **cumulatif** (gagner en Hard = checks Easy + Medium + Hard) ;
  niveau général d'une difficulté personnalisée = **le plus faible** des
  lignes (il n'est stocké nulle part en partie).
  → `victoire.cpp` : hook de `EndGame` (avant l'original, sous SEH) : log
  `[FIN] fin de partie "…" : VICTOIRE / DEFAITE (scenario …)` pour toute
  fin (refus du jeu signalé) ; victoire acceptée dans un des 6 scénarios →
  lignes visibles du scénario, niveau de chacune (GUID de niveau parmi les
  difficultés en vigueur, `DifficultySystem +0x30`), niveau général, puis
  checks `"<Scénario> - Victory"`, `"… - Victory (Hard)"`, `"… - Victory
  (<ligne>: Hard)"` (ligne = dernier segment de la clé `DifficultyName`) par
  `SendVictoryCheck` (popup « Check sent »), sans doublon dans la session.
  Mode = `MODE_TEST = Detail` (TEMPORAIRE, viendra des options du slot).
  Inconnus : noms des fins par scénario, lignes visibles par scénario,
  détection du niveau en ironman.
  **Test sans finir un scénario (2026-11-01)** : **Maj+F9 = victoire,
  Ctrl+F9 = défaite** (F9 seul reste le relevé des quêtes) →
  `VictoireDemanderTest` → `VictoirePompe` (thread du jeu, hook du setter de
  ressources, jeu **hors pause**) : liste les fins de la config
  (`FrostpunkGameConfig` = `base + 0x2C38DE0`, déduit de
  PhotoModeCameraEffects `+0x1E0` = `DAT_142c38fc0` dans `FUN_141ba79e0` ;
  réflexion `FUN_1412af2b0` : **`+0xB0` « Game endings »**, pointeurs, compte
  `+0xB8` ; collection des fins `DAT_142c38e70`), prend la 1re du type voulu
  et appelle `EndGame(*(base + 0x2C30158), fin, 0)` (passe par le hook).
  **La partie se termine vraiment** (sauvegarder avant). Non testé.
  **1er test Maj+F9 (2026-11-02, Refugees puis A New Home)** : 12 fins
  lues dans la config (adresse confirmée) — victoires `Main Story Ending`,
  `Tech Haven Ending`, `Refugees Ending`, `Winterhome Ending`, `Builders
  Ending`, `Builders Ending - Unfinished`, `DLC3 - Help`, `DLC3 -
  TakePopulation` ; défaites `Fail`, `Builders Ending - Fail`, `DLC3 - Help
  Unfinished`, `DLC3 - TakePopulation Unfinished`. EndGame passé par le hook
  (`VICTOIRE (scenario REFUGEES)`, puis `DEFAULT`). **4 lignes visibles**
  dans les deux scénarios : « People's needs », « Economy », « Weather »,
  « Society's attitude » ; mais **niveau inconnu partout** → aucun check, et
  id de ligne faux (`DifficultyN`, `Name` : la clé `DifficultyName` est
  générique). Cause (décompilé `FUN_141308380`) : le champ « Difficulty »
  d'un niveau de curseur est un **tableau de liens**
  (`Dynarray<EntryLink<FrostpunkDifficultyEntry>>` : ptr `+0x00`, compte
  `+0x08`, liens de 0x10), pas un lien unique : un curseur règle plusieurs
  réglages. → niveau = celui dont **tous** les liens sont en vigueur ; id de
  ligne = noms internes des réglages visés (`FrostpunkDifficultySettingEntry
  +0x00`, trouvés par la liste « Difficulties » de chaque réglage en
  vigueur), joints par `+`. Non testé.
  **2e test (2026-11-02) : plantage**, journal coupé au milieu de la liste
  des fins (relais perdu), jeu fermé. Cause présumée : id de ligne (plusieurs
  réglages joints) > 64 caractères → `strcat_s` / `sprintf_s` / `strcpy_s`
  sur un tampon trop petit = gestionnaire de paramètre invalide du CRT, qui
  **termine le processus** (pas une exception SEH : `__try` ne la rattrape
  pas). → id 192, nom de check 256, `_TRUNCATE` partout. **À retenir pour
  tout le mod : jamais de `*_s` sans `_TRUNCATE` sur une donnée lue du
  jeu.** Non testé.
  **3e test (2026-11-02, Refugees, Medium partout) : VALIDÉ.** Plus de
  plantage. Lignes : `Health+Nutrition` (« People's needs »),
  `Economy_Coal+Economy_Others+Technology+Economy_Food+Economy_StartingResources
  +Frostland_Resources` (« Economy »), `Weather`, `Hope+Discontent`
  (« Society's attitude »), toutes au niveau Medium ; niveau général
  Medium ; mode Detail → 8 checks (Easy + Medium par ligne, cumulatif).
  Reste : noms de checks trop longs pour « Economy » (id = réglages joints).
  → **Noms courts fixes** (décision de l'auteur, 2026-11-02) : table
  `NOMS_LIGNES` par 1er réglage de la ligne — `Health` → « People's
  needs », `Economy_Coal` → « Economy », `Weather` → « Weather », `Hope` →
  « Society's attitude » (sinon l'id) ; checks `"<Scénario> - Victory
  (<nom court>: <niveau>)"`. Journal : nom court, réglages et nom affiché.
  Non testé.

## 2026-11-21 : goal partiel

Le goal n'exige plus tous les scenarios du slot : option `scenarios_to_win`, voir `.claude/chantiers/options.md`.

