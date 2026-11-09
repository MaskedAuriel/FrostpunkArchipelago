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

## Paliers, arbres et prérequis (2026-11-03)

Besoin de l'apworld (règle Q4.1) : pour chaque techno des 6 scénarios AP,
son **palier**, son **arbre progressif**, son **rang** et ses **prérequis**.

**Décompilés du 2026-11-03** (aucun champ « palier » dans le gabarit : il
vient des prérequis ou de la grille, le relevé tranchera) :
- `FrostpunkTechnologiesConfig`, instance statique `base + 0x40744C0`
  (renvoyée par `FUN_14145b680`, recherche par GUID = vfunc `+0x88`) :
  `+0x78` onglets, `+0xB0` technos, `+0x100` ensembles (tableaux de
  pointeurs, compte `+8`). Même disposition que la config des lois.
- `FrostpunkTechnologyEntry` (0x128, base `FrostpunkProgressTreeEntry`) :
  `+0x18` nom, `+0x20` GUID (le hash du mod = ses 8 premiers octets),
  `+0x30` Enabled, `+0x38` Requirements, `+0x50` Show requirements,
  `+0x68` Display name, `+0x80` Icon tile, `+0x88` Grid position,
  `+0x90` Category (lien GUID vers l'onglet), `+0xA0` coût, `+0xB8` durée,
  `+0xC0` modificateurs globaux, `+0x10C` Display category (enum),
  `+0x110` Tree entry type (NameString).
- Prérequis (`FUN_1416a71f0`, type par vfunc `+0xE0`) :
  `FrostpunkTechnologyRequirement` (0x40) : `+0x30` GUID de la techno
  exigée, satisfait si elle est à l'état 2 ; `FrostpunkCompositeRequirement` :
  `+0x30` opérateur (0 ET, 1 OU, 2 NON), `+0x38` sous-prérequis.
- `FrostpunkTechnologyCategoryEntry` (0x38) : `+0x00` nom, `+0x08` GUID,
  `+0x18` Display name. `FrostpunkTechnologySetEntry` (0x30) : `+0x00` nom,
  `+0x08` GUID, `+0x18` Technologies (liens de 0x10).
- `FrostpunkScenariosConfig +0x1D8` : `FrostpunkScenarioTechnologySetupEntry`
  (0x50) : `+0x00` nom, `+0x08` GUID, `+0x18` Starting technologies,
  `+0x30` Technology set. `FrostpunkScenarioEntry +0x138` : Technology
  setups (liens).

**Relevé `[TECHNO]`** (greffé sur F4, choix de l'auteur, après le relevé des
lois, thread du jeu) : toute la config d'un coup, donc **un seul appui F4
dans n'importe quelle partie** couvre les 6 scénarios. Lignes à format fixe :
`CAT | nom | guid`, `T | hash | nom | onglet | col | ligne | type | catAff | active`,
`REQ | hash | prérequis`, `VOIR | hash | prérequis d'affichage`,
`SET | ensemble | n`, `SETT | ensemble | hash`,
`REG | réglage | ensemble | technos de départ`, `SCN | scénario | réglages`.
`DecrireExigence` (lois.cpp) décrit désormais aussi `techno <nom>`.
**1er test** : `config des technos introuvable a base + 0x40744C0 (classe
?$EntryCollection)` → cette adresse est la collection des technos, membre
de la config en `+0x90` (constructeur `FUN_14145a4c0`) : config en
**`base + 0x4074430`**, avec repli par balayage RTTI.
**2e test : relevé complet** (176 technos, 9 onglets, 8 ensembles). Constats :
- palier = prérequis `0N_TierUnlockN`, sans exception (ligne de la grille =
  2 × palier pour les technos normales) ; les `TierUnlock1..5` sont des
  `GOAL_ENTRY` en chaîne (N exige N−1) ;
- chaque techno active a **au plus un** parent techno : forêt d'arbres
  ramifiés ; le mod découpait les branches en arbres séparés, d'où un parent
  hors de l'arbre pour une trentaine de rangs 0 → le parent est donné
  explicitement ;
- erreurs de l'ancien `known_techs.h` : Steam avant Adv (Steel Mine, Coal
  Mine, WallDrill), Cheaper avant Faster Augmentation Parts, URS_Advanced
  (branche de URS_Steam → arbre 81), Automaton Engineers en arbre 15 dans
  Arks/Winterhome (= Gathering) → 79 partout, Hunters dans The Arks et
  WallDrill dans On the Edge (absents de leurs ensembles), Storm_research
  absente (→ arbre 80) ;
- désactivées : `[Winterhome] RC Malfunction predictions`, `RC
  Ability-GeneratorStress` (×2), `[SeasonPass] Bridge` ;
- ensemble ↔ scénario : DEFAULT → Default, TechHaven → TechHaven, REFUGEES →
  Refugees, WINTERHOME → Winterhome, BUILDERS → Builders, Reconquest →
  Reconquest ; technos de départ : Winterhome (21, dont TierUnlock 1-3),
  On the Edge (Beacon, Transport Depot).

**Décisions de l'auteur (2026-11-03)** : numéros d'arbres gardés, ordres
corrigés, parent du jeu ajouté ; technos cachées derrière un déblocage de
l'histoire = checks normaux, condition notée ; technos de départ exclues
(ni check ni item).

**Génération** : `outils\technos_table.py` (relevé F4 + `outils\technos_arbres.tsv`,
source de vérité hash → arbre, à éditer à la main) écrit `known_techs.h`
(un groupe par scénario AP, `TechInfo` + `tier` + `parent`),
`outils\technos_table.tsv` (pour l'apworld) et la table ci-dessous.
Mod adapté : `SCENARIO_TECHS` (scenario.cpp) = un groupe par scénario,
`TreeSize` filtré par scénario, `MAX_TREES` 79 → 96 (l'arbre 79 était hors
bornes), `TechEstUnCheck(hash)` (tech_brain) : une techno hors du groupe du
scénario n'est ni lançable à l'état 2, ni un check, ni iconée AP
(`hkCanResearch`, `OnResearchComplete`, IconSubManager). Non testé.

**Test de l'auteur (2026-11-03)** : **VALIDÉ**. Fall of Winterhome : les
technos de départ ne sont ni lançables ni des checks, icônes vanilla pour
elles et AP pour les autres. F7 : ordres corrigés (Steam avant Adv) et
tailles d'arbres bonnes (`[AP] arbre 24 position 2 (3/3) -> Adv Wall Drill`,
`arbre 4 position 1 (2/2)`). F4 après les changements : relevé complet
(`[TECHNO] releve termine`), table régénérée identique. Table transmise à
l'apworld.

## Table par scénario

<!-- TABLE TECHNOS (generee par outils/technos_table.py) -->

Table générée par `outils\technos_table.py` (relevé F4). Palier = TierUnlock exigé ; parent = techno exigée par le jeu (*italique* = techno de départ, déjà acquise) ; rang = ordre de l'item progressif dans l'arbre, dans ce scénario.

### A New Home (`DEFAULT`) — 88 technos

Technos de départ (exclues) : aucune.

| Arbre | Rang | Techno | Palier | Parent | Onglet | Condition d'affichage | Hash |
|---:|---:|---|---:|---|---|---|---|
| 0 | 0 | H 1_SteamHub | 0 |  | Heating |  | `A0E0D2727D0B336A` |
| 0 | 1 | H 3_Steamhub Upgrade - range | 2 | H 1_SteamHub | Heating |  | `8ACF1A3F6CD1A7FB` |
| 0 | 2 | H 5_Steamhub Upgrade - less coal | 4 | H 3_Steamhub Upgrade - range | Heating |  | `8169FDE4BB9BB141` |
| 1 | 0 | H 2_Generator Upgrade - power 1 | 1 |  | Heating |  | `A55C1E23258CB056` |
| 1 | 1 | H 4_Generator Upgrade - power 2 | 3 | H 2_Generator Upgrade - power 1 | Heating |  | `BDF0CF1D41987C65` |
| 1 | 2 | H 6_Generator Upgrade - power 3 | 5 | H 4_Generator Upgrade - power 2 | Heating |  | `B8DD400DADCDEF8E` |
| 2 | 0 | H 3_Generator Upgrade - less coal | 2 | H 2_Generator Upgrade - power 1 | Heating |  | `AAE37A9B8A8B25F1` |
| 2 | 1 | H 5 Generator Upgrade - less coal 2 | 4 | H 3_Generator Upgrade - less coal | Heating |  | `B6C37AB9E60FF207` |
| 3 | 0 | H 2_Generator Upgrade - range 1 | 1 |  | Heating |  | `A156A81B7E067BF5` |
| 3 | 1 | H 4_Generator Upgrade - range 2 | 3 | H 2_Generator Upgrade - range 1 | Heating |  | `94BF1A6EF2603AE0` |
| 3 | 2 | H 6_Generator Upgrade - range 2 | 5 | H 4_Generator Upgrade - range 2 | Heating |  | `B800B652FE01D119` |
| 4 | 0 | H 3_Generator Overdrive 1 | 2 | H 2_Generator Upgrade - power 1 | Heating |  | `8F0A051638529900` |
| 4 | 1 | H 6_Generator Upgrade - less stress | 5 | H 3_Generator Overdrive 1 | Heating |  | `91D2CE4C68CE4109` |
| 5 | 0 | H 1_Boilers | 0 |  | Heating |  | `85BCE42D5F9E4F3B` |
| 5 | 1 | H 3_Boiler Upgrade | 2 | H 1_Boilers | Heating |  | `B2366AD179BD4D26` |
| 5 | 2 | H 5_Boiler Upgrade 2 | 4 | H 3_Boiler Upgrade | Heating |  | `BA712FF32CF9CAAE` |
| 6 | 0 | H 4_Boiler Upgrade - less coal | 3 | H 3_Boiler Upgrade | Heating |  | `8095FB143BBF82CC` |
| 7 | 0 | E 1_Beacon | 0 |  | Exploration_Industry |  | `BD91C0E32EE8A6B2` |
| 8 | 0 | E 3_ExpeditionCenter | 2 | E 1_Beacon | Exploration_Industry |  | `A16CF8DC50B2727A` |
| 8 | 1 | E 5_ExpeditionUpgrade | 4 | E 3_ExpeditionCenter | Exploration_Industry |  | `B7D0B9057487D146` |
| 9 | 0 | E 2_ScoutsLimit | 1 | E 1_Beacon | Exploration_Industry |  | `BFB9EC0F1190EBC8` |
| 9 | 1 | E 5_ScoutsLimit2 | 4 | E 2_ScoutsLimit | Exploration_Industry |  | `903BF385083D378F` |
| 10 | 0 | E 3_ScoutsSpeed | 2 | E 2_ScoutsLimit | Exploration_Industry |  | `980DA1A7BA055FAD` |
| 10 | 1 | E 6_ScoutsSpeed 2 | 5 | E 3_ScoutsSpeed | Exploration_Industry |  | `AFDDF739B5C4D799` |
| 11 | 0 | E 3_Factory | 2 |  | Exploration_Industry |  | `A3E95C4CEB6C2B7F` |
| 11 | 1 | E 4_Cheaper Automatons | 3 | E 3_Factory | Exploration_Industry |  | `AD20BB9845A5DDC5` |
| 11 | 2 | E 6_Automatons no Steamcores | 5 | E 4_Cheaper Automatons | Exploration_Industry |  | `973B35A4E76361EE` |
| 12 | 0 | E 4_Cheaper Augmentation Parts | 3 | E 3_Factory | Exploration_Industry |  | `8AFE2FDE0DF97D69` |
| 12 | 1 | E 5_Faster Augmentation Parts production | 4 | E 4_Cheaper Augmentation Parts | Exploration_Industry |  | `981F59F3146CF4C6` |
| 13 | 0 | E 4_Automaton Efficiency | 3 | E 3_Factory | Exploration_Industry |  | `9E16C1962C1325C6` |
| 13 | 1 | E 5_Automaton Efficiency 2 | 4 | E 4_Automaton Efficiency | Exploration_Industry |  | `A181975459F6C673` |
| 13 | 2 | E 6_Automatons Efficiency 3 | 5 | E 5_Automaton Efficiency 2 | Exploration_Industry |  | `B0131D084E15541A` |
| 14 | 0 | E 5_Automaton Medical | 4 | E 4_Automaton Efficiency | Exploration_Industry |  | `B12F86943A6036E4` |
| 14 | 1 | E 6_Automatons Medical 2 | 5 | E 5_Automaton Medical | Exploration_Industry |  | `85C386E583A32865` |
| 15 | 0 | R 1_Gathering Upgrade | 0 |  | Resources |  | `8BC47F368138DA92` |
| 15 | 1 | R 3_Gathering Insulation | 2 | R 1_Gathering Upgrade | Resources |  | `B69B51DB31F4B001` |
| 15 | 2 | R 5_Gathering Insulation 2 | 4 | R 3_Gathering Insulation | Resources |  | `851032DABCC703AE` |
| 16 | 0 | R 2_ResourceDepotUpgrade | 1 | R 1_Gathering Upgrade | Resources |  | `B300360E7D7A3295` |
| 16 | 1 | R 3_Resource Depot Big | 2 | R 2_ResourceDepotUpgrade | Resources |  | `96031EA6A430BF85` |
| 17 | 0 | R 4_Gathering upgrade 2 | 3 | R 3_Gathering Insulation | Resources |  | `BF4935A464F03A66` |
| 18 | 0 | R 1_CoalThumper | 0 |  | Resources |  | `86A71762D9705649` |
| 18 | 1 | R 3_SteamCoalThumper | 2 | R 1_CoalThumper | Resources |  | `B5B909AE4C1E54F6` |
| 18 | 2 | R 6_Coal Thumper Upgrade | 5 | R 3_SteamCoalThumper | Resources |  | `86CCBA956FEBC7DE` |
| 19 | 0 | R 2_Coal Mine | 1 |  | Resources |  | `A3796F28B9AC2A23` |
| 19 | 1 | R 4_Steam Coal Mine | 3 | R 2_Coal Mine | Resources |  | `AB63782F194E8021` |
| 19 | 2 | R 6_Adv Coal Mine | 5 | R 4_Steam Coal Mine | Resources |  | `B82524B0D19D88D5` |
| 20 | 0 | R 3_Coal mine upgrade 1 | 2 | R 2_Coal Mine | Resources |  | `A5A0DE87E8514928` |
| 20 | 1 | R 5_Coal Mine upgrade 2 | 4 | R 3_Coal mine upgrade 1 | Resources |  | `960C27264EA17CF9` |
| 21 | 0 | R 2_CharcoalKiln | 1 |  | Resources |  | `BEC33A80C6853BA3` |
| 21 | 1 | R 3_Kiln upgrade | 2 | R 2_CharcoalKiln | Resources |  | `BBD8663CFA3F3D7D` |
| 21 | 2 | R 4_Kiln upgrade 2 | 3 | R 3_Kiln upgrade | Resources |  | `87221793ACD9ED8A` |
| 22 | 0 | R 1_Sawmill | 0 |  | Resources |  | `A9F39DF2AEEB21B1` |
| 22 | 1 | R 3_Steam Sawmill | 2 | R 1_Sawmill | Resources |  | `AC019A23402AB883` |
| 23 | 0 | R 2_Sawmill Upgrade | 1 | R 1_Sawmill | Resources |  | `9D7F762601622CA6` |
| 23 | 1 | R 5_Sawmill Upgrade - range | 4 | R 2_Sawmill Upgrade | Resources |  | `BC770E739218DAB4` |
| 24 | 0 | R 2_WallDrill | 1 |  | Resources |  | `B3C7DF784DC97A83` |
| 24 | 1 | R 4_Steam WallDrill | 3 | R 2_WallDrill | Resources |  | `88698E693179DD2D` |
| 24 | 2 | R 6_Adv Wall Drill | 5 | R 4_Steam WallDrill | Resources |  | `9DA3ABAE1FA72621` |
| 25 | 0 | R 1_Steel Mine | 0 |  | Resources |  | `87856A059CE65971` |
| 25 | 1 | R 3_Steam Steel Mine | 2 | R 1_Steel Mine | Resources |  | `A9AC3601AE5EA23C` |
| 25 | 2 | R 5_Adv Steel Mine | 4 | R 3_Steam Steel Mine | Resources |  | `884E0D684DAAB02D` |
| 26 | 0 | F 2_Plantation | 1 |  | Food_Health_Shelter |  | `BF7153CF70731641` |
| 26 | 1 | F 4_Steam Plantation | 3 | F 2_Plantation | Food_Health_Shelter |  | `B49A95770CB83298` |
| 26 | 2 | F 6_Plantation upgrade | 5 | F 4_Steam Plantation | Food_Health_Shelter |  | `A92780AFF69180BF` |
| 27 | 0 | F 3_Plantation Insulation | 2 | F 2_Plantation | Food_Health_Shelter |  | `95DCED5FD8466387` |
| 27 | 1 | F 5_Plantation Insulation | 4 | F 3_Plantation Insulation | Food_Health_Shelter |  | `94FCD02E48F30228` |
| 28 | 0 | F 1_HuntersUpgrade | 0 |  | Food_Health_Shelter |  | `BD90D178518E872C` |
| 28 | 1 | F 3_Flying Hunters | 2 | F 1_HuntersUpgrade | Food_Health_Shelter |  | `90F460E6380DFD7A` |
| 28 | 2 | F 5_FlyingHuntersUpgrade | 4 | F 3_Flying Hunters | Food_Health_Shelter |  | `93B19C3058E896DC` |
| 29 | 0 | F 3_Hunters Upgrade - less workers | 2 | F 1_HuntersUpgrade | Food_Health_Shelter |  | `9781BAE37ABD9B94` |
| 30 | 0 | F 2_MedPost Upgrade | 1 |  | Food_Health_Shelter |  | `AEBE6A8DC9857616` |
| 30 | 1 | F 3_Steam healthcare | 2 | F 2_MedPost Upgrade | Food_Health_Shelter |  | `AE0D5029C4FAFA6A` |
| 30 | 2 | F 5_Infirmary Upgrade | 4 | F 3_Steam healthcare | Food_Health_Shelter |  | `A8E5A152E5F619C4` |
| 31 | 0 | F 3_HealthCare Buildings - warmer | 2 | F 2_MedPost Upgrade | Food_Health_Shelter |  | `9EB2A329D39949C0` |
| 31 | 1 | F 6_Healthcare Buildings - warmer | 5 | F 3_HealthCare Buildings - warmer | Food_Health_Shelter |  | `881F9801EF69831F` |
| 32 | 0 | F 4_Infirmary Upgrade - less workers | 3 | F 3_Steam healthcare | Food_Health_Shelter |  | `B5C1BFCACE2F518A` |
| 33 | 0 | F 2_Bunkhouse | 1 |  | Food_Health_Shelter |  | `9479CC7A027A3F61` |
| 33 | 1 | F 4_House | 3 | F 2_Bunkhouse | Food_Health_Shelter |  | `8226B4149A4C642A` |
| 33 | 2 | F 6_House Insulation | 5 | F 4_House | Food_Health_Shelter |  | `A751C3903745ABD5` |
| 34 | 0 | F 3_Cantinee - warmer | 2 | F 2_Bunkhouse | Food_Health_Shelter |  | `9E93A93F1C0C2C51` |
| 35 | 0 | F 5_House Upgrade - cheaper | 4 | F 4_House | Food_Health_Shelter |  | `BFB89F7354D04AE2` |
| 36 | 0 | 01_TierUnlock 1 | 0 |  | - |  | `BE5B5F3661B74AAE` |
| 36 | 1 | 02_TierUnlock2 | 0 | 01_TierUnlock 1 | - |  | `99F67FD0200252EF` |
| 36 | 2 | 03_TierUnlock3 | 0 | 02_TierUnlock2 | - |  | `9D18A6629DC54E67` |
| 36 | 3 | 04_TierUnlock4 | 0 | 03_TierUnlock3 | - |  | `9F5C9E5513755596` |
| 36 | 4 | 05_TierUnlock5 | 0 | 04_TierUnlock4 | - |  | `8D5D8F90ED24296E` |
| 79 | 0 | E 5_Automaton Engineers | 4 | E 4_Automaton Efficiency | Exploration_Industry |  | `A01F9F7DBAB21917` |
| 80 | 0 | E X_Storm_research | 0 |  | Exploration_Industry | unlock StormResearch | `A1C4DF5688B79B2A` |

