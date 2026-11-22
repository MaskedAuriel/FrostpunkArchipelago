# Sites d'exploration : liste, tirage fixe, icones des marqueurs

> Historique detaille, copie mot pour mot depuis CLAUDE.md le 2026-11-02.
> Le resume a jour est dans CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

- **Liste des sites d'exploration (2026-11-01, demande de l'auteur : pour
  leur assigner les checks ; non testé)** : F9 fait aussi
  `ScootingReleverSites` (scooting.cpp) : `[SITES] scenario X : N site(s)`
  puis une ligne `[SXLS]` par `FrostpunkExpeditionSiteComponent` de la
  partie : nom interne (`NomDuGabarit`, clé de `SITE_PAYLOADS`), dossier du
  gabarit (`ExpeditionSites/<dossier>/` : Story, Refugees, _SHARED…), noms
  affichés Display / Unvisited / Explored / Depleted (gabarit `+0x270` /
  `+0x268` / `+0x278` / `+0x280`) et description `+0x290` traduits
  (`TraduireCleJeu`, exportée par quetes.cpp), état `+0x34`, contenu
  courant `comp +0x48` ({définition, quantité} au pas de 0x10 ; nom = qword
  à `définition + 8`, `AP` pour notre porteur), nombre de dilemmes
  d'exploration (`gabarit +0x3D8`). `outils\quetes_excel.py` en fait un
  onglet **« Sites - <scénario> »** par scénario relevé (un nouveau relevé
  du même scénario remplace l'ancien : faire F9 dans chaque scénario, le
  journal les cumule). Contenu = état de la partie au moment du F9
  (vanilla tant que le site n'est ni exploré ni configuré).
  **1er relevé (2026-11-01, A New Home)** : 42 sites (36 `Story`, 6
  `_SHARED`), noms et descriptions traduits OK (nom « exploré » souvent
  vide), 23 sans contenu ; noms des ressources « ? » (lus à `définition +
  8`) → v2 : nom essayé à `+0x00` puis `+0x08` de la définition.
  **Validé le 2026-11-01 (6 scénarios)** : 211 sites (A New Home 42, The
  Arks 31, Refugees 36, Winterhome 26, Last Autumn 31, On the Edge 45),
  ressources nommées (« Steam Cores 1, Steel 56 »…). Le script génère
  aussi **`known_sites.h`** (table `KNOWN_SITES` : scénario, nom interne =
  clé de `SITE_PAYLOADS`, dossier, nom affiché, contenu au relevé, nombre
  de dilemmes), comme `known_quests.h` ; il compile, inclus nulle part.
  **Limite relevée par l'auteur** : un F9 ne voit que les sites **tirés**
  dans la partie (The Arks : `resources01-04, 06`, pas `05`) : les sites
  `_SHARED` (`[t-plain]` / `[t-mountains]` × `resources` / `[landmark]` /
  `[steamcore]`) sont tirés au sort. Décompilés du 2026-11-01 : tables
  `FrostpunkScenarioWorldMapRandomizationTableEntry` (0x90, `FUN_1416108b0` :
  `+0x00` nom, `+0x08` GUID, **`+0x18` Templates** = GUID de gabarit d'entité
  au pas de 0x10, compte `+0x20`, `+0x30` / `+0x48` ressources aléatoires,
  `+0x60` survivants, `+0x78` conditions d'activation), rangées dans
  **`FrostpunkScenariosConfig +0x368`** (réflexion `FUN_14129e3e0`) ;
  résolution (système de tirage `FUN_1418f2a30`) : **`FUN_141035c50(&pool
  DAT_142b2e710, &guid, 1)` = stub par GUID**, `stub +0x08` = nom du
  gabarit, `FUN_14111c4f0(stub, tampon, taille, 1, 0)` = chemin ; puis
  `FUN_1411180b0(stub, type, 1)` = composant de site (conditions de tirage
  `+0x400`). Une variation de carte (`FrostpunkScenarioWorldMapScene
  VariationEntry`, 0x58, réflexion `FUN_141610cc0` : `+0x18` liens vers ses
  tables, `+0x30` entités dynamiques, `+0x48` nom affiché) appartient à un
  `FrostpunkScenarioWorldMapSetupEntry` (`FUN_141610ff0` : `+0x18` scène,
  `+0x20` variations) ; les paramètres d'une partie (`FUN_1412c67f0`) lient
  `+0x50` World map setup et `+0x60` World map scene variation.
  → F9 v3 : `ScootingReleverTablesDeSites` (scooting.cpp) : `[TXLS] T
  <table> <n>` puis `[TXLS] E <table> <site> <chemin>` pour **toutes** les
  tables ; onglet « Tables de tirage » du script.
  **1er test : jeu bloqué** au 1er site de la 1re table (« Refugees », 48
  gabarits, 36 tables) : `FUN_141035c50(pool, &guid, 1)` **charge** le
  gabarit et attend le thread de chargement (`FUN_14111c700` →
  `FUN_141004480`), verrou du pool tenu → interblocage. → recherche pure
  **`FUN_141035cf0(pool, &guid)`** (dichotomie sur les stubs triés par
  GUID : `pool +0x30`, compte `+0x38`, `stub +0x10` = GUID), sans verrou.
  **Validé le 2026-11-01** : 36 tables, tous les GUID résolus. Tables des
  scénarios : `Story Randomized` (A New Home, 44), `TechHaven` (36),
  `Refugees` (48), `Winterhome` (26), `Builders` (32), `Reconquest` (45) ;
  les autres sont Endless (`Endless_*`, `Custom_*`, `Refugees_Castaway`).
  **Une table = TOUS les sites possibles du scénario**, fixes compris (A New
  Home : les 42 vus + `resources03` / `06` ; aucun site vu hors table).
  Script : caches **`outils\sites_releves.tsv`** (relevés fusionnés d'une
  exécution à l'autre, amorcé depuis l'Excel) et **`outils\tables_tirage.tsv`**,
  liste complète = table ∪ relevés (`TABLE_DU_SCENARIO`), colonne « Vu en
  partie » ; `known_sites.h` : 231 sites, champ `vuEnPartie`. Jamais tirés :
  The Arks 5 (`landmark site01/02/05/06`, `resources05`), Refugees 12 (dont
  `[t-coast]Old_Storehouse`, 3 `[t-mountains]resources`), Last Autumn 1
  (`53. Goods Station`). Onglet « Tables de tirage » (toutes les tables).
  **Décision de l'auteur (2026-11-01) : option 2, tirage forcé** (un check
  par site ; la carte d'un scénario doit toujours être la même, « possible
  que les joueurs ne le remarquent même pas »). Décompilé :
  `FUN_1418f2a30(système, &emplacements, table)` = tirage d'une table
  (appelants `FUN_1418f1440`, `FUN_14172a770`) → **`FUN_1418f2290`** :
  mélange des emplacements et des candidats par groupes de priorité
  (`gabarit +0x430`), appariement `FUN_1418edc80` ; **seul hasard = le
  générateur global multiply-with-carry `DAT_142a12d60` / `DAT_142a12d64`**
  (`base + 0x2A12D60`, multiplicateurs `0x9069` / `18000`). → hook
  `hkTirerTable` (scooting.cpp, installé par `InstallHooksExpeditions`) :
  graine fixe (`GRAINE_SITES_A / _B`) le temps de l'appel, état d'origine
  rendu après ; `SITES_TIRAGE_FIXE = true`. Log `[SITES] tirage fixe :
  table "…", N emplacement(s)`. **Validé le 2026-11-01** (A New Home, 2
  parties neuves) : mêmes 42 sites (partagés : `resources01-04`, `landmark
  site09 / site10` ; les parties d'avant le hook variaient). Compte des
  emplacements lu désormais avant l'appel (le tirage vide la liste : le log
  affichait 0). Reste : un relevé par scénario sur partie neuve pour figer
  la carte de chacun dans `known_sites.h` (les sites jamais tirés y
  resteront possibles mais hors carte).
  Question de l'auteur : les **emplacements** sont-ils fixes aussi (un
  `resources01` proche de la ville dans une partie, à l'autre bout dans une
  autre) ? En principe oui (l'appariement emplacement ↔ gabarit est fait
  dans le même tirage), sauf si l'ordre des emplacements fournis varie.
  → F9 écrit aussi `[SPOS] <scénario> <site> <x> <z>` (entité `+0x140`).
  **Test (The Arks, 2 parties neuves)** : mêmes 31 sites mais **14
  échangent leurs emplacements** (sites partagés + `Snowcrawler`) : l'ordre
  des emplacements fournis au tirage varie. → `TrierEmplacements` (hook du
  tirage) : emplacements (enregistrements 0x10, 1er qword = entité ou
  poignée, les deux essayés) triés par position (x puis z) avant la graine.
  Log `… tries par position` / `NON tries` (+ classe du qword si
  l'entité est introuvable). **Validé le 2026-11-01** (The Arks, 3 parties
  neuves) : `tries par position` à chaque fois, mêmes 31 sites, **0 site
  déplacé** entre les relevés. Tirage fixe (sites + emplacements) acquis.
  **Cartes figées (2026-11-01)** : un F9 sur partie neuve dans chacun des 6
  scénarios (tous `tirage fixe … tries par position`). Le script retient
  comme **carte figée** le dernier relevé d'un scénario qui suit un
  `[SITES] tirage fixe` de sa table (cache **`outils\carte_fixe.tsv`**),
  colonne « Sur la carte (tirage fixe) » des onglets Sites, champ
  **`surCarte`** de `known_sites.h` : **211 sites sur la carte** (A New Home
  42 / 44 possibles, The Arks 31 / 36, Refugees 36 / 48, Winterhome 26 / 26,
  Last Autumn 31 / 32, On the Edge 45 / 45) = les sites qui porteront un
  check. Limite : un relevé fait après le chargement d'une sauvegarde
  antérieure au hook, sans nouvelle partie entre-temps, serait pris pour la
  carte figée. Corrigé au passage : plusieurs F9 dans un même journal
  dupliquaient les lignes `S` des quêtes (comptes ×6) ; dédoublonnées par
  (scénario, GUID).
