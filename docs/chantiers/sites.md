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