### The Arks (`TechHaven`) — 82 technos

Technos de départ (exclues) : aucune.

| Arbre | Rang | Techno | Palier | Parent | Onglet | Condition d'affichage | Hash |
|---:|---:|---|---:|---|---|---|---|
| 0 | 0 | H 1_SteamHub | 0 |  | Heating |  | `A0E0D2727D0B336A` |
| 0 | 1 | H 3_Steamhub Upgrade - range | 2 | H 1_SteamHub | Heating |  | `8ACF1A3F6CD1A7FB` |
| 0 | 2 | H 5_Steamhub Upgrade - less coal | 4 | H 3_Steamhub Upgrade - range | Heating |  | `8169FDE4BB9BB141` |
| 1 | 0 | H 2_Generator Upgrade - power 1 | 1 |  | Heating |  | `A55C1E23258CB056` |
| 1 | 1 | H 4_Generator Upgrade - power 2 | 3 | H 2_Generator Upgrade - power 1 | Heating |  | `BDF0CF1D41987C65` |
| 1 | 2 | H 6_Generator Upgrade - power 3 | 5 | H 4_Generator Upgrade - power 2 | Heating |  | `B8DD400DADCDEF8E` |
| 2 | 0 | H 3_Generator Upgrade - less coal | 2 | H 2_Generator Upgrade - power 1 | Heating |  | `AAE37A9B8A8B25F1` |
| 2 | 1 | H 5 Generator Upgrade - less coal 2 | 4 | H 3_Generator Upgrade - less coal | Heating |  | `B6C37AB9E60FF207` |
| 3 | 0 | H 2_Generator Upgrade - range 1 | 1 |  | Heating |  | `A156A81B7E067BF5` |
| 3 | 1 | H 4_Generator Upgrade - range 2 | 3 | H 2_Generator Upgrade - range 1 | Heating |  | `94BF1A6EF2603AE0` |
| 3 | 2 | H 6_Generator Upgrade - range 2 | 5 | H 4_Generator Upgrade - range 2 | Heating |  | `B800B652FE01D119` |
| 4 | 0 | H 3_Generator Overdrive 1 | 2 | H 2_Generator Upgrade - power 1 | Heating |  | `8F0A051638529900` |
| 4 | 1 | H 6_Generator Upgrade - less stress | 5 | H 3_Generator Overdrive 1 | Heating |  | `91D2CE4C68CE4109` |
| 5 | 0 | H 1_Boilers | 0 |  | Heating |  | `85BCE42D5F9E4F3B` |
| 5 | 1 | H 3_Boiler Upgrade | 2 | H 1_Boilers | Heating |  | `B2366AD179BD4D26` |
| 5 | 2 | H 5_Boiler Upgrade 2 | 4 | H 3_Boiler Upgrade | Heating |  | `BA712FF32CF9CAAE` |
| 6 | 0 | H 4_Boiler Upgrade - less coal | 3 | H 3_Boiler Upgrade | Heating |  | `8095FB143BBF82CC` |
| 7 | 0 | E 1_Beacon | 0 |  | Exploration_Industry |  | `BD91C0E32EE8A6B2` |
| 7 | 1 | E 2_AutomatonScouts_TechHeaven | 1 | E 1_Beacon | Exploration_Industry | unlock [TechHeaven] Quest3 | `9843404FB6931C76` |
| 9 | 0 | E 2_ScoutsLimit | 1 | E 1_Beacon | Exploration_Industry |  | `BFB9EC0F1190EBC8` |
| 9 | 1 | E 5_ScoutsLimit2 | 4 | E 2_ScoutsLimit | Exploration_Industry |  | `903BF385083D378F` |
| 10 | 0 | E 3_ScoutsSpeed | 2 | E 2_ScoutsLimit | Exploration_Industry |  | `980DA1A7BA055FAD` |
| 10 | 1 | E 6_ScoutsSpeed 2 | 5 | E 3_ScoutsSpeed | Exploration_Industry |  | `AFDDF739B5C4D799` |
| 11 | 0 | E 3_Factory | 2 |  | Exploration_Industry |  | `A3E95C4CEB6C2B7F` |
| 11 | 1 | E 4_Cheaper Automatons | 3 | E 3_Factory | Exploration_Industry |  | `AD20BB9845A5DDC5` |
| 11 | 2 | E 6_Automatons no Steamcores | 5 | E 4_Cheaper Automatons | Exploration_Industry |  | `973B35A4E76361EE` |
| 12 | 0 | E 4_Cheaper Augmentation Parts | 3 | E 3_Factory | Exploration_Industry |  | `8AFE2FDE0DF97D69` |
| 12 | 1 | E 5_Faster Augmentation Parts production | 4 | E 4_Cheaper Augmentation Parts | Exploration_Industry |  | `981F59F3146CF4C6` |
| 13 | 0 | E 4_Automaton Efficiency | 3 | E 3_Factory | Exploration_Industry |  | `9E16C1962C1325C6` |
| 13 | 1 | E 5_Automaton Efficiency 2 | 4 | E 4_Automaton Efficiency | Exploration_Industry |  | `A181975459F6C673` |
| 13 | 2 | E 6_Automatons Efficiency 3 | 5 | E 5_Automaton Efficiency 2 | Exploration_Industry |  | `B0131D084E15541A` |
| 14 | 0 | E 5_Automaton Medical | 4 | E 4_Automaton Efficiency | Exploration_Industry |  | `B12F86943A6036E4` |
| 14 | 1 | E 6_Automatons Medical 2 | 5 | E 5_Automaton Medical | Exploration_Industry |  | `85C386E583A32865` |
| 15 | 0 | R 1_Gathering Upgrade | 0 |  | Resources |  | `8BC47F368138DA92` |
| 15 | 1 | R 3_Gathering Insulation | 2 | R 1_Gathering Upgrade | Resources |  | `B69B51DB31F4B001` |
| 15 | 2 | R 5_Gathering Insulation 2 | 4 | R 3_Gathering Insulation | Resources |  | `851032DABCC703AE` |
| 16 | 0 | R 2_ResourceDepotUpgrade | 1 | R 1_Gathering Upgrade | Resources |  | `B300360E7D7A3295` |
| 16 | 1 | R 3_Resource Depot Big | 2 | R 2_ResourceDepotUpgrade | Resources |  | `96031EA6A430BF85` |
| 17 | 0 | R 4_Gathering upgrade 2 | 3 | R 3_Gathering Insulation | Resources |  | `BF4935A464F03A66` |
| 18 | 0 | R 1_CoalThumper | 0 |  | Resources |  | `86A71762D9705649` |
| 18 | 1 | R 3_SteamCoalThumper | 2 | R 1_CoalThumper | Resources |  | `B5B909AE4C1E54F6` |
| 18 | 2 | R 6_Coal Thumper Upgrade | 5 | R 3_SteamCoalThumper | Resources |  | `86CCBA956FEBC7DE` |
| 19 | 0 | R 2_Coal Mine | 1 |  | Resources |  | `A3796F28B9AC2A23` |
| 19 | 1 | R 4_Steam Coal Mine | 3 | R 2_Coal Mine | Resources |  | `AB63782F194E8021` |
| 19 | 2 | R 6_Adv Coal Mine | 5 | R 4_Steam Coal Mine | Resources |  | `B82524B0D19D88D5` |
| 20 | 0 | R 3_Coal mine upgrade 1 | 2 | R 2_Coal Mine | Resources |  | `A5A0DE87E8514928` |
| 20 | 1 | R 5_Coal Mine upgrade 2 | 4 | R 3_Coal mine upgrade 1 | Resources |  | `960C27264EA17CF9` |
| 21 | 0 | R 2_CharcoalKiln | 1 |  | Resources |  | `BEC33A80C6853BA3` |
| 21 | 1 | R 3_Kiln upgrade | 2 | R 2_CharcoalKiln | Resources |  | `BBD8663CFA3F3D7D` |
| 21 | 2 | R 4_Kiln upgrade 2 | 3 | R 3_Kiln upgrade | Resources |  | `87221793ACD9ED8A` |
| 22 | 0 | R 1_Sawmill | 0 |  | Resources |  | `A9F39DF2AEEB21B1` |
| 22 | 1 | R 3_Steam Sawmill | 2 | R 1_Sawmill | Resources |  | `AC019A23402AB883` |
| 23 | 0 | R 2_Sawmill Upgrade | 1 | R 1_Sawmill | Resources |  | `9D7F762601622CA6` |
| 23 | 1 | R 5_Sawmill Upgrade - range | 4 | R 2_Sawmill Upgrade | Resources |  | `BC770E739218DAB4` |
| 24 | 0 | R 2_WallDrill | 1 |  | Resources |  | `B3C7DF784DC97A83` |
| 24 | 1 | R 4_Steam WallDrill | 3 | R 2_WallDrill | Resources |  | `88698E693179DD2D` |
| 24 | 2 | R 6_Adv Wall Drill | 5 | R 4_Steam WallDrill | Resources |  | `9DA3ABAE1FA72621` |
| 25 | 0 | R 1_Steel Mine | 0 |  | Resources |  | `87856A059CE65971` |
| 25 | 1 | R 3_Steam Steel Mine | 2 | R 1_Steel Mine | Resources |  | `A9AC3601AE5EA23C` |
| 25 | 2 | R 5_Adv Steel Mine | 4 | R 3_Steam Steel Mine | Resources |  | `884E0D684DAAB02D` |
| 26 | 0 | F 2_Plantation | 1 |  | Food_Health_Shelter |  | `BF7153CF70731641` |
| 26 | 1 | F 4_Steam Plantation | 3 | F 2_Plantation | Food_Health_Shelter |  | `B49A95770CB83298` |
| 26 | 2 | F 6_Plantation upgrade | 5 | F 4_Steam Plantation | Food_Health_Shelter |  | `A92780AFF69180BF` |
| 27 | 0 | F 3_Plantation Insulation | 2 | F 2_Plantation | Food_Health_Shelter |  | `95DCED5FD8466387` |
| 27 | 1 | F 5_Plantation Insulation | 4 | F 3_Plantation Insulation | Food_Health_Shelter |  | `94FCD02E48F30228` |
| 30 | 0 | F 2_MedPost Upgrade | 1 |  | Food_Health_Shelter |  | `AEBE6A8DC9857616` |
| 30 | 1 | F 3_Steam healthcare | 2 | F 2_MedPost Upgrade | Food_Health_Shelter |  | `AE0D5029C4FAFA6A` |
| 30 | 2 | F 5_Infirmary Upgrade | 4 | F 3_Steam healthcare | Food_Health_Shelter |  | `A8E5A152E5F619C4` |
| 31 | 0 | F 3_HealthCare Buildings - warmer | 2 | F 2_MedPost Upgrade | Food_Health_Shelter |  | `9EB2A329D39949C0` |
| 31 | 1 | F 6_Healthcare Buildings - warmer | 5 | F 3_HealthCare Buildings - warmer | Food_Health_Shelter |  | `881F9801EF69831F` |
| 32 | 0 | F 4_Infirmary Upgrade - less workers | 3 | F 3_Steam healthcare | Food_Health_Shelter |  | `B5C1BFCACE2F518A` |
| 33 | 0 | F 2_Bunkhouse | 1 |  | Food_Health_Shelter |  | `9479CC7A027A3F61` |
| 33 | 1 | F 4_House | 3 | F 2_Bunkhouse | Food_Health_Shelter |  | `8226B4149A4C642A` |
| 33 | 2 | F 6_House Insulation | 5 | F 4_House | Food_Health_Shelter |  | `A751C3903745ABD5` |
| 34 | 0 | F 3_Cantinee - warmer | 2 | F 2_Bunkhouse | Food_Health_Shelter |  | `9E93A93F1C0C2C51` |
| 35 | 0 | F 5_House Upgrade - cheaper | 4 | F 4_House | Food_Health_Shelter |  | `BFB89F7354D04AE2` |
| 36 | 0 | 01_TierUnlock 1 | 0 |  | - |  | `BE5B5F3661B74AAE` |
| 36 | 1 | 02_TierUnlock2 | 0 | 01_TierUnlock 1 | - |  | `99F67FD0200252EF` |
| 36 | 2 | 03_TierUnlock3 | 0 | 02_TierUnlock2 | - |  | `9D18A6629DC54E67` |
| 36 | 3 | 04_TierUnlock4 | 0 | 03_TierUnlock3 | - |  | `9F5C9E5513755596` |
| 36 | 4 | 05_TierUnlock5 | 0 | 04_TierUnlock4 | - |  | `8D5D8F90ED24296E` |
| 79 | 0 | E 5_Automaton Engineers | 4 | E 4_Automaton Efficiency | Exploration_Industry |  | `A01F9F7DBAB21917` |