- **Icône AP sur les marqueurs de site de la carte (2026-11-01, VALIDÉ par
  l'auteur le 2026-11-01 : rendu « parfait », aucune exception, copie à
  48 × 48 en (18, 14), `icon_done` sur un site déjà exploré)** :
  le gabarit de site n'a pas d'icône. Marqueur =
  `FrostpunkUIExpeditionSiteSelectorTracker` (vtable `0x142217798`), mis à
  jour par **`FUN_141c1a0e0`** (commune à tous les sélecteurs de la carte :
  ville, groupes…) : tracker `+0x18` racine, `+0x28` entité (poignées) ;
  composant de site = `*(entité + 0x2A0 + FUN_14148c130() × 8)`. Arbre
  (presets `SIZE_SMALL` / `SIZE_LARGE`, gabarit `+0x3C8`) : `SITE_SELECTOR` →
  `BUTTON` → … → `highlight_anim_move_container` → `not_frozen_container` /
  `frozen_container`, chacun avec une image **`ICON`** (48 × 48, texture
  **`UI/Tiles/Icons.dds`** = atlas des bulles). Sa case est posée par
  l'**animateur d'état** (`FUN_1416ae7b0` = ApplyExpeditionSitePresets :
  `NOT_FROZEN` / `FROZEN`, `UNVISITED` / `VISITED` (État ≥ 1) / `EXPLORED`
  (≥ 2) / `DEPLETED` / `TARGETED_BY_TASK`, outpost, settlement, shelter,
  `MARKED`…). → hook de `FUN_141c1a0e0` (scooting.cpp,
  `IconeMarqueurSite`) : dans chaque conteneur, **copie de `ICON` sans
  animateur** (`IconeAPSurCopieDe`, lois_ap.cpp) avec notre case, original à
  taille nulle (revérifiée à chaque mise à jour) ; copie retrouvée par son
  animateur nul (pas de doublon). Type : `IconeCheckDeTest(nom du site, 0)`
  (TEMPORAIRE), `icon_done` dès l'état `+0x34` ≥ 2. Log `[SITEUI]`.
