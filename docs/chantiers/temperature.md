# Temperature / meteo

> Historique detaille, copie mot pour mot depuis CLAUDE.md le 2026-11-02.
> Le resume a jour est dans CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

- **Température (2026-11-01, demande de l'auteur)** : but final = retirer
  toutes les variations de température scriptées et en faire des checks /
  items AP. **Étape 1 (en cours) : cartographie** par scénario et difficulté,
  sans rien retirer. Décompilés du 2026-11-01 — la température vient de la
  **météo**, pilotée par des **données** :
  - `FrostpunkEnvironmentConfig` = instance statique **`base + 0x406B4D0`**
    (réflexion `FUN_1413f5320`) : `+0x58` niveau de température par défaut
    (lien), `+0x68` degrés par défaut (float, −10), `+0x6C` degrés par niveau
    (float, 10), `+0x90` niveaux (`FrostpunkTemperatureLevelEntry`, 0x68 :
    `+0x00` nom, `+0x08` GUID, `+0x40` Display name), `+0xF8` **météos**.
  - `FrostpunkWeatherEntry` (`FUN_1413f6dc0`) : `+0x00` nom, `+0x08` GUID,
    `+0x24` **Storm**, **`+0x54` Temperature offset from default level**
    (int, en niveaux), `+0xB8` Display name.
  - Chronologies : `FrostpunkScenarioWeatherTimelineEntry` (0x40, polymorphe :
    `+0x18` nom, `+0x20` GUID), rangées dans `FrostpunkScenariosConfig +0x1A0`.
    `…SimpleWeatherTimeline` (0x58) : `+0x40` changements
    (`FrostpunkWeatherChangeEntry`, pas 0x20 : `+0x00` jour u16, `+0x02`
    heure, `+0x03` minute, `+0x04` lien météo, `+0x18` marqueur de carte) ;
    `…CompositeWeatherTimeline` (0x60) : `+0x40` composantes (pas 0x18 :
    liens de chronologies), `+0x58` Loop last component.
  - Rattachement : `FrostpunkScenarioEntry +0x118` Weather timelines (liens) ;
    `+0xC8` Game acts setup → `FrostpunkScenarioGameActsSetupEntry` (0x38 :
    `+0x18` actes, `+0x30` 1er acte au départ) → `FrostpunkGameActEntry`
    (0x30 : `+0x18` Weather timelines). Un acte démarre par
    `FrostpunkStartGameActEffect` (`+0x18` lien d'acte) dans un événement.
  - Difficulté : `FrostpunkDifficultyEntry` (0x38) `+0x20` Modifications
    (`FrostpunkDifficultyModificationEntry`, 0x50 : `+0x30` scénario requis,
    `+0x40` interdit) ; météo : `…GameActDifficultyModification` (`+0x50`
    acte, `+0x60` chronologies de remplacement),
    `…ScenarioDifficultyModification` (`+0x50` scénario, `+0x60` idem),
    `…TemperatureLevelDifficultyModification` (`+0x50` niveau, `+0x60`
    chance de maladie : **pas** une variation de température, réglage
    « Health »).
  - Partie : `FrostpunkWeatherSystem` = `*(base + 0x4062D48)` (réflexion
    `FUN_14132de80`) : `+0x118` chronologie calculée {date, météo} (pas 0x10),
    `+0x130` chronologies retenues, `+0x148` index, `+0x150` météo courante,
    `+0x158` précédente. `FUN_14132f260(sys, météo)` = changer de météo
    (commande console `SetWeather`).
  - **F9 = relevé `[TEMP]`** (`temperature.cpp`) : config, toutes les
    chronologies, scénarios / actes, modifications de difficulté (via le
    système des difficultés : **en partie**), événements portant un
    `StartGameActEffect` ou un effet « Weather/Temperature »
    (`LoisParcourirEvenements`, lois.cpp), chronologie calculée de la partie.
  - **Relevé F9 du 2026-11-01** (partie Refugees, Medium). Échelle :
    degrés par défaut **−20 °C** (la donnée remplace le −10 du
    constructeur), 10 °C par niveau, niveau par défaut **Chilly** (indice 3 ;
    niveaux 0 Freezing, 1 Very cold, 2 Cold, 3 Chilly, 4 Liveable,
    5 Comfortable). Une météo porte son décalage en niveaux : le nom donne
    la température (`-40` = −2 niveaux = −40 °C ; variantes `_NoFog`,
    `_DLC3`, `_Endgame` = même décalage, autre visuel ; `10_Builders` = +3,
    `00_Builders_with_snow` = +2, `-00` = −1 malgré son nom). 52 météos,
    66 chronologies. **Aucune météo n'a `Storm` (+0x24) à 1**, pas même les
    `_Endgame` de la tempête.
    **Vérifié** : la chronologie calculée de la partie (12 changements)
    reproduit « Refugees » à l'identique ; `jour N 07:00` = 07:00 du N-ième
    jour affiché ; `jour 1 00:00` = météo de départ.
  - **Mécanisme par scénario** : chronologie du scénario (`+0x118`),
    **remplacée selon le réglage de difficulté « Weather »** (Easy / Hard /
    Frostbite : `…ScenarioDifficultyModification` par scénario ; Medium = la
    chronologie du scénario). Les autres réglages de difficulté n'ont aucune
    modification météo. Actes : A New Home, The Arks, Endless et scénarios de
    debug ont le réglage d'actes « Default » (Storm preparation → Storm),
    dont seul **Storm preparation** dépend de la difficulté ; Last Autumn :
    3 actes **sans** chronologie (Acte 1 à `U: Initial Statistic`, Acte 2 à
    `S: [Crisis] 3 - Breaking Point`, Acte 3 à `M: [Cut-off] 4 - Last
    delivery`) ; On the Edge : Acte 1 au départ, chronologie
    `Reconquest-normal`. Refugees et Winterhome : pas d'actes.
  - **Tableau complet (référence ; température en °C par jour de bascule, à
    07:00 sauf mention ; J1 = départ)** — seules les **montées** comptent
    (voir plus bas) :

    | Scénario | Difficulté « Weather » | Chronologie | Variations |
    |---|---|---|---|
    | A New Home | Medium | Default | J1 −20, J4 −40, J6 −30, J10 −40, J12 −30, J15 −40, J21 −60, J23 −40, J26 −70, J27 −50, J30 −60, J33 −70, J36 −80 |
    | A New Home | Easy | Default_easy | J1 −20, J5 −40, J6 −30, J11 −40, J12 −30, J16 −40, J22 −60, J23 −40, J26 −60, J27 −50, J31 −60, J34 −70, J37 −80 |
    | A New Home | Hard | Default_hard | J1 −20, J4 −40, J7 −30, J10 −40, J13 −30, J15 −40, J21 −60, J24 −40, J26 −70, J28 −50, J30 −60, J32 −70, J35 −80 |
    | A New Home | Frostbite | Default_frostbite | J1 −20, J4 −40, J8 −30, J10 −40, J14 −30, J15 −40, J21 −60, J25 −40, J26 −70, J29 −50, J30 −60, J32 −70, J34 −80 |
    | A New Home, acte Storm preparation (jours relatifs à l'acte) | Medium | Story - storm preparation | J1 −50, J3 −70, J5 −60, J8 −70, J9 −80, J10 −90 |
    | idem | Easy | …_easy | J1 −50, J4 −70, J5 −60, J9 −70, J10 −80, J10 23:00 −90 |
    | idem | Hard | …_hard | J1 −50, J2 −70, J5 −60, J7 −70, J9 −80, J10 −90 |
    | idem | Frostbite | …_frostbite | J1 −50, J2 −70, J5 −60, J6 −70, J8 −80, J10 −90 |
    | A New Home, acte Storm (toutes difficultés) | — | Story - storm | J1 −90, J2 −100, J3 −110, J4 −120, J6 02:00 −150, J6 07:00 −150 (outro), J6 07:45 −20 (`-00`, −1 niveau) |
    | The Arks | Medium | TechHaven-normal | J1 −20, J4 −30, J6 −40, J7 −30, J9 −40, J10 −30, J11 −40, J13 −60, J14 −40, J17 −50, J18 −60, J19 −50, J25 −60, J26 −70, J27 −80 |
    | The Arks | Easy | TechHaven-easy | J1 −20, J3 −30, J5 −20, J8 −30, J12 −40, J14 −50, J15 −40, J20 −50, J25 −60, J27 −70 |
    | The Arks | Hard | TechHaven-hard | J1 −20, J3 −30, J7 −40, J10 −50, J15 −60, J18 −70, J20 −80, J22 −90 |
    | The Arks | Frostbite | TechHaven-frostbite | J1 −20, J3 −30, J6 −40, J9 −50, J14 −60, J17 −70, J20 −80, J22 −90 |
    | Refugees | Medium | Refugees | J1 −20, J3 −30, J7 −40, J9 −30, J12 −50, J13 −30, J15 −40, J17 −50, J19 −60, J21 −50, J23 −60, J26 −70 |
    | Refugees | Easy | Refugees - easy | J1 −20, J7 −30, J12 −40, J13 −30, J15 −40, J19 −50, J21 −50 |
    | Refugees | Hard | Refugees - hard | J1 −20, J3 −30, J7 −50, J9 −40, J12 −50, J13 −40, J16 −50, J17 −60, J19 −70, J21 −60, J23 −70, J26 −80 |
    | Refugees | Frostbite | Refugees - frostbite | J1 −20, J3 −30, J6 −50, J9 −40, J11 −50, J13 −40, J16 −60, J19 −70, J22 −60, J23 −70, J26 −80 |
    | Fall of Winterhome | Medium | Winterhome-normal | J1 −30, J2 −40, J4 −30, J7 −50, J10 −30, J13 −40, J14 −50, J15 −60, J18 −50, J21 −60, J22 −40, J24 −50, J25 −60 |
    | Fall of Winterhome | Easy | Winterhome-easy | J1 −20, J2 −30, J3 −20, J7 −40, J10 −20, J13 −30, J14 −40, J18 −50, J22 −40, J25 −50 |
    | Fall of Winterhome | Hard | Winterhome-hard | J1 −40, J4 −30, J5 −40, J7 −50, J10 −40, J14 −50, J15 −60, J18 −50, J21 −70, J24 −50, J25 −70 |
    | Fall of Winterhome | Frostbite | Winterhome-frostbite | J1 −40, J6 −50, J10 −40, J13 −50, J15 −60, J17 −70, J18 −50, J21 −70, J24 −50, J25 −70 |
    | Last Autumn | toutes | Builders-* | J1 +10, puis pluie / sec à 05:00 (J3, J4, J8, J9, J11, J13, J14, J15, J18, J19, J23, J24 ; température inchangée), J27 0 (neige), J31 −10, J35 −20, J37 −30, J39 −40, … , J45 14:30 −20 |
    | Last Autumn | Medium | Builders-normal | … J39 −40, J42 −50, J45 14:30 −20 |
    | Last Autumn | Easy | Builders-easy | … J39 −40, J45 14:30 −20 |
    | Last Autumn | Hard | Builders-hard | … J39 −40, J42 −50, J43 −60, J45 14:30 −20 |
    | Last Autumn | Frostbite | Builders-frostbite | … J39 −40, J41 −50, J42 −60, J45 14:30 −20 |
    | On the Edge | Medium | Reconquest-normal | J1 −20, J3 −30, J5 −20, J7 −30, J9 −20, J10 −40, J11 −30, J12 −40, J14 −20, J15 −30, J17 −40, J20 −50, J21 −40, J23 −50, J26 −60, J27 −50, J30 −60, J33 −50, J35 −60, J36 −50, J37 −60 |
    | On the Edge | Easy | Reconquest-easy | J1 −20, J4 −30, J5 −20, J8 −30, J9 −20, J10 −40, J11 −30, J13 −40, J14 −20, J16 −30, J20 −40, J21 −30, J23 −40, J25 −50, J26 −60, J27 −40, J30 −50, J32 −60, J33 −40, J34 −50, J35 −60, J36 −50, J37 −60 |
    | On the Edge | Hard | Reconquest-hard | J1 −20, J3 −30, J8 −40, J10 −30, J12 −40, J16 −50, J17 −40, J19 −50, J20 −40, J21 −50, J22 −60, J23 −50, J26 −60, J29 −50, J30 −60, J33 −50, J34 −60, J36 −50, J37 −60 |
    | On the Edge | Frostbite | Reconquest-frostbite | J1 −20, J2 −30, J6 −40, J10 −30, J11 −40, J13 −50, J14 −40, J17 −50, J18 −60, J19 −50, J22 −60, J23 −50, J26 −60, J29 −50, J30 −60, J34 −50, J35 −60, J36 −50, J37 −60 |
    | Endless (5 cartes) | — | Endless_CompositeEasy (Hard / Builders : autres composites) | composantes tirées au hasard parmi 3 variantes A/B/C, dernière en boucle (détail dans le log) |

    Chronologies de test sans scénario : Always Clear0, BASIC_NO_STORMS,
    GAMESCOM, Default_DEBUG_ACT_2, Cold_Storm_prototype, EndgameTest_*,
    AvgPlayer, Endless_Test.
  - **Décision de l'auteur (2026-11-01)** : **Endless ignoré**, et **seules
    les montées de température deviennent des checks** (les baisses restent
    telles quelles). Montées par chronologie (calculées depuis le relevé ;
    « J9 −40→−30 » = au jour 9, 07:00, de −40 à −30 °C) :

    | Scénario | Difficulté | Montées |
    |---|---|---|
    | A New Home | Medium | 4 : J6 −40→−30, J12 −40→−30, J23 −60→−40, J27 −70→−50 |
    | A New Home | Easy | 4 : J6 −40→−30, J12 −40→−30, J23 −60→−40, J27 −60→−50 |
    | A New Home | Hard | 4 : J7 −40→−30, J13 −40→−30, J24 −60→−40, J28 −70→−50 |
    | A New Home | Frostbite | 4 : J8 −40→−30, J14 −40→−30, J25 −60→−40, J29 −70→−50 |
    | A New Home, acte Storm preparation | toutes | 1 : J5 de l'acte −70→−60 |
    | A New Home, acte Storm | toutes | 1 : J6 07:45 −150→−30 (fin de la tempête = fin de partie) |
    | The Arks | Medium | 4 : J7 −40→−30, J10 −40→−30, J14 −60→−40, J19 −60→−50 |
    | The Arks | Easy | 2 : J5 −30→−20, J15 −50→−40 |
    | The Arks | Hard / Frostbite | **0** (froid monotone) |
    | Refugees | Medium | 3 : J9 −40→−30, J13 −50→−30, J21 −60→−50 |
    | Refugees | Easy | 1 : J13 −40→−30 |
    | Refugees | Hard | 3 : J9 −50→−40, J13 −50→−40, J21 −70→−60 |
    | Refugees | Frostbite | 3 : J9 −50→−40, J13 −50→−40, J22 −70→−60 |
    | Fall of Winterhome | Medium | 4 : J4 −40→−30, J10 −50→−30, J18 −60→−50, J22 −60→−40 |
    | Fall of Winterhome | Easy | 3 : J3 −30→−20, J10 −40→−20, J22 −50→−40 |
    | Fall of Winterhome | Hard | 4 : J4 −40→−30, J10 −50→−40, J18 −60→−50, J24 −70→−50 |
    | Fall of Winterhome | Frostbite | 3 : J10 −50→−40, J18 −70→−50, J24 −70→−50 |
    | Last Autumn | toutes | 1 : J45 14:30 → −20 (depuis −50 / −40 / −60 / −60 selon Medium / Easy / Hard / Frostbite ; fin du scénario) |
    | On the Edge | **toutes** (l'Acte 1 impose `Reconquest-normal`) | 8 : J5 −30→−20, J9 −30→−20, J11 −40→−30, J14 −40→−20, J21 −50→−40, J27 −60→−50, J33 −60→−50, J36 −60→−50 |

    **On the Edge vérifié le 2026-11-01** (F9, difficulté « Weather » =
    Frostbite) : chronologie calculée = `Reconquest-normal` (21 changements,
    identiques à la donnée). L'Acte 1, démarré au lancement, **écrase** le
    remplacement de difficulté du scénario : les variantes
    `Reconquest-easy / -hard / -frostbite` ne servent jamais (sauf si un
    acte suivant les reprend : Actes 2 et 3 sans chronologie).

    **Exclues (décision de l'auteur, 2026-11-01)** : les montées de **fin de
    partie** (fin de la tempête de A New Home, J45 de Last Autumn) ne sont
    pas des checks. Donc Last Autumn n'a **aucune** montée, l'acte Storm
    non plus. The Arks Hard / Frostbite n'a aucune montée ; l'entrée dans un acte (ex. Storm preparation qui
    démarre à −50) n'est pas comptée comme un changement de la chronologie.
  - **Étape 2 (2026-11-01, non testé) : retrait des montées + item.**
    Décision de l'auteur : **toutes** les montées sont retirées, les baisses
    restent vanilla, donc le décalage se **cumule** (« on commence à −20, puis
    prochaine baisse −30, puis −50… ») ; un check reçu rend la montée.
    Décompilés : `FrostpunkTemperatureSystem` (réflexion `FUN_141813280`) :
    **`+0x21A0` CurrentTemperature** (int, en niveaux), `+0x20C0` événement
    GlobalTemperatureLevelChanged (écouteurs `+0x20D8`, compte `+0x20E0`) ;
    **`FUN_141812c10(sys)` = recalcul** : indice du niveau par défaut (config
    `+0x58` dans `+0x90`) + décalage de la météo courante (`+0x54`), écrit
    `+0x21A0` et diffuse si la valeur change ; seul appelant : l'écouteur
    `0x141811540` (objet `sys + 0x20A8`, inscrit par `FUN_141812500` sur
    l'événement « météo changée », `système météo +0x30`, déclenché par
    `FUN_14132f260` après la pose de `+0x150/+0x158/+0x160`). Température
    d'une entité (`FUN_141811e10`) = `+0x21A0` + chaleur locale.
    → `temperature.cpp` : hook de `FUN_141812c10`. À chaque changement de
    météo, si c'est une **montée** (décalage de la nouvelle > ancienne), elle
    est notée « retirée » (date `météo +0x160`, niveaux), sauf les montées de
    fin de partie (vers `-00` ; vers `-20_NoFog` dans `BUILDERS`). Décalage
    AP = niveaux reçus − montées retirées dont la date est passée ; il est
    ajouté à `météo courante +0x54` **le temps de l'appel original**, qui
    calcule et diffuse lui-même. Recalcul forcé (sans noter de changement) à
    la réception d'un item et au changement de génération de partie.
    Persistance **`mods\ap_temperature_<GameId>.txt`** (`A <reçus>`, `M <date
    hex> <niveaux>`), cumulative ; une montée rejouée après chargement d'une
    sauvegarde plus ancienne retombe sur la même date : pas comptée deux fois.
    Item : **`ReceiveTemperatureRise(levels, player)`** (archipelago, popup
    « Temperature rise »), **F8** = +1 niveau de test. Contrairement au
    tableau, une **entrée d'acte qui réchauffe** (changement de météo)
    est aussi retirée. Non traité : une partie commencée sans le mod (ses
    montées passées ne sont pas retirées). Journal `[TEMPAP]`.
    **Prévisions de l'interface (demande de l'auteur : retirées aussi)** —
    décompilés du 2026-11-01 : `FUN_141559730(panneau, i, &chrono[i],
    &chrono[i−1], p5)` = icône de la **frise météo** pour le changement i
    (`WEATHER_CHANGE_ICON`, case montée / baisse selon l'écart des décalages,
    infobulle `env +0x200 / +0x218`, icônes `+0x250 / +0x268`) ; écart nul →
    pas d'icône, renvoie 1 (boucles `FUN_141559f10`, `FUN_14155a620`,
    `FUN_14155acf0`, frise reconstruite sur l'événement « chronologie
    changée » `météo +0x68`). `FUN_141bcc1a0(panneau, météo à venir)` =
    icône du **panneau de température** (`TEMPERATURE_UP` / `_DOWN` /
    `STORM`, écart nul : rien), écouteur `0x141bcb540`. Le message
    « WeatherChangeApproaching » (n° 0x1B, `FUN_141680800`) n'est programmé
    que vers une météo plus haute dans la liste des météos (= plus froide) :
    rien à faire. → hooks : une montée retirée (même règle d'exclusion) est
    traitée comme un écart nul (frise : renvoie 1 ; panneau : appelé avec la
    météo courante).
    **Test du 2026-11-01 (On the Edge) : validé par l'auteur.** 3 hooks OK ;
    F8 : 8 appuis en pause regroupés à la reprise (`+8`, température 3 → 11
    niveaux), puis 3 → … 12 ; frise : montée du changement 2
    (`-30_DLC3 → -20_DLC3`) masquée. **Aucune borne** : le niveau monte
    au-delà de Comfortable (indice 5), à borner si nécessaire. Pas encore vu :
    une vraie montée retirée au changement de météo (`MONTEE … retiree`) et
    le rechargement d'une sauvegarde.
  - **Points ouverts** :
    - le **déclencheur réel des actes de A New Home** (Storm preparation,
      Storm) n'est dans aucun événement ni dilemme : seuls des événements
      de debug (commandes console `StartStormPreparation`, `StartStorm`,
      `changeAct`, `endGame`) portent un `StartGameActEffect` « Default ».
      Piste : quêtes, ou code (`StartGameAct`). The Arks a aussi le réglage
      « Default » : a-t-il réellement un acte de tempête ?
    - **Actes : prochaine grosse étape du mod** (décision de l'auteur,
      2026-11-01). La cartographie n'en dépend que pour la date réelle de la
      montée de Storm preparation (jour relatif au début de l'acte).
    - les `EntityTemperatureLevelComparisonExpression` (prison gelée,
      `[Act III] Cold steel`…) sont des **conditions** sur la température,
      pas des variations.
    Validation de l'auteur attendue.

### 2026-11-06 — Montée retirée : le temps changeait quand même à l'écran

- **Constat (auteur)** : à une montée vanilla retirée, la température ne
  bougeait pas mais **le temps changeait** à l'écran (météo plus douce).
- **Cause** : le mod laissait le jeu poser la nouvelle météo et compensait
  seulement son décalage dans le recalcul. **`FUN_14132f260`(système météo,
  météo)** (décompilé) : retire les calques SFX de l'ancienne météo, pose
  `+0x158` = ancienne, `+0x150` = nouvelle, `+0x160` = date, applique les
  visuels de la nouvelle (`FUN_14132b790`, globaux de rendu) et diffuse
  « météo changée » (dont le recalcul).
- **Correctif (compilé, déployé 20:42, non testé)** : hook `hkPoserMeteo` :
  la météo que le jeu veut poser devient la **météo vanilla**
  (`g_meteoVanilla`, ligne `V` du fichier local) ; la montée est notée
  (`NoterChangement(précédente vanilla, nouvelle, date)`, plus dans le
  recalcul) ; si c'est une montée par rapport à la météo **affichée**, elle
  n'est pas posée (l'écran garde la météo froide) et le recalcul est forcé.
  Recalcul : décalage AP = reçus − retirées + (décalage de la météo vanilla
  − décalage de la météo affichée). Partie chargée : météo vanilla = dernière
  entrée passée de la chronologie calculée (`+0x118`). Pendant un chargement
  (partie pas prête), pose laissée au jeu. Journal `[TEMPAP] meteo … non
  posee (montee retiree)`.
- Limite : après réception des items `Temperature Rise`, la température
  remonte mais l'écran garde la météo plus froide jusqu'au prochain
  changement de météo.

### 2026-11-10 — Frise : baisse annoncée = baisse réelle

Retour de l'auteur : frise « -2 niveaux », baisse appliquée -3 (-10 → -40 °C).
Vanilla : -40 → montée -30 (retirée, item) → baisse -60. Le recalcul part de
la météo **vanilla** (-30) : -3, juste (une fois tous les items reçus, la
température est la vanilla : A New Home 7 niveaux de montées en Medium /
Hard / Frostbite, 6 en Easy = 7 items). Décompilé `FUN_141559730` : l'icône
d'un changement = écart entre la météo du changement (`entrée +8`,
décalage `+0x54`) et celle de `précédente +8` ; pour le 1er changement à
venir, `précédente` porte la météo **courante**, que le mod laisse à
l'ancienne valeur après une montée retirée (-40). `FUN_141bcc1a0` (panneau)
n'affiche qu'une flèche. Correctif (`hkIconeFrise`) : si `précédente` porte
la météo courante et qu'elle diffère de la vanilla (`g_meteoVanilla`),
l'original reçoit une copie de `précédente` dont la météo est la vanilla.
Journal `[TEMPAP] frise : changement N annonce depuis la meteo vanilla …`.
Compilé, non testé.

### 2026-11-10 — Baisse vers la météo affichée jamais appliquée

Test de l'auteur (Hard, jour 28) : message « The temperature will drop
significantly (2 levels) », rien au moment venu. Relevé `[TEMPAP] frise :
changement N a la date …` : changement 9 -70 → -50 (montée retirée, l'écran
garde -70), changement 10 **-50 → -70** (baisse de 2), changement 11 -70 →
-60. Au changement 10, la nouvelle météo (-70) est celle que le mod a
laissée à l'écran : le jeu la juge identique à la courante et ne pose rien
(`FUN_14132f260`) → `hkPoserMeteo` ne voit rien, `g_meteoVanilla` reste à
-50, la baisse n'est jamais appliquée. **Correctif** (`TemperaturePompe`,
une fois par seconde) : météo vanilla suivie d'après la chronologie
(`MeteoVanillaChronologie`, dernière entrée passée) ; si elle change sans
pose du jeu → `NoterChangement` (montée retirée / baisse laissée), sauvegarde,
recalcul. Journal `[TEMPAP] chronologie : meteo vanilla … -> … sans pose par
le jeu`. Relevé des changements de la frise gardé (diagnostic). Compilé,
déployé Epic + Steam, non testé.
- **Suite (même jour)** : frise « baisse de 4 niveaux au jour 30 » après
  avoir chargé les autres scénarios (relevé des dilemmes) puis A New Home :
  la frise s'est dessinée avant la relecture de la partie, avec la météo
  vanilla d'On the Edge restée en mémoire (`annonce depuis la meteo vanilla
  -20_DLC3 … -> -60`). Correctif : `hkIconeFrise` lit la météo vanilla
  dans la chronologie de la partie en cours (`MeteoVanillaChronologie`),
  plus `g_meteoVanilla`. Compilé (18:30), à déployer (jeu ouvert).

### 2026-11-10 — The Arks : « −3 » annoncé, −2 appliqué

- Retour de l'auteur (The Arks Medium, Steam). Journal : la baisse de J13
  a bien été de 2 niveaux (`changement de meteo -40_NoFog -> -60_NoFog`,
  `temperature 1 -> -1`). La frise l'annonçait à −3.
- **Cause** : dans `hkIconeFrise`, le « 1er changement à venir » était
  reconnu par `avant == cour`, en comparant des **objets** météo. Or ils
  sont partagés par palier : le changement de J13 part du même objet
  `-40_NoFog` que la météo affichée (−40, montée de J10 retirée), et il
  était donc lui aussi annoncé depuis la vanilla (−30 → −60 = 3). Même
  défaut sur les changements suivants (journal : `changement 9 annonce
  depuis la meteo vanilla -30 … -> -50`).
- **Correctif** : substitution seulement si la date de `precedente` (+0)
  est passée et celle de l'entrée à venir (`Maintenant()`). Compilé ; non
  déployé (jeu lancé) ; non testé.