### Refugees (`REFUGEES`) — 87 technos

Technos de départ (exclues) : aucune.

| Arbre | Rang | Techno | Palier | Parent | Onglet | Condition d'affichage | Hash |
|---:|---:|---|---:|---|---|---|---|
| 0 | 0 | H 1_SteamHub | 0 |  | Heating |  | `A0E0D2727D0B336A` |
| 0 | 1 | H 3_Steamhub Upgrade - range | 2 | H 1_SteamHub | Heating |  | `8ACF1A3F6CD1A7FB` |
| 0 | 2 | H 5_Steamhub Upgrade - less coal | 4 | H 3_Steamhub Upgrade - range | Heating |  | `8169FDE4BB9BB141` |
| 1 | 0 | H 2_Generator Upgrade - power 1 | 1 |  | Heating |  | `A55C1E23258CB056` |
| 1 | 1 | H 4_Generator Upgrade - power 2 | 3 | H 2_Generator Upgrade - power 1 | Heating |  | `BDF0CF1D41987C65` |
| 1 | 2 | H 6_Generator Upgrade - power 3 | 5 | H 4_Generator Upgrade - power 2 | Heating |  | `B8DD400DADCDEF8E` |
| 2 | 0 | H 3_Generator Upgrade - less coal | 2 | H 2_Generator Upgrade - power 1 | Heating |  | `AAE37A9B8A8B25F1` |
| 2 | 1 | H 5 Generator Upgrade - less coal 2 | 4 | H 3_Generator Upgrade - less coal | Heating |  | `B6C37AB9E60FF207` |
| 3 | 0 | H 2_Generator Upgrade - range 1 | 1 |  | Heating |  | `A156A81B7E067BF5` |
| 3 | 1 | H 4_Generator Upgrade - range 2 | 3 | H 2_Generator Upgrade - range 1 | Heating |  | `94BF1A6EF2603AE0` |
| 3 | 2 | H 6_Generator Upgrade - range 2 | 5 | H 4_Generator Upgrade - range 2 | Heating |  | `B800B652FE01D119` |
| 4 | 0 | H 3_Generator Overdrive 1 | 2 | H 2_Generator Upgrade - power 1 | Heating |  | `8F0A051638529900` |
| 4 | 1 | H 6_Generator Upgrade - less stress | 5 | H 3_Generator Overdrive 1 | Heating |  | `91D2CE4C68CE4109` |
| 5 | 0 | H 1_Boilers | 0 |  | Heating |  | `85BCE42D5F9E4F3B` |
| 5 | 1 | H 3_Boiler Upgrade | 2 | H 1_Boilers | Heating |  | `B2366AD179BD4D26` |
| 5 | 2 | H 5_Boiler Upgrade 2 | 4 | H 3_Boiler Upgrade | Heating |  | `BA712FF32CF9CAAE` |
| 6 | 0 | H 4_Boiler Upgrade - less coal | 3 | H 3_Boiler Upgrade | Heating |  | `8095FB143BBF82CC` |
| 7 | 0 | E 1_Beacon | 0 |  | Exploration_Industry |  | `BD91C0E32EE8A6B2` |
| 8 | 0 | E 3_ExpeditionCenter | 2 | E 1_Beacon | Exploration_Industry |  | `A16CF8DC50B2727A` |
| 8 | 1 | E 5_ExpeditionUpgrade | 4 | E 3_ExpeditionCenter | Exploration_Industry |  | `B7D0B9057487D146` |
| 9 | 0 | E 2_ScoutsLimit | 1 | E 1_Beacon | Exploration_Industry |  | `BFB9EC0F1190EBC8` |
| 9 | 1 | E 5_ScoutsLimit2 | 4 | E 2_ScoutsLimit | Exploration_Industry |  | `903BF385083D378F` |
| 10 | 0 | E 3_ScoutsSpeed | 2 | E 2_ScoutsLimit | Exploration_Industry |  | `980DA1A7BA055FAD` |
| 10 | 1 | E 6_ScoutsSpeed 2 | 5 | E 3_ScoutsSpeed | Exploration_Industry |  | `AFDDF739B5C4D799` |
| 11 | 0 | E 3_Factory | 2 |  | Exploration_Industry |  | `A3E95C4CEB6C2B7F` |
| 11 | 1 | E 4_Cheaper Automatons | 3 | E 3_Factory | Exploration_Industry |  | `AD20BB9845A5DDC5` |
| 11 | 2 | E 6_Automatons no Steamcores | 5 | E 4_Cheaper Automatons | Exploration_Industry |  | `973B35A4E76361EE` |
| 12 | 0 | E 4_Cheaper Augmentation Parts | 3 | E 3_Factory | Exploration_Industry |  | `8AFE2FDE0DF97D69` |
| 12 | 1 | E 5_Faster Augmentation Parts production | 4 | E 4_Cheaper Augmentation Parts | Exploration_Industry |  | `981F59F3146CF4C6` |
| 13 | 0 | E 4_Automaton Efficiency | 3 | E 3_Factory | Exploration_Industry |  | `9E16C1962C1325C6` |
| 13 | 1 | E 5_Automaton Efficiency 2 | 4 | E 4_Automaton Efficiency | Exploration_Industry |  | `A181975459F6C673` |
| 13 | 2 | E 6_Automatons Efficiency 3 | 5 | E 5_Automaton Efficiency 2 | Exploration_Industry |  | `B0131D084E15541A` |
| 14 | 0 | E 5_Automaton Medical | 4 | E 4_Automaton Efficiency | Exploration_Industry |  | `B12F86943A6036E4` |
| 14 | 1 | E 6_Automatons Medical 2 | 5 | E 5_Automaton Medical | Exploration_Industry |  | `85C386E583A32865` |
| 15 | 0 | R 1_Gathering Upgrade | 0 |  | Resources |  | `8BC47F368138DA92` |
| 15 | 1 | R 3_Gathering Insulation | 2 | R 1_Gathering Upgrade | Resources |  | `B69B51DB31F4B001` |
| 15 | 2 | R 5_Gathering Insulation 2 | 4 | R 3_Gathering Insulation | Resources |  | `851032DABCC703AE` |
| 16 | 0 | R 2_ResourceDepotUpgrade | 1 | R 1_Gathering Upgrade | Resources |  | `B300360E7D7A3295` |
| 16 | 1 | R 3_Resource Depot Big | 2 | R 2_ResourceDepotUpgrade | Resources |  | `96031EA6A430BF85` |
| 17 | 0 | R 4_Gathering upgrade 2 | 3 | R 3_Gathering Insulation | Resources |  | `BF4935A464F03A66` |
| 18 | 0 | R 1_CoalThumper | 0 |  | Resources |  | `86A71762D9705649` |
| 18 | 1 | R 3_SteamCoalThumper | 2 | R 1_CoalThumper | Resources |  | `B5B909AE4C1E54F6` |
| 18 | 2 | R 6_Coal Thumper Upgrade | 5 | R 3_SteamCoalThumper | Resources |  | `86CCBA956FEBC7DE` |
| 19 | 0 | R 2_Coal Mine | 1 |  | Resources |  | `A3796F28B9AC2A23` |
| 19 | 1 | R 4_Steam Coal Mine | 3 | R 2_Coal Mine | Resources |  | `AB63782F194E8021` |
| 19 | 2 | R 6_Adv Coal Mine | 5 | R 4_Steam Coal Mine | Resources |  | `B82524B0D19D88D5` |
| 20 | 0 | R 3_Coal mine upgrade 1 | 2 | R 2_Coal Mine | Resources |  | `A5A0DE87E8514928` |
| 20 | 1 | R 5_Coal Mine upgrade 2 | 4 | R 3_Coal mine upgrade 1 | Resources |  | `960C27264EA17CF9` |
| 21 | 0 | R 2_CharcoalKiln | 1 |  | Resources |  | `BEC33A80C6853BA3` |
| 21 | 1 | R 3_Kiln upgrade | 2 | R 2_CharcoalKiln | Resources |  | `BBD8663CFA3F3D7D` |
| 21 | 2 | R 4_Kiln upgrade 2 | 3 | R 3_Kiln upgrade | Resources |  | `87221793ACD9ED8A` |
| 22 | 0 | R 1_Sawmill | 0 |  | Resources |  | `A9F39DF2AEEB21B1` |
| 22 | 1 | R 3_Steam Sawmill | 2 | R 1_Sawmill | Resources |  | `AC019A23402AB883` |
| 23 | 0 | R 2_Sawmill Upgrade | 1 | R 1_Sawmill | Resources |  | `9D7F762601622CA6` |
| 23 | 1 | R 5_Sawmill Upgrade - range | 4 | R 2_Sawmill Upgrade | Resources |  | `BC770E739218DAB4` |
| 24 | 0 | R 2_WallDrill | 1 |  | Resources |  | `B3C7DF784DC97A83` |
| 24 | 1 | R 4_Steam WallDrill | 3 | R 2_WallDrill | Resources |  | `88698E693179DD2D` |
| 24 | 2 | R 6_Adv Wall Drill | 5 | R 4_Steam WallDrill | Resources |  | `9DA3ABAE1FA72621` |
| 25 | 0 | R 1_Steel Mine | 0 |  | Resources |  | `87856A059CE65971` |
| 25 | 1 | R 3_Steam Steel Mine | 2 | R 1_Steel Mine | Resources |  | `A9AC3601AE5EA23C` |
| 25 | 2 | R 5_Adv Steel Mine | 4 | R 3_Steam Steel Mine | Resources |  | `884E0D684DAAB02D` |
| 26 | 0 | F 2_Plantation | 1 |  | Food_Health_Shelter |  | `BF7153CF70731641` |
| 26 | 1 | F 4_Steam Plantation | 3 | F 2_Plantation | Food_Health_Shelter |  | `B49A95770CB83298` |
| 26 | 2 | F 6_Plantation upgrade | 5 | F 4_Steam Plantation | Food_Health_Shelter |  | `A92780AFF69180BF` |
| 27 | 0 | F 3_Plantation Insulation | 2 | F 2_Plantation | Food_Health_Shelter |  | `95DCED5FD8466387` |
| 27 | 1 | F 5_Plantation Insulation | 4 | F 3_Plantation Insulation | Food_Health_Shelter |  | `94FCD02E48F30228` |
| 28 | 0 | F 1_HuntersUpgrade | 0 |  | Food_Health_Shelter |  | `BD90D178518E872C` |
| 28 | 1 | F 3_Flying Hunters | 2 | F 1_HuntersUpgrade | Food_Health_Shelter |  | `90F460E6380DFD7A` |
| 28 | 2 | F 5_FlyingHuntersUpgrade | 4 | F 3_Flying Hunters | Food_Health_Shelter |  | `93B19C3058E896DC` |
| 29 | 0 | F 3_Hunters Upgrade - less workers | 2 | F 1_HuntersUpgrade | Food_Health_Shelter |  | `9781BAE37ABD9B94` |
| 30 | 0 | F 2_MedPost Upgrade | 1 |  | Food_Health_Shelter |  | `AEBE6A8DC9857616` |
| 30 | 1 | F 3_Steam healthcare | 2 | F 2_MedPost Upgrade | Food_Health_Shelter |  | `AE0D5029C4FAFA6A` |
| 30 | 2 | F 5_Infirmary Upgrade | 4 | F 3_Steam healthcare | Food_Health_Shelter |  | `A8E5A152E5F619C4` |
| 31 | 0 | F 3_HealthCare Buildings - warmer | 2 | F 2_MedPost Upgrade | Food_Health_Shelter |  | `9EB2A329D39949C0` |
| 31 | 1 | F 6_Healthcare Buildings - warmer | 5 | F 3_HealthCare Buildings - warmer | Food_Health_Shelter |  | `881F9801EF69831F` |
| 32 | 0 | F 4_Infirmary Upgrade - less workers | 3 | F 3_Steam healthcare | Food_Health_Shelter |  | `B5C1BFCACE2F518A` |
| 33 | 0 | F 2_Bunkhouse | 1 |  | Food_Health_Shelter |  | `9479CC7A027A3F61` |
| 33 | 1 | F 4_House | 3 | F 2_Bunkhouse | Food_Health_Shelter |  | `8226B4149A4C642A` |
| 33 | 2 | F 6_House Insulation | 5 | F 4_House | Food_Health_Shelter |  | `A751C3903745ABD5` |
| 34 | 0 | F 3_Cantinee - warmer | 2 | F 2_Bunkhouse | Food_Health_Shelter |  | `9E93A93F1C0C2C51` |
| 35 | 0 | F 5_House Upgrade - cheaper | 4 | F 4_House | Food_Health_Shelter |  | `BFB89F7354D04AE2` |
| 36 | 0 | 01_TierUnlock 1 | 0 |  | - |  | `BE5B5F3661B74AAE` |
| 36 | 1 | 02_TierUnlock2 | 0 | 01_TierUnlock 1 | - |  | `99F67FD0200252EF` |
| 36 | 2 | 03_TierUnlock3 | 0 | 02_TierUnlock2 | - |  | `9D18A6629DC54E67` |
| 36 | 3 | 04_TierUnlock4 | 0 | 03_TierUnlock3 | - |  | `9F5C9E5513755596` |
| 36 | 4 | 05_TierUnlock5 | 0 | 04_TierUnlock4 | - |  | `8D5D8F90ED24296E` |
| 79 | 0 | E 5_Automaton Engineers | 4 | E 4_Automaton Efficiency | Exploration_Industry |  | `A01F9F7DBAB21917` |

