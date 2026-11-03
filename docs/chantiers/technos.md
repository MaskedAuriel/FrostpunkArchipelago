# Revue du systeme de technos (icones, noms, persistance)

> Historique detaille, copie mot pour mot depuis CLAUDE.md le 2026-11-02.
> Le resume a jour est dans CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

- **Revue du système de technos (2026-11-01)** : (1) **la détection
  d'ouverture de l'arbre n'a jamais fonctionné** : `g_instantArbre` est posé
  par `TexteArbreTechno` (hooks_loc), appelée nulle part → `SurveillerArbreTechno`
  ne posait jamais rien ; seules les écritures permanentes faites à la fin
  d'une recherche (`RafraichirIconeTechno`) agissaient, d'où quelques icônes
  seulement (1er test de l'auteur). Défauts annexes de l'ancien mécanisme :
  icône du check au lieu de l'icône effective à la réouverture, icône AP
  d'une miniature écrite dans le gabarit du bâtiment (donc dans le **menu de
  construction**), double sauvegarde d'un gabarit. → **Nouveau mécanisme**
  (`InstallHooksIconesTechnos`, IconSubManager, appelé par
  `InstallResearchHooks`), décompilés du 2026-11-01 : l'image d'une techno est
  posée par `FUN_141bc5810(panneau, techno, sortie, p4)` (nœud de l'arbre,
  appelé par `FUN_141bc3eb0` = construction de l'arbre, à chaque sélection
  d'un onglet par `FUN_141bc8440`), `FUN_141be67f0(panneau)` (recherche en
  cours, techno = `*(*(base + 0x4062BA8) + 0x2260)`) et
  `FUN_141bb2b60(panneau, techno)` ; chacune pose `BUILDING_IMAGE` (techno
  qui débloque un bâtiment, `FUN_1416a83b0` : case du gabarit `+0x178`,
  atlas #3037) ou `TECH_IMAGE` (techno `+0x80`, atlas #1179) par
  `FUN_141bb2d30(image, case x | y << 32, nb)`. Pendant ces trois appels
  seulement : techno `+0x80` = notre case (rendue après), et dans
  `FUN_141bb2d30` toute autre case (= celle du bâtiment) devient notre case
  des miniatures. **Aucune écriture permanente** ; `SurveillerArbreTechno`
  ne fait plus que rendre d'éventuelles icônes de bâtiment encore
  sauvegardées. **1er test (captures de l'auteur)** : icônes AP partout, mais
  **décentrées** pour les technos qui débloquent un bâtiment (Coal Thumper
  dans l'arbre, Coal Mining dans l'atelier) : nos cases de l'atlas des
  miniatures #3037 ne sont pas cadrées comme dans `BUILDING_IMAGE`. → hook
  de **`FUN_1416a83b0(DAT_14408ea70, techno)`** (liste des bâtiments
  débloqués, recherche pure) : pendant la construction et pour la techno
  AP en cours seulement, renvoie 0 → le jeu prend `TECH_IMAGE` avec notre
  case de l'atlas de l'arbre. **Validé visuellement le 2026-11-01.**
  Test suivant : une techno recherchée **arbre déjà construit** (Sawmill)
  gardait l'icône de son check et son nom sans `[Researched]` : les nœuds
  ne sont reconstruits qu'à la sélection d'un onglet. → `IconesTechnoRafraichirNoeud`
  (appelée par `hkResearchComplete`) : nœuds du panneau (mémorisé par le
  hook du nœud) `+0x1A8` / compte `+0x1B0`, enregistrements de 0x80
  (`+0x00` clé = techno `+0x20`, `+0x10` nœud, `+0x18` texte `TECH_NAME`,
  `+0x20` image affichée) ; image reposée par `FUN_141107140(image, base +
  0x4074548, 0, 0)` + `FUN_141bb2d30(image, case, *(base + 0x4074550))`, nom
  par `FUN_14111bd70(texte, techno + 0x18, base + 0x2BFD628)`. Non testé.
  **Cases de l'atlas de l'arbre fausses** (signalé par l'auteur : « done »
  affiché en violet = progressive). Blob déployé `mods\ap_icons_recherche.gz`
  extrait (gzip → niveau 2048² DXT5 en fin de blob → DDS → texconv) : (0,2)
  important, (0,3) filler, **(0,4) done**, **(0,5) trap**, **(0,6)
  progressive**, (0,7) vanilla → `techCol/techRow` corrigés dans
  `ARCHIPELAGO_ICONS_TECH`. **Validé le 2026-11-01** (couleurs bonnes).
  Test suivant : recherche terminée, chargement d'une sauvegarde faite
  **pendant** cette recherche, recherche terminée à nouveau → **check
  renvoyé**. → `OnResearchComplete` n'envoie plus rien si la techno était
  déjà `researched` (sauvegarde locale cumulative) ; log `check deja envoye
  dans cette partie, pas de renvoi`. Non testé.
  **Nom du nœud = nom du check** (demande de l'auteur, 2026-11-01) :
  `AP_NomDuCheck(hash)` (tech_brain, TEMPORAIRE = nom interne de la techno,
  le même que dans `[AP] Sending: …`), + ` [Researched]` une fois recherchée,
  posé en texte brut (`FUN_14111bef0`) sur `TECH_NAME` du nœud (`sortie[1]`
  de `FUN_141bc5810`, et après une recherche par
  `IconesTechnoRafraichirNoeud`). Seul l'arbre change. Non testé.
  **Décision de l'auteur (2026-11-01)** : la sauvegarde locale des technos
  reste **cumulative** (une techno recherchée dans une sauvegarde plus
  récente de la même partie reste recherchée après chargement d'une plus
  ancienne : modèle Archipelago).
  (2) Seules 27 technos de test avaient une icône → **toutes
  les technos connues** (171) en ont une, type TEMPORAIRE
  `IconeCheckDeTest(nom, 0)` (comme lois et sites). (3) Retirés :
  `OpenKeyLogWindow(10000)` à chaque recherche terminée (debug) ; décalage
  d'un cran dans `DumpProgression`. Non testé.
  Restent à surveiller : `OnItemReceived` (F7) écrit l'état des technos
  depuis le thread du mod ; un item reçu pendant la recherche de la même
  techno la met à l'état 2 en cours de recherche (annulation refusée par
  `hkCanResearch`, complétion non vérifiée).

## Classification Archipelago des arbres

<!-- CLASSIFICATION ARBRES (generee par outils/technos_classification.py) -->

Table générée par `outils\technos_classification.py` depuis `technos_table.tsv`. **progression** = un autre arbre du scénario dépend de lui (parent d'une de ses technos), ou forcé (Tier, éclaireurs) ; **useful** = « important » côté mod. Parent de départ et condition d'affichage ne comptent pas.

### A New Home (`DEFAULT`) — 39 arbres : 14 progression, 25 useful

| Arbre | Item | Technos | Classification | Raison | Arbres enfants | Arbres parents |
|---:|---|---:|---|---|---|---|
| 0 | SteamHub | 3 | useful | aucun arbre ne depend de lui |  |  |
| 1 | Generator Upgrade - power 1 | 3 | **progression** | parent d'un autre arbre | 2 Generator Upgrade - less coal, 4 Generator Overdrive 1 |  |
| 2 | Generator Upgrade - less coal | 2 | useful | aucun arbre ne depend de lui |  | 1 Generator Upgrade - power 1 |
| 3 | Generator Upgrade - range 1 | 3 | useful | aucun arbre ne depend de lui |  |  |
| 4 | Generator Overdrive 1 | 2 | useful | aucun arbre ne depend de lui |  | 1 Generator Upgrade - power 1 |
| 5 | Boilers | 3 | **progression** | parent d'un autre arbre | 6 Boiler Upgrade - less coal |  |
| 6 | Boiler Upgrade - less coal | 1 | useful | aucun arbre ne depend de lui |  | 5 Boilers |
| 7 | Beacon | 1 | **progression** | force : eclaireurs | 8 ExpeditionCenter, 9 ScoutsLimit |  |
| 8 | ExpeditionCenter | 2 | useful | aucun arbre ne depend de lui |  | 7 Beacon |
| 9 | ScoutsLimit | 2 | **progression** | parent d'un autre arbre | 10 ScoutsSpeed | 7 Beacon |
| 10 | ScoutsSpeed | 2 | useful | aucun arbre ne depend de lui |  | 9 ScoutsLimit |
| 11 | Factory | 3 | **progression** | parent d'un autre arbre | 12 Cheaper Augmentation Parts, 13 Automaton Efficiency |  |
| 12 | Cheaper Augmentation Parts | 2 | useful | aucun arbre ne depend de lui |  | 11 Factory |
| 13 | Automaton Efficiency | 3 | **progression** | parent d'un autre arbre | 14 Automaton Medical, 79 Automaton Engineers | 11 Factory |
| 14 | Automaton Medical | 2 | useful | aucun arbre ne depend de lui |  | 13 Automaton Efficiency |
| 15 | Gathering Upgrade | 3 | **progression** | parent d'un autre arbre | 16 ResourceDepotUpgrade, 17 Gathering upgrade 2 |  |
| 16 | ResourceDepotUpgrade | 2 | useful | aucun arbre ne depend de lui |  | 15 Gathering Upgrade |
| 17 | Gathering upgrade 2 | 1 | useful | aucun arbre ne depend de lui |  | 15 Gathering Upgrade |
| 18 | CoalThumper | 3 | useful | aucun arbre ne depend de lui |  |  |
| 19 | Coal Mine | 3 | **progression** | parent d'un autre arbre | 20 Coal mine upgrade 1 |  |
| 20 | Coal mine upgrade 1 | 2 | useful | aucun arbre ne depend de lui |  | 19 Coal Mine |
| 21 | CharcoalKiln | 3 | useful | aucun arbre ne depend de lui |  |  |
| 22 | Sawmill | 2 | **progression** | parent d'un autre arbre | 23 Sawmill Upgrade |  |
| 23 | Sawmill Upgrade | 2 | useful | aucun arbre ne depend de lui |  | 22 Sawmill |
| 24 | WallDrill | 3 | useful | aucun arbre ne depend de lui |  |  |
| 25 | Steel Mine | 3 | useful | aucun arbre ne depend de lui |  |  |
| 26 | Plantation | 3 | **progression** | parent d'un autre arbre | 27 Plantation Insulation |  |
| 27 | Plantation Insulation | 2 | useful | aucun arbre ne depend de lui |  | 26 Plantation |
| 28 | HuntersUpgrade | 3 | **progression** | parent d'un autre arbre | 29 Hunters Upgrade - less workers |  |
| 29 | Hunters Upgrade - less workers | 1 | useful | aucun arbre ne depend de lui |  | 28 HuntersUpgrade |
| 30 | MedPost Upgrade | 3 | **progression** | parent d'un autre arbre | 31 HealthCare Buildings - warmer, 32 Infirmary Upgrade - less workers |  |
| 31 | HealthCare Buildings - warmer | 2 | useful | aucun arbre ne depend de lui |  | 30 MedPost Upgrade |
| 32 | Infirmary Upgrade - less workers | 1 | useful | aucun arbre ne depend de lui |  | 30 MedPost Upgrade |
| 33 | Bunkhouse | 3 | **progression** | parent d'un autre arbre | 34 Cantinee - warmer, 35 House Upgrade - cheaper |  |
| 34 | Cantinee - warmer | 1 | useful | aucun arbre ne depend de lui |  | 33 Bunkhouse |
| 35 | House Upgrade - cheaper | 1 | useful | aucun arbre ne depend de lui |  | 33 Bunkhouse |
| 36 | Tier | 5 | **progression** | force : paliers |  |  |
| 79 | Automaton Engineers | 1 | useful | aucun arbre ne depend de lui |  | 13 Automaton Efficiency |
| 80 | Storm_research | 1 | useful | aucun arbre ne depend de lui |  |  |

### The Arks (`TechHaven`) — 35 arbres : 13 progression, 22 useful

| Arbre | Item | Technos | Classification | Raison | Arbres enfants | Arbres parents |
|---:|---|---:|---|---|---|---|
| 0 | SteamHub | 3 | useful | aucun arbre ne depend de lui |  |  |
| 1 | Generator Upgrade - power 1 | 3 | **progression** | parent d'un autre arbre | 2 Generator Upgrade - less coal, 4 Generator Overdrive 1 |  |
| 2 | Generator Upgrade - less coal | 2 | useful | aucun arbre ne depend de lui |  | 1 Generator Upgrade - power 1 |
| 3 | Generator Upgrade - range 1 | 3 | useful | aucun arbre ne depend de lui |  |  |
| 4 | Generator Overdrive 1 | 2 | useful | aucun arbre ne depend de lui |  | 1 Generator Upgrade - power 1 |
| 5 | Boilers | 3 | **progression** | parent d'un autre arbre | 6 Boiler Upgrade - less coal |  |
| 6 | Boiler Upgrade - less coal | 1 | useful | aucun arbre ne depend de lui |  | 5 Boilers |
| 7 | Beacon | 2 | **progression** | force : eclaireurs | 9 ScoutsLimit |  |
| 9 | ScoutsLimit | 2 | **progression** | parent d'un autre arbre | 10 ScoutsSpeed | 7 Beacon |
| 10 | ScoutsSpeed | 2 | useful | aucun arbre ne depend de lui |  | 9 ScoutsLimit |
| 11 | Factory | 3 | **progression** | parent d'un autre arbre | 12 Cheaper Augmentation Parts, 13 Automaton Efficiency |  |
| 12 | Cheaper Augmentation Parts | 2 | useful | aucun arbre ne depend de lui |  | 11 Factory |
| 13 | Automaton Efficiency | 3 | **progression** | parent d'un autre arbre | 14 Automaton Medical, 79 Automaton Engineers | 11 Factory |
| 14 | Automaton Medical | 2 | useful | aucun arbre ne depend de lui |  | 13 Automaton Efficiency |
| 15 | Gathering Upgrade | 3 | **progression** | parent d'un autre arbre | 16 ResourceDepotUpgrade, 17 Gathering upgrade 2 |  |
| 16 | ResourceDepotUpgrade | 2 | useful | aucun arbre ne depend de lui |  | 15 Gathering Upgrade |
| 17 | Gathering upgrade 2 | 1 | useful | aucun arbre ne depend de lui |  | 15 Gathering Upgrade |
| 18 | CoalThumper | 3 | useful | aucun arbre ne depend de lui |  |  |
| 19 | Coal Mine | 3 | **progression** | parent d'un autre arbre | 20 Coal mine upgrade 1 |  |
| 20 | Coal mine upgrade 1 | 2 | useful | aucun arbre ne depend de lui |  | 19 Coal Mine |
| 21 | CharcoalKiln | 3 | useful | aucun arbre ne depend de lui |  |  |
| 22 | Sawmill | 2 | **progression** | parent d'un autre arbre | 23 Sawmill Upgrade |  |
| 23 | Sawmill Upgrade | 2 | useful | aucun arbre ne depend de lui |  | 22 Sawmill |
| 24 | WallDrill | 3 | useful | aucun arbre ne depend de lui |  |  |
| 25 | Steel Mine | 3 | useful | aucun arbre ne depend de lui |  |  |
| 26 | Plantation | 3 | **progression** | parent d'un autre arbre | 27 Plantation Insulation |  |
| 27 | Plantation Insulation | 2 | useful | aucun arbre ne depend de lui |  | 26 Plantation |
| 30 | MedPost Upgrade | 3 | **progression** | parent d'un autre arbre | 31 HealthCare Buildings - warmer, 32 Infirmary Upgrade - less workers |  |
| 31 | HealthCare Buildings - warmer | 2 | useful | aucun arbre ne depend de lui |  | 30 MedPost Upgrade |
| 32 | Infirmary Upgrade - less workers | 1 | useful | aucun arbre ne depend de lui |  | 30 MedPost Upgrade |
| 33 | Bunkhouse | 3 | **progression** | parent d'un autre arbre | 34 Cantinee - warmer, 35 House Upgrade - cheaper |  |
| 34 | Cantinee - warmer | 1 | useful | aucun arbre ne depend de lui |  | 33 Bunkhouse |
| 35 | House Upgrade - cheaper | 1 | useful | aucun arbre ne depend de lui |  | 33 Bunkhouse |
| 36 | Tier | 5 | **progression** | force : paliers |  |  |
| 79 | Automaton Engineers | 1 | useful | aucun arbre ne depend de lui |  | 13 Automaton Efficiency |

### Refugees (`REFUGEES`) — 38 arbres : 14 progression, 24 useful

| Arbre | Item | Technos | Classification | Raison | Arbres enfants | Arbres parents |
|---:|---|---:|---|---|---|---|
| 0 | SteamHub | 3 | useful | aucun arbre ne depend de lui |  |  |
| 1 | Generator Upgrade - power 1 | 3 | **progression** | parent d'un autre arbre | 2 Generator Upgrade - less coal, 4 Generator Overdrive 1 |  |
| 2 | Generator Upgrade - less coal | 2 | useful | aucun arbre ne depend de lui |  | 1 Generator Upgrade - power 1 |
| 3 | Generator Upgrade - range 1 | 3 | useful | aucun arbre ne depend de lui |  |  |
| 4 | Generator Overdrive 1 | 2 | useful | aucun arbre ne depend de lui |  | 1 Generator Upgrade - power 1 |
| 5 | Boilers | 3 | **progression** | parent d'un autre arbre | 6 Boiler Upgrade - less coal |  |
| 6 | Boiler Upgrade - less coal | 1 | useful | aucun arbre ne depend de lui |  | 5 Boilers |
| 7 | Beacon | 1 | **progression** | force : eclaireurs | 8 ExpeditionCenter, 9 ScoutsLimit |  |
| 8 | ExpeditionCenter | 2 | useful | aucun arbre ne depend de lui |  | 7 Beacon |
| 9 | ScoutsLimit | 2 | **progression** | parent d'un autre arbre | 10 ScoutsSpeed | 7 Beacon |
| 10 | ScoutsSpeed | 2 | useful | aucun arbre ne depend de lui |  | 9 ScoutsLimit |
| 11 | Factory | 3 | **progression** | parent d'un autre arbre | 12 Cheaper Augmentation Parts, 13 Automaton Efficiency |  |
| 12 | Cheaper Augmentation Parts | 2 | useful | aucun arbre ne depend de lui |  | 11 Factory |
| 13 | Automaton Efficiency | 3 | **progression** | parent d'un autre arbre | 14 Automaton Medical, 79 Automaton Engineers | 11 Factory |
| 14 | Automaton Medical | 2 | useful | aucun arbre ne depend de lui |  | 13 Automaton Efficiency |
| 15 | Gathering Upgrade | 3 | **progression** | parent d'un autre arbre | 16 ResourceDepotUpgrade, 17 Gathering upgrade 2 |  |
| 16 | ResourceDepotUpgrade | 2 | useful | aucun arbre ne depend de lui |  | 15 Gathering Upgrade |
| 17 | Gathering upgrade 2 | 1 | useful | aucun arbre ne depend de lui |  | 15 Gathering Upgrade |
| 18 | CoalThumper | 3 | useful | aucun arbre ne depend de lui |  |  |
| 19 | Coal Mine | 3 | **progression** | parent d'un autre arbre | 20 Coal mine upgrade 1 |  |
| 20 | Coal mine upgrade 1 | 2 | useful | aucun arbre ne depend de lui |  | 19 Coal Mine |
| 21 | CharcoalKiln | 3 | useful | aucun arbre ne depend de lui |  |  |
| 22 | Sawmill | 2 | **progression** | parent d'un autre arbre | 23 Sawmill Upgrade |  |
| 23 | Sawmill Upgrade | 2 | useful | aucun arbre ne depend de lui |  | 22 Sawmill |
| 24 | WallDrill | 3 | useful | aucun arbre ne depend de lui |  |  |
| 25 | Steel Mine | 3 | useful | aucun arbre ne depend de lui |  |  |
| 26 | Plantation | 3 | **progression** | parent d'un autre arbre | 27 Plantation Insulation |  |
| 27 | Plantation Insulation | 2 | useful | aucun arbre ne depend de lui |  | 26 Plantation |
| 28 | HuntersUpgrade | 3 | **progression** | parent d'un autre arbre | 29 Hunters Upgrade - less workers |  |
| 29 | Hunters Upgrade - less workers | 1 | useful | aucun arbre ne depend de lui |  | 28 HuntersUpgrade |
| 30 | MedPost Upgrade | 3 | **progression** | parent d'un autre arbre | 31 HealthCare Buildings - warmer, 32 Infirmary Upgrade - less workers |  |
| 31 | HealthCare Buildings - warmer | 2 | useful | aucun arbre ne depend de lui |  | 30 MedPost Upgrade |
| 32 | Infirmary Upgrade - less workers | 1 | useful | aucun arbre ne depend de lui |  | 30 MedPost Upgrade |
| 33 | Bunkhouse | 3 | **progression** | parent d'un autre arbre | 34 Cantinee - warmer, 35 House Upgrade - cheaper |  |
| 34 | Cantinee - warmer | 1 | useful | aucun arbre ne depend de lui |  | 33 Bunkhouse |
| 35 | House Upgrade - cheaper | 1 | useful | aucun arbre ne depend de lui |  | 33 Bunkhouse |
| 36 | Tier | 5 | **progression** | force : paliers |  |  |
| 79 | Automaton Engineers | 1 | useful | aucun arbre ne depend de lui |  | 13 Automaton Efficiency |

### Fall of Winterhome (`WINTERHOME`) — 40 arbres : 8 progression, 32 useful

| Arbre | Item | Technos | Classification | Raison | Arbres enfants | Arbres parents |
|---:|---|---:|---|---|---|---|
| 0 | Steamhub Upgrade - range | 2 | useful | aucun arbre ne depend de lui |  |  |
| 1 | Generator Upgrade - power 3 | 1 | useful | aucun arbre ne depend de lui |  |  |
| 2 | Generator Upgrade - less coal | 2 | useful | aucun arbre ne depend de lui |  |  |
| 3 | Generator Upgrade - range 3 | 1 | useful | aucun arbre ne depend de lui |  |  |
| 4 | Generator Overdrive 1 | 2 | useful | aucun arbre ne depend de lui |  |  |
| 5 | Boiler Upgrade | 2 | **progression** | parent d'un autre arbre | 6 Boiler Upgrade - less coal |  |
| 6 | Boiler Upgrade - less coal | 1 | useful | aucun arbre ne depend de lui |  | 5 Boiler Upgrade |
| 8 | ExpeditionUpgradeWinterhome | 1 | useful | aucun arbre ne depend de lui |  |  |
| 9 | ScoutsLimit | 2 | **progression** | parent d'un autre arbre | 10 ScoutsSpeed |  |
| 10 | ScoutsSpeed | 2 | useful | aucun arbre ne depend de lui |  | 9 ScoutsLimit |
| 11 | Factory | 3 | **progression** | parent d'un autre arbre | 12 Cheaper Augmentation Parts, 13 Automaton Efficiency |  |
| 12 | Cheaper Augmentation Parts | 2 | useful | aucun arbre ne depend de lui |  | 11 Factory |
| 13 | Automaton Efficiency | 3 | **progression** | parent d'un autre arbre | 14 Automaton Medical, 79 Automaton Engineers | 11 Factory |
| 14 | Automaton Medical | 2 | useful | aucun arbre ne depend de lui |  | 13 Automaton Efficiency |
| 15 | Gathering Upgrade | 3 | **progression** | parent d'un autre arbre | 16 ResourceDepotUpgrade, 17 Gathering upgrade 2 |  |
| 16 | ResourceDepotUpgrade | 2 | useful | aucun arbre ne depend de lui |  | 15 Gathering Upgrade |
| 17 | Gathering upgrade 2 | 1 | useful | aucun arbre ne depend de lui |  | 15 Gathering Upgrade |
| 18 | SteamCoalThumper | 2 | useful | aucun arbre ne depend de lui |  |  |
| 19 | Steam Coal Mine | 2 | useful | aucun arbre ne depend de lui |  |  |
| 20 | Coal mine upgrade 1 | 2 | useful | aucun arbre ne depend de lui |  |  |
| 21 | Kiln upgrade | 2 | useful | aucun arbre ne depend de lui |  |  |
| 22 | Steam Sawmill | 1 | useful | aucun arbre ne depend de lui |  |  |
| 23 | Sawmill Upgrade | 2 | useful | aucun arbre ne depend de lui |  |  |
| 24 | Steam WallDrill | 2 | useful | aucun arbre ne depend de lui |  |  |
| 25 | Steam Steel Mine | 2 | useful | aucun arbre ne depend de lui |  |  |
| 26 | Steam Plantation | 2 | useful | aucun arbre ne depend de lui |  |  |
| 27 | Plantation Insulation | 2 | useful | aucun arbre ne depend de lui |  |  |
| 28 | Flying Hunters | 2 | useful | aucun arbre ne depend de lui |  |  |
| 29 | Hunters Upgrade - less workers | 1 | useful | aucun arbre ne depend de lui |  |  |
| 30 | MedPost Upgrade | 3 | **progression** | parent d'un autre arbre | 31 HealthCare Buildings - warmer, 32 Infirmary Upgrade - less workers |  |
| 31 | HealthCare Buildings - warmer | 2 | useful | aucun arbre ne depend de lui |  | 30 MedPost Upgrade |
| 32 | Infirmary Upgrade - less workers | 1 | useful | aucun arbre ne depend de lui |  | 30 MedPost Upgrade |
| 33 | House Insulation | 1 | useful | aucun arbre ne depend de lui |  |  |
| 34 | Cantinee - warmer | 1 | useful | aucun arbre ne depend de lui |  |  |
| 35 | House Upgrade - cheaper | 1 | useful | aucun arbre ne depend de lui |  |  |
| 36 | Tier | 2 | **progression** | force : paliers |  |  |
| 37 | E RepairStation | 4 | **progression** | parent d'un autre arbre | 38 E RC Ability, 39 E RC Less workers |  |
| 38 | E RC Ability | 2 | useful | aucun arbre ne depend de lui |  | 37 E RepairStation |
| 39 | E RC Less workers | 1 | useful | aucun arbre ne depend de lui |  | 37 E RepairStation |
| 79 | Automaton Engineers | 1 | useful | aucun arbre ne depend de lui |  | 13 Automaton Efficiency |

### The Last Autumn (`BUILDERS`) — 32 arbres : 14 progression, 18 useful

| Arbre | Item | Technos | Classification | Raison | Arbres enfants | Arbres parents |
|---:|---|---:|---|---|---|---|
| 36 | Tier | 5 | **progression** | force : paliers |  |  |
| 40 | HC Brazier | 2 | **progression** | parent d'un autre arbre | 41 HC BrazierRange1, 42 HC BrazierCoalOpt1 |  |
| 41 | HC BrazierRange1 | 1 | useful | aucun arbre ne depend de lui |  | 40 HC Brazier |
| 42 | HC BrazierCoalOpt1 | 2 | useful | aucun arbre ne depend de lui |  | 40 HC Brazier |
| 43 | HH Heater | 3 | useful | aucun arbre ne depend de lui |  |  |
| 44 | HI HealthCare Insulation1 | 2 | useful | aucun arbre ne depend de lui |  |  |
| 45 | GS ScaffoldingManufacture | 3 | **progression** | parent d'un autre arbre | 46 GS ProtectiveStructure1 |  |
| 46 | GS ProtectiveStructure1 | 2 | useful | aucun arbre ne depend de lui |  | 45 GS ScaffoldingManufacture |
| 47 | GM SteelPlateForge | 2 | **progression** | parent d'un autre arbre | 48 GM ForgeCheaperMaterials |  |
| 48 | GM ForgeCheaperMaterials | 1 | useful | aucun arbre ne depend de lui |  | 47 GM SteelPlateForge |
| 49 | GG SteamMill | 3 | **progression** | parent d'un autre arbre | 50 GG Supercharge1 |  |
| 50 | GG Supercharge1 | 2 | useful | aucun arbre ne depend de lui |  | 49 GG SteamMill |
| 51 | GF Air Scrubbers1 | 3 | **progression** | parent d'un autre arbre | 52 GF Air ScrubbersOptimalization |  |
| 52 | GF Air ScrubbersOptimalization | 2 | useful | aucun arbre ne depend de lui |  | 51 GF Air Scrubbers1 |
| 53 | GU GeneratorUpgrade1 | 3 | useful | aucun arbre ne depend de lui |  |  |
| 54 | FF Foragers | 1 | **progression** | force : eclaireurs | 55 FF Foragers_+1, 56 FF ForagersSpeedUpgrade |  |
| 55 | FF Foragers_+1 | 2 | useful | aucun arbre ne depend de lui |  | 54 FF Foragers |
| 56 | FF ForagersSpeedUpgrade | 2 | useful | aucun arbre ne depend de lui |  | 54 FF Foragers |
| 57 | FH Fishing Harbor | 2 | **progression** | parent d'un autre arbre | 58 FH Fishing Harbor_efficiencyBoost |  |
| 58 | FH Fishing Harbor_efficiencyBoost | 1 | useful | aucun arbre ne depend de lui |  | 57 FH Fishing Harbor |
| 59 | FM MedPost Upgrade | 3 | **progression** | parent d'un autre arbre | 60 FM Infirmary Upgrade |  |
| 60 | FM Infirmary Upgrade | 1 | useful | aucun arbre ne depend de lui |  | 59 FM MedPost Upgrade |
| 61 | RD Docksv1 | 3 | **progression** | parent d'un autre arbre | 62 RU URS, 64 RD Docks faster reload |  |
| 62 | RU URS | 1 | **progression** | parent d'un autre arbre | 63 RU URS_Steam | 61 RD Docksv1 |
| 63 | RU URS_Steam | 2 | **progression** | parent d'un autre arbre | 81 RU URS_Advanced | 62 RU URS |
| 64 | RD Docks faster reload | 1 | useful | aucun arbre ne depend de lui |  | 61 RD Docksv1 |
| 65 | RG Gathering Upgrade | 2 | **progression** | parent d'un autre arbre | 66 RG ResourceDepotUpgrade |  |
| 66 | RG ResourceDepotUpgrade | 2 | useful | aucun arbre ne depend de lui |  | 65 RG Gathering Upgrade |
| 67 | RW Sawmill | 2 | **progression** | parent d'un autre arbre | 68 RW Sawmill Upgrade |  |
| 68 | RW Sawmill Upgrade | 2 | useful | aucun arbre ne depend de lui |  | 67 RW Sawmill |
| 69 | RC CharcoalKiln | 3 | useful | aucun arbre ne depend de lui |  |  |
| 81 | RU URS_Advanced | 1 | useful | aucun arbre ne depend de lui |  | 63 RU URS_Steam |

### On the Edge (`Reconquest`) — 30 arbres : 11 progression, 19 useful

| Arbre | Item | Technos | Classification | Raison | Arbres enfants | Arbres parents |
|---:|---|---:|---|---|---|---|
| 5 | Boilers | 3 | **progression** | parent d'un autre arbre | 6 Boiler Upgrade - less coal |  |
| 6 | Boiler Upgrade - less coal | 1 | useful | aucun arbre ne depend de lui |  | 5 Boilers |
| 9 | ScoutsLimit | 2 | **progression** | parent d'un autre arbre | 10 ScoutsSpeed |  |
| 10 | ScoutsSpeed | 2 | useful | aucun arbre ne depend de lui |  | 9 ScoutsLimit |
| 11 | Factory | 1 | **progression** | parent d'un autre arbre | 12 Cheaper Augmentation Parts |  |
| 12 | Cheaper Augmentation Parts | 2 | useful | aucun arbre ne depend de lui |  | 11 Factory |
| 15 | Gathering Upgrade | 3 | **progression** | parent d'un autre arbre | 16 ResourceDepotUpgrade, 17 Gathering upgrade 2 |  |
| 16 | ResourceDepotUpgrade | 2 | useful | aucun arbre ne depend de lui |  | 15 Gathering Upgrade |
| 17 | Gathering upgrade 2 | 1 | useful | aucun arbre ne depend de lui |  | 15 Gathering Upgrade |
| 18 | CoalThumper | 3 | useful | aucun arbre ne depend de lui |  |  |
| 19 | Coal Mine | 3 | **progression** | parent d'un autre arbre | 20 Coal mine upgrade 1 |  |
| 20 | Coal mine upgrade 1 | 2 | useful | aucun arbre ne depend de lui |  | 19 Coal Mine |
| 21 | CharcoalKiln | 3 | useful | aucun arbre ne depend de lui |  |  |
| 22 | Sawmill | 2 | **progression** | parent d'un autre arbre | 23 Sawmill Upgrade |  |
| 23 | Sawmill Upgrade | 2 | useful | aucun arbre ne depend de lui |  | 22 Sawmill |
| 30 | MedPost Upgrade | 3 | **progression** | parent d'un autre arbre | 31 HealthCare Buildings - warmer, 32 Infirmary Upgrade - less workers |  |
| 31 | HealthCare Buildings - warmer | 2 | useful | aucun arbre ne depend de lui |  | 30 MedPost Upgrade |
| 32 | Infirmary Upgrade - less workers | 1 | useful | aucun arbre ne depend de lui |  | 30 MedPost Upgrade |
| 33 | Bunkhouse | 3 | **progression** | parent d'un autre arbre | 34 Cantinee - warmer, 35 House Upgrade - cheaper |  |
| 34 | Cantinee - warmer | 1 | useful | aucun arbre ne depend de lui |  | 33 Bunkhouse |
| 35 | House Upgrade - cheaper | 1 | useful | aucun arbre ne depend de lui |  | 33 Bunkhouse |
| 36 | Tier | 5 | **progression** | force : paliers |  |  |
| 70 | HC Brazier | 2 | **progression** | parent d'un autre arbre | 71 HC BrazierRange, 72 HC BrazierCoalOpt1 |  |
| 71 | HC BrazierRange | 1 | useful | aucun arbre ne depend de lui |  | 70 HC Brazier |
| 72 | HC BrazierCoalOpt1 | 2 | useful | aucun arbre ne depend de lui |  | 70 HC Brazier |
| 73 | R VaultEfficiencyBoost | 1 | useful | aucun arbre ne depend de lui |  |  |
| 74 | R VaultUpgrade1 | 3 | **progression** | parent d'un autre arbre | 75 R VaultMultitasking, 76 R VaultInsulation, 77 R VaultSteamCoresUpgrade |  |
| 75 | R VaultMultitasking | 1 | useful | aucun arbre ne depend de lui |  | 74 R VaultUpgrade1 |
| 76 | R VaultInsulation | 1 | useful | aucun arbre ne depend de lui |  | 74 R VaultUpgrade1 |
| 77 | R VaultSteamCoresUpgrade | 1 | useful | aucun arbre ne depend de lui |  | 74 R VaultUpgrade1 |

<!-- FIN CLASSIFICATION ARBRES -->