- **Apparition des sites (2026-11-03, demande de la session apworld : rattacher
  chaque site à la région logique où il devient visible ; non testé)**.
  Les 211 sites existent tous dès la partie neuve (relevés F9) : la question
  est leur **visibilité**. Décompilés du 2026-11-03 :
  `FrostpunkWorldMapEntityComponent` (réflexion `FUN_14197cbd0`, 0x78) :
  `+0x30` Discovered, `+0x38` DiscoveryTime, `+0x48` Visible, `+0x4C`
  SquaredDistanceToCity ; type = `FUN_14132fd90()`, composant = `*(entité +
  0x2A0 + type × 8)`. Gabarit `FrostpunkWorldMapEntityComponentTemplate`
  (`FUN_14197d2f0`) : `+0x28` **Initially revealed**, `+0x29` **Revealed in
  sight range**. Site (`FUN_141a157b0`) : `+0xF8` **Reveals**
  (`SafePtrDynarray<Entity>` : sites révélés par son exploration), `+0xE0`
  AdditionalConnections. Emplacement de tirage
  (`FrostpunkWorldMapRandomizationComponentTemplate`, `FUN_141ab74a0`) :
  `+0x58` Reveals (noms d'emplacements), `+0x7C` Initially revealed. Effets :
  `FrostpunkRevealWorldMapEntityEffect` (sans champ : révèle l'entité du
  contexte), `FrostpunkAddSiteRevealsEffect` (`+0x18` GUID de gabarits),
  `FrostpunkCreateWorldMapEntityEffect`.
  Quatre façons d'apparaître : (1) révélé au départ ; (2) à portée de vue
  (ville / éclaireurs) ; (3) par l'exploration d'un autre site (liste
  Reveals) ; (4) par un effet d'événement / quête / dilemme.
  → **F9** (`ScootingReleverSites`) : `[SREV]\tV\t<scénario>\t<site>\t
  <découvert>\t<visible>\t<révélé au départ>\t<à portée de vue>\t<distance>`
  et `[SREV]\tR\t<scénario>\t<site>\t<site révélé>` ; relevé des quêtes :
  `[SREV]\tE\t<classe>\t<répertoire>\t<source>\t<moment>\t<cibles>` + déclencheurs
  de l'événement. **Trace en jeu** (`SurveillerVisibilite`, tick) :
  `[SREV] CHANGE "<site>" (<scénario>) : decouvert a -> b, visible c -> d`,
  à lire avec les `[QUETEAP] FIN` du même journal.
  **1er relevé (2026-11-03, parties neuves ; REFUGEES absent : aucun F9
  dans ce scénario avec la nouvelle DLL)** : composants de carte et gabarits
  lus pour tous les sites (type du composant = 53). « Initially revealed » et
  « Revealed in sight range » valent 0 partout sauf `NewLondon` (On the
  Edge) : la portée de vue ne révèle rien ; ce sont les **listes Reveals**
  (140 liens) qui font apparaître la carte. Visibles au départ : A New Home
  2 (`Site_001`, `Site_002`), The Arks 1 (`[quest]Expedition01`),
  Winterhome 5, Last Autumn 1 (`02. Killed Moose`), On the Edge 4. Fermeture
  par les listes Reveals (= région `Start`, l'exploration ne demande que le
  Beacon / Forager Quarters) : A New Home 30 / 42, The Arks 10 / 31,
  Winterhome 24 / 26, Last Autumn 23 / 31, On the Edge 14 / 45. Sites
  « racines » (cachés et révélés par aucun site) : A New Home `Site_049_A0`,
  `053_B0`, `057_C0` (chaînes A / B / C) ; The Arks `[quest]SouthPass`,
  `[quest]MistyValley`, `[NM]Expedition01` ; Winterhome `[lore05]_Trail_South`,
  `[t-plain][landmark]site08` ; Last Autumn `61. Workers Camp`, `42. Natures
  Sculpture`, `66. Polar Bear Cave` ; On the Edge `29. Natural Terrace`,
  `08. Tower Ruins`, `20. Transshipment Station 01`, `23. Ice Caverns` (cibles
  des `AddSiteRevealsEffect` de `[3] Food settlement` et `[5] Prepare for
  confrontation`, effets initiaux) et les 9 `TG* Generic` / `TD* Dilemma`
  (aucune source trouvée). Les `RevealWorldMapEntityEffect` révèlent l'entité
  du **contexte** : leur cible n'est pas lisible statiquement. Sources
  candidates par scénario : A New Home `[ENDGAME] 002b … SURVIVORS`
  (initiaux), `U: Refugees in the City3` ; The Arks `003 EXPLORE THE
  FROSTLAND` (initiaux, succès), `005 NEW MANCHESTER - FINDING THE WAY`
  (initiaux) ; Winterhome `E: Trail South Discovered`, `003 PREPARE
  EVACUATION` (initiaux) ; Last Autumn `003 GENERATOR CONSTRUCTION SITE`
  (succès), `New Allerton` (initiaux), dilemmes `[Act III] Missing lookout
  II`, `[Telegram] 2`, `[Telegram] 4a`. → table provisoire
  **`outils\sites_apparition.tsv`** (région `Start` certaine ; le reste
  « A CONFIRMER », avec la chaîne et la source candidate).
  **Écart de carte figée** : Last Autumn tire ici `53. Goods Station` au lieu
  de `49. EIC Train Station` (`known_sites.h`, `carte_fixe.tsv`).
  Last Autumn : le Beacon est remplacé par le **Forager Quarters** (même
  rôle, réponse de l'auteur du 2026-11-03).
  **v2 (2026-11-03, non testé)** : (1) **cible des révélations** : le
  contexte vient d'un effet parent à sélecteur (`FrostpunkSelectEffect`
  `+0x18` Selector, `+0x50` Effects). Sélecteurs décompilés :
  `FrostpunkExpeditionSitesSelector` (`FUN_1415f8ed0`, `+0x18` GUID du
  gabarit de site), `FrostpunkEntitiesByNameSelector` (`FUN_1415f8ac0`,
  `+0x18` « Entity name »), `…WithTagSelector`, `FrostpunkUnionSelector`. F9 :
  `[SREV]\tC\t<rép.>\t<source>\t<moment>\t<effet>\t<sélecteur=cible>` pour
  tout effet à sélecteur qui contient (≤ 4 niveaux) une révélation.
  (2) **Tirage de Last Autumn** : décompilé `FUN_1418f2a30` : chaque candidat
  est écarté si ses **conditions de tirage** (gabarit de site `+0x400`,
  compte `+0x408`) sont fausses, évaluées seulement si `DAT_142a2afa4 == 1`
  (`FUN_141696fe0`) — hasard hors graine possible : l'état de la partie.
  Trace après chaque tirage : `[STIR] table … : N candidat(s), evaluation des
  conditions = X` puis `[STIR]\t<table>\t<i>\t<site>\tconditions=N\t
  priorite=P\t<conditions>`.
  **Relevé v2 (2026-11-03)** : Last Autumn, 2 parties neuves : **49 dans
  l'une, 53 dans l'autre** (le tirage varie bien) ; `evaluation des
  conditions = 1` mais `conditions=-1` partout (lecture fausse : le stub
  passé au lieu du gabarit chargé `stub +0x38` → corrigé). Cibles des
  révélations lues (`[SREV] C`) : presque toutes par
  **`EntitiesByNameSelector=DefaultRandomizationPoint_NNN`** (nom de
  l'**emplacement** de tirage), quelques-unes par nom de site
  (`Site_036_Crows_Nest_000`, `Dreadnought`) ou par gabarit
  (`ExpeditionSitesSelector` : On the Edge `[3] Food settlement` →
  `04. Skull Cave` ; `[5] Prepare for confrontation` → `07. Stone Wall`,
  `18. Observatory Ruins`, `22. Fortified Hut` : sites dont la liste Reveals
  reçoit Natural Terrace / Tower Ruins / Transshipment / Ice Caverns).
  Décompilé `FUN_1415fd000` (sélection par nom) : `FUN_141017230(*(base +
  0x2B2DE98), &nom, &sortie)` (index des noms du gestionnaire d'entités) →
  **F9 v3** : `… -> <site>` résolu dans la partie courante
  (`ScootingSitesDuNom` : entité du site, sinon même position).
  **Refugees** (1er relevé) : 1 site visible au départ (`[ref]Builder_Camp`),
  14 / 36 atteints par les Reveals. **Paires exclusives** sur la carte :
  `[ref]Trail_Bend` / `-dead`, `[ref]Black_Spire` / `-dead`,
  `[ref]White_Pass` / `-dead` (variantes selon un dilemme, ex.
  `Survivors09 - BlackSpire` / `-noBeacon`) : **un seul des deux** apparaîtra
  dans une partie → un check par paire au plus (à signaler à l'apworld).
  **Relevé v3 (2026-11-03, 6 scénarios, Last Autumn ×2)** : (1) conditions
  de tirage lues : **aucune** dans Last Autumn (seuls les `[steamcore]` de
  The Arks / Refugees en ont une), et les 2 parties n'ont qu'**1 position
  sur 31 en commun** : ce ne sont pas les mêmes emplacements. Cause
  (décompilés) : **`FUN_14131de20(paramètres, scénario)`** applique les
  réglages du scénario à la création de la partie et tire au hasard
  (générateur global) un élément de chaque liste : dispositions de ville
  `+0x1D8`, de carte `+0x208`, autres listes ; paramètres `+0xA0` =
  disposition de carte, `+0xA8` = **variante** tirée par
  **`FUN_14160c0f0`** (disposition `+0x20`, éléments de 0x58, compte
  `+0x28`). La variante décide quels emplacements existent. → hook
  **`hkReglagesScenario`** (scooting.cpp) : graine fixe le temps de l'appel
  pour les 6 scénarios AP (fige aussi la disposition de la ville), log
  `[SITES] reglages "<scénario>" : disposition de carte … (N possible(s)),
  variante … (N possible(s))`. Non testé. **La carte figée de Last Autumn
  est à refaire** (`known_sites.h`, `carte_fixe.tsv`).
  (2) **Cibles résolues** (`-> site`) : toutes les racines ont une source,
  sauf Winterhome `[t-plain][landmark]site08`. A New Home A0 / B0 / C0 ←
  `[ENDGAME] 002b … SURVIVORS` (début) ou `U: Refugees in the City3` ; The
  Arks SouthPass / MistyValley ← `003 EXPLORE THE FROSTLAND` (début),
  `[NM]Expedition01` ← `005 NEW MANCHESTER - FINDING THE WAY` (début) ;
  Winterhome `Trail_South` ← `E: Trail South Discovered` ; Last Autumn
  Natures Sculpture ← `003 GENERATOR CONSTRUCTION SITE` (succès) /
  `[Telegram] 2`, `4a`, Workers Camp ← `New Allerton` (début) / `[Telegram]
  4a`, Polar Bear Cave ← `[Act III] Missing lookout II` ; On the Edge
  `TG1-6` / `TD1-3` ← quêtes optionnelles `[TradeRoutes]` (Ark Springs,
  Creve-Neige, Children's Mine) ; Refugees : `Trail_Bend`, `Landing_Place`,
  `White_Pass`, `Black_Spire` ← dilemmes `[1][OurPeople] Survivors04 / 06 /
  07 / 09`, variantes `-dead` ← `Our People - timeout`, `Radio_Mast` ←
  `[3][Lords] Radio Mast`, `[lordCamp] Camp_A/B/C` ← événements des Lords.
  → **table v2 `outils\sites_apparition.tsv`** (région `Start` sûre,
  A New Home A/B/C = `apres 006 REFUGEES`, le reste « A VALIDER » avec la
  question posée ; Last Autumn « A REFAIRE »).
  **Variante fixe VALIDÉE le 2026-11-03** (Last Autumn, 2 parties neuves) :
  `[SITES] reglages "BUILDERS" : disposition de carte "Builders" (1
  possible(s)), variante "Builders Random A" (2 possible(s)) - graine fixe`
  les deux fois, mêmes 31 sites aux mêmes positions. Carte figée régénérée
  (`quetes_excel.py`) : Last Autumn a `53. Goods Station`, plus `49. EIC
  Train Station` (`known_sites.h` : seules les lignes BUILDERS changent).
  **Décisions de l'auteur (2026-11-03)** : The Arks `003 EXPLORE THE
  FROSTLAND` et `005 NEW MANCHESTER - FINDING THE WAY` → après `001 FIND
  FACTORY PLANS` ; On the Edge : Natural Terrace et les sites de `[5] Prepare
  for confrontation` → même région que `[3] Food settlement` (après) ;
  Refugees : sites `[1][OurPeople]` → après `001 SAVE YOUR PEOPLE`, **un seul
  check par paire exclusive**, camps des Lords et Radio Mast → après `003
  FACE THE LORDS` ; Last Autumn : Workers Camp et Polar Bear Cave → après
  `006 GENERATOR FINALIZATION` ; Winterhome `Trail_South` et `site08`
  (« Broken Tree », aucune source au relevé) → dernière section, après
  `004A FINAL HOURS` ; On the Edge `TG*` / `TD*` (routes commerciales
  optionnelles) → après `[3] Food settlement`.
  **Table VALIDÉE le 2026-11-03** : 211 sites, régions Start 125, A New Home
  après 006 : 12, The Arks après 001 : 21, Refugees après 001 : 14 / après
  003 : 8, Winterhome après 004A : 2, Last Autumn après 003 : 3 / après 006 :
  5, On the Edge après [3] : 31 ; 3 paires exclusives à Refugees (un seul
  check par paire). Transmise à la session apworld.
  **Outil** : `outils\sites_apparition.py [journal]` (décisions dans
  `RACINES`) ; cache des relevés `outils\sites_revelations.tsv` (D visible au
  départ, C caché, R lien Reveals ; un scénario relevé remplace ses lignes ;
  amorcé le 2026-11-03 depuis les analyses des journaux écrasés, sans les
  liens Reveals des sites `Start`).
- **Contenu réel des sites (2026-11-03, demande de la session apworld :
  survivants et noyaux = item useful ; non testé)**. `known_sites.h` ne voyait
  que `comp +0x48` (105 sites « (rien) »). Décompilés : composant
  (`FUN_141a157b0`, 0x170) `+0x48` Resources, `+0x60` BonusResources, `+0x78`
  Technologies, `+0x90` Artifacts, **`+0xA8` OutsiderSurvivors**
  (`FrostpunkSurvivorsEntry` 0x24 : `+0x00` Count, `+0x04` lien de classe
  sociale), `+0x110` ExplorationDilemma (indice retenu). Gabarit
  (`FUN_141a161d0`) : **`+0x118` Treasure** (`FrostpunkExpeditionSiteTreasure
  Entry` 0xF0, `FUN_141a17aa0` : `+0x00` Resources / `+0x18` Bonus
  (`FrostpunkResourceAmount` 0x14 : lien `+0x00`, Amount `+0x10`), `+0x30`
  Technologies, `+0x48` Artifacts, `+0x70` Survivors, `+0x88` dilemme
  d'arrivée en ville, `+0xA8` / `+0xC0` ressources aléatoires (0x20), `+0xD8`
  survivants aléatoires (0x30)) ; `+0x3D0` ExplorationDilemma (0x28 : `+0x00`
  lien du dilemme, `+0x10` conditions) ; `+0x3E8` ExplorationInstantEffect
  (0x78 : `+0x00` liens `FrostpunkWorldMapEffectEntry`, `+0x18` conditions).
  Effets à ressources : `ChangeExpeditionResourcesEffect` et
  `GiveResourcesToStationingExpeditionEffect` (`+0x18` montants),
  `ChangeResourcesEffect` (`+0x18` ressource, `+0x28` Addition, expression).
  Collections (recherche par lien = vtable `+0x88`) : classes sociales `base +
  0x4072078` (PopulationConfig `0x4072020 +0x58`), effets de carte `base +
  0x40806F8` (WorldMapConfig `0x40805A0 +0x158`, entrée `+0x18` effets).
  `FrostpunkResourceEntry` : `+0x28` Display name.
  → **F9 v4** : `sites_contenu.cpp` (`SitesContenuReleve`, appelé par
  `QuetesPompe`) : lignes `[SCONT]` S / R / P / X / D / C / E (format en tête
  du fichier) et `[SRES]` (ressources de l'économie : nom interne, GUID, nom
  affiché, pour vérifier `structuralprofiles` / `steelcomposite` /
  `steamexchanger` de Last Autumn). À faire sur **partie neuve** dans les 6
  scénarios.
  **Relevé du 2026-11-03 (6 scénarios, parties neuves, 211 sites)** :
  survivants dans le composant (`+0xA8`) pour 30 sites (A New Home 14, The
  Arks 1, Refugees 7, Winterhome 3, Last Autumn 2, On the Edge 3) ; classes
  sociales `Workers` / `Specialists` / `Children` / `Automaton`. Gabarit et
  composant concordent : les survivants et ressources « aléatoires » du
  gabarit sont tirés à la création du site (valeurs du composant). Aucun
  survivant ni noyau ne vient d'un choix de dilemme : les dilemmes
  d'exploration ordonnent surtout une tâche à l'expédition
  (`OrderStationingExpeditionToPerformTaskEffect`, `+0x18` = n° de tâche),
  donnent une technologie aléatoire (4 sites A New Home : `Site_051_A2`,
  `056_B3`, `060_C3`, `052_A3`, choix « Take technology ») ou retirent des
  ressources (`63. Convoy in Distress` : −10 Steel Plates, −20 Steam Tools).
  76 sites ne donnent rien (landmarks, mirages, sites `[food]` de Last
  Autumn exploités par avant-poste, variantes `-dead` de Refugees, `TG*` /
  `TD*` d'On the Edge). `BonusResources` (`+0x60`) non vide sur 7 sites.
  **Noms mod de Last Autumn confirmés par GUID** (`[SRES]`) : `Wooden
  Materials` = `structuralprofiles`, `Steel Plates` = `steelcomposite`,
  `Steam Tools` = `steamexchanger` (noms affichés hors Last Autumn :
  Structural Profiles, Steel Composites, Steam Exchangers).
  → **`outils\sites_contenu.py [journal]`** → **`outils\sites_contenu.tsv`**
  (scenario | site | source `direct` / `bonus` / `dilemme <nom> choix N` |
  ressources | survivants | technologies | texte du choix), 224 lignes.
  Limite : les **quantités** tirées ne sont peut-être pas couvertes par la
  graine fixe (non vérifié sur deux parties) ; la présence d'un type l'est.
  **Retour de l'auteur (2026-11-03)** : les 4 sites « Take technology »
  d'A New Home ne donnent **rien** en jeu → l'effet `GiveRandomTechnology…`
  est ignoré par le script (sites vides). Les quantités dépendent
  probablement de la difficulté et sont tirées au hasard : seule la nature
  du contenu compte (survivants / noyaux / ressources seules).
  **Quantités et difficulté (2026-11-03, vérification demandée par
  l'auteur : des quantités variables ajouteraient des checks)**. Décompilés :
  `FUN_1418f1440` (système de tirage) appelle, après le tirage des sites,
  **`FUN_1418ef2f0(&sortie, &sites, table)`** : les **totaux** de la table du
  scénario (`+0x30` ressources, `+0x48` bonus, `+0x60` survivants,
  `FrostpunkResourceAmount` / survivants) sont répartis entre les sites tirés
  (enregistrements 0x18, 1er qword = entité) après un mélange par le
  générateur global `DAT_142a12d60/64` ; puis `FUN_141889690` (ressources
  `+0x48`), `FUN_141889610` (bonus `+0x60`), `FUN_141889930` (survivants
  `+0xA8`) **ajoutent tel quel**. **Aucune lecture de la difficulté** dans
  cette chaîne : les quantités ne varient que par le hasard (graine non fixée
  jusqu'ici : `hkTirerTable` ne couvrait que `FUN_1418f2a30`). → hook
  **`hkRepartirContenu`** (scooting.cpp, `+0x18EF2F0`) : sites triés par
  position, graine fixe (`GRAINE_SITES_A/B ^ 0x5A5A5A5A`), état rendu après.
  Log `[SITES] repartition du contenu fixe : N site(s), tries par position`.
  Non testé : deux parties neuves du même scénario doivent donner les mêmes
  lignes `[SCONT]`.
  **VALIDÉ le 2026-11-03** (A New Home, 2 parties neuves) : `repartition du
  contenu fixe : 42 site(s), tries par position` les deux fois, 144 lignes
  de contenu identiques. Les relevés des 5 autres scénarios (avant le hook)
  sont à refaire ; `sites_contenu.py` est désormais **cumulatif** (un
  scénario relevé remplace ses lignes de la table).
  **Relevé des 5 autres scénarios (2026-11-03, répartition fixe)** : table
  `outils\sites_contenu.tsv` complète (220 lignes, 211 sites). Mêmes comptes
  qu'avant le hook : sites avec survivants A New Home 14, The Arks 1,
  Refugees 7, Winterhome 3, Last Autumn 2, On the Edge 3 ; avec noyaux
  (contenu direct) 10 / 10 / 9 / 6 / 3 / 0 : seules les quantités bougeaient.
  **Table VALIDÉE par l'auteur le 2026-11-03.**

## 2026-11-10 — Steam : icônes et noms AP des sites absents (getters de type mal appariés)

Retour de l'auteur (partie Steam) : icônes des marqueurs et noms des sites
d'exploration redevenus vanilla. Journal : hook `[SITEUI]` posé mais
aucune ligne d'icône ; `IconeMarqueurSite` sortait en silence, faute de
composant de site.

Cause : les getters de type de composant (`FUN_14148c130` et ses
semblables, init de statique de thread + `FUN_14109a700`) sont des
centaines de fonctions identiques à l'octet près ; l'appariement Steam
« par voisinage » en avait décalé trois d'un cran (0x90). Vérification
par la chaîne d'appels jusqu'à la chaîne du `.h` de la classe
(`frostpunk…component.h`, citée par l'assert du PropertyManager), sur
toute la table `VERSION_STEAM` :

| Epic | Steam (faux) | Steam (corrigé) | Classe |
|---|---|---|---|
| `0x12EC200` | `0x1186A30` (building) | `0x1186AC0` | SelectionComponent (`groupes.cpp`, `OFF_TYPE_CARTE`) |
| `0x132FD90` | `0x11CA550` (expedition sending) | `0x11CA5E0` | WorldMapEntityComponent (`scooting.cpp`, `OFF_TYPE_ENTITE_CARTE`) |
| `0x148C130` | `0x1326860` (canteen) | `0x1326980` | ExpeditionSiteComponent (`scooting.cpp`, marqueurs et noms) |

Les autres getters de ce motif dans la table étaient justes. Non testé en
jeu.

## 2026-11-10 — Nom du site sur la carte = nom du check

Demande de l'auteur : le nom d'un site sur la carte doit être le nom du
check qu'il contient. `hkTranslate` (hooks_loc.cpp) : si la clé est l'un
des 4 noms par état du gabarit d'un site de la carte (`0x270`, `0x268`,
`0x278`, `0x280`, comparés à la clé traduite :
`ScootingSitePourCleNom`, scooting.cpp) et que le site a un scout
(`ApNomDuCheckSite`, archipelago.cpp), on renvoie notre propre objet de
traduction (`ApLocSite`, un par site, 128 au plus) avec
`"<joueur> - <item>"` (`ApNomDuScout`, donc contenu exact pour les
Supplies). Descriptions inchangées ; sans scout : nom vanilla. Le même
nom sert partout où le jeu traduit ces clés (panneau du site compris).
Log `[SITE] nom sur la carte : "<site>" -> "<check>"`. Non testé en jeu.

### 2026-11-10 — Boîte du nom sur la carte trop petite

Capture de l'auteur : le nom du check est coupé (1re et dernière lignes
manquantes). Relevé de l'arbre du marqueur (preset de site) :

```
site_name_container (tr -129, 0)
  background_under_text 104 x 122 (bandeau : left/center/right_bg_underTXT)
  "" (tr -9, 0)
    name_mask    UIPicture  tr (179, 71)   0 x 77   (masque, largeur animee)
      NAME       UITextBox  tr (-142, -19) 289 x 105 (tracker +0x80)
```

Le masque (77 de haut) coupe le texte. `AgrandirNomMarqueur`
(scooting.cpp, à chaque mise à jour du tracker) : NAME 420 x 160 en
(-207,5, 0), masque 160 de haut en y = 24,5 (centres conservés), largeur
du masque laissée à l'animation (journalisée : `largeur du masque du
nom`). Seulement sur la géométrie du relevé. Poseurs exportés de
lois_ap.cpp : `ElementPoserTaille` (`FUN_14102b170`),
`ElementPoserPosition` (`FUN_14102a3e0`). Le bandeau de fond n'est pas
retouché. Non testé en jeu.

### 2026-11-10 — Décision de l'auteur : nom du check dans la description

Test de l'agrandissement : le texte n'apparaissait que brièvement au survol
(l'animation du masque reprend la main). L'auteur change d'avis : **le nom
du site reste vanilla** (carte et panneau), **le nom du check remplace la
description** du panneau du site sélectionné. `AgrandirNomMarqueur` et les
poseurs exportés retirés ; `ScootingSitePourCleNom` devient
`ScootingSitePourCleDescription` (4 descriptions par état du gabarit :
`0x290`, `0x288`, `0x298`, `0x2A0`). Log `[SITE] description du site :
"<site>" -> "<check>"`. Non testé en jeu.

### 2026-11-10 — The Arks : Lone Boulder et Broken Tree sans leur check

Retour de l'auteur (graine `AP_99291626087738687974`, Steam) : la
description de ces deux sites reste vanilla. Les scouts sont là
(`7747300058` Progressive Beacon, `7747300062` Progressive Steel Mine),
mais aucune ligne `[SITE] description du site` pour eux. **Cause** (lue
dans `localizations.dat`) : les landmarks `_SHARED/[t-plain][landmark]siteNN`
n'ont **pas de clé de description propre**, seulement Display / Unvisited /
Depleted name. Leur texte commun (« A distinct spot in Frostland… ») vient
d'une clé partagée qui ne nomme pas le site, donc `NomDeSite` échoue. Ça
touche **tous les landmarks**, dans tous les scénarios. Dans The Arks, seuls
site04 et site08 sont visibles dès le départ, les autres après 001.
**Correctif** (`ScootingSitePourCleDescription`) : 1) clé propre au site,
comme avant ; 2) sinon, site de l'entité **survolée** (sélection `+0x2298`),
puis des entités **sélectionnées** (`+0x2250`, pas de 0x10, compte
`+0x2258`), à condition que la clé soit une description de son gabarit.
Risque à surveiller : si le jeu garde en cache la traduction d'une clé, tous
les landmarks montreront le check du premier affiché. Compilé, déployé
Epic + Steam, non testé.

## 2026-11-22 — On the Edge : New London retiré des checks

- **Constat (auteur)** : une expédition de l'histoire (exp 5, créée par le
  jeu après la bulle « [Act 1-6] Independence day ») arrive à New London et
  ne disparaît pas (« même problème que The Arks »).
- **Journal** : `NewLondon` était une location de site (`7747300199`,
  contenu vanilla vide) ; le mod y fournissait un tableau de contenu avec
  l'item AP à chaque fois que le jeu le vidait (`[SITE] NewLondon : tableau
  fourni`, 8 fois, aux dilemmes `[Diplomacy] New London …`). Le site n'est
  jamais « exploré » (pas de `site NewLondon explore`) : check impossible à
  obtenir, contenu AP permanent sur le site, expédition qui y stationne.
- **Décision de l'auteur** : la location est retirée de l'apworld.
  `SITES_SANS_CHECK` (generer.py) ; `data.py` / `known_ap_ids.h`
  régénérés (On the Edge : 130 locations hors température) ; id gardé
  dans `ids.tsv`. Le mod ne voit plus New London comme site AP (contenu
  vanilla, description vanilla, **marqueur de carte vanilla** :
  `IconeMarqueurSite` ignore un site de `g_sites` sans contenu AP).
- **Les graines déjà générées** gardent la location New London (jamais
  obtenable) ; il faut une nouvelle graine. apworld réinstallé
  (`custom_worlds\frostpunk.apworld`, 2026-11-22 00:16 ; contient aussi le
  travail non commité des autres sessions). Génération non relancée.
- Compilé, déployé Steam (00:15). Non testé.