### Fall of Winterhome (`WINTERHOME`) — 73 technos

Technos de départ (exclues) : 01_TierUnlock 1, 02_TierUnlock2, 03_TierUnlock3, E 1_Beacon, E 3_ExpeditionCenter, F 1_HuntersUpgrade, F 2_Bunkhouse, F 2_Plantation, F 4_House, H 1_Boilers, H 1_SteamHub, H 2_Generator Upgrade - power 1, H 2_Generator Upgrade - range 1, H 4_Generator Upgrade - power 2, H 4_Generator Upgrade - range 2, R 1_CoalThumper, R 1_Sawmill, R 1_Steel Mine, R 2_CharcoalKiln, R 2_Coal Mine, R 2_WallDrill.

| Arbre | Rang | Techno | Palier | Parent | Onglet | Condition d'affichage | Hash |
|---:|---:|---|---:|---|---|---|---|
| 0 | 0 | H 3_Steamhub Upgrade - range | 2 | *H 1_SteamHub* | Heating |  | `8ACF1A3F6CD1A7FB` |
| 0 | 1 | H 5_Steamhub Upgrade - less coal | 4 | H 3_Steamhub Upgrade - range | Heating |  | `8169FDE4BB9BB141` |
| 1 | 0 | H 6_Generator Upgrade - power 3 | 5 | *H 4_Generator Upgrade - power 2* | Heating |  | `B8DD400DADCDEF8E` |
| 2 | 0 | H 3_Generator Upgrade - less coal | 2 | *H 2_Generator Upgrade - power 1* | Heating |  | `AAE37A9B8A8B25F1` |
| 2 | 1 | H 5 Generator Upgrade - less coal 2 | 4 | H 3_Generator Upgrade - less coal | Heating |  | `B6C37AB9E60FF207` |
| 3 | 0 | H 6_Generator Upgrade - range 2 | 5 | *H 4_Generator Upgrade - range 2* | Heating |  | `B800B652FE01D119` |
| 4 | 0 | H 3_Generator Overdrive 1 | 2 | *H 2_Generator Upgrade - power 1* | Heating |  | `8F0A051638529900` |
| 4 | 1 | H 6_Generator Upgrade - less stress | 5 | H 3_Generator Overdrive 1 | Heating |  | `91D2CE4C68CE4109` |
| 5 | 0 | H 3_Boiler Upgrade | 2 | *H 1_Boilers* | Heating |  | `B2366AD179BD4D26` |
| 5 | 1 | H 5_Boiler Upgrade 2 | 4 | H 3_Boiler Upgrade | Heating |  | `BA712FF32CF9CAAE` |
| 6 | 0 | H 4_Boiler Upgrade - less coal | 3 | H 3_Boiler Upgrade | Heating |  | `8095FB143BBF82CC` |
| 8 | 0 | E 5_ExpeditionUpgradeWinterhome | 4 | *E 3_ExpeditionCenter* | Exploration_Industry | unlock [Winterhome] EvacuationUpgradeDescription | `AF702239C79A412B` |
| 9 | 0 | E 2_ScoutsLimit | 1 | *E 1_Beacon* | Exploration_Industry |  | `BFB9EC0F1190EBC8` |
| 9 | 1 | E 5_ScoutsLimit2 | 4 | E 2_ScoutsLimit | Exploration_Industry |  | `903BF385083D378F` |
| 10 | 0 | E 3_ScoutsSpeed | 2 | E 2_ScoutsLimit | Exploration_Industry |  | `980DA1A7BA055FAD` |
| 10 | 1 | E 6_ScoutsSpeed 2 | 5 | E 3_ScoutsSpeed | Exploration_Industry |  | `AFDDF739B5C4D799` |
| 11 | 0 | E 3_Factory | 2 |  | Exploration_Industry |  | `A3E95C4CEB6C2B7F` |
| 11 | 1 | E 4_Cheaper Automatons | 3 | E 3_Factory | Exploration_Industry |  | `AD20BB9845A5DDC5` |
| 11 | 2 | E 6_Automatons no Steamcores | 5 | E 4_Cheaper Automatons | Exploration_Industry |  | `973B35A4E76361EE` |
| 12 | 0 | E 4_Cheaper Augmentation Parts | 3 | E 3_Factory | Exploration_Industry |  | `8AFE2FDE0DF97D69` |
| 12 | 1 | E 5_Faster Augmentation Parts production | 4 | E 4_Cheaper Augmentation Parts | Exploration_Industry |  | `981F59F3146CF4C6` |
| 13 | 0 | E 4_Automaton Efficiency | 3 | E 3_Factory | Exploration_Industry |  | `9E16C1962C1325C6` |
| 13 | 1 | E 5_Automaton Efficiency 2 | 4 | E 4_Automaton Efficiency | Exploration_Industry |  | `A181975459F6C673` |
| 13 | 2 | E 6_Automatons Efficiency 3 | 5 | E 5_Automaton Efficiency 2 | Exploration_Industry |  | `B0131D084E15541A` |
| 14 | 0 | E 5_Automaton Medical | 4 | E 4_Automaton Efficiency | Exploration_Industry |  | `B12F86943A6036E4` |
| 14 | 1 | E 6_Automatons Medical 2 | 5 | E 5_Automaton Medical | Exploration_Industry |  | `85C386E583A32865` |
| 15 | 0 | R 1_Gathering Upgrade | 0 |  | Resources |  | `8BC47F368138DA92` |
| 15 | 1 | R 3_Gathering Insulation | 2 | R 1_Gathering Upgrade | Resources |  | `B69B51DB31F4B001` |
| 15 | 2 | R 5_Gathering Insulation 2 | 4 | R 3_Gathering Insulation | Resources |  | `851032DABCC703AE` |
| 16 | 0 | R 2_ResourceDepotUpgrade | 1 | R 1_Gathering Upgrade | Resources |  | `B300360E7D7A3295` |
| 16 | 1 | R 3_Resource Depot Big | 2 | R 2_ResourceDepotUpgrade | Resources |  | `96031EA6A430BF85` |
| 17 | 0 | R 4_Gathering upgrade 2 | 3 | R 3_Gathering Insulation | Resources |  | `BF4935A464F03A66` |
| 18 | 0 | R 3_SteamCoalThumper | 2 | *R 1_CoalThumper* | Resources |  | `B5B909AE4C1E54F6` |
| 18 | 1 | R 6_Coal Thumper Upgrade | 5 | R 3_SteamCoalThumper | Resources |  | `86CCBA956FEBC7DE` |
| 19 | 0 | R 4_Steam Coal Mine | 3 | *R 2_Coal Mine* | Resources |  | `AB63782F194E8021` |
| 19 | 1 | R 6_Adv Coal Mine | 5 | R 4_Steam Coal Mine | Resources |  | `B82524B0D19D88D5` |
| 20 | 0 | R 3_Coal mine upgrade 1 | 2 | *R 2_Coal Mine* | Resources |  | `A5A0DE87E8514928` |
| 20 | 1 | R 5_Coal Mine upgrade 2 | 4 | R 3_Coal mine upgrade 1 | Resources |  | `960C27264EA17CF9` |
| 21 | 0 | R 3_Kiln upgrade | 2 | *R 2_CharcoalKiln* | Resources |  | `BBD8663CFA3F3D7D` |
| 21 | 1 | R 4_Kiln upgrade 2 | 3 | R 3_Kiln upgrade | Resources |  | `87221793ACD9ED8A` |
| 22 | 0 | R 3_Steam Sawmill | 2 | *R 1_Sawmill* | Resources |  | `AC019A23402AB883` |
| 23 | 0 | R 2_Sawmill Upgrade | 1 | *R 1_Sawmill* | Resources |  | `9D7F762601622CA6` |
| 23 | 1 | R 5_Sawmill Upgrade - range | 4 | R 2_Sawmill Upgrade | Resources |  | `BC770E739218DAB4` |
| 24 | 0 | R 4_Steam WallDrill | 3 | *R 2_WallDrill* | Resources |  | `88698E693179DD2D` |
| 24 | 1 | R 6_Adv Wall Drill | 5 | R 4_Steam WallDrill | Resources |  | `9DA3ABAE1FA72621` |
| 25 | 0 | R 3_Steam Steel Mine | 2 | *R 1_Steel Mine* | Resources |  | `A9AC3601AE5EA23C` |
| 25 | 1 | R 5_Adv Steel Mine | 4 | R 3_Steam Steel Mine | Resources |  | `884E0D684DAAB02D` |
| 26 | 0 | F 4_Steam Plantation | 3 | *F 2_Plantation* | Food_Health_Shelter |  | `B49A95770CB83298` |
| 26 | 1 | F 6_Plantation upgrade | 5 | F 4_Steam Plantation | Food_Health_Shelter |  | `A92780AFF69180BF` |
| 27 | 0 | F 3_Plantation Insulation | 2 | *F 2_Plantation* | Food_Health_Shelter |  | `95DCED5FD8466387` |
| 27 | 1 | F 5_Plantation Insulation | 4 | F 3_Plantation Insulation | Food_Health_Shelter |  | `94FCD02E48F30228` |
| 28 | 0 | F 3_Flying Hunters | 2 | *F 1_HuntersUpgrade* | Food_Health_Shelter |  | `90F460E6380DFD7A` |
| 28 | 1 | F 5_FlyingHuntersUpgrade | 4 | F 3_Flying Hunters | Food_Health_Shelter |  | `93B19C3058E896DC` |
| 29 | 0 | F 3_Hunters Upgrade - less workers | 2 | *F 1_HuntersUpgrade* | Food_Health_Shelter |  | `9781BAE37ABD9B94` |
| 30 | 0 | F 2_MedPost Upgrade | 1 |  | Food_Health_Shelter |  | `AEBE6A8DC9857616` |
| 30 | 1 | F 3_Steam healthcare | 2 | F 2_MedPost Upgrade | Food_Health_Shelter |  | `AE0D5029C4FAFA6A` |
| 30 | 2 | F 5_Infirmary Upgrade | 4 | F 3_Steam healthcare | Food_Health_Shelter |  | `A8E5A152E5F619C4` |
| 31 | 0 | F 3_HealthCare Buildings - warmer | 2 | F 2_MedPost Upgrade | Food_Health_Shelter |  | `9EB2A329D39949C0` |
| 31 | 1 | F 6_Healthcare Buildings - warmer | 5 | F 3_HealthCare Buildings - warmer | Food_Health_Shelter |  | `881F9801EF69831F` |
| 32 | 0 | F 4_Infirmary Upgrade - less workers | 3 | F 3_Steam healthcare | Food_Health_Shelter |  | `B5C1BFCACE2F518A` |
| 33 | 0 | F 6_House Insulation | 5 | *F 4_House* | Food_Health_Shelter |  | `A751C3903745ABD5` |
| 34 | 0 | F 3_Cantinee - warmer | 2 | *F 2_Bunkhouse* | Food_Health_Shelter |  | `9E93A93F1C0C2C51` |
| 35 | 0 | F 5_House Upgrade - cheaper | 4 | *F 4_House* | Food_Health_Shelter |  | `BFB89F7354D04AE2` |
| 36 | 0 | 04_TierUnlock4 | 0 | *03_TierUnlock3* | - |  | `9F5C9E5513755596` |
| 36 | 1 | 05_TierUnlock5 | 0 | 04_TierUnlock4 | - |  | `8D5D8F90ED24296E` |
| 37 | 0 | [Winterhome] E 1_RepairStation | 0 |  | Heating | unlock [Winterhome] Repair Station | `A24E361D87891912` |
| 37 | 1 | [Winterhome] E 3_RC Efficiency 1 | 0 | [Winterhome] E 1_RepairStation | Heating | unlock [Winterhome] Repair Station Upgrades | `9004FB1D2E95369F` |
| 37 | 2 | [Winterhome] E 5_RC Efficiency 2 | 4 | [Winterhome] E 3_RC Efficiency 1 | Heating | unlock [Winterhome] Repair Station Upgrades | `9BAE7B748D5B7820` |
| 37 | 3 | [Winterhome] E 6_RC Efficiency 3 | 5 | [Winterhome] E 5_RC Efficiency 2 | Heating | unlock [Winterhome] Repair Station Upgrades | `9A306E92F391690A` |
| 38 | 0 | [Winterhome] E 3_RC Ability | 0 | [Winterhome] E 1_RepairStation | Heating | unlock [Winterhome] Repair Station Upgrades | `A54820F7892B9652` |
| 38 | 1 | [Winterhome] E 6_RC Ability | 5 | [Winterhome] E 3_RC Ability | Heating | unlock [Winterhome] Repair Station Upgrades | `8A15192C335D2FEC` |
| 39 | 0 | [Winterhome] E 5_RC Less workers | 4 | [Winterhome] E 3_RC Efficiency 1 | Heating | unlock [Winterhome] Repair Station Upgrades | `BC7C654BF3D6E484` |
| 79 | 0 | E 5_Automaton Engineers | 4 | E 4_Automaton Efficiency | Exploration_Industry |  | `A01F9F7DBAB21917` |

