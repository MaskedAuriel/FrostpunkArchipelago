# Actes et quetes

> Historique detaille, copie mot pour mot depuis CLAUDE.md le 2026-11-02.
> Le resume a jour est dans CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

- **Actes et quêtes (2026-11-01, prochaine grosse étape)** : les « actes »
  de données (`FrostpunkGameActEntry`) ne sont **pas** les actes du joueur.
  A New Home en a 4 selon l'auteur : (1) trouver Winterhome, (2) convaincre
  les Londoniens de rester, (3) recevoir les réfugiés en attendant la
  tempête, (4) survivre à la tempête ; (3) et (4) correspondent aux actes de
  données Storm preparation / Storm, (1) et (2) aux répertoires
  d'événements « Main Scenario » / « Londoners ». Première quête de A New
  Home (donnée par l'auteur) : **Fight the Cold** — Stockpile some Coal
  (0/200), Turn the Generator on.
  Décompilés du 2026-11-01 : **`FrostpunkQuestsConfig` = `base + 0x407AE80`**
  (`FUN_14148d280`), collection hiérarchique en `+0x58`, **répertoires en
  `+0x98`** (compte `+0xA0`) ; `EntryDirectory<FrostpunkQuestEntry>` (0x30) :
  `+0x00` nom, `+0x08` GUID, `+0x18` quêtes ; **`FrostpunkQuestEntry`**
  (0xF0, `FUN_1414a5930`) : `+0x00` nom, `+0x08` GUID, `+0x18` Timeout,
  `+0x30` **Objectives**, `+0x48` / `+0x60` / `+0x78` effets initiaux /
  succès / échec, `+0x90` Social problem, `+0xA0` Displayed name, `+0xA8`
  Description, `+0xB0` Time left text, `+0xD8` Quest type ;
  **`FrostpunkQuestObjectiveEntry`** (0x88, polymorphe, `FUN_1414a61e0`) :
  `+0x18` Conditions, `+0x31` Hide progress, `+0x32` Display remaining
  progress, `+0x33` Mandatory, `+0x38` icône, `+0x50` texte, `+0x58`
  Description, `+0x60` GUID, `+0x70` entités surlignées ; 61 sous-classes
  (`GatherResource`, `ActivateBuildingOfType`, `ConstructBuildingOfType`,
  `ResearchTechnology`, `ActivateSociotech`, `CompleteQuest`, `EndGame`,
  `GeneratorConstructionStepFinished`, `RescueSurvivors`…), champs propres
  au-delà de `+0x88`. Classe d'effet : `FrostpunkProgressCustomQuestObjectiveEffect`
  (+ effets `…Quest…` vus par le relevé). **F9 = relevé `[QUETE]`**
  (`quetes.cpp`) : chaque quête (nom, clés du nom / de la description, type,
  classes des effets), chaque objectif (classe, obligatoire, clés de texte,
  classes des conditions, champs propres en entiers / textes), puis les
  événements portant un effet de quête ou d'acte (`LoisParcourirEvenements`,
  lien présumé en `+0x18` résolu en nom de quête).
  **1er relevé (2026-11-01) : validé.** 8 répertoires : `Default` (47, A New
  Home), `Scenarios` (33 : TechHeaven, Refugees, Winterhome), `Simple Snops`
  (36), `MiniSnops` (10), `Achievements` (118, type 9), `CustomScenarios`
  (63, Endless), `Scenario Builders` (63), `Reconquest` (26). Clés de texte
  `@Generated/QuestsConfig/Quests/<répertoire>/Entries/<nom>/…`.
  **« Fight the Cold » = `[Main Narrative Line] 001 ACTIVATE GENERATOR`**
  (type 1) : `GatherResourceQuestObjective` + `ActivateBuildingOfCategory…`.
  Fil principal de A New Home (types 1 / 3 / 4 / 5) : 001 ACTIVATE
  GENERATOR, 002 MEANS FOR SURVIVAL, 003 RESCUE SURVIVORS, 004 FIND OTHER
  CITIES, **005 LONDONERS** (type 5), 006 REFUGEES, 007 CRUEL HORIZON /
  007 The End, `[ENDGAME]` 001 Research tech, 002 Prepare for the storm
  (type 3, sous-quêtes 002a FOOD / b SURVIVORS / c OUTPOSTS / d TEMPERATURE,
  type 4), 003 Final preparation (type 4), 004 Survive the storm.
  Correspondance avec les 4 actes de l'auteur (présumée) : (1) 001–004,
  (2) 005, (3) 006 + ENDGAME 001–003, (4) ENDGAME 004 / 007.
  Lancements trouvés dans les événements : 001 ← `U: Start first quest`
  (Main Scenario) ; 004 Survive the storm ← `U: Gameplay Storm Start`,
  fini par `End` (déclencheur `WeatherChangedEventTrigger`) ; 003 Final
  preparation ← `U: Final preparation quest trigger` ; 005 fini par
  `BREAKPOINT_order_dillema` / `BREAKPOINT_faith_dillema` (Sequences) ;
  006 progressé par `U: Refugees in the City1..3`. Effets vus :
  `StartQuestEffect`, `FinishQuestEffect`, `ProgressCustomQuestObjectiveEffect`.
  Les autres enchaînements passent par les **effets des quêtes** →
  F9 v2 : `LoisParcourirEffets` (lois.cpp) sur les effets initiaux / de
  succès / d'échec de chaque quête (lignes `-> … dans les effets …`), actes
  nommés par `TemperatureNomActe`.
  **Relevé v2 (2026-11-01) : validé.** Chaîne de A New Home :
  001 ← `U: Start first quest` (`GameStartedEventTrigger`) ; 001 succès →
  002 ; 002 → 003 **non trouvé** (succès = `SwitchEffect`, branches non
  parcourues) ; 003 succès → 004 ; 004 → 005 **non trouvé** (succès =
  `SelectEffect`) ; **005 succès ou échec → 006** ; **006 succès →
  acte « Storm preparation »** + `[ENDGAME] 001 Research tech` (échec →
  001 Research tech) ; 001 Research tech succès → 002 Prepare for the storm +
  002a FOOD / b SURVIVORS / c OUTPOSTS / d TEMPERATURE ; 003 Final
  preparation ← `U: Final preparation quest trigger` (4 ×
  `QuestFinishedEventTrigger`) ; 004 Survive the storm ←
  `U: Gameplay Storm Start` (`RandomTimeOfDayEventTrigger`) ; **004 Survive
  the storm, effets initiaux → acte « Storm »** + fin de toutes les quêtes
  ENDGAME 001–003 ; 004 fini par `End` (changement de météo) ; 007 CRUEL
  HORIZON → 007 The End (lancement de 007 : seulement un événement de
  debug trouvé). Le **déclencheur des actes de tempête est donc résolu** :
  Storm preparation = succès de 006 REFUGEES (fin de votre acte 3, début de
  l'attente de la tempête), Storm = début de 004 Survive the storm.
  Reste : parcourir les branches de `SwitchEffect` / `SelectEffect` pour
  les maillons 002 → 003, 004 → 005, et le lancement de 007.
  **Cause trouvée (décompilés du 2026-11-01)** : `FrostpunkSwitchEffect`
  (0x30, `FUN_141943d00`) : `+0x18` Cases = pointeurs vers des
  **`FrostpunkCaseEntry` sans vtable** (0x38, `FUN_141935030` : `+0x00`
  nom, `+0x08` conditions, `+0x20` effets) ; le parcours générique ne
  descendait que dans les objets à RTTI. `FrostpunkSelectEffect` (0x68,
  `FUN_1419408d0`) : `+0x18` Selector, `+0x20` Conditions, `+0x38`
  Probability, `+0x40` Max count, `+0x48` Max fraction, `+0x50` Effects.
  → `ParcourirObjet` (lois.cpp) fouille aussi les éléments **sans RTTI**
  d'un tableau de pointeurs (leurs tableaux d'objets « Frostpunk… » dans
  les 0x80 premiers octets). **F9 v3** (`quetes.cpp` réécrit) : graphe des
  liens (`S` lance, `F` termine, `A` acte ; source = événement + répertoire,
  ou quête + moment initiaux / succès / échec, sans doublon), détail de
  chaque quête avec ses liens entrants / sortants (hors Achievements et
  CustomScenarios), puis **par scénario** (hors Endless, Debug, `Scenario`) :
  répertoires d'événements = ceux de ses réglages (`scénario +0xE8` →
  config `+0xC0` → `+0x18`) **plus ceux qu'aucun réglage ne cite** (marqués
  `*`, supposés chargés partout), et **quêtes atteignables** (lancées par un
  événement d'un répertoire actif, puis fermeture par les liens quête →
  quête), avec leurs sources et les actes lancés.
  **Relevé v3 (2026-11-01)** : 396 quêtes, 52 répertoires d'événements,
  516 liens (368 depuis des événements, 148 depuis des quêtes) ; 002 → 003
  désormais trouvé. Défauts : les répertoires « communs » (`Sequences`,
  `Utility - General`…) portent des événements de **tous** les scénarios
  (A New Home « atteignait » des quêtes de Refugees, Builders, Winterhome,
  The Arks), `Debug` et `CustomScenarioEvents` (Endless) polluaient ; **005
  LONDONERS n'est lancée par aucun `StartQuestEffect`** (seulement terminée
  par `BREAKPOINT_order_dillema` / `BREAKPOINT_faith_dillema`, Sequences).
  Lancement d'une quête : **`FUN_1416e8570(système des quêtes
  DAT_14408f0e8, quête, &contexte) -> char`** (appelée par la commande
  console `StartQuest`, `FUN_1413abc00`). Classes d'effets de quête :
  `FrostpunkStartQuestEffect`, `FrostpunkFinishQuestEffect` seulement ;
  système de séquences : `FrostpunkSequenceSupportSystem`,
  `FrostpunkSequenceSignalEntry` / `…EventTrigger`, `FrostpunkSequenceFlagEntry`
  (non décompilé). **`FrostpunkCurrentScenarioCheckExpression`** (0x30,
  `FUN_1415e12f0`) : `+0x18` lien scénario, `+0x28` Negate.
  → **F9 v4** : sources exclues `Debug`, `CustomScenarioEvents`,
  `Achivments`, `Scenario Builders Debug` ; un événement n'est point
  d'entrée d'un scénario que si ses conditions (`+0x40`, récursivement dans
  les expressions imbriquées) ne l'excluent pas (cités sans Negate =
  autorisés, avec Negate = interdits). **Trace en jeu** :
  `InstallHooksQuetes` (hook de `FUN_1416e8570`) journalise chaque
  démarrage (`[QUETE] DEMARRAGE #n "nom" -> lancee / refusee ; evenement en
  cours : …`, `g_nomEvtEnCours` exposé par evenements.cpp) et la pile des 40
  premiers (`[TRACE]`).
  **Relevé v4 (2026-11-01)** : listes resserrées (A New Home 28, The Arks
  31, Refugees 22, Winterhome 23, Last Autumn 50, On the Edge 29) mais
  encore polluées par des événements de `Sequences` / `Utility - General`
  propres à un autre scénario **sans condition de scénario** (ex.
  `[Refugees-Scenario]Lords_are_coming_SpawnCTA`, `[Builders]GeneratorGet_
  resolve`, `[TH]_ManchesterSurvivor_CTA`, `U: Endlog-refugees2`).
  **Trace en jeu (1 démarrage)** : `[Sociotech Related] Burials` lancée
  **par les effets de la loi Cemetery** (pile `FUN_1416e8570` ←
  `FUN_141697870` ← `FUN_14169aee0` ← `FUN_14169ae90` ← `FUN_1416fdbc0`,
  effets de loi appliqués par lois_ap) : les quêtes `[Sociotech Related]`
  sont lancées par les **lois** (`loi +0xE0`).
  → **F9 v5** : (1) liens depuis les **effets de toutes les lois** (source
  « Lois », admise dans tous les scénarios, la quête visée filtrée par son
  étiquette) ; (2) **filtre par étiquette** de nom (`[Refugees`,
  `Refugees-Scenario`, `Endlog-refugees` → REFUGEES ; `[Builders` →
  BUILDERS ; `[Winterhome` → WINTERHOME ; `[TH]`, `[TH_`, `[TechHeaven`,
  `[NM]` → TechHaven ; `[Reconquest`, `[DLC3`, `[aDLC3` → Reconquest) sur
  l'événement source et sur la quête visée. Reste : le
  lancement de 005 LONDONERS (à tracer en jouant jusqu'aux Londoniens).
  **Relevé v5 (2026-11-01)** : 560 liens (dont **44 depuis des lois** :
  chaque quête `[Sociotech Related]` est lancée par sa loi, ex. Neighbourhood
  watchtower ← `[Order - level 1] Neighborhood watch`, Temple ←
  `[Faith - level 2] Temple`, Labour Union ← `Builders [Left - level 0]
  Labour Unions`). A New Home : intrus de Refugees / Builders / Winterhome /
  The Arks éliminés, mais les quêtes des lois de Last Autumn et Endless y
  apparaissaient → étiquettes `Builders [` (BUILDERS) et `- Endless]`
  (Endless) appliquées aussi au nom de la loi source.
  **Trace en jeu** : 72 démarrages (partie neuve jouée jusqu'aux
  Londoniens) : ~67 succès Steam au lancement, puis 001, 002, 004, Hope
  Fail Last chance, **005 LONDONERS** ; « événement en cours » **toujours
  vide**, y compris pour 001 (lancée par `U: Start first quest`) : les
  effets ne s'exécutent pas pendant l'invocation hookée (`FUN_1417b9b70`),
  effets différés. Pile non relevée pour 005 (limitée aux 40 premiers,
  tous des succès). → pile tracée pour **toute quête hors Achievements /
  CustomScenarios** (200 max), répertoire de la quête journalisé.
  **Trace du 2026-11-01 (suite)** : pile de 005 LONDONERS =
  `FUN_1416e8570` ← `FUN_141697870` (exécution d'un effet) ←
  `FUN_14169aee0` (liste d'effets) ← **`FUN_14169d890`** ← `FUN_141268a70`
  (mise à jour du jeu). Décompilé : `FUN_14169d890(exécuteur)` = **file
  des effets temporisés** (`exécuteur +0xA8` file à priorité, compte `+0xB0`,
  entrée : `+0x00` date, `+0x10` objet `TimedEffect`, `+0x20` poignée de
  contexte) ; à échéance, « Executing scheduled timed effect... » puis
  `FUN_14169aee0(exécuteur, TimedEffect + 0x40, &contexte, &sortie)`.
  005 est donc lancée par un **`TimedEffect`** programmé plus tôt.
  → hook de **`FUN_14169aee0`** (`hkExecuterEffets`, 6 arguments) : pile par
  thread des tableaux d'effets en cours ; à chaque démarrage de quête hors
  succès, chaque tableau est cherché dans les données (`ChercherProprietaire` :
  objet « Frostpunk… » dont un champ `+0x18..+0x60` est ce tableau, dans les
  événements, quêtes et lois ; effets directs `+0x58` d'événement, `+0x48 /
  +0x60 / +0x78` de quête, `+0xE0` de loi) : lignes `[QUETE] tableau
  d'effets = …`.
  **Test** : pour 005 LONDONERS et Hope Fail Last chance, la liste exécutée
  (`TimedEffect +0x40`) n'appartient à **aucune donnée parcourue** (copie
  ou structure non atteinte). → `NoterTimedEffects` : à chaque liste
  exécutée, chaque `FrostpunkTimedEffect` qu'elle contient est noté (512 max)
  avec l'événement en cours (`g_nomEvtEnCours`), la liste qui le porte et
  la liste la plus extérieure ; au démarrage d'une quête depuis un
  `TimedEffect`, son contexte de programmation est journalisé et ses listes
  cherchées dans les données.
  **Partie neuve du 2026-11-01 (jusqu'aux Londoniens)** : démarrages hors
  succès : **001** ← `TimedEffect` de `U: Start first quest` (Main
  Scenario) ; **002** ← `TimedEffect` des effets de 001 ; **003 jamais
  lancée** dans cette partie ; **004** ← `TimedEffect` de `[Act 1 Arc 1] U:
  Step 3 controller` (Main Scenario ; objet partagé avec `FirstRefugees`,
  Sequences) ; **005** ← `TimedEffect` d'une liste **sans propriétaire
  trouvé**, à 0x380 en mémoire de celui de 004 (même zone de données).
  « Événement en cours » toujours vide, y compris à la programmation : les
  effets de ces événements ne passent pas par l'invocation hookée.
  → repli : objet parcouru le plus proche **en dessous** du tableau
  (< 0x2000) avec son événement, et classes des effets du tableau.
  **Partie neuve du 2026-11-01 (trajets de 15 min) : lancement de 005
  trouvé** (ordre du journal) : éclaireurs sur `Site_007_City_Ruins`
  (Winterhome) → événement **`StartWinterhomeSequence`**
  (`FrostpunkStartSequenceEffect` : **séquence cinématique**, système
  `SequenceSystem` / `FrostpunkSequenceSupportSystem`, données propres
  « Sequences », non parcourues par le relevé) → événements
  `Winterhome_InitiateSocialComments`, `Winterhome_ModifyHope`,
  `Winterhome_SpawnCTA` → dilemmes **`[Scenario] M: Pick your way
  (protest)`** (choix Ordre / Foi ; le livre Purpose est construit juste
  après) et **`Winterhome decision`** → plus tard, dilemme **`![London]S:
  Londoners appear FAITH`** (variante selon la voie choisie ; 1 création
  réussie, 5 refusées : max d'occurrences) → message tutoriel **`27.
  Londoners`** → **005 LONDONERS** lancée par un `TimedEffect` dont la liste
  exécutée = `[CreateMessageEffect, TimedEffect]`. Présumé : cette liste =
  **effets initiaux du dilemme Londoners** (`dilemme +0x48`, exécutés par
  `FUN_1417cde10`) — `ChercherProprietaire` ne fouille pas les dilemmes,
  d'où « propriétaire introuvable » (le voisin `BREAKPOINT_faith_dillema`
  n'est qu'un repli d'adresse). Classes d'actions de séquence vues dans
  l'image : `SequenceActionCreateDilemma`, `SequenceActionEmitSignal`
  (→ `SequenceSignalEventTrigger` des événements `BREAKPOINT_*`),
  `SequenceActionStartSequence`… Donc l'acte 2 de l'auteur (Londoniens)
  commence à l'**exploration de Winterhome** par les éclaireurs.
  À vérifier : propriétaire exact de la liste (dilemmes à ajouter à la
  recherche) et qui crée le dilemme Londoners (choix de « Pick your way » /
  « Winterhome decision », ou action de séquence).
  Debug des trajets : **validé** (5 trajets de 10,8 à 15,2 h ramenés à
  0,25 h).
  **Export Excel (demande de l'auteur, 2026-11-01, non testé)** : F9 écrit
  aussi des lignes tabulées `[QXLS]` (Q quête : répertoire, type, nom et
  description **traduits** ; O objectif : nom interne = segment de clé après
  `/Objectives/`, classe, obligatoire, texte et description traduits ; S
  scénario : ordre, quête, genres des sources L loi / E événement / Q quête).
  Traduction = appel du jeu **`FUN_140feb760(*(base + 0x2A28D80), &clé,
  &DAT_142a25a30 (GUID nul = langue courante), 0, 1, 0)`** (comme
  `FUN_14116b850`), résultat : qword +0 → UTF-16 ; non-ASCII sorti en
  `\uXXXX`. Puis `python outils\quetes_excel.py` (depuis la racine du
  projet) → `quetes_par_scenario.xlsx` (une feuille par scénario hors
  Endless, catégories Histoire / Secondaire Simple Snops, MiniSnops / Lancée
  par une loi, + feuille « Sans scénario ») **et `known_quests.h`** (table
  `KNOWN_QUESTS` : scénario, GUID de la quête `+0x08`, nom interne, nom
  affiché, catégorie, nombre de tâches ; demande de l'auteur : à terme,
  terminer certaines quêtes donnera un check). Le texte est celui de la
  **langue du jeu**. 1er F9 : tout « (non traduit) » (212 quêtes) — la clé
  était passée avec son `@` ; le jeu le saute avant de traduire
  (`FUN_141ce5a40`) → corrigé, et GUID ajouté aux lignes `Q`.
  **Validé le 2026-11-01** : 212 quêtes, traductions OK sauf **40 textes**
  (« (non traduit) ») : textes à paramètres / variantes (noms des quêtes
  ENDGAME, objectifs `Custom…`, `RescueSurvivors…`, `Unload…`, `Health -
  facility…`, Builders 002–005), que le jeu remplit avec un contexte.
  Le script écarte les quêtes lancées seulement par des lois **absentes** du
  scénario (`LOIS_ABSENTES` : Purpose dans The Arks / On the Edge, lois des
  enfants aux Arks, Organic Fertiliser à On the Edge), que F9 ne filtrait
  pas. `known_quests.h` : A New Home 18 histoire + 13 lois, The Arks 22 + 3,
  Refugees 17 + 12, Winterhome 14 + 13, Last Autumn 43 + 32, On the Edge
  18 + 6 ; Simple Snops / MiniSnops rattachées à aucun scénario (feuille
  « Sans scénario »). L'en-tête compile mais n'est inclus nulle part.
  **v2 (2026-11-01, demande de l'auteur, non testé)** : onglet « Lois » à
  part (colonne A = scénarios) ; 86 quêtes restaient « Sans scénario », dont
  005 LONDONERS / 006 REFUGEES et toute leur suite ENDGAME (005 lancée par un
  dilemme créé par une séquence). Ajouts :
  (1) **tous les dilemmes comme sources** : `FrostpunkDilemmasConfig`
  (réflexion `FUN_1413e6a10`, 0x1E8 ; « Dilemma templates » `+0x78`,
  « Categories » `+0xB0`, **« Dilemmas » `+0x130`** = répertoires
  `EntryDirectory<FrostpunkDilemmaEntry>`, « Choice types » `+0x168`,
  « Preloaded dilemma directories » `+0x1D0`), instance statique
  **présumée `base + 0x4069880`** (collection `DAT_144069970` = config
  `+0xF0`, à vérifier : log `[QUETE] dilemmes : config +4069880, N
  repertoire(s)`) ; effets initiaux / ignorés / des choix / des résultats,
  et **`choix +0x38` = lien de quête lancée directement à la résolution**
  (`FUN_1417d40c0` → `FUN_14155e350` → `FUN_1416e8570`). Source
  `Dilemmes/<répertoire>`, active dans un scénario si un répertoire
  d'événements du **même nom** y est actif (log par répertoire de dilemmes :
  « repertoire d'evenements du meme nom : oui / NON »). Genre `D`.
  (2) **repli par répertoire de quête** dans le script : `Reconquest` → On
  the Edge, `Scenario Builders` ou `[Builders]` → Last Autumn, `Scenarios` +
  `[Refugees` / `[Winterhome` / `[TechHeaven` / `[TH` ; `CUT …` et
  `[Endless]…` écartées ; catégorie « Déduit du répertoire ».
  (4) **GUID sur les lignes O et S** (des noms internes existent dans deux
  répertoires) ; le script indexe par GUID et refuse un ancien relevé.
  Autres appelants de `FUN_1416e8570` : `FUN_141736590` / `FUN_141736730`
  (quêtes de répertoire au GUID `B66D8367-…`, succès probables), commande
  console. Simple Snops / MiniSnops : déclencheur non trouvé (système du
  jeu présumé, à chercher).
  **Relevé v2 (2026-11-01)** : config des dilemmes **confirmé à `base +
  0x4069880`** (49 répertoires, 411 liens depuis des dilemmes). **005
  LONDONERS ← dilemmes `![London]S: Londoners appear ORDER` / `FAITH`**
  (répertoire de dilemmes « Londoners »), 006 ← succès / échec de 005.
  Mais seuls 7 répertoires de dilemmes ont un homonyme côté événements
  (Debug, Sociotech Hints, Londoners, Endgame, Holiday Gimmick, SeasonPass)
  → **v3** : `ScenarioDuRepDilemmes` rattache par le nom (Refugees…,
  TechHaven…, Winterhome…, Builders…, Reconquest… ; Main Scenario +
  Endgame → A New Home ; écartés : Debug, CustomScenario, Reconquest
  Endless, Holiday Gimmick, SeasonPass, répertoires « unused / backup / WIP
  / TEMP / temp » ; les autres = communs, filtrés par étiquette), et
  étiquettes `[Main Narrative Line` / `[ENDGAME]` → A New Home (Londoners
  actif aussi aux Arks). Résultat du script avec le relevé v2 : 56 quêtes
  sans scénario (Simple Snops, MiniSnops, [Endless] Crysis…, 007 CRUEL
  HORIZON / The End…). **v3 validée le 2026-11-01** : **17 quêtes sans
  scénario** (6 `[Endless] Crysis…`, `CUT …`, 007 CRUEL HORIZON / The End,
  `[Story Randomized] … TEST`, Food Shortage, Use Public Penance / Round Up,
  2 MiniSnops, 2 Simple Snops) ; fil principal (001–006, ENDGAME) dans A New
  Home seulement ; Simple Snops (32, 33 à Last Autumn) et MiniSnops (4, 8 à
  Last Autumn) rattachées partout par les dilemmes « Simple Version Snops »
  / « miniSNOPs ». Le script réserve aussi `[LondonerArc]` à A New Home
  (`RESERVEES` : lois de Purpose, retirées des Arks et de Refugees).
  **Tri avec l'auteur (2026-11-01)** : « Burials et Snow Pit, c'est de la loi
  à 100 % » → catégorie Loi dès qu'**une** loi lance la quête (même si un
  événement ou un dilemme la lance aussi : « S: Death - First death »,
  « [Sociotech Hints]S: FirstDead »), et pour tout nom `[Sociotech Related]`
  (« Burials again » n'est relancée que par « U: Cemetery relocation »).
  Onglet Lois : 39 quêtes ; une quête de loi dont toutes les lois sources
  sont absentes du scénario reste écartée (Guardposts, Prison, Faith Keepers
  aux Arks malgré les dilemmes « Londoners »).
  Décision suivante : « Game Ending avec Last chance, c'est du sans scénario,
  comme c'est présent partout » → toute quête (hors lois) rattachée aux **6**
  scénarios passe dans l'onglet « Sans scenario », catégorie « Commune a
  tous les scenarios (…) », et dans `known_quests.h` avec `scenario = "*"`
  (40 : les 2 `[GAME ENDING] … Last Chance`, `[consequences] Augmentation`,
  `[Disability] Care House`, 32 Simple Snops, 4 MiniSnops). Les variantes
  `[Builders]` restent à Last Autumn. Une quête présente dans 5 scénarios
  sur 6 reste dans ses feuilles.
  Puis : les 14 quêtes Last Autumn au nom affiché « Pass "<loi>" »
  (`[Builders] Pass Workers Councils`, `… Safety Procedures`, `… Terror`,
  `… Servitude`…) sont des quêtes de loi → onglet Lois (règle sur le nom
  affiché, appliquée aussi au repli par répertoire). De même les quêtes
  `[Builders] … Build <bâtiment de loi>` (`[Cut-off] Build Peoples Militia`,
  `[Cut-off] Build Security Outposts`) ; règle limitée à `[Builders]`
  (`[ENDGAME] Build Coal Thumpers` et le générateur de Last Autumn ne sont
  pas des lois).
  **Tri manuel de Last Autumn** (`CLASSEMENT_MANUEL` du script, nom interne →
  catégorie, prioritaire) : en Lois `Pass First Safety Law`, `Open House of
  Pleasure`, `No more soup`, `Open Bath House`, `Open Public House`, `Open
  Care House`, `Dismantle guard posts`, `Better food` ; en nouvelle
  catégorie « Secondaire (hors quête principale) » (`QueteCat::Secondaire`)
  `[Crisis] Resolve strike`, `Heat construction site (research / no
  research)`, `[Discontent] General Strike`, `Find escaped convicts`, `Safe
  Construction Site`. Reste en histoire de Last Autumn : 001 / 002 / 003 /
  004 / 005 / 006 / 006a / 007, New Allerton (+ survivors).
  Demande suivante : les feuilles de scénario ne gardent **que l'histoire** ;
  toutes les catégories « Secondaire … » (hors quête principale, Simple
  Snops, MiniSnops propres à un scénario) vont dans un onglet
  **« Secondaires »** (colonne A = catégorie - scénarios). `known_quests.h`
  inchangé (catégories gardées).
  **Checks de fin de quête (2026-11-01, demande de l'auteur, non testé)** :
  seules les quêtes de l'**histoire** (catégories Histoire / Dilemme /
  Répertoire de `known_quests.h`, pour le **scénario courant** ; pas les
  lois, secondaires ni communes `"*"`) envoient un check, à leur **succès**.
  Décompilés : **`FUN_1416d22a0(système des quêtes, id d'instance, succès)`
  = terminer une quête** (16 appelants : objectifs remplis, effet
  `FinishQuestEffect` = `FUN_14168e090` → `FUN_1416d2710(sys, quête,
  succès)`, console, délai…) ; instances `système +0x1C0` (enregistrements
  0x90, indice par `FUN_1416c5ec0(système + 0x1C0, &id)`) : `+0x10` quête,
  `+0x18` état (−1 active, puis `!succès`), `+0x70` date de fin ; effets de
  succès `quête +0x60` / d'échec `+0x78` exécutés là. → hook
  `hkTerminerQuete` (quetes.cpp) : quête lue **avant** l'original, puis
  `QueteTerminee` : log `[QUETEAP] FIN "…" (scénario) : succes / echec
  [-> quete de l'histoire]` (100 premières hors histoire), check par
  **`SendQuestCheck(nom, nomAffiché)`** (archipelago, popup « Check sent » /
  « Quest: … »), sans doublon : **`mods\ap_quetes_<GameId>.txt`** (`Q
  <GUID>`, cumulatif). Scénario courant = `ScenarioSystem +0x68 → +0x00`.
  Quêtes d'histoire : A New Home 18, The Arks 11, Refugees 8, Winterhome
  14, Last Autumn 12, On the Edge 20.
  **Validé le 2026-11-01** (A New Home) : `[QUETEAP] FIN "[Main Narrative
  Line] 001 ACTIVATE GENERATOR" (DEFAULT) : succes -> quete de l'histoire`,
  puis `[AP CHECK] quete … ("FIGHT THE COLD")` et popup. Partie suivante :
  001, 002 (« SECURE FOOD »), 003 (« PEOPLE FROM OUR CONVOY ») → 3 checks.
  **Non-doublon validé** (2026-11-01) : 001 refaite après rechargement →
  `check deja envoye dans cette partie`, pas de 2e check. Chantier validé.

### 2026-11-04 — Quête 003 d'A New Home sans les survivants de Lost Expedition

- **Problème (test grandeur nature)** : depuis que les sites AP ne
  contiennent plus que leur check (survivants du composant `+0xA8` retirés,
  `scooting.cpp`), l'objectif « Rescue survivors of our Lost Expedition »
  (`FrostpunkRescueSurvivorsQuestObjective`) de `[Main Narrative Line] 003
  RESCUE SURVIVORS` ne peut plus être rempli ; 004 FIND OTHER CITIES
  (goulot) ne démarre pas.
- **Ghidra** : objectif (réflexion `FUN_1414a88b0`, 0xA8) : `+0x88` Agent
  type, `+0x8C` Survivor count, `+0x90` Escorted only, `+0x91` Include
  already rescued survivors, `+0x92` Count only after reaching the city,
  `+0x93` Expedition site (GUID). Terminer une quête comme
  `FinishQuestEffect` (`FUN_14168e090`) : **`FUN_1416d2710(*(base +
  0x408F0E8), quête, succès)`** (ne finit que si la quête est active, puis
  `FUN_1416d22a0`, donc `hkTerminerQuete` et le check).
- **Décision de l'auteur** : objectif validé sans survivants.
- **Correctif (compilé, déployé, non testé)** : `TerminerQuetesSansSurvivants`
  (quetes.cpp, appelée par `QuetesPompe`, thread du jeu, 1 fois par
  seconde) : table `QUETES_SANS_SURVIVANTS` (scénario, quête, site) ; quand
  le site est exploré (`ScootingEtatSite`, état `+0x34` non nul) et la quête
  active, elle est terminée avec succès. Journal `[QUETEAP] "…" terminee avec
  succes : site "…" explore`. La quête entière est terminée (aussi son
  objectif « Build a Workshop » s'il ne l'est pas).
- **Risque à trancher** : autres objectifs « Rescue survivors » de site
  (`[ENDGAME] 002b` : Ice Crevasse, Snow Burrows, Deep Hollow ; Refugees :
  Trail Bend, White Pass, Black Spire). Ces sites n'ont pas de survivants à
  la génération ; s'ils en reçoivent plus tard (effet de quête), le retrait
  continu de `SurveillerSites` les enlèverait aussi.
- **Étendu (accord de l'auteur, même jour ; compilé, déployé, non testé)** :
  table `QUETES_SANS_SURVIVANTS` = une quête et jusqu'à 3 sites, terminée
  quand **tous** ses sites sont explorés (`"a|b"` = l'un ou l'autre) :
  A New Home 003 (Lost Expedition) ; `[ENDGAME] 002b_Prepare for the storm
  SURVIVORS` (Ice Crevasse `Site_049_A0`, Snow Burrows `Site_053_B0`, Deep
  Hollow `Site_057_C0` : 11-12 ouvriers + 7 enfants chacun à la génération,
  donc retirés) ; `[Refugees] 001 SAVE YOUR PEOPLE` (goulot ; Trail Bend,
  White Pass, Black Spire, variante `-dead` comprise). La quête entière est
  terminée : ses autres objectifs aussi (003 : Build a Workshop ; Refugees
  001 : objectifs 0 à 2).

### 2026-11-06 — Touche de debug F11 : terminer la quête en cours

- Demande de l'auteur (tester 006 et les autres quêtes) : **F11** →
  `QuetesDemanderFinQueteEnCours` → `QuetesPompe` (thread du jeu) termine
  avec succès la 1re quête d'histoire **active** du scénario
  (`QueteHistoire`, `FUN_1416d2710`) ; journal `[QUETEAP] F11 : "…"
  terminee avec succes`. Le check part comme pour une fin normale.

### 2026-11-10 — Texte des objectifs « Rescue survivors » validés par l'exploration

- Retour de l'auteur (A New Home, 003 atteint, Steam) : l'objectif
  « Rescue survivors of our Lost Expedition » (20 personnes) reste affiché
  alors qu'il est rempli par l'exploration du site. Demande : texte du type
  « Send explorer to the Lost Expedition site ».
- **Fait (compilé, déployé, non testé)** : table `TEXTES_OBJECTIFS_AP`
  (quetes.cpp), clé = nom interne de l'objectif (segment après
  `/Objectives/`, `quetes_par_scenario.xlsx`) : 882 → « Send scouts to the
  Lost Expedition site » ; ENDGAME 002b 706 / 293 / 826 → Ice Crevasse /
  Snow Burrows / Deep Hollow ; Refugees 001 182 / 342 / 964 → Trail Bend /
  White Pass / Black Spire. `ReleverClesObjectifsAP` (appelée par
  `TerminerQuetesSansSurvivants`, thread du jeu, une fois après le
  recensement) lit les clés de texte (`+0x50`) et de description (`+0x58`)
  de ces objectifs et lève **Hide progress** (`+0x31`) : plus de compteur
  0 / 20. `hkTranslate` → `QuetesTexteObjectifAP` → objet de traduction du
  pool `ApLocSite` (clé = texte). Journal `[QUETEAP] texte de l'objectif …`
  et `[QUETEAP] N objectif(s) "Rescue survivors" retexte(s)`.

### 2026-11-10 — Fin de partie A New Home : checks de quête manquants

- Partie terminée (victoire). Checks de quête non faits : « Prepare For
  The Storm » (002 terminée en **échec** : 002a FOOD réussie après le début
  de 003 Final preparation, règle vanilla), « The Londoners » (005 échouée),
  « Give Away Wood Supplies » (quête jamais lancée : bois donné tout de
  suite). **Décision de l'auteur** : checks de quête sur **succès
  seulement** ; quêtes conditionnelles laissées au joueur.
- **Émeutes du bois** (demande de l'auteur) : dilemme `M: Wood Riots`
  (Endgame : « Give away {wood} Wood » / « We will have Wood for
  everybody » / « Disperse the crowd » / « The city comes first! ») ; son
  **1er choix** envoie le check `[ENDGAME] Gather wood` (« Give Away Wood
  Supplies »). `hkResoudreDilemme` lit le nom du dilemme actif avant la
  résolution (`NomDilemmeActif` : `FUN_1416c5ec0(sys + 0x21A0, &id)`,
  entrée `+0x10 + i × 0x90`) ; journal `[DIL] "M: Wood Riots" : bois donne
  (choix 1) -> check …`. Compilé, déployé Epic + Steam, non testé.
- **Crise du charbon forcée** (demande de l'auteur, même jour) : la quête
  `[ENDGAME] Build Coal Thumpers` (check) n'est lancée que par la crise du
  charbon (dilemmes Endgame `[coal crisis] Mines freeze`, `More hands
  needed`, `Fear over coal supply`, `Coal stealing` ; événements `[coal
  mines crisis] U: Step N controller`), qui suppose des mines. Conditions
  exactes non relevées. **Correctif** : au démarrage de `[ENDGAME] 002
  Prepare for the storm - main quest` (`hkDemarrerQuete`), le dilemme
  `[coal crisis] Fear over coal supply` (choix 1 « We'll build Coal
  Thumpers » = démarre la quête) est créé d'office (`DilemmeParNom`,
  contexte par défaut, dans `DilemmesPompe`), sauf si le check est déjà
  fait (`ApQueteDejaCochee`). Une fois forcé dans la partie, sa création
  vanilla est refusée (`hkCreerDilemme`). Journal `[DIL] crise du charbon
  forcee : … -> id`. Compilé, déployé Epic + Steam, non testé.
