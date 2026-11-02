# Lois (sociotechs)

> Historique detaille, copie mot pour mot depuis CLAUDE.md le 2026-11-02.
> Le resume a jour est dans CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

- **Lois** (demande de l'auteur, 2026-10-30 : déblocage **progressif** par
  arbre, comme les technologies). En interne, une loi = **sociotech**
  (« NewLawIntroduced » est émis avec la catégorie `"Sociotech"`).
  Décompilés du 2026-10-30 :
  - `FrostpunkSocietyConfig` = instance statique **`base + 0x4070460`** :
    `+0x78` catégories (= arbres), `+0xB0` **toutes les lois** (pointeurs,
    compte `+0xB8`, `DAT_144070510`).
  - `FrostpunkSociotechEntry` (0x240), base `FrostpunkProgressTreeEntry`
    (comme les technos) : `+0x18` nom, `+0x20` GUID, `+0x30` Enabled,
    `+0x38` Requirements (compte `+0x40`), `+0x68` Display name, `+0x88`
    Grid position, `+0x94` Signable, `+0xD0` Activation dilemma, `+0xE0`
    Effects, `+0x140` Exclusion group, **`+0x148` Category** (lien GUID),
    `+0x1C8` Special node.
  - `FrostpunkSociotechCategoryEntry` (0x90) : `+0x00` nom, `+0x08` GUID,
    `+0x18` Display name, `+0x78` Requirements.
  - `FrostpunkSociotechRequirement` (0x40) : `+0x30` lien vers la loi requise.
  - `FrostpunkScenariosConfig` = instance statique **`base + 0x2C34490`** :
    `+0x88` scénarios (`FrostpunkScenarioEntry` : `+0x00` nom, `+0x1B0`
    liens « Sociotech setups »), `+0x210` réglages
    (`FrostpunkScenarioSociotechSetupEntry` : `+0x18` lois de départ,
    `+0x30` lois verrouillées, liens de 0x10), `+0x4F8` scénarios de debug.
  - Système des lois : `*(base + 0x408FBD0)` ; `+0xD8` Map `{loi,
    SociotechData}` au pas de 0x18 (compte `+0xE0`), état à `+0x10` de
    l'élément (2 = posée d'office par le réglage de départ, cf.
    `FUN_1416ffbe0`). `FUN_1416ff990(sys, loi)` = active ?,
    `FUN_1416fd350(sys, loi)` = activer. Réglage courant :
    `*(DAT_144062870 + 0xE0)`. Collection des lois : `FUN_141437900()`,
    méthode `+0x88(coll, &guid)`.
  - **F4 = `DiagLois`** (lois.cpp ; remplace `ResetSeenKeys`) : liste par
    catégorie (nom, clé d'affichage, grille, prérequis), réglages et
    scénarios, état courant. **Validé le 2026-10-30.**
  - **Relevé F4 (2026-10-30)** : **112 lois, 4 catégories**.
    - `Purpose` (22) : branches `Order` (11, niveaux 1→5, fin
      `DESPOTIZM`) et `Faith` (11, niveaux 1→6, fin `FANATIZM`).
    - `Adaptation` (30) : branche `Adaptation` (16), branche `People` (11),
      et 3 lois `[… - Endless]` (Cocaine, Field pharmacy, Bathhouse).
    - `Labour` (37, préfixe `Builders` = Last Autumn) : sous-branches
      `Labour` (14), `Left` (12, fin `TERROR`), `Right` (11, fin
      `SERVITUDE`). **8 lois désactivées** (Enabled = 0).
    - `ADMINISTRATION` (23, `Builders [LifeConditions …]`).
    - Paires exclusives par **groupe d'exclusion** (`+0x140`) : Burials,
      Sick, Sick2, Soup, kids1, kids2, Work, Housing, Disabled.
    - Types de prérequis vus : `SociotechRequirement` (loi requise),
      `SociotechExclusionRequirement`, `CompositeRequirement`,
      `UnlockRequirement` (contenu des deux derniers **non relevé**).
  - **6 réglages, 2 seulement rattachés à un scénario** : `WINTERHOME` →
    `Winterhome` (5 lois de départ) ; `BUILDERS` → `Builders` (verrouille
    **toutes** les lois `Purpose` et `Adaptation`, hors Endless).
    `Faith Unlock`, `Order Unlock`, `Benchmark`, `AvgPlayer` : rattachés à
    aucun scénario (debug / tests). 14 scénarios : DEFAULT, Endless Canyon /
    Default / Points / Bridges / Largest Circle, TechHaven, REFUGEES,
    WINTERHOME, Debug_Minimal, Scenario, Debug_Builders, BUILDERS,
    Reconquest. Inconnu : ce qui **masque** `Labour` / `ADMINISTRATION`
    hors Last Autumn (probablement les prérequis des catégories, `cat
    +0x78`, non relevés).
  - État en partie : `Kids work in light jobs = 2` (loi signée → état 2).
  - **Prérequis (décompilés du 2026-10-30)** : base
    `FrostpunkRequirementEntry` (0x30 : `+0x18` lien d'erreur, `+0x28`
    texte d'erreur) ; `CompositeRequirement` (0x50 : `+0x30` Operator
    `eBooleanOperator`, `+0x38` Requirements) ; `SociotechExclusionRequirement`
    (0x38 : `+0x30` ExclusionGroup) ; `UnlockRequirement` (0x40 : `+0x30`
    lien vers `FrostpunkUnlockEntry` : `+0x00` nom, `+0x08` GUID, `+0x18`
    Display name, rangées dans `FrostpunkUnlocksConfig +0x78`, instance
    cherchée par RTTI autour de son gestionnaire `base + 0x4082EC0`).
    → F4 décrit les prérequis récursivement (`op<N>(…)`) et ceux des
    catégories. **Validé le 2026-10-30.** `FrostpunkUnlocksConfig` à
    **`base + 0x4082F10`**. `eBooleanOperator` (déduit) : **0 = ET, 1 = OU,
    2 = NON**.
  - **Visibilité des arbres** (prérequis des catégories) : `Purpose` ←
    unlock `IdeologyTree` ; `Adaptation` ← **NON** unlock `[Builders]Builders
    Scenario` ; `Labour` ← unlock `[Builders] SOCIOTECH Labour tree -
    SAFETY` ; `ADMINISTRATION` ← unlock `[Builders]Builders Scenario`. Donc
    Purpose + Adaptation partout sauf Last Autumn, Labour + ADMINISTRATION
    seulement dans Last Autumn. Les 3 lois « Endless » n'ont **aucune**
    restriction de scénario (Cocaine ← OU soupe, Field pharmacy ← OU soins,
    Bathhouse ← Care House).
  - Autres conditions : Neighborhood watch / House of Prayer ← OU de deux
    `PolicyRequirement` (choix de l'objectif, contenu non relevé) ; DESPOTIZM
    ← OU(Prison beating, Propaganda ability) ; Kids light jobs / Child
    shelters ← NON unlock `[TechHeaven] Quest1` ; Augmentation / Medical
    Repatriation (Last Autumn) ← NON unlock `[Builders] Act 3 - The End` ;
    Right (Factory Inspectorate) ← unlock `STRIKE R` + exclusion `Labour`,
    Union delegates (désactivée) ← `STRIKE L` : Left / Right se décide par
    l'histoire de Last Autumn.
  - **Arbres d'Adaptation (hors Last Autumn)** :
    Burials [Cemetery | Put them in the snow] → [Burial Ceremony |
    Transplantation] → Corps Processing (côté neige seulement) ;
    Sick [RadicalTreatment | Life Sustain] → OU → [Double Ration | People on
    the floor] (Sick2) → Triage, et → Care House → Augmentation, Bathhouse,
    et → Field pharmacy ; Soup [Poor Soup | Sawdust] → OU → Alternative Food
    Source, Cocaine ; kids1 [light jobs | Child shelters] → hard jobs (light)
    | [Med Kids | EngineerKids] (shelters) ; Fightclub → PublicHouses →
    Prostitutes, Moonshine ; Fightclub → Dueling Law ; Emergancy shift →
    Hard work rigor.
  - **ADMINISTRATION (Last Autumn)** : Burials [Cemetery | Morgue] →
    [Burial Ceremony | Steel coffins] ; Soup [Gruel | Better food] → Cocaine,
    Alternative Food Source ; Sick [Radical | Life] → [People on the floor |
    Double Ration], Care House → [Augmentation | Medical Repatriation] ;
    Bathhouse → [Crowded | Comfortable Quarters] ; Pub → Prostitutes,
    Bare-Knuckle Bouts ; Chapel → Evening of Prayers. **Labour** : Work
    [Hard work rigor | Double Shift] → [compensations | optimisation] ;
    Safety Procedures → After-hours ; Light Work rigor → upgrade ; Field
    pharmacy.
  - **Correction de l'auteur (2026-10-30)** : les arbres ci-dessus mélangent
    **tous les scénarios** ; il faut raisonner **scénario par scénario**.
    Capture du livre de Refugees : Fighting Arena (→ Public House, Duelling
    Law), Emergency Shift (→ Extended Shift), Child Labour Safe Jobs (→ All
    Jobs) / Medic | Engineer Apprentices, Soup | Food Additives, Cemetery |
    Corpse Disposal (→ Ceremonial Funerals | Organ Transplants), **Sustain
    Life seule** (→ Extra Rations | Overcrowding) ; **pas** de Radical
    Treatment, Corps Processing, House of Healing, prothèses, etc. Cause
    présumée : les **prérequis d'affichage** (`ProgressTreeEntry +0x50`
    « Show requirements », non relevés jusque-là) et les unlocks du scénario.
    → **F4 v3** : liste aussi `affichage : …` par loi, et une section
    **« scenario courant … : lois presentes »** : catégorie et affichage
    évalués avec les **unlocks actifs** (`FrostpunkUnlocksSystem`, trouvé
    par l'annuaire, `+0x68` = Set de pointeurs d'entrées, compte `+8` ;
    propman `DAT_14408fd10`, réflexion `FUN_1417018c0`) et le réglage
    courant (`*(*(base + 0x4062870) + 0xE0)`, lois verrouillées). Statuts :
    DESACTIVEE, VERROUILLEE, arbre absent, CACHEE, ?, PRESENTE (bloquée ou
    non par un unlock). Prérequis de loi / d'exclusion tenus pour vrais ;
    types inconnus (`PolicyRequirement`…) → `?`. Limite : les unlocks
    **ajoutés plus tard** dans le scénario ne sont pas connus (photo de
    l'instant). Non testé.
  - **1er relevé v3 (Refugees)** : 4 unlocks actifs (IdeologyTree,
    SociotechButton, TechTreeButton, TechnologyPanelOpened). Les lois
    absentes de la capture (Radical Treatment, Care House, Triage,
    Augmentation, Moonshine…) n'ont **aucun** prérequis d'affichage : la
    capture ne montrait qu'une partie du livre (grille jusqu'à y ≈ 5,
    |x| ≈ 6 ; Care House est en (0, 7), Moonshine en (−11, 0)) et une paire
    signée n'affiche que la loi choisie. Prérequis d'affichage réels :
    `ScenarioRequirement` (Corps Processing) et `PolicyRequirement`
    (Census Station, Propaganda, Mechanical Agitator, DESPOTIZM, Temple,
    Captains message, Righteous Denunciation, Public conversion, FANATIZM,
    Alternative Food Source, et 2 chacune pour les 3 lois Endless), NON
    `[TechHeaven] Quest1` (lois des enfants). Décompilés :
    `FrostpunkScenarioRequirement` (0x48 : `+0x30` lien scénario, `+0x40`
    Negate) ; `FrostpunkPolicyRequirement` (0x40 : `+0x30` lien
    `FrostpunkPolicyEntry`, `+0x00` nom, `+0x08` GUID, rangées dans les
    enjeux `FrostpunkSocietyConfig +0x150` → `FrostpunkPolicyIssueEntry
    +0x18`) ; politiques en vigueur : `FrostpunkPolicySystem +0x68` (Map
    `{enjeu, GUID}` au pas de 0x18). → F4 v4 les décrit et les évalue
    (scénario = `ScenarioSystem +0x68 +0x08`). **Validé le 2026-10-30.**
  - **Relevé v4 (Refugees)** : 96 politiques en vigueur (enjeux de tous les
    scénarios, ex. `[Refugees Scenario] Epidemy`, `[SOCIOTECH]Path chosen=
    Both`, `[CustomScenario]IsEndless=No`). Prérequis d'affichage décodés :
    - 9 lois de Purpose ← `[Scenario Fall of Winterhome] Sociotechs
      blocker = Avalible` (vrai hors Winterhome ; Winterhome les retire en
      cours de partie) ;
    - Alternative Food Source ← `[Sociotech] Alternative food source
      avalible = Avalible` (Refugees : `Hidden` → cachée ; politique qui
      peut changer en cours de partie) ;
    - Corps Processing ← **PAS** scénario `Reconquest` (= On the Edge,
      enjeux `[aDLC3]` / `[Reconquest]`) → **présente** dans Refugees selon
      les données, alors que l'auteur dit ne pas y avoir accès : à vérifier
      (case (8, 6), sous Organ Transplants, hors de la capture) ;
    - Cocaine, Field pharmacy, Bathhouse ← ET(`[CustomScenario]
      IsEndlessBuilders = Yes`, `[TechHaven] Manchester Quest =
      ManchesterQuestFinished`) : conditions de deux modes différents,
      donc a priori **jamais** visibles ;
    - lois des enfants ← NON unlock `[TechHeaven] Quest1`.
    Statut Refugees : Purpose 22 présentes, Adaptation 26 présentes
    (cachées : les 3 Endless, Alternative Food Source), Labour et
    ADMINISTRATION absents. **Limite** : politiques et unlocks évoluent en
    cours de partie, un relevé ne vaut que pour l'instant.
  - **2e capture de Refugees (aucune loi de soins signée)** : visibles
    (estompées) Soup | Food Additives, Radical Treatment | Sustain Life,
    Extra Rations | Overcrowding, **Care House**, Cemetery | Corpse Disposal,
    Ceremonial Funerals, Organ Transplants, Medic | Engineer Apprentices ;
    **absentes** Triage, Augmentation, Corps Processing (rien sous Organ
    Transplants). `FrostpunkSociotechCategoryDisplayData` (cat `+0x60`,
    `FUN_1419f1030`) n'est pas un filtre (Modifier, 2 images, Root node
    name). **Hypothèse** cohérente avec les 2 captures : le livre ne montre
    une loi que si ses lois requises sont **signées ou sans prérequis**
    (dévoilement à 2 niveaux) : Care House (← racines Radical / Sustain)
    visible, Triage (← paire Sick2, non signée) et Corps Processing (←
    Organ Transplants, non signée) cachées, Duelling Law / Public House
    (← Fighting Arena, racine) visibles. Donc Corps Processing **existe**
    dans Refugees mais n'apparaît qu'après Organ Transplants.
    **CONFIRMÉ le 2026-10-30** (captures après signatures en chaîne) :
    Organ Transplants signée → **Organic Fertiliser** (= Corps Processing)
    apparaît ; Care House signée → **Prosthetics** (= Augmentation) ;
    Sustain Life signée → Triage (estompée) sous Extra Rations | Overcrowding ;
    Fighting Arena signée → **House of Pleasure** (= Prostitutes) et
    **Moonshine** (estompées) derrière Public House. Règle : une loi est
    affichée quand ses lois requises sont **signées ou signables**. Les
    données (F4 v4) donnent donc bien la liste par scénario : Refugees =
    Purpose 22 + Adaptation 26.
    Noms affichés relevés : Kids work in light / hard jobs = Child Labour –
    Safe Jobs / All Jobs, Child shelters (non vue), Med Kids / EngineerKids =
    Medic / Engineer Apprentices, Fightclub = Fighting Arena, PublicHouses
    unlock = Public House, Dueling Law = Duelling Law, Emergancy shift =
    Emergency Shift, Hard work rigor = Extended Shift, Poor Soup / Sawdust =
    Soup / Food Additives, Put them in the snow = Corpse Disposal, Burial
    Ceremony = Ceremonial Funerals, Transplantation = Organ Transplants, Life
    Sustain = Sustain Life, Double Ration / People on the floor = Extra
    Rations for the Ill / Overcrowding.
  - **Noms affichés dans F4 (2026-10-30, non testé)** : le nom interne
    (`+0x18`, ex. `Corps Processing - fertilizer`) diffère du nom affiché
    (« Organic Fertiliser », clé `+0x68`). `hkTranslate` appelle
    `LoisNoterTraduction(cle, valeur)` pour chaque traduction ; les clés
    `Generated/SocietyConfig/Sociotechs/…/Display name` sont mémorisées
    (256 max, texte ASCII), et la section « lois presentes » de F4 affiche
    le nom vu, ou « (nom affiche non vu) » si le jeu ne l'a pas encore
    traduit (ouvrir le livre de lois avant F4).
    **Validé le 2026-10-30** (A New Home, Last Autumn). Relevés :
    - **A New Home (`DEFAULT`)** : Adaptation 26 présentes (mêmes que
      Refugees) ; Purpose **fermé au moment du relevé** (unlock
      `IdeologyTree` donné plus tard par l'histoire).
    - **Last Autumn (`BUILDERS`)** : Purpose et Adaptation verrouillées par
      le réglage ; ADMINISTRATION 22 présentes (Alternative Food Source
      cachée), noms affichés : Pub = Public House, Prostitutes = House of
      Pleasure, Evening of Prayers = Evening Service, Cemetery = On-Site
      Burials, Morgue = Repatriation of Bodies, Steel coffins = Company
      Funerals, Better food = Hearty Meals, Bathhouse = Bath House, Cocaine
      = Cocaine Pills, Augmentation = Prostheses Shipments ; Labour **fermé
      au moment du relevé** (unlock `SOCIOTECH Labour tree - SAFETY`).
    - Noms de Purpose non vus (jamais traduits dans ces parties).
    → F4 v5 : un arbre fermé n'efface plus ses lois (statut `PRESENTE si
    l'arbre s'ouvre`, prérequis de l'arbre en clair) ; `LireTexte` lit par
    blocs de 16 (un nom interne sortait `?` près d'une fin de page).
    Réponse à l'auteur : **pas besoin** de tout débloquer avant F4 ; ouvrir
    le livre suffit pour les noms affichés (globaux, une fois par session).
  - **Relevé F4 v5 de tous les scénarios (2026-10-30)** — lois existantes
    par scénario (hors désactivées, hors cachées) :
    | Scénario (interne) | Purpose | Adaptation | Labour | ADMINISTRATION |
    |---|---|---|---|---|
    | A New Home (`DEFAULT`) | 22, arbre ouvert plus tard (`IdeologyTree`) | 26 | — | — |
    | The Arks (`TechHaven`) | **aucune** (vérifié par l'auteur) | 21 : **sans les 5 lois des enfants** (unlock `[TechHeaven] Quest1` actif dès le départ) | — | — |
    | Refugees (`REFUGEES`) | 22 (ouvert) | 26 | — | — |
    | Fall of Winterhome (`WINTERHOME`) | 22 (ouvert) | 26, réglage `Winterhome` (5 lois de départ) | — | — |
    | Last Autumn (`BUILDERS`) | verrouillé | verrouillé | 9 (`Labour`) + Left / Right selon l'histoire (`STRIKE L` / `STRIKE R`) ; arbre ouvert plus tard (`SAFETY`) | 22 (Alternative Food Source cachée) |
    | On the Edge (`Reconquest`) | **aucune** (vérifié par l'auteur) | 25 : **sans Organic Fertiliser** | — | — |
    Toujours cachées : les 3 lois Endless, Alternative Food Source (sauf
    changement de politique en cours de partie). Labour / ADMINISTRATION
    hors Last Autumn : leurs unlocks n'existent que dans Last Autumn.
    Un 7e relevé (scénario `?`, système des lois illisible) a été fait hors
    partie : à ignorer. **Inconnu** : quels scénarios donnent réellement
    `IdeologyTree` (The Arks, On the Edge) ; il est donné par l'histoire
    (effet `FrostpunkAddUnlockEffect`), pas par les données du scénario.
    Noms affichés de Purpose relevés : Neighbourhood Watch, Morning
    Gathering, Patrol, Guard Stations, Foreman, Propaganda Centre (Census
    Station), Prison, Pledge of Loyalty (Propaganda ability), Forceful
    Persuasion (Prison beating), Agitator, NEW ORDER (DESPOTIZM), Shrines,
    House of Healing, The Temple, Protector of the Truth (Captains message),
    Righteous Denunciation, Public Penance (Public conversion), Field
    Kitchens, NEW FAITH (FANATIZM), House of Prayer, Evening Prayers, Faith
    Keepers.
  - **Qui donne les unlocks (F4 v6, 2026-10-30, non testé)** : question de
    l'auteur « The Arks / On the Edge ont-ils Purpose ? ». Les unlocks sont
    donnés par des **données** (effets d'événements et de dilemmes), pas par
    le code → F4 les parcourt en mémoire. Décompilés :
    `FrostpunkAddUnlockEffect` (0x28, réflexion `FUN_141934300`) : `+0x18`
    lien unlock ; `FrostpunkScenarioEntry` : `+0xE8` liens « Events
    setups », `+0x100` liens « Unlocks setups », `+0x1B0` « Sociotech
    setups » ; config des scénarios `+0xC0` réglages d'événements
    (`FrostpunkScenarioGameEventsSetupEntry`, `FUN_14160ed60` : `+0x18`
    « Event directories ») et `+0x248` réglages d'unlocks
    (`FrostpunkScenarioUnlocksSetupEntry`, `FUN_1416103b0` : `+0x18`
    « Starting unlocks »). Parcours (`DiagUnlocks`, lois.cpp) : tous les
    répertoires d'événements (`base + 0x406DDA8`), effets `+0x58`,
    récursion générique dans les tableaux d'objets « Frostpunk… » des
    0x100 premiers octets de chaque effet, et dilemmes des effets
    `…DilemmaEffect` (lien `+0x18`, résolu par
    `(*DAT_144069970)->vtable[0x88]`, effets initiaux / ignorés, effets et
    résultats des choix). Sortie `[UNLK] <unlock> <- evenement "…"
    (repertoire …)`, puis par scénario ses réglages d'événements / d'unlocks
    et le détail des réglages. **F4 s'exécute désormais sur le thread du
    jeu** (`LoisDemanderReleve` → `LoisPompe`) : jeu **hors pause**.
    **1er test : plantage** juste après `52 repertoires d'evenements`.
    Cause : `ResoudreDilemme` lisait `*(base + 0x4069970)` comme pointeur
    de collection, alors que **`DAT_144069970` est la collection
    elle-même** (`FUN_1413e4240()` renvoie `&DAT_144069970` ; vtable en
    `+0`) → appel d'une adresse prise dans le code. Corrigé : collection =
    `base + 0x4069970`, vtable et fonction bornées à l'image, appel sous
    SEH (`AppelerTrouverDilemme`). Log `[UNLK] resolution des dilemmes :
    vtable +…, fonction +…`.
    **2e test (2026-10-30) : pas de plantage**, 510 effets AddUnlock, mais
    **aucun dilemme suivi** (ligne de résolution absente) et ensemble des
    objets vus saturé (8192). Réglages : `DEFAULT` et les Endless →
    événements `Default` (Main Scenario, Londoners, Beacon, Endgame…) ;
    `TechHaven` → TechHeaven Quest, Londoners, Beacon ; `REFUGEES` →
    Refugees Quest + **unlocks de départ `Winterhome` = IdeologyTree,
    TechTreeButton** ; `WINTERHOME` → idem ; `BUILDERS` → Builders Quest /
    Consequences / Utility / Debug ; `Reconquest` → Beacon, Reconquest
    Quest / Utility / Improvements, unlocks de départ vides. Qui donne :
    - **IdeologyTree** : réglages d'unlocks Winterhome (donc Refugees et
      Winterhome dès le départ), **« U: Sick from the dead - start - first
      dead » (répertoire `Health`)**, « Endless Start » (×3,
      `CustomScenarioEvents`), et des événements de debug ;
    - `SOCIOTECH Labour tree - SAFETY` : « M: [Crisis] 1 - First Work
      Accident (Safety Unlock) » (Builders Quest) ;
    - `STRIKE L` / `STRIKE R` : seulement des événements de **debug**
      trouvés (le vrai déblocage passe sans doute par un dilemme, non suivi) ;
    - `[TechHeaven] Quest1` : « U: Intro Arks » (dès le départ des Arks) ;
    - `[Builders]Builders Scenario` : « U: Initial Statistic ».
    Les répertoires `Health`, `Sequences`, `Utility - General`… ne sont
    rattachés à **aucun** réglage de scénario : chargés partout ? → F4 v7 :
    compteurs des effets de dilemme (lien nul / non résolu / résolu),
    ensemble des vus en table de hachage (65536), et liste des
    **répertoires actifs** du système d'événements (`*(base + 0x40935C8)
    +0x2128`, format supposé : tableau de pointeurs). **Validé le
    2026-10-30** (The Arks, On the Edge) : collection des dilemmes à vtable
    `+0x1F22468`, fonction de recherche **`+0xFF8F90`** ; 683 effets de
    dilemme, tous résolus ; répertoires actifs lus correctement (32 dans
    The Arks, 33 dans On the Edge). **`Health` est actif dans les deux**
    (avec `Sequences`, `Utility - SocioTech`, `Hope Related Events`…,
    `Londoners` aux Arks seulement, les répertoires `Scenario Reconquest …`
    à On the Edge, et `Scenario Reconquest Endless` dans les deux).
    Défaut : un effet partagé n'était attribué qu'au **premier** événement
    qui l'atteignait (« Sick from the dead » disparu du relevé, IdeologyTree
    attribué à « E: No alive people in the city » de Winterhome). → F4 v8 :
    ensemble des vus **vidé à chaque événement** (`ViderVus`, cases
    mémorisées), et pour tout événement qui donne IdeologyTree, état activé
    (`+0x20`) et tableaux d'objets de l'entrée (déclencheurs `+0x28`,
    conditions…) décrits comme des prérequis (`DecrireEvenement`).
    **Test v8 : attributions fausses** (événements de debug, `[Hint]` de
    Winterhome « donnant » IdeologyTree ; « Sick from the dead » absent).
    Deux causes : (1) le parcours générique lisait 0x100 octets dans des
    effets de 0x28 → débordement dans les objets voisins ; (2)
    `FrostpunkGameEventEntry` fait **0x90 octets, entrées contiguës** :
    `+0xB8…` = l'événement suivant. Disposition confirmée par le relevé :
    `+0x20` Enabled, `+0x28` déclencheurs, `+0x40` conditions (expressions,
    ex. `CurrentScenarioParametersCheckExpression`, `UnlockCheckExpression`),
    `+0x58` effets. → F4 v9 : taille réelle de chaque objet par sa **fiche
    de réflexion** (méthode `[3]` = vtable `+0x18`, fiche `+0x88` = taille,
    `FUN_140e4e520` ; appel sous SEH), parcours générique borné à cette
    taille ; `DecrireEvenement` borné à 0x90. Non testé.
    **Recherche close (2026-10-30)** : l'auteur a vérifié ailleurs que
    **The Arks et On the Edge n'ont pas l'arbre Purpose**. `DiagUnlocks`
    reste dans F4 (relevé lourd, à couper si gênant).
  - **Debug : pas de délai entre deux lois (2026-10-30, VALIDÉ)** :
    commande console `ResetSociotechCooldown` = `FUN_1413a64c0` →
    `FUN_141700760(système des lois)` : `système +0xF8`
    (SociotechActivationCooldown) = `DAT_142c2fca8` (temps nul), puis
    événement « cooldown expiré » (`système +0xA0`) s'il a changé ;
    `+0xF0` = LastSociotechActivationTime. → `LoisPompe` (lois.cpp,
    appelée par le hook du setter de ressources après `DilemmesPompe`,
    thread du jeu) l'appelle dès que `+0xF8` diffère du temps nul, tant que
    `LOIS_SANS_DELAI = true` (**à remettre à false** hors debug). Log
    `[LOIS] debug : delai … remis a zero` (10 premiers). Comme les autres
    pompes, ne tourne pas en pause.
  - **Modèle de checks voulu par l'auteur (2026-10-30)** : dans un petit
    arbre d'Adaptation, la loi exclusive (l'une OU l'autre) = **un seul
    check**, la loi qui en découle (quelle que soit la branche) = **un
    seul check** ; l'item **progressif** de l'arbre débloque l'**effet** de
    la loi effectivement signée à ce niveau (signer = check, effet en
    attente de l'item, comme `{researched, received}` des technos). Cas
    hors modèle à trancher : 3e niveau d'un seul côté (Corps Processing),
    exclusion au 2e niveau (Housing, Disabled, kids2), branches sans
    exclusion, Purpose. Lois désactivées : à exclure.
    **Règles tranchées par l'auteur (2026-10-30)** : (1) une loi **sans
    exclusion** = **un check à elle seule** (on peut tout prendre) ;
    (2) une **paire exclusive**, quel que soit son niveau, = **un seul
    check** (ex. Medic | Engineer Apprentices). Les « suites communes » ne
    sont pas exclusives entre elles (aucun groupe d'exclusion : Care House,
    Triage, Prosthetics, Alternative Food Source, Cocaine…) → règle (1).
    (3) Des lois **exclusives par leurs parents** (chacune n'est
    accessible que par une branche différente d'une même paire) forment
    aussi **un seul check** : Child Labour – All Jobs | Medic Apprentices |
    Engineer Apprentices (← Safe Jobs | Child Shelters) ; Ceremonial
    Funerals | Organ Transplants ; Last Autumn : Ceremonial Funerals |
    Company Funerals, compensations | optimisation (travail). Organic
    Fertiliser (côté Corpse Disposal seulement) reste un check à part.
    (4) **Décision de l'auteur** : côté Cemetery, **Ceremonial Funerals
    vaut 2 checks** (l'équivalent d'Organ Transplants + Organic
    Fertiliser), pour que les deux côtés envoient autant de checks.
    **Order / Faith et Left / Right** (branches entières exclusives) :
    même nombre de lois (11 / 11 ; 10 / 10 hors désactivées) mais
    **formes différentes**. Order : NW → {MG → Foreman ; GS → {Census →
    {Pledge, Agitator} ; Prison → Forceful Persuasion ; Patrol}} → NEW
    ORDER (← OU Pledge, Forceful), profondeur 5. Faith : HoP → {Shrines →
    {House of Healing, Field Kitchens} ; Evening Prayers → {Temple →
    Righteous Denunciation ; Faith Keepers → Public Penance → Protector of
    the Truth → NEW FAITH}}, profondeur 6. Left : LU → {CB ; WC → OD →
    {SS ; Prop ; Abolition → Militia → {Show of force, TERROR}}} ; Right :
    FI → {IP ; Overseers → SO → {Class Seg., Strike Busting, Penal Colony
    → {Rationalised HC ; Round-ups → SERVITUDE}}} (identiques jusqu'à la
    profondeur 4, différents ensuite). **Retenu par l'auteur** : checks
    **par rang** (la N-ième loi signée dans la branche choisie envoie le
    check N ; 11 pour Purpose, 10 pour Left / Right).
  - **Signer sans effet / effet sans signer (décompilés du 2026-10-30)** :
    `FUN_1416fd350(sys, loi)` = signer : frustration (`loi +0x90`, sur les
    entités de `DAT_144098320`), map `+0x08` date / `+0x10` état 2,
    `sys +0xF0` / `+0xF8` délai (`loi +0xC8`), **`FUN_1416fdbc0(sys, loi)`**,
    espoir (`FUN_1418d8440`, `loi +0xB0/+0xB4`), alignement (`loi +0x98`,
    `FUN_1418fa5e0`), commentaires (`loi +0x1D0`), événement `sys +0x30`,
    `FUN_1416ffe70` (rafraîchit le livre). `FUN_1416fdbc0` = modificateurs
    globaux `+0xF8` / d'entité `+0x110` / de lieu `+0x128` inscrits **une
    fois** dans le système de modificateurs (`DAT_144093180`, liste
    `+0x2128`), puis effets `+0xE0` (`FUN_14169ae90`, contexte par défaut).
    Aussi appelée par `FUN_1416ffbe0` (lois de départ du réglage, état 2
    posé à la main) : la séparation état / effet existe déjà dans le jeu.
    Mais l'état est **aussi lu en continu** : `FUN_1416ff990` (« active ? »)
    a 29 appelants, dont l'expression « Sociotech is researched »
    (`FUN_1415d5ce0`, conditions d'événements), le panneau du refuge
    d'enfants (Medic / Engineer, `FUN_14152bb30`), le système des groupes
    (`FUN_1418b3d50`), les prérequis des lois et le livre.
    **Décision de l'auteur (option A)** : signer = état 2 tout de suite
    (le jeu réagit à l'état, événements compris) ; seuls les
    modificateurs et effets (`FUN_1416fdbc0`) attendent l'item.
    Espoir (`FUN_1418d8440` = `frostpunkcitizenhopesystem.cpp`) et
    « alignements » (`FUN_1418fa5e0`, système `DAT_14409d2e0` : jauges
    0..1 par alignement, liste `DAT_14407fea8`, commande console
    `SetAlignment`) : **à la signature** (décision de l'auteur,
    2026-10-30), comme la frustration, le délai et les commentaires.
    Donc à la signature : tout `FUN_1416fd350` **sauf** `FUN_1416fdbc0`,
    appelé à la réception de l'item (ou à la signature si l'item est
    déjà reçu).
  - **Ce que la sauvegarde du jeu garde (décompilés du 2026-10-30)** :
    `SociotechData` (`FUN_141700520`) = `ActivationTime` + `State`
    (`eSociotechState`, 1er nom « Inactive ») → l'état 2 d'une loi signée
    est sauvegardé. Le système de modificateurs (`FUN_1417a0220`)
    sauvegarde **lui-même** ses modificateurs (`GlobalModifierHost`,
    `ModifierExpireQueue`, `ActiveModifiersWithConditions`,
    `EntitiesOfTypeModifiers`, `BuildingsOfCategoryModifiers`) : ils ne
    sont **pas** recalculés depuis l'état des lois au chargement
    (`FUN_1416fdbc0` n'est appelée que par la signature et par le réglage
    de départ, qui saute les lois déjà à l'état 2). Une loi « signée, effet
    en attente » est donc rechargée telle quelle par le jeu ; seul le mod
    doit se souvenir de ce qui est en attente.
  - **Implémenté (2026-10-30, non testé) : `lois_ap.h/.cpp`.** Hooks de
    `FUN_1416fd350` (signature) et `FUN_1416fdbc0` (effets : retenus
    seulement s'ils sont appelés **dans** une signature et que l'arbre n'a
    pas assez d'items ; le réglage de départ passe toujours). **Arbres**
    construits une fois depuis les données : composantes des
    `SociotechRequirement` (composites compris) et des groupes d'exclusion
    (`+0x140`, par catégorie) ; Order + Faith = `Purpose`, Left + Right =
    `Politics` ; autres arbres nommés par leur 1re loi (nom interne) ; lois
    désactivées exclues. Liste dans le journal `[LOIAP]`. **Rang** = 1 +
    poids des lois signées avant dans l'arbre (date d'activation) ;
    Ceremonial Funerals d'Adaptation pèse 2 ; lois de départ (date
    `DAT_14293fde8`) ignorées. Une loi de rang r, poids p envoie les checks
    r..r+p−1 (`SendLawCheck(arbre, rang, nom, effetEnAttente)`,
    popup « Check sent ») ; effet appliqué quand l'arbre a ≥ r items
    (`ReceiveLawProgressive(arbre, joueur)`, popup « Law received » ;
    application par `LoisAPPompe`, thread du jeu, hook du setter).
    **Persistance** `mods\ap_lois_<GameId>.txt` (A items / C checks /
    E guid + date d'activation + date d'application), cumulative ; relue au
    changement de partie et après `LoadGame` (`LoisAPApresChargement` : date
    de la sauvegarde) ; effet présent ssi même date d'activation et
    application ≤ date de la sauvegarde, sinon réappliqué si l'item est là.
    **F5** = un item pour l'arbre de la loi en attente la plus ancienne
    (remplace `BaselineKeys`). Les lois signées par un événement (appelants
    `FUN_14168c160`, `FUN_14186b190`) passent aussi par le hook.
    **1er test (2026-10-31)** : 104 lois, 18 arbres (Politics = Left + Right
    seuls, Labour découpé en petits arbres : comme voulu). Cemetery signée →
    check + effet en attente ; F5 → effet appliqué ; Ceremonial Funerals →
    checks 2 et 3. **Bug** : « Child Labour – Safe Jobs », signée **avant** le
    mod dans cette partie, comptait comme en attente → F5 lui a appliqué son
    effet une **2e fois**. Corrigé : ligne `P <guid> <date>` à chaque effet
    retenu ; à la relecture, une loi signée sans E ni P de la même date
    d'activation est « signée hors suivi » → effet tenu pour présent (E
    écrit) ; checks manquants rattrapés. Arbres nommés par leur 1re racine
    (loi sans loi en prérequis, hors Endless). Tirets typographiques des
    noms affichés → `-`. Non testé.
    **2e test (partie neuve, 2026-10-31)** : Cemetery signée, effet bien
    retenu (journal), espoir OK, mais **cimetière (bâtiment) débloqué quand
    même** : le bâtiment a son propre prérequis « loi signée »
    (`FrostpunkSociotechRequirement`) évalué par **`FUN_1416a71f0`**
    (`frostpunkrequirementsystem.cpp`, évaluateur de tous les prérequis ;
    cas 4 techno, 5, 6, **7 loi** → `FUN_1416ff990`, 12–15 scénario…).
    → hook `hkEvaluerPrerequis` : un SociotechRequirement (vtable relevée)
    qui **n'est pas** un prérequis de loi (ensemble trié des objets de
    `+0x38` et `+0x50` de toutes les lois, composites compris) répond 0 si
    sa loi est `retenue` (signée, effet en attente). Le livre garde donc
    l'état signé. Log `[LOIAP] prerequis … refuse`. Inconnu : le menu de
    construction se rafraîchit-il seul quand l'item arrive ? Non testé.
    **3e test** : un refus journalisé à la signature (prérequis hors lois de
    Cemetery), mais **bâtiment toujours débloqué**. Tout prérequis passe
    par `FUN_1416a71f0` (appelants : `FUN_1416a79e0` liste,
    `FUN_1416a62d0` composite, `FUN_1416a79d0`). Système des bâtiments
    (`DAT_142c3c408`, réflexion `FUN_1412f0980`) : cache non sauvegardé
    **`Ignored_LockedBuildings` `+0x22B8`** (éléments 0x18), événements
    `BuildingsUnlocked` `+0x2280`, `BuildingTypeConstructibleCheck`
    `+0x2130`. Écartés (pas le bâtiment) : `FUN_1416d5650` (objectifs de
    quête), `FUN_1416fec10` (date d'activation), `FUN_141700de0`,
    `FUN_1416ffe70` (livre), `FUN_141bc09f0` (UI du livre), `FUN_1415d66b0`
    (« temps depuis l'activation »), `FUN_14152e990` / `FUN_14152bb30`
    (refuge d'enfants). → **diagnostic** : chaque évaluation d'un prérequis
    d'une loi retenue (60, pile des 8 premiers refus) et hook de
    `FUN_1416ff990` qui journalise les « active ? » sur une loi retenue
    (30, pile des 8 premiers), sans changer le résultat.
    **Résultat (2026-10-31)** : le blocage **fonctionne** dans une partie
    déjà suivie (le menu de construction évalue bien ce prérequis :
    `FUN_1412f07c0`, système des bâtiments, appelé par l'événement de
    signature, et l'UI `FUN_141b81f40`, tous refusés). Le déblocage observé
    venait d'une **partie neuve mise en pause dès le début** : la pompe ne
    tourne pas en pause, le nouveau GameId n'était pas lu, la signature a été
    traitée avec l'état de la partie précédente (« check #1 déjà envoyé »),
    puis à la reprise la loi a été prise pour « signée hors suivi » → effet
    tenu pour présent, bâtiment débloqué, **effet jamais appliqué**. →
    `AssurerPartie(exclure)` : relecture au changement de GameId faite aussi
    **depuis la signature** (hkEffets), la loi en cours étant exclue de la
    vérification. Hook de diagnostic de `FUN_1416ff990` retiré. Non testé.
  - **Icônes des lois = type du check (2026-10-31, non testé)**, comme les
    technos. Réflexion `FUN_14145bdd0` (`FrostpunkProgressTreeEntry`) :
    `+0x80` **Icon tile = case (x, y) seule** ; la texture est commune à
    toutes les lois : `FrostpunkSocietyConfig` (réflexion `FUN_141439ef0`)
    `+0x328` « Sociotech icons texture », `+0x330` nombre de cases, `+0x338`
    / `+0x340` / `+0x348` ornements. `RafraichirIcones` (lois_ap.cpp, après
    chaque signature et chaque relecture de partie) : loi active →
    `icon_done` ; sinon icône du **prochain check de son arbre** (rang =
    1 + poids des lois signées), donc la même pour toutes les lois non
    signées d'un arbre. Type du check : `IconeDuCheckDeTest(arbre, rang)`
    (TEMPORAIRE, hachage stable) en attendant le serveur.
    **1er relevé** : texture = **`UI/Tiles/Icons.dds`** 16 × 16 = l'**atlas
    des bulles (#2350)**, où nos icônes sont déjà → cases
    **`bulleCol/bulleRow`** (`techCol/techRow` si c'était `tech_tree_atlas`).
    Toutes les lois sont en **(2,13)** (flèche jaune ronde), sauf Strike
    Busting (3, −8). Le livre (`FUN_141bbc320`, image `TECH_IMAGE` de chaque
    nœud `TECH_ENTRY` / `LEFT_OR_ENTRY` / `RIGHT_OR_ENTRY`) pose la texture
    `+0x328` (`FUN_141107140`) et la case `loi +0x80` (`FUN_141bb2d30` →
    `FUN_141107360`) à la **construction** de l'arbre.
    **Test (capture de l'auteur)** : rien de visible. Loi non signée = rond
    vide, loi signée = **plume** (case (4,4) d'`Icons.dds`) dans un rond
    bleu : `TECH_IMAGE` n'est pas l'image affichée. → diagnostic
    `hkConstruireLivre` (hook de `FUN_141bbc320`, 4 premières constructions) :
    nœuds du livre (`livre +0x1B0`, compte `+0x1B8`, pas 0x80 : `+0x00` GUID
    de la loi, `+0x10` poignée de l'élément), arbre d'éléments d'un nœud
    signé et d'un nœud non signé (`[LOIUI]` : nom, classe, visible
    `+0x270` bit 0, couleur `+0x120`, textures trouvées entre `+0x280` et
    `+0x600`).
    **Relevé** : le nœud (`TECH_ENTRY` / `LEFT_OR_ENTRY` / `RIGHT_OR_ENTRY`,
    UIButton → `STANDARD_NODE` / `SPECIAL_NODE` → `STATE_ELEM` → `light`,
    `ROTATING` (`ORNAMENT`…), `graphic`, `active`, `selected`,
    `highlighted` (cercles), **`circle_inside`** (UIPicture), `signed_text`,
    et `TECH_NAME`) n'a **aucun `TECH_IMAGE`** : la case `+0x80` n'est
    jamais affichée. La plume d'une loi signée = **`circle_inside`**
    (visible si signée, caché sinon, teinte (0.13, 0.78, 0.84)). Aucune
    texture lisible en clair dans les éléments (scan `+0x280..+0x600` vide).
    → `PoserIconeNoeud` (à chaque construction du livre, tous les nœuds) :
    tuile = texture config `+0x328` + case `bulleCol/bulleRow` posée sur
    `circle_inside` par `FUN_1410260c0`, élément montré (`FUN_14105ac90`) et
    remis en blanc (`FUN_1410297f0`).
    **Test (capture)** : icônes AP **visibles** (done sur Cemetery, autres
    selon l'arbre), mais **~3 fois trop grandes**, et **paires exclusives
    inchangées** (`LEFT_OR_ENTRY` / `RIGHT_OR_ENTRY` : pas de
    `circle_inside`, élément 0x0). → taille d'origine relevée sur la 1re
    image, puis `FUN_14102b170(image, l / 3, h / 3)` (`ICONE_REDUCTION`,
    `+0x210/+0x214`) ; arbre complet du 1er nœud de chaque sorte sans
    `circle_inside`.
    **Test** : taille OK, mais icônes décalées en haut à gauche (la position
    est celle du coin haut-gauche ; `circle_inside` d'origine 87 × 87 en
    (−13, −28)). → recentrage par **`FUN_14102a3e0(élément, vec4)`**
    (translation, mise en file) sur le centre (−13 + 43,5) **en x comme en
    y** (la plume d'origine n'est pas centrée en y ; décalage mesuré sur la
    capture ≈ 30 / 45). **Paires** : relevé de `LEFT_OR_ENTRY` /
    `RIGHT_OR_ENTRY` = demi-ronds dans un `Clip` (`STATE_ELEM` → `defoult`,
    `active`…), sans image intérieure ; le demi-rond **gauche** a, hors du
    `Clip`, une image vide **`BlurNotAvaiable`** (alpha 0) → l'icône de la
    paire y est posée (`IconeDePaire` : `icon_done` si l'une des deux lois
    est active), taille des nœuds simples, centrée sur sa propre géométrie
    (journalisée). Rien sur le demi-rond droit.
    **Test** : nœuds simples presque centrés (un peu bas → `ICONE_DECALAGE_Y`
    = −4) ; paires : tuile bien posée sur `BlurNotAvaiable` (263 × 163 en
    (−80, −25)) mais **invisible** (flou « indisponible », alpha remis à 0
    par l'état). → `PoserIconePaire` : **copie de `circle_inside`** d'un
    nœud simple (recette `+0x1E8` / ressource `+0x1F0` mémorisées,
    `FUN_14110c780`), ajoutée au demi-rond gauche (`FUN_141023d70`), tuile,
    taille 29, centrée sur le rond de la paire (translations cumulées
    `Clip` → `STATE_ELEM` → `defoult` → `circle_bg` + demi-taille). Paires
    traitées après les nœuds simples.
    **4e test** : paires visibles mais décalées d'environ une demi-taille de
    rond (~48 en bas à droite) → la translation de `circle_bg` est déjà son
    **centre** : plus de demi-taille (chaîne journalisée une fois) ; nœuds
    simples trop hauts à −4 → `ICONE_DECALAGE_Y = −2`. Loi **signée** :
    icône verdâtre, car l'animation d'état reteint `circle_inside` en
    (0.13, 0.78, 0.84) après notre blanc. → nœuds simples aussi sur une
    **copie** (`IconeSurCopie`, ajoutée au même parent `STATE_ELEM`, non
    liée aux animations ; réutilisée si déjà là), original réduit à taille
    nulle (plume cachée) ; tuile posée **directement** sur la copie
    (`FUN_141107140` texture + `FUN_141107360` case, corps de
    `FUN_1410260c0` sans la recherche par nom). Non testé.
    **5e test** : toujours teintée (paire signée bleue) et paire décentrée.
    Chaîne relevée : `Clip` (−6, −55) 107 × 200, `STATE_ELEM` (105, 96,7)
    59 × 59, `defoult` (0, 0) 64 × 64, `circle_bg` (−13, −28) 87 × 87 : les
    translations cumulées ne donnent pas le centre (ancres / échelle
    inconnues). → centre **mesuré** sur deux captures (point rouge) et
    interpolé : `PAIRE_CENTRE = (103, 32)`. Couleur : hook de
    `FUN_1410297f0` (poseur de couleur) qui force le blanc (alpha gardé) sur
    nos copies (`g_copies`, vidée à chaque construction du livre) et
    journalise qui les teinte ; couleur / mode de la recette de
    `circle_inside` journalisés. Si aucun appel n'est journalisé, la teinte
    passe par un autre chemin (animateur). Non testé.
    **6e test** : aucun appel du poseur de couleur sur nos copies, et la
    « recette » lue en `+0x1E8` donne des flottants absurdes : **`élément
    +0x1E8` = animateur d'état** (`FUN_1410240d0` : `animateur->vtable
    [0x138]` puis récursion sur les enfants, pointeur nul = ignoré). La
    copie rejouait l'état du nœud avec l'animateur de `circle_inside`. →
    `NouvelleCopie` met `copie +0x1E8` à 0 ; hook du poseur de couleur
    retiré. Paire : icône **sous le demi-rond droit** (dessiné après) →
    copie ajoutée **en dernier au parent commun** des nœuds (`racine
    +0x230` ; `FUN_141023e70` : `+0x228` dernier enfant, `+0x238`/`+0x240`
    frères), à `translation de la racine + (101, 35)` (moyenne des 3
    mesures) ; anciennes copies détruites (vtable `+0xE0`) à la
    construction suivante si leur parent a survécu.
    **7e test** : parent commun = `TREE_PANEL` (UIElement), racines de
    paire en (1500, 1100), (1180, 1450)… (`+0xB0` = 100, 100 ; `+0x8C` = 1) ;
    icône « plus du tout centrée ». Hypothèse : la translation d'une racine
    est son **centre** → position = `tr − taille / 2 + (101, 35)` ; tailles
    de la racine et du panneau journalisées.
    **8e test** : couleur OK (animateur vidé), au-dessus du demi-rond droit
    OK ; icône à (+84, −10) px du centre (racine 100 × 100, `TREE_PANEL`
    2600 × 2400, ~1,22 px / unité) → centre = **translation de la racine
    gauche + (−18, −7)** (`PAIRE_DECALAGE`).
    **9e test** : presque centrée, ~16 px trop à gauche, ~2 px trop bas →
    `PAIRE_DECALAGE = (−5, −9)`.
    **10e test** : un peu trop haut. Remarque de l'auteur : une fois la loi
    d'une paire signée, la **plume du jeu** s'affiche au centre exact du
    rond (sous notre icône) → s'en servir comme référence. Relevé de
    l'arbre (avec translations et tailles) du demi-rond de la loi signée,
    une fois par côté (`arbre du demi-rond SIGNE`).
    **Journal** : aucun demi-rond signé — une loi signée d'une paire devient
    un **nœud simple** (`TECH_ENTRY`, ex. « Child Labour - Safe Jobs » état 2,
    icône done déjà centrée par la logique des nœuds simples) et l'autre
    reste en demi-rond (`LEFT_OR_ENTRY` « Child Shelters »). Le « logo de
    signature » vu par l'auteur = cette icône du nœud simple ; la nôtre
    faisait doublon. → plus d'icône de paire quand l'autre loi est signée ;
    translation de la racine de chaque nœud journalisée (`racine en (x,
    y)`) pour caler le centre des paires non signées sur celui du nœud
    simple qui les remplace.
    **11e test** : doublon disparu ; paire encore ~10 px à gauche, ~3 px
    haut (capture à ~1,55 px / unité) → `PAIRE_DECALAGE = (2, −7)`.
    **12e test** : ~4 px trop haut, et ~2 px trop à droite selon l'auteur
    → `PAIRE_DECALAGE = (0,7, −5)`.
    **13e test** : 2 px plus bas demandés → `(0,7, −3,7)`.
    **14e test** (plein écran 1920, ~1,65 px / unité) : 3 px plus bas →
    `(0,7, −1,9)`. **VALIDÉ le 2026-11-01** (placement bon selon l'auteur).
    **Lois inaccessibles (2026-11-01, demande de l'auteur)** : une loi qu'on
    ne pourra plus signer perd son icône AP (`Accessible`, lois_ap.cpp) :
    non active et (une autre loi de son groupe d'exclusion, même catégorie,
    est active) ou (prérequis insatisfaisables : loi requise inaccessible,
    composites ET / OU évalués, NON et autres types tenus pour
    satisfaisables). Ex. Cemetery signée → Corpse Disposal, Organ
    Transplants, Organic Fertiliser sans icône. Non testé.
    **Nom du nœud = nom du check (2026-11-01, demande de l'auteur, comme
    l'arbre des technos ; non testé)** : `PoserNomCheckLoi` (lois_ap.cpp,
    appelée par `DiagNoeudsLivre` après la construction du livre, pour les
    lois accessibles) pose en texte brut (`FUN_14111bef0`) sur l'enfant
    `TECH_NAME` du nœud le **nom de l'arbre**, + ` [Signed]` pour une loi
    signée. **Pas de rang** (`#n`) : décision de l'auteur (2026-11-01), il ne
    pourrait pas être tenu à jour quand le check sera chez un autre joueur. **Panneau d'info** d'une loi sélectionnée :
    `FUN_141bc1440(livre)` (appelé par la sélection `FUN_141bc02f0`) pose le
    nom (`loi +0x68`) de la loi sélectionnée (GUID `livre +0x1C8`) sur
    `TECH_NAME_TEXT` (poignée `livre +0x130`) → hook `hkInfoLoi` : même nom
    de check après l'original, si la loi est accessible. Non testé.
    Question de l'auteur : l'icône des lois non signées change quand on
    signe dans l'arbre (Organ / Ceremonial Funerals : trap → important) —
    **normal dans le modèle par rang** : toutes les lois non signées d'un
    arbre montrent le prochain check (rang = 1 + poids signés ; Ceremonial
    Funerals pèse 2), tiré au hasard stable par `IconeDuCheckDeTest`.