### Last Autumn (`BUILDERS`) — 67 technos

Technos de départ (exclues) : aucune.

| Arbre | Rang | Techno | Palier | Parent | Onglet | Condition d'affichage | Hash |
|---:|---:|---|---:|---|---|---|---|
| 36 | 0 | 01_TierUnlock 1 | 0 |  | - |  | `BE5B5F3661B74AAE` |
| 36 | 1 | 02_TierUnlock2 | 0 | 01_TierUnlock 1 | - |  | `99F67FD0200252EF` |
| 36 | 2 | 03_TierUnlock3 | 0 | 02_TierUnlock2 | - |  | `9D18A6629DC54E67` |
| 36 | 3 | 04_TierUnlock4 | 0 | 03_TierUnlock3 | - |  | `9F5C9E5513755596` |
| 36 | 4 | 05_TierUnlock5 | 0 | 04_TierUnlock4 | - |  | `8D5D8F90ED24296E` |
| 40 | 0 | [Builders] HC 2_Brazier | 2 |  | Builders_Heat | unlock [Builder] HeatTab | `9A3EB5C5736C5CF8` |
| 40 | 1 | [Builders] HC 4_BrazierPower1 | 4 | [Builders] HC 2_Brazier | Builders_Heat | unlock [Builder] HeatTab | `8BA6E8ADEAA56A13` |
| 41 | 0 | [Builders] HC 4_BrazierRange1 | 4 | [Builders] HC 2_Brazier | Builders_Heat | unlock [Builder] HeatTab | `BE2A8E7B932D9C2D` |
| 42 | 0 | [Builders] HC 3_BrazierCoalOpt1 | 3 | [Builders] HC 2_Brazier | Builders_Heat | unlock [Builder] HeatTab | `84FFDAF28948F16B` |
| 42 | 1 | [Builders] HC 5_BrazierCoalOpt2 | 5 | [Builders] HC 3_BrazierCoalOpt1 | Builders_Heat | unlock [Builder] HeatTab | `A21A49B6CC78F2C0` |
| 43 | 0 | [Builders] HH 2_Heater | 2 |  | Builders_Heat | unlock [Builder] HeatTab | `8F3095A50ABE5B8C` |
| 43 | 1 | [Builders] HH 4_Heater Upgrade | 4 | [Builders] HH 2_Heater | Builders_Heat | unlock [Builder] HeatTab | `98973939A041BFB3` |
| 43 | 2 | [Builders] HH 5_Heater Upgrade - less coal | 5 | [Builders] HH 4_Heater Upgrade | Builders_Heat | unlock [Builder] HeatTab | `95884318394691F1` |
| 44 | 0 | [Builders] HI 2_HealthCare Insulation1 | 2 |  | Builders_Heat | unlock [Builder] HeatTab | `AD9A3CCFA5A5E216` |
| 44 | 1 | [Builders] HI 4_Healthcare Insulation2 | 4 | [Builders] HI 2_HealthCare Insulation1 | Builders_Heat | unlock [Builder] HeatTab | `89E11B6D8D3EBB34` |
| 45 | 0 | [Builders] GS 0_ScaffoldingManufacture | 0 |  | Builders_Generator |  | `B64F631E2C554692` |
| 45 | 1 | [Builders] GS 2_ManufactureCheaperMaterials | 2 | [Builders] GS 0_ScaffoldingManufacture | Builders_Generator |  | `A73DE9D28A567C7D` |
| 45 | 2 | [Builders] GS 3_Manufacture safety improve | 3 | [Builders] GS 2_ManufactureCheaperMaterials | Builders_Generator |  | `A585F6C76A51C68D` |
| 46 | 0 | [Builders] GS 1_ProtectiveStructure1 | 1 | [Builders] GS 0_ScaffoldingManufacture | Builders_Generator |  | `A3744892FFB106B8` |
| 46 | 1 | [Builders] GS 3_ProtectiveStructure2 | 3 | [Builders] GS 1_ProtectiveStructure1 | Builders_Generator |  | `9B1DFC4166BDF1F2` |
| 47 | 0 | [Builders] GM 2_SteelPlateForge | 2 |  | Builders_Generator |  | `869CB004219D9B9F` |
| 47 | 1 | [Builders] GM 4_Forge safety improve | 4 | [Builders] GM 2_SteelPlateForge | Builders_Generator |  | `B340712E1AA67558` |
| 48 | 0 | [Builders] GM 3_ForgeCheaperMaterials | 3 | [Builders] GM 2_SteelPlateForge | Builders_Generator |  | `BB76809681658ED6` |
| 49 | 0 | [Builders] GG 1_SteamMill | 1 |  | Builders_Generator |  | `A6AB1E204FB27849` |
| 49 | 1 | [Builders] GG 3_Steam Mill safety improve | 3 | [Builders] GG 1_SteamMill | Builders_Generator |  | `8626EF58D76A77AA` |
| 49 | 2 | [Builders] GG 4_SteamMillCheaperMaterials | 4 | [Builders] GG 3_Steam Mill safety improve | Builders_Generator |  | `B52481405FA109E4` |
| 50 | 0 | [Builders] GG 2_Supercharge1 | 2 | [Builders] GG 1_SteamMill | Builders_Generator |  | `907F270A35547E8E` |
| 50 | 1 | [Builders] GG 4_Supercharge2 | 4 | [Builders] GG 2_Supercharge1 | Builders_Generator |  | `A3DF3639D1906909` |
| 51 | 0 | [Builders] GF 1_Air Scrubbers1 | 1 |  | Builders_Generator |  | `B7DB8BC2F8AA8D18` |
| 51 | 1 | [Builders] GF 3_Air Scrubbers2 | 3 | [Builders] GF 1_Air Scrubbers1 | Builders_Generator |  | `AB7A0E6FE556795C` |
| 51 | 2 | [Builders] GF 5_Air Scrubbers3 | 5 | [Builders] GF 3_Air Scrubbers2 | Builders_Generator |  | `91D3615B9CA9BD25` |
| 52 | 0 | [Builders] GF 3_Air ScrubbersOptimalization | 3 | [Builders] GF 1_Air Scrubbers1 | Builders_Generator |  | `9E446E15CA7F3829` |
| 52 | 1 | [Builders] GF 5_Air ScrubbersOptimalization2 | 5 | [Builders] GF 3_Air ScrubbersOptimalization | Builders_Generator |  | `BF2E2AB5620FFB4C` |
| 53 | 0 | [Builders] GU 3_GeneratorUpgrade1 | 3 |  | Builders_Generator | unlock [Builder] Generator Upgrades | `B1E32EC5BA936484` |
| 53 | 1 | [Builders] GU 4_GeneratorUpgrade2 | 4 |  | Builders_Generator | unlock [Builder] Generator Upgrades | `8F91A5E56C1F7808` |
| 53 | 2 | [Builders] GU 5_GeneratorUpgrade3 | 5 |  | Builders_Generator | unlock [Builder] Generator Upgrades | `8E6360937ECF4770` |
| 54 | 0 | [Builders] FF 0_Foragers | 0 |  | Builders_Health_Food_Exploration |  | `84CE92C6D1B6B9A6` |
| 55 | 0 | [Builders] FF 1_Foragers_+1 | 1 | [Builders] FF 0_Foragers | Builders_Health_Food_Exploration |  | `B2EBD63D29F02BAA` |
| 55 | 1 | [Builders] FF 3_Foragers_+2 | 3 | [Builders] FF 1_Foragers_+1 | Builders_Health_Food_Exploration |  | `A5C87129AA78EB4C` |
| 56 | 0 | [Builders] FF 1_ForagersSpeedUpgrade | 1 | [Builders] FF 0_Foragers | Builders_Health_Food_Exploration |  | `BB86A865575F0898` |
| 56 | 1 | [Builders] FF 4_ForagersSpeedUpgrade2 | 4 | [Builders] FF 1_ForagersSpeedUpgrade | Builders_Health_Food_Exploration |  | `BC30C632069AB76B` |
| 57 | 0 | [Builders] FH 1_Fishing Harbor | 1 |  | Builders_Health_Food_Exploration |  | `9926B4EED59BBBBD` |
| 57 | 1 | [Builders] FH 4_Fishing Harbor2 | 4 | [Builders] FH 1_Fishing Harbor | Builders_Health_Food_Exploration |  | `B3BCC2FDE230AD82` |
| 58 | 0 | [Builders] FH 2_Fishing Harbor_efficiencyBoost | 2 | [Builders] FH 1_Fishing Harbor | Builders_Health_Food_Exploration |  | `8F721C114E1F5EA1` |
| 59 | 0 | [Builders] FM 1_MedPost Upgrade | 1 |  | Builders_Health_Food_Exploration |  | `B25899C67C3E13C7` |
| 59 | 1 | [Builders] FM 3_Steam healthcare | 3 | [Builders] FM 1_MedPost Upgrade | Builders_Health_Food_Exploration |  | `9BEFF8545E4A0C48` |
| 59 | 2 | [Builders] FM 5_Infirmary Upgrade - less workers | 5 | [Builders] FM 3_Steam healthcare | Builders_Health_Food_Exploration |  | `A6C2C6A453C1036F` |
| 60 | 0 | [Builders] FM 4_Infirmary Upgrade | 4 | [Builders] FM 3_Steam healthcare | Builders_Health_Food_Exploration |  | `92BD99FA0B8CF5FD` |
| 61 | 0 | [Builders] RD 0_Docksv1 | 0 |  | Builders_Resources |  | `8BA655C00C38E5DB` |
| 61 | 1 | [Builders] RD 2_Docksv2 | 2 | [Builders] RD 0_Docksv1 | Builders_Resources |  | `BC42749C23D732DE` |
| 61 | 2 | [Builders] RD 4_Docksv3 | 4 | [Builders] RD 2_Docksv2 | Builders_Resources |  | `A45EC7F7C4042BA7` |
| 62 | 0 | [Builders] RU 1_URS | 1 | [Builders] RD 0_Docksv1 | Builders_Resources |  | `AB3378C1C0F63FDF` |
| 63 | 0 | [Builders] RU 3_URS_Steam | 3 | [Builders] RU 1_URS | Builders_Resources |  | `9DA9438616748A1B` |
| 63 | 1 | [Builders] RU 2_URS_Faster gathering | 4 | [Builders] RU 3_URS_Steam | Builders_Resources |  | `A189AA1BA1E45B98` |
| 64 | 0 | [Builders] RD 1_Docks faster reload | 1 | [Builders] RD 0_Docksv1 | Builders_Resources |  | `A4A3B3D1583AE97D` |
| 65 | 0 | [Builders] RG 0_Gathering Upgrade | 0 |  | Builders_Resources |  | `8EDC34ADC0A71BD9` |
| 65 | 1 | [Builders] RG 2_Gathering upgrade 2 | 2 | [Builders] RG 0_Gathering Upgrade | Builders_Resources |  | `A383E8E076BDAC46` |
| 66 | 0 | [Builders] RG 1_ResourceDepotUpgrade | 1 | [Builders] RG 0_Gathering Upgrade | Builders_Resources |  | `9CA531E645AA9138` |
| 66 | 1 | [Builders] RG 2_Resource Depot Big | 2 | [Builders] RG 1_ResourceDepotUpgrade | Builders_Resources |  | `9767F8931DCF733E` |
| 67 | 0 | [Builders] RW 0_Sawmill | 0 |  | Builders_Resources |  | `9B8662D8557103EF` |
| 67 | 1 | [Builders] RW 3_Steam Sawmill | 2 | [Builders] RW 0_Sawmill | Builders_Resources |  | `853470E14031EC9D` |
| 68 | 0 | [Builders] RW 1_Sawmill Upgrade | 1 | [Builders] RW 0_Sawmill | Builders_Resources |  | `B3E26DFFB50E015E` |
| 68 | 1 | [Builders] RW 3_Sawmill Upgrade - range | 3 | [Builders] RW 1_Sawmill Upgrade | Builders_Resources |  | `9222D9DF836D090A` |
| 69 | 0 | [Builders] RC 2_CharcoalKiln | 2 |  | Builders_Resources |  | `9D93FA7554513F8D` |
| 69 | 1 | [Builders] RC 3_Kiln upgrade | 3 | [Builders] RC 2_CharcoalKiln | Builders_Resources |  | `9A9531B0F90E6D29` |
| 69 | 2 | [Builders] RC 4_Kiln upgrade 2 | 4 | [Builders] RC 3_Kiln upgrade | Builders_Resources |  | `9A8EA035E1F6BD7D` |
| 81 | 0 | [Builders] RU 5_URS_Advanced | 5 | [Builders] RU 3_URS_Steam | Builders_Resources |  | `82F3957C59928F95` |

### On the Edge (`Reconquest`) — 60 technos

Technos de départ (exclues) : E 1_Beacon, [Reconquest] E 1_Transport Depot.

| Arbre | Rang | Techno | Palier | Parent | Onglet | Condition d'affichage | Hash |
|---:|---:|---|---:|---|---|---|---|
| 5 | 0 | H 1_Boilers | 0 |  | Heating |  | `85BCE42D5F9E4F3B` |
| 5 | 1 | H 3_Boiler Upgrade | 2 | H 1_Boilers | Heating |  | `B2366AD179BD4D26` |
| 5 | 2 | H 5_Boiler Upgrade 2 | 4 | H 3_Boiler Upgrade | Heating |  | `BA712FF32CF9CAAE` |
| 6 | 0 | H 4_Boiler Upgrade - less coal | 3 | H 3_Boiler Upgrade | Heating |  | `8095FB143BBF82CC` |
| 9 | 0 | E 2_ScoutsLimit | 1 | *E 1_Beacon* | Exploration_Industry |  | `BFB9EC0F1190EBC8` |
| 9 | 1 | E 5_ScoutsLimit2 | 4 | E 2_ScoutsLimit | Exploration_Industry |  | `903BF385083D378F` |
| 10 | 0 | E 3_ScoutsSpeed | 2 | E 2_ScoutsLimit | Exploration_Industry |  | `980DA1A7BA055FAD` |
| 10 | 1 | E 6_ScoutsSpeed 2 | 5 | E 3_ScoutsSpeed | Exploration_Industry |  | `AFDDF739B5C4D799` |
| 11 | 0 | E 3_Factory | 2 |  | Exploration_Industry |  | `A3E95C4CEB6C2B7F` |
| 12 | 0 | E 4_Cheaper Augmentation Parts | 3 | E 3_Factory | Exploration_Industry |  | `8AFE2FDE0DF97D69` |
| 12 | 1 | E 5_Faster Augmentation Parts production | 4 | E 4_Cheaper Augmentation Parts | Exploration_Industry |  | `981F59F3146CF4C6` |
| 15 | 0 | R 1_Gathering Upgrade | 0 |  | Resources |  | `8BC47F368138DA92` |
| 15 | 1 | R 3_Gathering Insulation | 2 | R 1_Gathering Upgrade | Resources |  | `B69B51DB31F4B001` |
| 15 | 2 | R 5_Gathering Insulation 2 | 4 | R 3_Gathering Insulation | Resources |  | `851032DABCC703AE` |
| 16 | 0 | R 2_ResourceDepotUpgrade | 1 | R 1_Gathering Upgrade | Resources |  | `B300360E7D7A3295` |
| 16 | 1 | R 3_Resource Depot Big | 2 | R 2_ResourceDepotUpgrade | Resources |  | `96031EA6A430BF85` |
| 17 | 0 | R 4_Gathering upgrade 2 | 3 | R 3_Gathering Insulation | Resources |  | `BF4935A464F03A66` |
| 18 | 0 | R 1_CoalThumper | 0 |  | Resources |  | `86A71762D9705649` |
| 18 | 1 | R 3_SteamCoalThumper | 2 | R 1_CoalThumper | Resources |  | `B5B909AE4C1E54F6` |
| 18 | 2 | R 6_Coal Thumper Upgrade | 5 | R 3_SteamCoalThumper | Resources |  | `86CCBA956FEBC7DE` |
| 19 | 0 | R 2_Coal Mine | 1 |  | Resources |  | `A3796F28B9AC2A23` |
| 19 | 1 | R 4_Steam Coal Mine | 3 | R 2_Coal Mine | Resources |  | `AB63782F194E8021` |
| 19 | 2 | R 6_Adv Coal Mine | 5 | R 4_Steam Coal Mine | Resources |  | `B82524B0D19D88D5` |
| 20 | 0 | R 3_Coal mine upgrade 1 | 2 | R 2_Coal Mine | Resources |  | `A5A0DE87E8514928` |
| 20 | 1 | R 5_Coal Mine upgrade 2 | 4 | R 3_Coal mine upgrade 1 | Resources |  | `960C27264EA17CF9` |
| 21 | 0 | R 2_CharcoalKiln | 1 |  | Resources |  | `BEC33A80C6853BA3` |
| 21 | 1 | R 3_Kiln upgrade | 2 | R 2_CharcoalKiln | Resources |  | `BBD8663CFA3F3D7D` |
| 21 | 2 | R 4_Kiln upgrade 2 | 3 | R 3_Kiln upgrade | Resources |  | `87221793ACD9ED8A` |
| 22 | 0 | R 1_Sawmill | 0 |  | Resources |  | `A9F39DF2AEEB21B1` |
| 22 | 1 | R 3_Steam Sawmill | 2 | R 1_Sawmill | Resources |  | `AC019A23402AB883` |
| 23 | 0 | R 2_Sawmill Upgrade | 1 | R 1_Sawmill | Resources |  | `9D7F762601622CA6` |
| 23 | 1 | R 5_Sawmill Upgrade - range | 4 | R 2_Sawmill Upgrade | Resources |  | `BC770E739218DAB4` |
| 30 | 0 | F 2_MedPost Upgrade | 1 |  | Food_Health_Shelter |  | `AEBE6A8DC9857616` |
| 30 | 1 | F 3_Steam healthcare | 2 | F 2_MedPost Upgrade | Food_Health_Shelter |  | `AE0D5029C4FAFA6A` |
| 30 | 2 | F 5_Infirmary Upgrade | 4 | F 3_Steam healthcare | Food_Health_Shelter |  | `A8E5A152E5F619C4` |
| 31 | 0 | F 3_HealthCare Buildings - warmer | 2 | F 2_MedPost Upgrade | Food_Health_Shelter |  | `9EB2A329D39949C0` |
| 31 | 1 | F 6_Healthcare Buildings - warmer | 5 | F 3_HealthCare Buildings - warmer | Food_Health_Shelter |  | `881F9801EF69831F` |
| 32 | 0 | F 4_Infirmary Upgrade - less workers | 3 | F 3_Steam healthcare | Food_Health_Shelter |  | `B5C1BFCACE2F518A` |
| 33 | 0 | F 2_Bunkhouse | 1 |  | Food_Health_Shelter |  | `9479CC7A027A3F61` |
| 33 | 1 | F 4_House | 3 | F 2_Bunkhouse | Food_Health_Shelter |  | `8226B4149A4C642A` |
| 33 | 2 | F 6_House Insulation | 5 | F 4_House | Food_Health_Shelter |  | `A751C3903745ABD5` |
| 34 | 0 | F 3_Cantinee - warmer | 2 | F 2_Bunkhouse | Food_Health_Shelter |  | `9E93A93F1C0C2C51` |
| 35 | 0 | F 5_House Upgrade - cheaper | 4 | F 4_House | Food_Health_Shelter |  | `BFB89F7354D04AE2` |
| 36 | 0 | 01_TierUnlock 1 | 0 |  | - |  | `BE5B5F3661B74AAE` |
| 36 | 1 | 02_TierUnlock2 | 0 | 01_TierUnlock 1 | - |  | `99F67FD0200252EF` |
| 36 | 2 | 03_TierUnlock3 | 0 | 02_TierUnlock2 | - |  | `9D18A6629DC54E67` |
| 36 | 3 | 04_TierUnlock4 | 0 | 03_TierUnlock3 | - |  | `9F5C9E5513755596` |
| 36 | 4 | 05_TierUnlock5 | 0 | 04_TierUnlock4 | - |  | `8D5D8F90ED24296E` |
| 70 | 0 | [Reconquest] HC 1_Brazier | 0 |  | Heating |  | `AC4103808CBCB5B8` |
| 70 | 1 | [Reconquest] HC 4_BrazierPower | 3 | [Reconquest] HC 1_Brazier | Heating |  | `87BA3B1685C7F1D1` |
| 71 | 0 | [Reconquest] HC 4_BrazierRange | 3 | [Reconquest] HC 1_Brazier | Heating |  | `B6F142D34017C5E2` |
| 72 | 0 | [Reconquest] HC 3_BrazierCoalOpt1 | 2 | [Reconquest] HC 1_Brazier | Heating |  | `B81384D36C8EEABF` |
| 72 | 1 | [Reconquest] HC 5_BrazierCoalOpt2 | 4 | [Reconquest] HC 3_BrazierCoalOpt1 | Heating |  | `B4E312D3864D6BE2` |
| 73 | 0 | [Reconquest] R 1_VaultEfficiencyBoost | 0 |  | Resources | unlock [Reconquest] SteamCores | `BAD9632D2482EB30` |
| 74 | 0 | [Reconquest] R 1_VaultUpgrade1 | 0 |  | Resources | unlock [Reconquest] SteamCores | `A0F1BE0F4175B30F` |
| 74 | 1 | [Reconquest] R 3_VaultUpgrade2 | 2 | [Reconquest] R 1_VaultUpgrade1 | Resources | unlock [Reconquest] SteamCores | `BF5C4DDAD1CAFE3B` |
| 74 | 2 | [Reconquest] R 5_VaultUpgrade3 | 4 | [Reconquest] R 3_VaultUpgrade2 | Resources | unlock [Reconquest] SteamCores | `876F9433DD7C12DA` |
| 75 | 0 | [Reconquest] R 2_VaultMultitasking | 1 | [Reconquest] R 1_VaultUpgrade1 | Resources | unlock [Reconquest] SteamCores | `A6B7DCE3504E6494` |
| 76 | 0 | [Reconquest] R 4_VaultInsulation | 3 | [Reconquest] R 3_VaultUpgrade2 | Resources | unlock [Reconquest] SteamCores | `98356E3CE54E08AD` |
| 77 | 0 | [Reconquest] R 6_VaultSteamCoresUpgrade | 5 | [Reconquest] R 5_VaultUpgrade3 | Resources | unlock [Reconquest] SteamCores | `A0606FEF83CB14C8` |

<!-- FIN TABLE TECHNOS -->

## Classification Archipelago des arbres

<!-- CLASSIFICATION ARBRES (generee par outils/technos_classification.py) -->

Table générée par `outils\technos_classification.py` depuis `technos_table.tsv`. **progression** = arbre principal (aucune de ses technos n'a son parent dans un autre arbre), ou forcé (Tier, éclaireurs) ; **useful** = dépend d'un autre arbre (règle du 2026-11-04). Parent de départ et condition d'affichage ne comptent pas.

### A New Home (`DEFAULT`) — 39 arbres : 19 progression, 20 useful

| Arbre | Item | Technos | Classification | Raison | Arbres enfants | Arbres parents |
|---:|---|---:|---|---|---|---|
| 0 | SteamHub | 3 | **progression** | arbre principal |  |  |
| 1 | Generator Upgrade - power 1 | 3 | **progression** | arbre principal | 2 Generator Upgrade - less coal, 4 Generator Overdrive 1 |  |
| 2 | Generator Upgrade - less coal | 2 | useful | depend d'un autre arbre |  | 1 Generator Upgrade - power 1 |
| 3 | Generator Upgrade - range 1 | 3 | **progression** | arbre principal |  |  |
| 4 | Generator Overdrive 1 | 2 | useful | depend d'un autre arbre |  | 1 Generator Upgrade - power 1 |
| 5 | Boilers | 3 | **progression** | arbre principal | 6 Boiler Upgrade - less coal |  |
| 6 | Boiler Upgrade - less coal | 1 | useful | depend d'un autre arbre |  | 5 Boilers |
| 7 | Beacon | 1 | **progression** | force : eclaireurs | 8 ExpeditionCenter, 9 ScoutsLimit |  |
| 8 | ExpeditionCenter | 2 | useful | depend d'un autre arbre |  | 7 Beacon |
| 9 | ScoutsLimit | 2 | useful | depend d'un autre arbre | 10 ScoutsSpeed | 7 Beacon |
| 10 | ScoutsSpeed | 2 | useful | depend d'un autre arbre |  | 9 ScoutsLimit |
| 11 | Factory | 3 | **progression** | arbre principal | 12 Cheaper Augmentation Parts, 13 Automaton Efficiency |  |
| 12 | Cheaper Augmentation Parts | 2 | useful | depend d'un autre arbre |  | 11 Factory |
| 13 | Automaton Efficiency | 3 | useful | depend d'un autre arbre | 14 Automaton Medical, 79 Automaton Engineers | 11 Factory |
| 14 | Automaton Medical | 2 | useful | depend d'un autre arbre |  | 13 Automaton Efficiency |
| 15 | Gathering Upgrade | 3 | **progression** | arbre principal | 16 ResourceDepotUpgrade, 17 Gathering upgrade 2 |  |
| 16 | ResourceDepotUpgrade | 2 | useful | depend d'un autre arbre |  | 15 Gathering Upgrade |
| 17 | Gathering upgrade 2 | 1 | useful | depend d'un autre arbre |  | 15 Gathering Upgrade |
| 18 | CoalThumper | 3 | **progression** | arbre principal |  |  |
| 19 | Coal Mine | 3 | **progression** | arbre principal | 20 Coal mine upgrade 1 |  |
| 20 | Coal mine upgrade 1 | 2 | useful | depend d'un autre arbre |  | 19 Coal Mine |
| 21 | CharcoalKiln | 3 | **progression** | arbre principal |  |  |
| 22 | Sawmill | 2 | **progression** | arbre principal | 23 Sawmill Upgrade |  |
| 23 | Sawmill Upgrade | 2 | useful | depend d'un autre arbre |  | 22 Sawmill |
| 24 | WallDrill | 3 | **progression** | arbre principal |  |  |
| 25 | Steel Mine | 3 | **progression** | arbre principal |  |  |
| 26 | Plantation | 3 | **progression** | arbre principal | 27 Plantation Insulation |  |
| 27 | Plantation Insulation | 2 | useful | depend d'un autre arbre |  | 26 Plantation |
| 28 | HuntersUpgrade | 3 | **progression** | arbre principal | 29 Hunters Upgrade - less workers |  |
| 29 | Hunters Upgrade - less workers | 1 | useful | depend d'un autre arbre |  | 28 HuntersUpgrade |
| 30 | MedPost Upgrade | 3 | **progression** | arbre principal | 31 HealthCare Buildings - warmer, 32 Infirmary Upgrade - less workers |  |
| 31 | HealthCare Buildings - warmer | 2 | useful | depend d'un autre arbre |  | 30 MedPost Upgrade |
| 32 | Infirmary Upgrade - less workers | 1 | useful | depend d'un autre arbre |  | 30 MedPost Upgrade |
| 33 | Bunkhouse | 3 | **progression** | arbre principal | 34 Cantinee - warmer, 35 House Upgrade - cheaper |  |
| 34 | Cantinee - warmer | 1 | useful | depend d'un autre arbre |  | 33 Bunkhouse |
| 35 | House Upgrade - cheaper | 1 | useful | depend d'un autre arbre |  | 33 Bunkhouse |
| 36 | Tier | 5 | **progression** | force : paliers |  |  |
| 79 | Automaton Engineers | 1 | useful | depend d'un autre arbre |  | 13 Automaton Efficiency |
| 80 | Storm_research | 1 | **progression** | arbre principal |  |  |

### The Arks (`TechHaven`) — 35 arbres : 17 progression, 18 useful

| Arbre | Item | Technos | Classification | Raison | Arbres enfants | Arbres parents |
|---:|---|---:|---|---|---|---|
| 0 | SteamHub | 3 | **progression** | arbre principal |  |  |
| 1 | Generator Upgrade - power 1 | 3 | **progression** | arbre principal | 2 Generator Upgrade - less coal, 4 Generator Overdrive 1 |  |
| 2 | Generator Upgrade - less coal | 2 | useful | depend d'un autre arbre |  | 1 Generator Upgrade - power 1 |
| 3 | Generator Upgrade - range 1 | 3 | **progression** | arbre principal |  |  |
| 4 | Generator Overdrive 1 | 2 | useful | depend d'un autre arbre |  | 1 Generator Upgrade - power 1 |
| 5 | Boilers | 3 | **progression** | arbre principal | 6 Boiler Upgrade - less coal |  |
| 6 | Boiler Upgrade - less coal | 1 | useful | depend d'un autre arbre |  | 5 Boilers |
| 7 | Beacon | 2 | **progression** | force : eclaireurs | 9 ScoutsLimit |  |
| 9 | ScoutsLimit | 2 | useful | depend d'un autre arbre | 10 ScoutsSpeed | 7 Beacon |
| 10 | ScoutsSpeed | 2 | useful | depend d'un autre arbre |  | 9 ScoutsLimit |
| 11 | Factory | 3 | **progression** | arbre principal | 12 Cheaper Augmentation Parts, 13 Automaton Efficiency |  |
| 12 | Cheaper Augmentation Parts | 2 | useful | depend d'un autre arbre |  | 11 Factory |
| 13 | Automaton Efficiency | 3 | useful | depend d'un autre arbre | 14 Automaton Medical, 79 Automaton Engineers | 11 Factory |
| 14 | Automaton Medical | 2 | useful | depend d'un autre arbre |  | 13 Automaton Efficiency |
| 15 | Gathering Upgrade | 3 | **progression** | arbre principal | 16 ResourceDepotUpgrade, 17 Gathering upgrade 2 |  |
| 16 | ResourceDepotUpgrade | 2 | useful | depend d'un autre arbre |  | 15 Gathering Upgrade |
| 17 | Gathering upgrade 2 | 1 | useful | depend d'un autre arbre |  | 15 Gathering Upgrade |
| 18 | CoalThumper | 3 | **progression** | arbre principal |  |  |
| 19 | Coal Mine | 3 | **progression** | arbre principal | 20 Coal mine upgrade 1 |  |
| 20 | Coal mine upgrade 1 | 2 | useful | depend d'un autre arbre |  | 19 Coal Mine |
| 21 | CharcoalKiln | 3 | **progression** | arbre principal |  |  |
| 22 | Sawmill | 2 | **progression** | arbre principal | 23 Sawmill Upgrade |  |
| 23 | Sawmill Upgrade | 2 | useful | depend d'un autre arbre |  | 22 Sawmill |
| 24 | WallDrill | 3 | **progression** | arbre principal |  |  |
| 25 | Steel Mine | 3 | **progression** | arbre principal |  |  |
| 26 | Plantation | 3 | **progression** | arbre principal | 27 Plantation Insulation |  |
| 27 | Plantation Insulation | 2 | useful | depend d'un autre arbre |  | 26 Plantation |
| 30 | MedPost Upgrade | 3 | **progression** | arbre principal | 31 HealthCare Buildings - warmer, 32 Infirmary Upgrade - less workers |  |
| 31 | HealthCare Buildings - warmer | 2 | useful | depend d'un autre arbre |  | 30 MedPost Upgrade |
| 32 | Infirmary Upgrade - less workers | 1 | useful | depend d'un autre arbre |  | 30 MedPost Upgrade |
| 33 | Bunkhouse | 3 | **progression** | arbre principal | 34 Cantinee - warmer, 35 House Upgrade - cheaper |  |
| 34 | Cantinee - warmer | 1 | useful | depend d'un autre arbre |  | 33 Bunkhouse |
| 35 | House Upgrade - cheaper | 1 | useful | depend d'un autre arbre |  | 33 Bunkhouse |
| 36 | Tier | 5 | **progression** | force : paliers |  |  |
| 79 | Automaton Engineers | 1 | useful | depend d'un autre arbre |  | 13 Automaton Efficiency |

### Refugees (`REFUGEES`) — 38 arbres : 18 progression, 20 useful

| Arbre | Item | Technos | Classification | Raison | Arbres enfants | Arbres parents |
|---:|---|---:|---|---|---|---|
| 0 | SteamHub | 3 | **progression** | arbre principal |  |  |
| 1 | Generator Upgrade - power 1 | 3 | **progression** | arbre principal | 2 Generator Upgrade - less coal, 4 Generator Overdrive 1 |  |
| 2 | Generator Upgrade - less coal | 2 | useful | depend d'un autre arbre |  | 1 Generator Upgrade - power 1 |
| 3 | Generator Upgrade - range 1 | 3 | **progression** | arbre principal |  |  |
| 4 | Generator Overdrive 1 | 2 | useful | depend d'un autre arbre |  | 1 Generator Upgrade - power 1 |
| 5 | Boilers | 3 | **progression** | arbre principal | 6 Boiler Upgrade - less coal |  |
| 6 | Boiler Upgrade - less coal | 1 | useful | depend d'un autre arbre |  | 5 Boilers |
| 7 | Beacon | 1 | **progression** | force : eclaireurs | 8 ExpeditionCenter, 9 ScoutsLimit |  |
| 8 | ExpeditionCenter | 2 | useful | depend d'un autre arbre |  | 7 Beacon |
| 9 | ScoutsLimit | 2 | useful | depend d'un autre arbre | 10 ScoutsSpeed | 7 Beacon |
| 10 | ScoutsSpeed | 2 | useful | depend d'un autre arbre |  | 9 ScoutsLimit |
| 11 | Factory | 3 | **progression** | arbre principal | 12 Cheaper Augmentation Parts, 13 Automaton Efficiency |  |
| 12 | Cheaper Augmentation Parts | 2 | useful | depend d'un autre arbre |  | 11 Factory |
| 13 | Automaton Efficiency | 3 | useful | depend d'un autre arbre | 14 Automaton Medical, 79 Automaton Engineers | 11 Factory |
| 14 | Automaton Medical | 2 | useful | depend d'un autre arbre |  | 13 Automaton Efficiency |
| 15 | Gathering Upgrade | 3 | **progression** | arbre principal | 16 ResourceDepotUpgrade, 17 Gathering upgrade 2 |  |
| 16 | ResourceDepotUpgrade | 2 | useful | depend d'un autre arbre |  | 15 Gathering Upgrade |
| 17 | Gathering upgrade 2 | 1 | useful | depend d'un autre arbre |  | 15 Gathering Upgrade |
| 18 | CoalThumper | 3 | **progression** | arbre principal |  |  |
| 19 | Coal Mine | 3 | **progression** | arbre principal | 20 Coal mine upgrade 1 |  |
| 20 | Coal mine upgrade 1 | 2 | useful | depend d'un autre arbre |  | 19 Coal Mine |
| 21 | CharcoalKiln | 3 | **progression** | arbre principal |  |  |
| 22 | Sawmill | 2 | **progression** | arbre principal | 23 Sawmill Upgrade |  |
| 23 | Sawmill Upgrade | 2 | useful | depend d'un autre arbre |  | 22 Sawmill |
| 24 | WallDrill | 3 | **progression** | arbre principal |  |  |
| 25 | Steel Mine | 3 | **progression** | arbre principal |  |  |
| 26 | Plantation | 3 | **progression** | arbre principal | 27 Plantation Insulation |  |
| 27 | Plantation Insulation | 2 | useful | depend d'un autre arbre |  | 26 Plantation |
| 28 | HuntersUpgrade | 3 | **progression** | arbre principal | 29 Hunters Upgrade - less workers |  |
| 29 | Hunters Upgrade - less workers | 1 | useful | depend d'un autre arbre |  | 28 HuntersUpgrade |
| 30 | MedPost Upgrade | 3 | **progression** | arbre principal | 31 HealthCare Buildings - warmer, 32 Infirmary Upgrade - less workers |  |
| 31 | HealthCare Buildings - warmer | 2 | useful | depend d'un autre arbre |  | 30 MedPost Upgrade |
| 32 | Infirmary Upgrade - less workers | 1 | useful | depend d'un autre arbre |  | 30 MedPost Upgrade |
| 33 | Bunkhouse | 3 | **progression** | arbre principal | 34 Cantinee - warmer, 35 House Upgrade - cheaper |  |
| 34 | Cantinee - warmer | 1 | useful | depend d'un autre arbre |  | 33 Bunkhouse |
| 35 | House Upgrade - cheaper | 1 | useful | depend d'un autre arbre |  | 33 Bunkhouse |
| 36 | Tier | 5 | **progression** | force : paliers |  |  |
| 79 | Automaton Engineers | 1 | useful | depend d'un autre arbre |  | 13 Automaton Efficiency |

### Fall of Winterhome (`WINTERHOME`) — 40 arbres : 28 progression, 12 useful

| Arbre | Item | Technos | Classification | Raison | Arbres enfants | Arbres parents |
|---:|---|---:|---|---|---|---|
| 0 | Steamhub Upgrade - range | 2 | **progression** | arbre principal |  |  |
| 1 | Generator Upgrade - power 3 | 1 | **progression** | arbre principal |  |  |
| 2 | Generator Upgrade - less coal | 2 | **progression** | arbre principal |  |  |
| 3 | Generator Upgrade - range 3 | 1 | **progression** | arbre principal |  |  |
| 4 | Generator Overdrive 1 | 2 | **progression** | arbre principal |  |  |
| 5 | Boiler Upgrade | 2 | **progression** | arbre principal | 6 Boiler Upgrade - less coal |  |
| 6 | Boiler Upgrade - less coal | 1 | useful | depend d'un autre arbre |  | 5 Boiler Upgrade |
| 8 | ExpeditionUpgradeWinterhome | 1 | **progression** | arbre principal |  |  |
| 9 | ScoutsLimit | 2 | **progression** | arbre principal | 10 ScoutsSpeed |  |
| 10 | ScoutsSpeed | 2 | useful | depend d'un autre arbre |  | 9 ScoutsLimit |
| 11 | Factory | 3 | **progression** | arbre principal | 12 Cheaper Augmentation Parts, 13 Automaton Efficiency |  |
| 12 | Cheaper Augmentation Parts | 2 | useful | depend d'un autre arbre |  | 11 Factory |
| 13 | Automaton Efficiency | 3 | useful | depend d'un autre arbre | 14 Automaton Medical, 79 Automaton Engineers | 11 Factory |
| 14 | Automaton Medical | 2 | useful | depend d'un autre arbre |  | 13 Automaton Efficiency |
| 15 | Gathering Upgrade | 3 | **progression** | arbre principal | 16 ResourceDepotUpgrade, 17 Gathering upgrade 2 |  |
| 16 | ResourceDepotUpgrade | 2 | useful | depend d'un autre arbre |  | 15 Gathering Upgrade |
| 17 | Gathering upgrade 2 | 1 | useful | depend d'un autre arbre |  | 15 Gathering Upgrade |
| 18 | SteamCoalThumper | 2 | **progression** | arbre principal |  |  |
| 19 | Steam Coal Mine | 2 | **progression** | arbre principal |  |  |
| 20 | Coal mine upgrade 1 | 2 | **progression** | arbre principal |  |  |
| 21 | Kiln upgrade | 2 | **progression** | arbre principal |  |  |
| 22 | Steam Sawmill | 1 | **progression** | arbre principal |  |  |
| 23 | Sawmill Upgrade | 2 | **progression** | arbre principal |  |  |
| 24 | Steam WallDrill | 2 | **progression** | arbre principal |  |  |
| 25 | Steam Steel Mine | 2 | **progression** | arbre principal |  |  |
| 26 | Steam Plantation | 2 | **progression** | arbre principal |  |  |
| 27 | Plantation Insulation | 2 | **progression** | arbre principal |  |  |
| 28 | Flying Hunters | 2 | **progression** | arbre principal |  |  |
| 29 | Hunters Upgrade - less workers | 1 | **progression** | arbre principal |  |  |
| 30 | MedPost Upgrade | 3 | **progression** | arbre principal | 31 HealthCare Buildings - warmer, 32 Infirmary Upgrade - less workers |  |
| 31 | HealthCare Buildings - warmer | 2 | useful | depend d'un autre arbre |  | 30 MedPost Upgrade |
| 32 | Infirmary Upgrade - less workers | 1 | useful | depend d'un autre arbre |  | 30 MedPost Upgrade |
| 33 | House Insulation | 1 | **progression** | arbre principal |  |  |
| 34 | Cantinee - warmer | 1 | **progression** | arbre principal |  |  |
| 35 | House Upgrade - cheaper | 1 | **progression** | arbre principal |  |  |
| 36 | Tier | 2 | **progression** | force : paliers |  |  |
| 37 | E RepairStation | 4 | **progression** | arbre principal | 38 E RC Ability, 39 E RC Less workers |  |
| 38 | E RC Ability | 2 | useful | depend d'un autre arbre |  | 37 E RepairStation |
| 39 | E RC Less workers | 1 | useful | depend d'un autre arbre |  | 37 E RepairStation |
| 79 | Automaton Engineers | 1 | useful | depend d'un autre arbre |  | 13 Automaton Efficiency |

### The Last Autumn (`BUILDERS`) — 32 arbres : 16 progression, 16 useful

| Arbre | Item | Technos | Classification | Raison | Arbres enfants | Arbres parents |
|---:|---|---:|---|---|---|---|
| 36 | Tier | 5 | **progression** | force : paliers |  |  |
| 40 | HC Brazier | 2 | **progression** | arbre principal | 41 HC BrazierRange1, 42 HC BrazierCoalOpt1 |  |
| 41 | HC BrazierRange1 | 1 | useful | depend d'un autre arbre |  | 40 HC Brazier |
| 42 | HC BrazierCoalOpt1 | 2 | useful | depend d'un autre arbre |  | 40 HC Brazier |
| 43 | HH Heater | 3 | **progression** | arbre principal |  |  |
| 44 | HI HealthCare Insulation1 | 2 | **progression** | arbre principal |  |  |
| 45 | GS ScaffoldingManufacture | 3 | **progression** | arbre principal | 46 GS ProtectiveStructure1 |  |
| 46 | GS ProtectiveStructure1 | 2 | useful | depend d'un autre arbre |  | 45 GS ScaffoldingManufacture |
| 47 | GM SteelPlateForge | 2 | **progression** | arbre principal | 48 GM ForgeCheaperMaterials |  |
| 48 | GM ForgeCheaperMaterials | 1 | useful | depend d'un autre arbre |  | 47 GM SteelPlateForge |
| 49 | GG SteamMill | 3 | **progression** | arbre principal | 50 GG Supercharge1 |  |
| 50 | GG Supercharge1 | 2 | useful | depend d'un autre arbre |  | 49 GG SteamMill |
| 51 | GF Air Scrubbers1 | 3 | **progression** | arbre principal | 52 GF Air ScrubbersOptimalization |  |
| 52 | GF Air ScrubbersOptimalization | 2 | useful | depend d'un autre arbre |  | 51 GF Air Scrubbers1 |
| 53 | GU GeneratorUpgrade1 | 3 | **progression** | arbre principal |  |  |
| 54 | FF Foragers | 1 | **progression** | force : eclaireurs | 55 FF Foragers_+1, 56 FF ForagersSpeedUpgrade |  |
| 55 | FF Foragers_+1 | 2 | useful | depend d'un autre arbre |  | 54 FF Foragers |
| 56 | FF ForagersSpeedUpgrade | 2 | useful | depend d'un autre arbre |  | 54 FF Foragers |
| 57 | FH Fishing Harbor | 2 | **progression** | arbre principal | 58 FH Fishing Harbor_efficiencyBoost |  |
| 58 | FH Fishing Harbor_efficiencyBoost | 1 | useful | depend d'un autre arbre |  | 57 FH Fishing Harbor |
| 59 | FM MedPost Upgrade | 3 | **progression** | arbre principal | 60 FM Infirmary Upgrade |  |
| 60 | FM Infirmary Upgrade | 1 | useful | depend d'un autre arbre |  | 59 FM MedPost Upgrade |
| 61 | RD Docksv1 | 3 | **progression** | arbre principal | 62 RU URS, 64 RD Docks faster reload |  |
| 62 | RU URS | 1 | useful | depend d'un autre arbre | 63 RU URS_Steam | 61 RD Docksv1 |
| 63 | RU URS_Steam | 2 | useful | depend d'un autre arbre | 81 RU URS_Advanced | 62 RU URS |
| 64 | RD Docks faster reload | 1 | useful | depend d'un autre arbre |  | 61 RD Docksv1 |
| 65 | RG Gathering Upgrade | 2 | **progression** | arbre principal | 66 RG ResourceDepotUpgrade |  |
| 66 | RG ResourceDepotUpgrade | 2 | useful | depend d'un autre arbre |  | 65 RG Gathering Upgrade |
| 67 | RW Sawmill | 2 | **progression** | arbre principal | 68 RW Sawmill Upgrade |  |
| 68 | RW Sawmill Upgrade | 2 | useful | depend d'un autre arbre |  | 67 RW Sawmill |
| 69 | RC CharcoalKiln | 3 | **progression** | arbre principal |  |  |
| 81 | RU URS_Advanced | 1 | useful | depend d'un autre arbre |  | 63 RU URS_Steam |

### On the Edge (`Reconquest`) — 30 arbres : 14 progression, 16 useful

| Arbre | Item | Technos | Classification | Raison | Arbres enfants | Arbres parents |
|---:|---|---:|---|---|---|---|
| 5 | Boilers | 3 | **progression** | arbre principal | 6 Boiler Upgrade - less coal |  |
| 6 | Boiler Upgrade - less coal | 1 | useful | depend d'un autre arbre |  | 5 Boilers |
| 9 | ScoutsLimit | 2 | **progression** | arbre principal | 10 ScoutsSpeed |  |
| 10 | ScoutsSpeed | 2 | useful | depend d'un autre arbre |  | 9 ScoutsLimit |
| 11 | Factory | 1 | **progression** | arbre principal | 12 Cheaper Augmentation Parts |  |
| 12 | Cheaper Augmentation Parts | 2 | useful | depend d'un autre arbre |  | 11 Factory |
| 15 | Gathering Upgrade | 3 | **progression** | arbre principal | 16 ResourceDepotUpgrade, 17 Gathering upgrade 2 |  |
| 16 | ResourceDepotUpgrade | 2 | useful | depend d'un autre arbre |  | 15 Gathering Upgrade |
| 17 | Gathering upgrade 2 | 1 | useful | depend d'un autre arbre |  | 15 Gathering Upgrade |
| 18 | CoalThumper | 3 | **progression** | arbre principal |  |  |
| 19 | Coal Mine | 3 | **progression** | arbre principal | 20 Coal mine upgrade 1 |  |
| 20 | Coal mine upgrade 1 | 2 | useful | depend d'un autre arbre |  | 19 Coal Mine |
| 21 | CharcoalKiln | 3 | **progression** | arbre principal |  |  |
| 22 | Sawmill | 2 | **progression** | arbre principal | 23 Sawmill Upgrade |  |
| 23 | Sawmill Upgrade | 2 | useful | depend d'un autre arbre |  | 22 Sawmill |
| 30 | MedPost Upgrade | 3 | **progression** | arbre principal | 31 HealthCare Buildings - warmer, 32 Infirmary Upgrade - less workers |  |
| 31 | HealthCare Buildings - warmer | 2 | useful | depend d'un autre arbre |  | 30 MedPost Upgrade |
| 32 | Infirmary Upgrade - less workers | 1 | useful | depend d'un autre arbre |  | 30 MedPost Upgrade |
| 33 | Bunkhouse | 3 | **progression** | arbre principal | 34 Cantinee - warmer, 35 House Upgrade - cheaper |  |
| 34 | Cantinee - warmer | 1 | useful | depend d'un autre arbre |  | 33 Bunkhouse |
| 35 | House Upgrade - cheaper | 1 | useful | depend d'un autre arbre |  | 33 Bunkhouse |
| 36 | Tier | 5 | **progression** | force : paliers |  |  |
| 70 | HC Brazier | 2 | **progression** | arbre principal | 71 HC BrazierRange, 72 HC BrazierCoalOpt1 |  |
| 71 | HC BrazierRange | 1 | useful | depend d'un autre arbre |  | 70 HC Brazier |
| 72 | HC BrazierCoalOpt1 | 2 | useful | depend d'un autre arbre |  | 70 HC Brazier |
| 73 | R VaultEfficiencyBoost | 1 | **progression** | arbre principal |  |  |
| 74 | R VaultUpgrade1 | 3 | **progression** | arbre principal | 75 R VaultMultitasking, 76 R VaultInsulation, 77 R VaultSteamCoresUpgrade |  |
| 75 | R VaultMultitasking | 1 | useful | depend d'un autre arbre |  | 74 R VaultUpgrade1 |
| 76 | R VaultInsulation | 1 | useful | depend d'un autre arbre |  | 74 R VaultUpgrade1 |
| 77 | R VaultSteamCoresUpgrade | 1 | useful | depend d'un autre arbre |  | 74 R VaultUpgrade1 |

<!-- FIN CLASSIFICATION ARBRES -->

### 2026-11-04 — Techno reçue sans recherche : effets non appliqués (ScoutsLimit)

- **Problème (test grandeur nature)** : `Progressive ScoutsLimit` reçu,
  ScoutsLimit pas recherchée → pas de 2e équipe d'éclaireurs. L'auteur : la
  **réception seule** doit suffire.
- **Cause (Ghidra)** : `ResearchComplete` (`FUN_141327120`) remet l'état à 0
  puis appelle **`AddResearchedTechnology` (`FUN_141325e40`(système, techno,
  1))** : si l'état n'est pas déjà 2, état = 2, effets de la techno
  (`FUN_141326650`) puis écouteurs de « techno recherchée » (système
  `+0x2108`, compteur `+0x2120`). Le mod écrivait seulement l'état 2 à la
  réception : les bâtiments (lus sur l'état) se débloquaient, mais pas les
  modificateurs (équipes d'éclaireurs, améliorations...). D'où le « double
  standard ».
- **Correctif (compilé, déployé 23:21, non testé)** : `OnItemReceived`
  (thread du jeu) → `TechAppliquerEffets` (hooks_research.cpp) appelle
  `AddResearchedTechnology` pour une techno pas encore recherchée ; déjà
  recherchée, ses effets sont venus de la recherche, seul l'état 2 est posé.
  Hook de `AddResearchedTechnology` : pendant une `ResearchComplete` d'une
  techno **déjà reçue**, rien n'est réappliqué (pas de double effet). Journal
  `[AP] techno … recue : effets appliques`.
- Limites : une partie existante où la techno a déjà été écrite à l'état 2
  sans effets n'est pas rattrapée (nouvelle partie ou nouvelle réception
  nécessaires). Recherche **sans** item : comportement inchangé (le jeu
  applique les effets à la recherche, l'état revient à 0).

### 2026-11-06 — Descriptions vanilla « passé un certain point »

- **Constat (auteur)** : Generator efficiency upgrade, Improved Heater 2…
  gardaient leur description vanilla. `ApDescBuffer` (hooks_loc.cpp) : un
  tampon par **clé** de texte, 256 au plus pour toute la session (toutes
  parties et scénarios) ; plein → `nullptr` → texte vanilla. Porté à 2048,
  journal `[TECHDESC] pool des descriptions plein` s'il se remplit encore.
  (hooks_loc.cpp est en Windows-1252 : modifié en conservant l'encodage.)

### 2026-11-06 — Menu des bâtiments en bleu à la réception d'une techno

- **Constat (auteur)** : à la réception d'une techno, l'icône du menu des
  bâtiments et sa catégorie passent au bleu (« nouveau »), sans nouveau
  bâtiment. Vient de `TechAppliquerEffets` (`AddResearchedTechnology`, qui
  prévient les écouteurs « techno recherchée » avec son 3e argument, 1 pour
  une recherche).
- **Essai (compilé, déployé 20:42, non testé)** : 3e argument à **0** à la
  réception (notification présumée coupée). À vérifier : plus de bleu, et les
  effets toujours appliqués (ex. ScoutsLimit).
- **2026-11-07 (auteur)** : à 0, plus aucun bleu. Voulu : bleu à la
  **réception**, pas à la recherche. → réception : 3e argument 1 ;
  `ResearchComplete` d'une techno AP pas encore reçue : 0 (hook
  `hkAjouterRecherchee`). Compilé, déployé 09:34, non testé.
