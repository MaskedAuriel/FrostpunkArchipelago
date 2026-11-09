# Groupes AP : creation, livraison, dilemme d'arrivee, inventaire

> Historique detaille, copie mot pour mot depuis CLAUDE.md le 2026-11-02.
> Le resume a jour est dans CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

- **Livraison des items par groupe de réfugiés** (demandé le 2026-10-28) :
  prendre le contrôle des groupes de survivants qui marchent vers la ville
  (scénario Refugees, entre autres) pour y faire **arriver** les items AP
  (citoyens surtout : trop de citoyens d'emblée déséquilibre un scénario),
  au lieu d'un crédit instantané. Étape 1 en cours : diagnostic **F9**
  (`DiagSystemesGroupes`, mod_common) sur `SurvivorGroup 0x6D`,
  `Settlement 0x6C`, `Outpost 0x6A`, `OutpostTransport 0x6B`,
  `ExpeditionHost 0x69`, `Expedition 0x68`, `AbsentCitizen 0xB7`,
  `WorldMapMarker 0x61`. Reste à trouver dans Ghidra la fonction qui
  **crée** un groupe (événement « des réfugiés approchent »).
  - **Exigences de l'auteur** : livraison possible pour les **citoyens ET
    les ressources** (les technos peut-être) ; **le joueur choisit**, par
    catégorie, entre arrivée immédiate et arrivée par groupe ; le groupe
    doit être **créable dans n'importe quel scénario, à n'importe quel
    moment** (pas seulement Refugees).
  - Conception visée : le groupe sert de **véhicule**. On mémorise les items
    qu'il porte ; à son **arrivée**, on applique les `Receive*` immédiats
    (ressources, technos), et pour les citoyens soit la charge native du
    groupe, soit un ajout direct. Il faut donc : (a) créer un groupe vers la
    ville, (b) détecter son arrivée, (c) neutraliser sa charge native quand
    elle ne sert pas.
  - **F9 du 2026-10-28** (Refugees, 2 groupes visibles) :
    `FrostpunkSurvivorGroupSystem` (`0x6D`) → liste `+0x2078` = **2 paires**
    `{CompoundEntity, FrostpunkSurvivorGroupComponent}`. Ce ne sont **pas**
    des expéditions (liste Expedition vide). Composant de **0xA0** octets
    (contigus) : `+0x18` entité, `+0x20`
    `FrostpunkSurvivorGroupComponentTemplate`, `+0x28 = 2` (état ?),
    `+0x30 = {1,1}`, `+0x48` et `+0x80` pointeurs sans RTTI, `+0x78` float
    0.0001 + 1. Entité nommée **`RefugeeScenarioSurvivors01`**, son
    `CompoundTemplate` à `+0x28` : gabarit **propre au scénario** — d'où la
    difficulté pour les autres scénarios. Settlement / Outpost /
    OutpostTransport / AbsentCitizen vides. F9 dumpe maintenant aussi le
    template du groupe et les **propriétés nommées** (réflexion) des deux
    classes.
  - **`FrostpunkSurvivorGroupComponentTemplate`** (réflexion, 2026-10-28) :
    `+0x028` **Survivors** (tableau dyn. d'objets intégrés : ptr, `+0x30`
    compte = 1 dans Refugees), `+0x040` Base movement speed (objet intégré ;
    dword `0x447DC000` = 1015.0), `+0x048` Initialization on world map
    unlock, `+0x050` Display name (`@Expeditions/SurvivorPanelHeader/
    [RefugeesScenario]OurPeople`), `+0x060` Description, `+0x070` Icon,
    `+0x0A0` Image, `+0x0D0` SFX, `+0x0E0` Agents from origin text
    (`@Expeditions/SurvivorGroupName`), `+0x0E8` Agents without origin text,
    `+0x0F0` Hint text, `+0x0F8` **City reached dilemma** (lien d'entrée :
    dilemme à l'arrivée), `+0x108` Can be escorted, `+0x10C` **Starting
    state** (énumération), `+0x110` Death rate. Le template lui-même est
    propre au scénario (textes `[RefugeesScenario]`).
  - La fiche de réflexion de `FrostpunkSurvivorGroupComponent` (le
    composant, pas le template) ne liste **aucune** propriété : ses champs
    d'exécution (état `+0x28`…) restent à nommer.
  - Recherche de fiche (`FindClassRecord`) : 81 s pour le template — à ne
    pas appeler en jeu hors diagnostic.
  - Classes RTTI « Survivor » (Ghidra, 2026-10-28) : `SurvivorGroupComponent`
    / `…Template` / `…System`, **`MakeSurvivorGroupKnownEffect`**,
    `SurvivorGroupReachedCityEventTrigger`, `SurvivorGroupsSelector`,
    `SurvivorsEntry`, `RandomizedSurvivorsEntry`,
    `ScenarioRandomizedSurvivorsEntry` / `…Modification`, `RescuedSurvivor`,
    `RescuedSurvivorsSelector`, `RescueSurvivorsQuestObjective`,
    `CitizenSurvivorCountAccumulator`, `ExpeditionSiteSurvivorsTextParameter`,
    `UISurvivorGroupSelection`. **Aucune classe de création** : en vanilla,
    les groupes sont **pré-posés** sur la carte, cachés, et un effet les rend
    **connus**. Pour un groupe dans n'importe quel scénario, il faudra
    **instancier l'entité nous-mêmes** (piste : le chemin de création des
    expéditions, créées en cours de partie).
  - Ghidra n'a pas étiqueté les `vftable` de ces classes →
    `VtablesParNomDeClasse` (mod_common) les retrouve par RTTI (chaîne →
    TypeDescriptor → COL → vtable). F9 affiche, pour 6 classes (dont
    `FrostpunkExpeditionComponent`), vtable, méthodes et constructeurs, avec
    les adresses Ghidra.
  - **Résultat F9 (2026-10-28)** — vtables et fonctions qui les chargent :

    | Classe | vtable (Ghidra) | fonction(s) qui la chargent |
    |---|---|---|
    | `SurvivorGroupComponent` | `0x1421490F8` | `0x141A0EB70` (0x93) |
    | `MakeSurvivorGroupKnownEffect` | `0x1420F7898` | `0x141928160` (2 lea), `0x14192E9B0` |
    | `SurvivorGroupReachedCityEventTrigger` | `0x141F34F60` | `0x14140D310` |
    | `SurvivorGroupSystem` | `0x1420C2E68` | `0x1418AFA20` |
    | `ExpeditionComponent` | `0x142137D48` | `0x1419E9560` (0x1FF) |
    | `SurvivorsEntry` | aucune (struct sans vtable) | — |

    Les entrées `[1,2,4..7,9..11]` sont **communes** à toutes ces classes
    (base de réflexion) : seules `[0]`, `[3]`, `[8]` sont propres. Les
    méthodes de gameplay (exécution de l'effet…) sont donc au-delà de `[11]`.
    Le groupe est une `CompoundEntity` instanciée depuis un `CompoundTemplate`
    (`entité +0x28`, chemin `@Generated/Templates/GameEntities/WorldMap/
    SurvivorGroups/…`), comme probablement les expéditions : la création
    générique « template → entité » est la cible.
  - Décompilés des constructeurs (2026-10-28) : `0x1419E9560`
    (ExpeditionComponent, ≥ 0x20D octets) et `0x141A0EB70`
    (SurvivorGroupComponent, 0xA0) ne font qu'initialiser des champs à 0
    après le constructeur de base `FUN_140e19e90(this, -1, 0, 0)`. Groupe :
    `+0x28` dword = 0 à la construction (2 en partie : posé ailleurs),
    `+0x38` = `DAT_142c2fca8`, `+0x68` 16 octets = `DAT_142a12558`, `+0x78`
    float 1.0, `+0x7C` = 1. La vtable de `MakeSurvivorGroupKnownEffect` a
    29 entrées : propres à la classe seulement `[0]`, `[3]` `0x141932210`,
    `[8]` `0x1419317F0`, `[28]` `0x140D9C650` ; `[18..27]` sont des
    `_guard_check_icall` (méthodes vides).
  - **Les classes de réflexion n'ont pas de code de gameplay** (2026-10-28) :
    `[8]` renvoie le nom de la classe, `[3]` sa fiche de réflexion
    (`DAT_14409e8a0` pour l'effet). `FUN_141928160` est la **fabrique** de
    l'effet (`this == 0` → `MoteurAlloc(0x18)` puis construction, sinon
    construction sur place) : l'effet ne fait que 0x18 octets, aucun champ.
    Son exécution est ailleurs (système qui reconnaît la classe).
    Appelants des constructeurs : composant d'expédition `0x1419E94CA`
    (`CALL`) / `0x1419E94EB` (`JMP`) = sa fabrique, et `0x1419E9D72` ;
    groupe `0x141A0E7BA` / `0x141A0E7DB` = sa fabrique, et `0x141A0F062`
    (dans la méthode `[0]`, probablement un clonage).
    → **Traceur** `InstallTraceCreationEntites` (mod_common, posé au
    démarrage) : hooks des deux constructeurs, balayage de pile des
    12 premières constructions (adresses de retour précédées d'un `call`),
    log `[TRACE]` avec adresses Ghidra. But : la fonction générique
    « template → entité », appelée quand on envoie des éclaireurs.
  - **Résultat du traceur (2026-10-28)** (adresses de retour Ghidra, de
    l'appelé vers l'appelant) :
    - Démarrage : 1 construction de chaque sur la **pile** (prototypes de
      réflexion), sans intérêt.
    - Chargement d'une sauvegarde Refugees, 2 groupes :
      `0x141A0F067` (FUN_141a0f020, création du composant par son
      template ?) ← `0x140E36565` (FUN_140e36460) ← `0x1410439D1` ←
      `0x1410E5B1F` ← `0x140E24106` ← `0x1410F0330` … (chargement de scène).
    - **Création d'une équipe d'éclaireurs en partie** (suivie d'un
      `wood -40`, catégorie 4 = coût) : `0x1419E9D77` (FUN_1419e9d30) ←
      `0x141170F03` (FUN_141170ef9, fragment) ← `0x141172462`
      (FUN_141171cf0) ← `0x1410EBB01` (FUN_1410ebab0) ← `0x1411F00F6`
      (FUN_1411f00e0) ← `0x141017F68` (FUN_141017f38) ← `0x141016413`
      (FUN_141016250) ← `0x1418A3021` (FUN_1418a2f20, zone des systèmes de
      gameplay) ← `0x1410584FF` … (boucle de mise à jour).
      La fonction générique « template → entité » est dans cette chaîne,
      entre `FUN_1418a2f20` (appelant de gameplay) et `FUN_1419e9d30`.
  - **`FUN_1418a2f20` décompilé (2026-10-28) = « créer une équipe
    d'éclaireurs »** `(hôte, bâtiment, template, coût, forcer, p6)` :
    - `forcer == 0` → `FUN_1418a2850()` (peut-on payer ?) sinon `return 0` ;
    - **création** : `entité = FUN_141016250(*(base+0x2B2DE98), template, 0,
      &transform)` — `transform` = 64 octets : 3 lignes de rotation copiées de
      `DAT_1429292d0` (48 octets, identité présumée) + position (vec4)
      calculée depuis le bâtiment (`bâtiment +0x140` = x, `+0x148` = z) ;
    - **composant d'une entité par type** : `*(entité + 0x2A0 + idType × 8)`,
      `idType = FUN_14148c0a0()` ;
    - `FUN_1418a49b0(hôte, entité, bâtiment)` : rattachement de l'expédition ;
    - coût : tableau d'entrées de 0x14 (de `coût +0x20` si `coût +0x18`,
      sinon `template du composant +0x40`) débité par
      `FUN_141740440(économie, &tableau, -1, bâtiment)` (le `wood -40`).
    → **`FUN_141016250` est la fonction de création générique** à appeler
    pour un groupe, avec le `CompoundTemplate` du groupe.
  - **`FUN_141016250` décompilé (2026-10-28)** — `entitymanager.cpp`,
    **5 paramètres** : `Creer(gestionnaire, template, parent, &transform,
    drapeaux)` (l'équipe d'éclaireurs passe `parent = 0`, `drapeaux = 0`) :
    - limite `DAT_142b2de10 < 0xFFFF` entités vivantes (assert sinon) ;
    - `entité = template->vtable[0x158](template, gestionnaire)` ;
    - `gestionnaire +0x178` += 1 ; `template +0xE1` → drapeau `0x40000000000` ;
    - `entité +0x24C` (calque/scène) = celui du `parent`, ou de
      `FUN_141017490(gestionnaire)` si `parent == 0` ;
    - `entité->vtable[0x160](entité, drapeaux)` (init) ;
    - si `!(drapeaux & 0x20)` : `FUN_1410ee9d0(entité, &transform)`
      (pose la transformation) ;
    - `FUN_1410eceb0(entité, parent, 0, 1)` (rattachement au parent) ;
    - `FUN_141018240(gestionnaire + 0x48, entité)` (enregistrement) ;
    - si `!(drapeaux & 4)` : `FUN_141017dd0(gestionnaire, entité)`
      (activation dans les systèmes).
  - **`FUN_1418a49b0` décompilé** = rattacher l'expédition à son bâtiment :
    `composant +0x40` = **poignée** vers le bâtiment (`FUN_140e1a120(obj)`
    = obtenir la poignée, `FUN_140e1a110` = +1 réf., `FUN_140e1a260` =
    −1 réf. ; `*poignée = objet`), liste des expéditions de l'hôte
    (`FUN_1418a3470(hôte) + 0x60`, compte `+0x68`), puis événement de
    l'hôte (écouteurs `+0x2228`, compte `+0x2230`, compteur `+0x2240`).
    Rien de propre aux groupes.
  - **Entité** (`entity.cpp`, décompilés `FUN_1410ee9d0` = poser la
    transformation, `FUN_1410eceb0` = `SetParent`) : `+0x40` **parent**
    (le `SceneEntity` vu dans F9), `+0xD0` transformation **locale** 4×4
    (64 octets, position à `+0x100`), `+0x110` transformation **globale**,
    `+0x210` drapeau remis à 0 à chaque pose, `+0x248` octet de calque,
    `+0x24C` scène, `+0x278` (`[0x4f]`) gestionnaire.
    `FUN_141017a40(gestionnaire, scène)` = racine de la scène.
  - **Test F3 (`groupes.cpp`, 2026-10-28 ; pas F10 = menu des options du jeu)** : copie du premier groupe
    existant — `Creer(gestionnaire, entité +0x28, entité +0x40,
    transformation décalée de +20 en x et z, 0)`, sur le **thread du jeu**
    (`GroupesPompe`, appelée par le hook du setter de ressources à côté de
    `PompeNotifications`), sous SEH. Log `[GRP]` : classes du template,
    du parent et du gestionnaire, position, nombre de groupes avant/après,
    état `+0x28` de chaque composant.
  - **VALIDÉ le 2026-10-28 (Refugees)** : F3 crée une **copie conforme**
    d'un groupe (emplacement, nom, contenu), 2 → 3 → 4 groupes dans le
    système, état 2 comme les originaux, aucun plantage. Gestionnaire =
    `GameEntityManager`. Coordonnées de la carte du monde de l'ordre du
    millier (groupe vers x = −1675, z = −546) : le décalage de +20 est
    invisible. Les groupes **marchent** (x −1675 → −1672 entre deux appuis).
  - Itération suivante de F3 : relève la chaîne de parents et la scène
    (`+0x24C`) du groupe, d'un **site** et de la **ville** (le parent du
    site est-il celui du groupe ?), et, dans un scénario **sans** groupe,
    pose le template mémorisé (`g_tplMemo`, vu plus tôt dans la session)
    sur le premier site, avec le parent du site. But : savoir si le
    template survit au changement de scénario et si la scène de la carte
    est commune.
  - **Décision de l'auteur (2026-10-28)** : peu importe que le template
    survive à un changement de scénario ; il faut **le trouver où qu'on
    soit**, y compris dans un scénario qui n'a jamais de groupe. Et pour
    tester l'arrivée, il faut pouvoir **régler le temps de trajet restant**
    (cible : 30 minutes de jeu).
    → `InstallCaptureTemplatesGroupes` (groupes.cpp, au démarrage) : vtable
    de `FrostpunkSurvivorGroupComponentTemplate` par RTTI, fonctions qui la
    chargent (`FonctionsChargeantVtable`, mod_common), hook de chacune
    (≤ 4), capture des objets de la bonne classe + pile des 3 premiers
    (`[CAPGRP]`, `[TRACE]`). Templates de composant **contigus au pas de
    0x140**.
    → **F9 = `DiagGroupes`** (l'ancien `DiagSystemesGroupes` n'est plus
    lié à une touche) : templates capturés (nom, chemin `@Generated/…`,
    vitesse `+0x40`, nombre de survivants `+0x30`, état de départ
    `+0x10C`), dump du `CompoundTemplate` du groupe (où sont rangés ses
    templates de composant ?), puis **instantanés** du premier groupe
    (composant, objets `+0x48` et `+0x80`, transformation) avec les dwords
    modifiés depuis l'appui précédent (`[DIFF]`).
  - **Résultats (2026-10-28)** :
    - Fonction capturée : `0x141A0EC10` (seule à charger la vtable du
      template). Autre scénario lancé d'abord : **0 template de groupe**.
      Au chargement de Refugees : 2 construits (vitesses 825 et 1015,
      1 entrée de survivants, état de départ 1). **Les templates de groupe
      ne sont chargés qu'avec leur scénario** → il faut provoquer leur
      chargement nous-mêmes.
    - Ils sont construits sur un **thread de chargement** (≠ thread du
      jeu), pile : `0x141A0F0BE` (FUN_141a0f080, fabrique) ←
      `0x140E36565` (FUN_140e36460) ← `0x141043D57` (FUN_141043c20) ←
      `0x141CA8B52` / `0x141CA86F6` (FUN_141ca8b32 / FUN_141ca7260,
      désérialisation récursive) ← `0x140E5392B` (FUN_140e53830) ←
      `0x140E1CB85` (FUN_140e1cb30) ← `0x140E252C2` (FUN_140e24fe0) ←
      `0x140E0AFB6` (FUN_140e0af40) ← … (boucle du thread de chargement).
    - `CompoundTemplate` : `+0x20` **chemin de ressource**
      `templates/GameEntities/WorldMap/SurvivorGroups/Refugees/Refugee…`,
      `+0x38` → `ResourceTemplatePool` (le gestionnaire de ces ressources).
    - Marche (2 F9 à 1–2 h de jeu d'écart) : position `x −366.8 → −291.9`,
      `z −3.75 → −3.38` (≈ 75 unités ; la ville semble à l'origine) ;
      composant `+0x38` (qword) `0x3_9DDE3BC0 → 0x6_0CCD5390` (horodatage ?) ;
      `comp+0x48` = tampon de texte sans rapport, `comp+0x80` inchangé.
    - Heures notées de mémoire par l'auteur : ~15h21 puis ~13h02 (le
      lendemain ?), soit ~21,7 h pour 75 unités : **non fiable**.
  - **Décompilés de la pile du chargeur : faux appelants.** `FUN_140e53830`
    = décompression zlib (`decompressioncontext.cpp`), `FUN_141ca7260` =
    `inflate`, `FUN_140e24fe0` = lecture de flux de fichier
    (`filesysteminputstream.cpp`), `FUN_140e1cb30` = copie de chaîne. Le
    balayage brut de pile ramasse des **adresses de retour périmées**
    (appels terminés plus profonds). Seuls les 3 premiers cadres étaient
    sûrs : `FUN_141a0f080` ← `FUN_140e36460` ← `FUN_141043c20`.
    → `TracerPile` déroule maintenant la pile par `.pdata`
    (`RtlCaptureStackBackTrace`, lignes `cadre N`) ; balayage brut en
    repli seulement.
  - **F8 = rapprocher le premier groupe** (`RapprocherPremierGroupe`,
    thread du jeu) : replace le groupe à 10 % de sa distance à la ville
    (position de l'entité de `FrostpunkCityComponent`) par le poseur de
    transformation du moteur `FUN_1410ee9d0` (`+0x10EE9D0`). Si le groupe
    revient en arrière, sa progression est stockée ailleurs. L'ancien F8
    (bilan des captures, validé) est retiré.
  - **Résultat F8 / F9 (2026-10-28)** : la pose de position **ne tient
    pas** (106,4 visé, puis −346,9 → −308,2 au F9 suivant) : la position
    est **calculée** à chaque tick. Ville (entité de
    `FrostpunkCityComponent`) en `156,7 / 27,1 / −1,2` sur la carte.
    Relevé de l'auteur : 21h58 → 23h05 (67 min), restant affiché
    14h39 → 13h31. **Temps restant = distance à la ville / (vitesse du
    template / 24)** : 503,1 / (825 / 24) = 14,6 h. Vitesse = unités par
    jour de jeu. Seul champ variable du composant : **qword `+0x38`**
    (+6,80e9 pour 38,7 unités = 1,126 h → ~6,04e9 par heure de jeu),
    supposé être le temps de marche écoulé.
    → F8 v2 : avance `+0x38` de `(restant − 0,5 h) × 6,04e9` (log
    `[GRP] compteur +0x38`).
  - **Pile exacte du chargement des templates** (déroulage, thread de
    chargement) : `FUN_141a0f080` (fabrique) ← `FUN_140e36460` ←
    `FUN_141043c20` ← `FUN_140e3ec2b` ← `FUN_140e3eb40` ← `FUN_140e3e7c0`
    ← `FUN_140e38280` (méthode `[7]` commune de réflexion) ←
    `FUN_140e38170` ← **`FUN_14112e3c0`** ← **`FUN_1411d0940`** ←
    **`FUN_141037da0`** ← **`FUN_141038960`** ← `FUN_140e23e30` ←
    `FUN_140e23b00` (entrée du thread ?). Les 4 en gras sont les candidats
    « charger la ressource ».
  - **Décompilés du chargeur (2026-10-28)** :
    - `FUN_141038960(gestionnaire, p2)` = **traitement des requêtes de
      chargement** : deux files de requêtes (`+0x1F8`/compte `+0x200`,
      `+0x210`/compte `+0x218`), chaque requête : `+0x08` nom (chemin),
      `+0x14` état (2 = chargée), `vtable[0]` = type (index dans la table
      `PTR_s_Texture_141e366c0`), `vtable[+0x28]` = charger. Appelle
      `FUN_141037da0(gestionnaire, requête)` (« Loading {0} resource: {1} »,
      puis état 2). Verrou `gestionnaire +0x180`.
    - `FUN_14112e3c0(res)` = lecture binaire d'un template
      (`entitytemplateserialization.cpp`) : `res +0x08` = chemin (ou `#…`
      pour le lecteur optimisé `DAT_142b2e6c0`), fichier ouvert comme
      « BinaryTemplate ». L'objet est à `res − 0x18` : **la ressource est
      l'interface à `+0x18` du `CompoundTemplate`** (d'où le chemin à
      `+0x20`).
    - `FUN_1411d0940(res)` = **chargement d'un `CompoundTemplate`** :
      éléments à `+0x120` (compte `+0x128`, pas 0xA0) ; pour chacun,
      `élément[4]` = référence du template de composant, résolue par
      **`FUN_141035dd0(&DAT_142b2e710, élément[4], 1)`** (pool global de
      templates, protégé par la section critique `DAT_142b2e710`) →
      `élément[0x11]`, puis instance `FUN_14111d100(tpl, 0)` →
      `élément[0x12]`. Messages « Compound template element '{0}' … ».
    → `FUN_141035dd0` est le candidat « trouver / charger un template par
    référence ».
  - **Chaîne de chargement d'un template par chemin (Ghidra via MCP,
    2026-10-28)** :
    - `DAT_142b2e710` = **pool des templates** : commence par une section
      critique (`FUN_140e22d80(cs, 1)` = `EnterCriticalSection`) ; `+0x28` =
      `unique_ptr` vers la **racine d'un arbre de répertoires**.
    - `FUN_141035dd0(pool, chemin, p3)` = **recherche pure** (aucun
      chargement) : `FUN_14112f330(racine, chemin, p3)` découpe le chemin
      sur `/` ou `\` ; répertoire : sous-répertoires triés à `+0x10`
      (compte `+0x18`, `sousRep[0]` = nom, recherche `FUN_14112eff0`),
      feuilles triées à `+0x28` (compte `+0x30`, `feuille +0x08` = nom),
      comparaison `_stricmp`. `p3` n'est pas utilisé. Renvoie la feuille ou 0.
    - Feuille = **`EntityTemplateStub`** (`entitytemplatestub.cpp`) :
      `[0]` répertoire, `[1]` nom, `[4]` dword, `[5]` chemin complet ou 0,
      octet `+0x33`, **`+0x38` = template chargé** (0 sinon).
    - `FUN_14111d100(stub, p2)` = **obtenir le template, en le chargeant au
      besoin** : déjà chargé → +1 réf. (`FUN_1410042c0`, compteur `res+0x18`
      sous la section critique `DAT_142a2d650`) et renvoie `stub +0x38` ;
      sinon `FUN_14111ce10(stub, !p2)` (`SetupTemplateResource` : chemin =
      répertoire + nom, `FUN_1410056f0(&DAT_142a2d4d0, 5, chemin, …)` crée la
      ressource de type 5, `stub +0x38 = ressource − 0x18`), puis
      `vtable[+0x10]`, +1 réf., et `FUN_141003160(res+0x18, p2)`.
    - `FUN_141003160(res, p2)` : `p2 == 0` → état `+0x14` passe à 1 puis
      **`FUN_141037da0(&DAT_142a2d4d0, res)` charge sur le thread
      appelant** (c'est le chemin pris par `FUN_1411d0940`) ; `p2 != 0` →
      passage par `FUN_1410bf7a0` / `FUN_141004480` (file ou attente, non
      décompilé). `DAT_142a2d4d0` = gestionnaire de ressources.
    - Le template obtenu est l'objet complet (ressource − 0x18), donc pour
      un `CompoundTemplate` c'est ce que `Creer` attend (= `entité +0x28`).
      Son chargement (`FUN_1411d0940`) résout lui-même ses éléments.
    - `FUN_141af9770` montre l'usage inverse : stub → chemin texte par
      `FUN_14111c4f0(stub, tampon, 0x104, 1)`.
    → Recette visée, sur le thread du jeu : `Enter(pool)` ;
    `stub = FUN_141035dd0(pool, chemin, 1)` ; `tpl = FUN_14111d100(stub, 0)` ;
    `Leave(pool)` ; `Creer(gestionnaire, tpl, parent, &transform, 0)`.
    **Inconnues** : forme exacte du chemin (préfixe `templates/` ?
    extension ?) et présence des stubs d'un autre scénario hors de ce
    scénario → à lever en parcourant l'arbre depuis `pool +0x28`.
  - **F9 (parcours de l'arbre, 2026-10-28)** : 628 répertoires, 6292 stubs.
    Chemin **relatif à la racine, sans préfixe ni extension** :
    `GameEntities/WorldMap/SurvivorGroups/Refugees/RefugeeScenarioSurvivors01`.
    **Tous** les groupes de tous les scénarios ont leur stub, chargés ou
    non (32 sous `SurvivorGroups` : Builders, Refugees, TechHaven,
    Winterhome, StoryModeRefugees1..3, `SurvivorsFromExpeditionSite`,
    `SurvivorsFromLostExpedition`, `AutomataFromExpeditionSite`). Dans
    Refugees, `stub +0x38` = `entité +0x28` du groupe (vérifié) ; seuls
    Survivors01 et 02 y sont chargés. Le stub est aussi à
    `CompoundTemplate +0x68`.
  - Premier essai de F3 : rien. La pompe (`GroupesPompe`, dans le hook du
    setter de ressources) **ne tourne pas en pause**, et une demande arrivée
    pendant un chargement était jetée. Corrigé : la demande attend que la
    partie soit prête ; il faut **reprendre le jeu** pour qu'elle s'exécute.
  - **F3 dans TechHaven (2026-10-28) : création validée sans groupe
    préexistant.** `AutomataFromExpeditionSite` (non chargé) chargé à la
    demande, `CompoundTemplate`, entité `AutomataFromExpeditionSite*5552`
    posée sur un site, parent `WorldMaps/WorldMapTechHaven` (le même pour
    les sites et la ville : c'est la scène de la carte), système 0 → 1
    groupe, état 2. Aucun plantage. Pas vu à l'écran : carte inaccessible
    sans balise.
  - **Accès à la carte du monde** (`FrostpunkWorldMapSystem`, id `0x60`,
    décompilés du 2026-10-28) : `+0x2280` Initialized, `+0x2281`
    **WorldMapUnlocked**, `+0x2284` **WorldMapAvailabilityBuildingsCount**
    (bâtiments d'accès : la balise), `+0x2288` état de transition,
    `+0x21B8`/`+0x21C0`/`+0x21D0` écouteurs / nombre / compteur de
    l'événement LockedUnlocked, `+0x2248` événement AvailabilityChanged.
    Bouton actif ssi `Unlocked && compte >= 1 && *(DAT_14408dd50 + 0xA8) != 2`
    (`FUN_1413b8e10(DAT_144065ae0, dispo)` pose `+0x21C` de l'interface).
    `FUN_141332cd0(sys)` = **déverrouiller** ; `FUN_141331940(sys, 2)` =
    recalculer bouton + événement ; `FUN_1413319f0` = balise posée/détruite
    (±1, ferme la carte si le compte tombe à 0) ; `FUN_141331600` = init
    (déverrouille si `DAT_142c343c5`, drapeau de debug jamais écrit) ;
    `FUN_1413302d0` = recomptage des balises au chargement d'anciennes
    sauvegardes.
    → `CarteAssurerAcces` (groupes.cpp, appelée par le hook du setter à
    côté de `GroupesPompe`) : déverrouille si besoin, relève le compte à 1
    s'il est nul, puis rafraîchit. Log `[CARTE] acces force …`.
    **Validé le 2026-10-28** : carte ouverte sans balise, groupe créé par F3
    visible.
  - **Personnalisation d'un groupe : décompilés du 2026-10-28.**
    - Template (`FUN_141a10070`, réflexion) : `+0x28` Survivors
      (`Dynarray<FrostpunkSurvivorsEntry>`), `+0x40` Base movement speed
      (`ChangeRate` : float `+0x40`, unité `+0x44`), `+0x48` Initialization
      on world map unlock (octet), `+0x50` Display name (**2 variantes**
      `+0x50`/`+0x58`, clés 8 bits), `+0x60` Description (2 variantes),
      `+0x70` Icon (**2 variantes de 0x18** : `+0x70`/`+0x88`), `+0xA0` Image,
      `+0xD0` GUID, `+0xE0`/`+0xE8`/`+0xF0` textes, `+0xF8` City reached
      dilemma, `+0x108` Can be escorted, `+0x10C` Starting state, `+0x110`
      Death rate (`ChangeRate`).
    - `FrostpunkSurvivorsEntry` (`FUN_141a18ac0`), **0x24 octets** : `+0x00`
      Count (int), `+0x04` Social class (`EntryLink<FrostpunkSocialClassEntry>`,
      16 octets), `+0x14` Initial statistics (`EntryLink<…InitialStatistics…>`).
    - Composant de groupe : `+0x20` template, `+0x30` index de variante
      (nom/icône), `+0x34` Initialized, `+0x35` ReachedCity, `+0x36` Escorted,
      **`+0x38` = minuteur des morts** (pas le temps de marche ! cf.
      `FUN_1418b2f70`, tick de mortalité : tue un survivant par
      `FUN_1418b18f0` au bout d'un délai aléatoire), **`+0x48` = contenu
      propre au groupe** (copie des Survivors, pas 0x24, compte `+0x50`),
      `+0x78` copie du Death rate.
    - Système de groupes (vtable `0x1420C2E68`) : **`FUN_1418b1cc0` =
      composant ajouté** : copie les survivants du template
      (`FUN_1418afb80(comp+0x48, tpl+0x28)`), copie le nom
      (`FUN_141a0f1c0` = `tpl + 0x50 + variante × 8`) et l'icône
      (`FUN_141a0f2a0` = `tpl + 0x70 + variante × 0x18`) dans un autre
      composant de l'entité (`+0x30` nom, `+0x40..+0x58` icône), puis, si le
      système est initialisé et (`!tpl+0x48` ou carte déverrouillée),
      **`FUN_1418b1620` = départ** : vitesse `FUN_1418b0d40` = (`DAT_142c343c8`
      × conversion d'unité × `tpl+0x40` + `sys+0x21D8`) × `sys+0x21F8`,
      destination `FUN_141884720`, puis `FUN_1418853b0` (durée = distance /
      vitesse) → `FUN_141885250`.
    - **`LinearMovementComponent`** (`frostpunklinearmovementsystem.cpp`,
      système `DAT_14409ac18`, composant par `FUN_141884960(sys, entité)`) :
      `+0x30` départ (vec4), `+0x40` destination, `+0x50` date de départ,
      **`+0x58` date d'arrivée**, `+0x60` mode. Date courante :
      `*(DAT_142c32a40 + 0xB0)`. Position de l'entité utilisée : `+0x140`.
    - **Temps** (`frostpunktime.cpp`) : date interne = secondes × 2³¹
      (`DAT_141eb5808` = 2³¹) ; une heure de jeu = `*(int*)DAT_142c38ff0`
      secondes (global d'exécution) ; unités 0 = secondes, 1 = jours, 2 = heures.
  - **Groupes Archipelago (groupes.cpp, 2026-10-28, non testé)** :
    `GroupeCreer(GroupeAP)` (thread du jeu) — template par chemin, posé sur
    le premier site ; hook de `FUN_1418b1cc0` qui remplace `comp +0x20` par
    une **copie du template** (`MoteurAlloc(0x140)`) dont le nom et la
    description (`+0x50/+0x58`, `+0x60/+0x68`) sont nos clés
    `@AP/Groupe/<slot>/N|D` (`CleMoteur`, traduites par `hkTranslate` →
    `GroupeTexte` → `ApLocGroupe`) ; contenu : `Count` de la 1re entrée de
    `comp +0x48`, les autres à 0 ; `sansMorts` : `comp +0x78` = 0 ; durée :
    `ReglerDuree` réécrit départ / date de départ / date d'arrivée du
    `LinearMovementComponent` et met la vitesse de la copie à l'échelle.
    16 slots, oubliés au changement de partie. F3 = groupe de test (5
    survivants, 1 h, `SurvivorsFromExpeditionSite`), F8 = 30 min restantes
    (l'ancien F8, qui avançait le minuteur des morts `+0x38`, est remplacé).
    F9 dumpe les icônes (`tpl +0x70..+0xB8`) et le contenu du premier groupe.
    `CarteAssurerAcces` passe désormais **avant** `GroupesPompe`.
  - **Test F3 du 2026-10-28 (TechHaven)** : groupe créé, hook du composant
    vu, **durée OK (1 h), F8 OK (30 min)**. Mais : nom resté « Survivors »
    (`tpl +0x50` et `+0x60` sont **nuls** : les clés sont en `+0x58` nom et
    `+0x68` description ; le modèle d'en-tête lu en `+0x50` était nul →
    aucune clé créée), et **0 survivant** : `SurvivorsFromExpeditionSite` n'a
    **aucune** `SurvivorsEntry` (en vanilla, le site remplit le groupe) →
    `comp +0x48` vide, rien à l'arrivée. Icône (`tpl +0x70`) : chemin
    `UI\ExpeditionUI\Icons_Scouts_Activity.dds` à `+0x18` de la structure.
  - **`FUN_1418afb80`** = copie des survivants du template vers le composant
    (memcpy des entrées de 0x24, `tpl +0x28` → `comp +0x48`), appelée par
    `FUN_1418b1cc0`. **`FUN_1418b2870(système, entité)` = arrivée en ville** :
    crée les citoyens du contenu (`FUN_1418b7c00`) au drapeau d'arrivée,
    `+0x35` ReachedCity = 1, secourus en `comp +0x88` (compte `+0x90`),
    contenu vidé, événement `système +0x2128`. Contenu vide → rien.
  - **Corrections (non testées)** : modèle de clé = première clé valide parmi
    `+0x50, +0x58, +0x60, +0x68, +0xE0, +0xE8` ; écriture seulement dans les
    champs qui portaient une clé valide (`+0xE0/+0xE8` = noms d'agents, peut-
    être la source de « Survivors »). Survivants : `PreparerSurvivants` charge
    un **template donneur** (`GroupeAP::cheminSurvivants`, sinon liste :
    BuildersScenarioWorkers, RefugeeScenarioSurvivors01,
    TechHavenManchesterSurvivor, WinterhomeEngineersSurvivors), trouve son
    template de composant dans les éléments du `CompoundTemplate`
    (`TemplateGroupeDe`), copie sa 1re entrée (classe sociale + stats) avec
    notre `Count`, et le hook pose ce tableau dans la copie **avant**
    l'original. **Hook de l'arrivée** (`hkGroupeArrive`) : log, puis
    `GroupeAP::surArrivee(slot)` ; le test F3 livre 50 charbon + 1 cœur de
    vapeur par `ReceiveResource`. Slot réutilisable après l'arrivée.
  - **Test du 2026-10-28 (suite)** : **nom OK** (« Livraison Archipelago » ;
    champs remplacés : `+0x58`, `+0x68`, `+0xE0` `@Expeditions/
    SurvivorGroupName`, `+0xE8` `@UI/WorldMapSelectionPanel/Expedition/
    CarriedSurvivors`), **hook d'arrivée OK, ressources livrées**. Mais
    0 citoyen : les 4 donneurs se chargent, pourtant `template 0x0`, car les
    éléments du `CompoundTemplate` ne sont pas en `+0x120`
    (`FUN_1411d0940` reçoit la ressource = compound `+0x18` → essai en
    `+0x138`), avec un repli sur les templates capturés dont la clé `+0x58`
    contient `/<chemin du donneur>/`.
  - Test suivant : éléments bien en **`+0x138`** (2 par `CompoundTemplate`,
    `+0x120` nul), mais **aucun pointeur direct** vers le template de
    groupe ; et la clé `+0x58` n'est pas un chemin (repli inopérant). En
    revanche **chaque chargement de donneur construit un template**
    (captures 2 → 5). → Le donneur est désormais **la capture apparue
    pendant `ObtenirTemplateParChemin`** (différence avant / après) ; repli
    sur les éléments, avec dump des 2 premiers (classe RTTI ou texte de
    chaque qword) pour comprendre leur structure.
  - **VALIDÉ (2026-10-28)** : donneur trouvé par capture, **5 citoyens
    arrivent en ville**, ressources livrées. Au 2e F3, le donneur déjà
    chargé n'est plus reconstruit → mauvais donneur (Refugees) : cache
    chemin → template ajouté (non testé). À l'arrivée, un **dilemme
    générique** se déclenche.
  - **Dilemme d'arrivée** (`FUN_141a0f440`, init du composant depuis son
    template) : `comp +0x80` = dilemme résolu depuis le lien
    `tpl +0xF8` (City reached dilemma, `FUN_140e1a930` = lien valide ?,
    puis `FUN_1413e4240()->vtable[0x88]`) ; log « Overriding city reached
    dilemma » si `+0x80` était déjà posé. Même fonction : `comp +0x78` =
    `tpl +0x110`, `comp +0x30` = `tpl +0x10C`. Le lancement du dilemme
    depuis `comp +0x80` n'est pas encore localisé.
    `FUN_140e1a930` = comparaison de GUID de 16 octets (avec le GUID nul
    `DAT_142a12558`). **Décision de l'auteur : supprimer le dilemme** des
    groupes AP → le hook efface `clone +0xF8` (16 octets) et `comp +0x80` ;
    log `dilemme d'arrivee apres Creer` pour voir si le jeu le repose.
    **Résultat** : `comp +0x80` valait **déjà 0** (template sans dilemme) ;
    la bulle reste. Cache du donneur **validé** (`template deja connu`).
    La bulle (cercle en ville qui ouvre un dilemme à l'expiration) est un
    **événement du jeu** branché sur l'arrivée.
  - **Système d'événements** (`FrostpunkGameEventSystem`, réflexion
    `FUN_1417c3400`, global `DAT_1440935c8`) : `+0x2128` répertoires,
    `+0x2140` événements enregistrés, `+0x2170` état d'exécution,
    `+0x21A0` événement courant, `+0x21A8` AllowEventInvokes, et une liste de
    déclencheurs par type (`+0x2D08` Escorted / **`+0x2D20` Unescorted
    SurvivorGroupReachedCity**, `+0x2918` ExpeditionSent…).
    `FUN_1417c0fd0` = enregistrer un événement, `FUN_1417c1220(sys, évt,
    déclencheur)` = enregistrer un déclencheur.
  - **Commande console `InvokeEvent`** (`FUN_1413a3ec0(nom, forcer)`) :
    répertoires d'événements `DAT_14406dda8` (compte `DAT_14406ddb0`),
    chacun `+0x18` entrées / `+0x20` compte ; `FrostpunkGameEventEntry` :
    `+0x00` nom, `+0x20` Enabled, `+0x28` déclencheurs (compte `+0x30`),
    **`+0x58` effets**. Contexte : `{poignée d'entité, position vec4,
    vec4, 0}` (entité sélectionnée ou défaut `DAT_1440b1b30`).
  - **`FUN_1417b9b70(sys, entrée, contexte, forcer)` = invoquer un
    événement** : `forcer` → conditions (`FUN_1417aebc0`) ; effets exécutés
    par `FUN_14169ae90(DAT_14408e4b8, entrée + 0x58, contexte, nom)` si
    AllowEventInvokes ; puis compteurs et écouteurs (`sys +0x2108`).
    → Voie visée pour **nos propres dilemmes** (demande de l'auteur, 2026-10-28) :
    cloner un `FrostpunkGameEventEntry` (et son dilemme) avec nos clés de
    texte, puis l'invoquer avec le groupe pour contexte.
  - **Test du 2026-10-28** : à l'arrivée d'un groupe AP, **6 événements**
    ont le groupe pour contexte (tous ignorés à ce test) : `[NM]
    ManchesterSurvivor` (SelectEffect), `[NM] SpawnQuest` et `Some Question
    Helping NM - CTA` (TimedEffect), `S: Automaton from Frostland`
    (**`FrostpunkCreateSurvivoursDilemmaEffect`**), **`S: Survivors -
    universal`** (SelectEffect, la bulle générique présumée), `S: Heal
    Incomers (if Cold Resistance)`. Les invocations ont `forcer = 1`
    (conditions vérifiées). Les autres classes d'effets vues :
    `TimedEffect`, `SwitchEffect`, `SelectEffect`, `RandomizedEffect`,
    `FinishQuestEffect`, `ModifyCitizenNeedEffect`,
    `ModifySocialAttitudesEffect`, `ProfileProgressEffect`.
  - **Décision de l'auteur (2026-10-28)** : **garder la bulle**, puis la
    **remplacer par la nôtre** (nos propres dilemmes : prochaine tâche).
    La suppression est retirée ; `EvenementsSuivreEntite` remplace
    `EvenementsIgnorerEntite` et fait un relevé détaillé (`[EVT] GROUPE AP`).
  - **`FrostpunkCreateSurvivoursDilemmaEffect`** (0x28, `FUN_141937a70`) :
    `+0x18` lien (GUID) vers un `FrostpunkDilemmaEntry`. Résolution d'un
    lien de dilemme : `(*DAT_144069970)[0x88](DAT_144069970, &guid)`
    (disassemblé dans `FUN_141a0f440`).
  - **`FrostpunkDilemmaEntry`** (0x250, réflexion `FUN_1419dbde0`) : `+0x18`
    Variables, `+0x30` Text parameters, `+0x48` Initial effects, `+0x60`
    Ignored effects, **`+0x78` Choices** (pointeurs de
    `FrostpunkDilemmaChoiceEntry`), `+0x90` Sequence flags, **`+0xD8`
    Displayed name, `+0xE0` Arc subtitle, `+0xE8` Description, `+0xF0` Type
    text**, `+0xF8` Chat settlement greeting, `+0x108` Main icon, `+0x120`
    Settlement icon, `+0x150` Template (lien `FrostpunkDilemmaTemplateEntry`),
    `+0x188` Subject type, `+0x1B0` Modal, `+0x1B4` Ignorable, `+0x1B8`
    Dilemma type, `+0x1CC` Max occurences, `+0x1D4` Countdown, `+0x1E0` Time
    to countdown, `+0x1F0`/`+0x210`/`+0x230` images.
  - Relevé détaillé (validé) : effets récursifs (textes, objets RTTI,
    tableaux), et pour un `CreateSurvivoursDilemmaEffect` le dilemme résolu :
    textes, choix (qwords texte / RTTI des 0x100 premiers octets).
  - **La bulle = dilemme `[Beacon]S: Survivors group`** (GUID `FF DC 2F 34
    B3 3E 65 9F 85 4F 08 02 53 01 0C 5A`), via `S: Survivors - universal` →
    `FrostpunkSelectEffect` (`+0x18` `FrostpunkRescuedSurvivorsSelector`,
    `+0x50` effets) → `CreateSurvivoursDilemmaEffect`. Titre
    `@Events/Survivors/Days10-16/Title_universal` (« New Arrivals »),
    description « Survivors reached the city », 1 choix « I see » (clé en
    `choix +0xA8`). **Pas de RTTI** sur `FrostpunkDilemmaEntry` ni sur les
    choix ; nom de l'entrée en `+0x00`. Autres dilemmes vus :
    `[Beacon]S: Automata group`, `FrostpunkCreateDilemmaEffect` (TechHaven).
  - **`FrostpunkDilemmaChoiceEntry`** (0x150, `FUN_1419daf40`) : `+0x18`
    Choice type, `+0x28` Default choice, `+0x48` Show conditions, `+0x60`
    Conditions, `+0x78` Effects, `+0x90` Outcomes, **`+0xA8` texte**,
    `+0xB0` Description, `+0xB8` Effect text, `+0xC0` Disabled text,
    `+0xC8` Arc subtitle, `+0x110`/`+0x118` texte / titre du résultat.
  - **Exécution de `CreateSurvivoursDilemmaEffect`** (`FUN_141697870`,
    désassemblé vers `0x1416984dd`) : composant de groupe du contexte ; **si
    `comp +0x80` != 0, c'est ce dilemme qui est créé**, sinon celui du lien
    `+0x18` (résolu par `DAT_144069970`). Création :
    **`FUN_1417cde10(DAT_144095a98, dilemme, contexte, p4)`** (`p4` =
    survivants `FUN_1418b0e10(DAT_14409b698, entité)` dans le 1er cas, 0
    sinon) — fonction à retenir pour lancer nos dilemmes ailleurs.
  - **Remplacement de la bulle (non testé)** : `DilemmeCloner`
    (evenements.cpp) copie l'entrée (0x250) et ses choix (0x150), titre
    `+0xD8`, description `+0xE8`, texte de chaque choix `+0xA8` remplacés
    par nos clés ; `hkGroupeArrive` pose la copie de « Survivors group » en
    `comp +0x80` **avant** l'original. Clés `@AP/Groupe/<slot>/T|X|C`
    (`GroupeTexte(slot, champ)` : N, D, T, X, C ; 5 objets de traduction
    par slot dans `ApLocGroupe`, 80 au total). `GroupeAP::dilemmeTitre /
    dilemmeTexte / dilemmeChoix` (défaut : nom, description, « Accueillir »).
    Copie superficielle : effets / conditions / résultats des choix
    partagés avec l'original. Inconnu : sérialisation d'un dilemme en
    attente (EntryPtr de notre copie) lors d'une sauvegarde.
  - F8 ne vise plus un groupe déjà arrivé (durée absurde de 171798662 h).
  - **Test** : copie créée et posée en `comp +0x80`, événements exécutés
    **pendant** l'arrivée (synchrones), mais **bulle d'origine**. Cause
    présumée : le `SelectEffect` à `RescuedSurvivorsSelector` exécute
    l'effet interne avec un **survivant secouru** pour contexte, pas le
    groupe → pas de composant de groupe → branche du lien `+0x18`.
  - **`FUN_1417cde10(sys, dilemme, contexte, p4)` décompilé** = créer un
    dilemme : max d'occurrences (état par entrée, `sys + 0x437*8`),
    `FUN_1419dad00(dilemme)` = son template, conditions `FUN_1417ceae0`,
    variables (`+0x18`), log « [Dilemma][{0}] Dilemma created », effets
    initiaux (`FUN_14169aee0(…, dilemme + 0x48, …)`) ; renvoie l'id ou -1.
    → **Hook** `hkCreerDilemme` (evenements.cpp) : pendant un événement dont
    le contexte **extérieur** est un groupe suivi (`g_suivieEnCours`, posé
    autour de l'original dans `hkInvoquerEvenement`), le dilemme
    `orig` est remplacé par sa copie (`EvenementsRemplacerDilemme(entité,
    orig, copie)`, appelé par `hkGroupeArrive`). Log `[DIL] creation du
    dilemme …` pour **toute** création. **VALIDÉ le 2026-10-28** : bulle et
    dilemme d'arrivée avec nos textes.
  - **Dilemmes pièges** (demande de l'auteur, 2026-10-28 : les pièges
    Archipelago doivent être visibles et inévitables, via des dilemmes à
    plusieurs choix). `FUN_1417d40c0(sys, id, choix, drapeaux)` = **résoudre
    un dilemme** (dilemmes actifs `sys +0x21A0`, enregistrements 0x90,
    `+0x10` = entrée ; choix −1 = aucun ; `drapeaux & 1` → effets ignorés
    `+0x60`, sinon effets du choix `+0x78` puis du résultat). Création sans
    entité de contexte : marqueur posé sur la 1re entité de
    `*(DAT_14408dc20 + 0x2208)` ; contexte par défaut `base + 0x40B1B30`
    (celui de la commande `InvokeEvent`). Système des dilemmes
    `*(base + 0x4095A98)`.
    → `DilemmePiegeCreer(titre, texte, choix[], n, choixSiIgnore)` (thread
    du jeu) : copie de « Survivors group » avec n choix (copies du 1er choix
    d'origine), **tous les effets du jeu vidés** (initiaux `+0x48`, ignorés
    `+0x60`, effets `+0x78` et résultats `+0x90` des choix), clés
    `@AP/Dil/<slot>/T|X|C<i>` (`DilemmePiegeTexte`, `ApLocDilemme` : 8 slots
    × 6), création par l'original de `FUN_1417cde10`. `hkResoudreDilemme` :
    pour un de nos ids, applique la perte du choix (`Res_Add(res, −q)`,
    popup « Piege Archipelago ») ; choix hors bornes (ignoré) →
    `choixSiIgnore`. **F6** = piège de test (perdre 50 charbon / 50 acier).
    Inconnus : position de la bulle sans entité, et un dilemme en attente
    lors d'une sauvegarde.
  - **1er test F6** : `FUN_1417cde10` renvoie **−1**. Cause :
    `FUN_1417ceae0(dilemme, contexte, sortie)` vérifie le **type de sujet**
    (`dilemme +0x188`, −1 → template `+0x18`) : **0 global** (contexte par
    défaut `DAT_1440b1b30`), **1 entité** (exige une entité vivante dans le
    contexte), **2 lieu** (exige un point). « Survivors group » = entité.
    → La copie du piège est posée en sujet **global** (0). Non testé.
    L'auteur : le générateur comme sujet convient (c'est a priori l'entité
    par défaut du marqueur d'un dilemme global).
  - **Test F6 du 2026-10-28** : bulle **sur le générateur**, choix et pertes
    OK ; icône fausse. Le relevé a montré que la tuile **optionnelle** du
    marqueur a son **drapeau en tête** (`+0x230`) et la tuile en `+0x238`
    (texture `+0x238`, case `+0x240` = (14, 6) pour « Survivors group »,
    nombre de cases `+0x248`) ; l'hypothèse « drapeau après la tuile » avait
    écrasé le nombre de cases. `+0x170` du gabarit de bâtiment n'est pas
    non plus une tuile complète (nombre de cases illisible). Textures des
    tuiles : pas un `char*` lisible directement (NameString). → Correction :
    on garde texture et nombre de cases du marqueur d'origine et on ne
    change que la case → (15, 5) `icon_trap`. Non testé ; si ce n'est pas
    l'atlas #3037, il faudra identifier la texture du marqueur.
  - **2e test** : texture du marqueur = **`UI\Tiles\Icons.dds`** (16 × 16
    cases, lisible dans la NameString), case (14, 6) → pas un atlas que
    nous avons modifié (le #2680 est l'atlas des ressources, 512², cases de
    tailles variées ; vérifié en le convertissant). → La tuile du marqueur
    reprend celle d'une **techno** de l'arbre (`techno +0x78` texture,
    `+0x80` case, `+0x88` nombre de cases ; miniatures (0,2) exclues), case
    `icon_trap` de l'atlas #1179 (0, 4). Et « New Arrivals » = sous-titre
    d'arc `+0xE0` : clé `@AP/Dil/<slot>/S` (« Archipelago »), 7 objets de
    traduction par slot dans `ApLocDilemme`. Non testé.
  - **3e test** : sous-titre OK ; **aucune icône** avec l'atlas de l'arbre
    (la bulle ne sait sans doute afficher que son atlas). **Décision de
    l'auteur** : il ajoutera nos icônes dans l'atlas des bulles
    `UI\Tiles\Icons.dds` (16 × 16) ; il faut son **numéro d'entrée** dans
    `textures-s3`.
  - **Index des archives** (`textures-s3.idx`, décompilés du 2026-10-28) :
    en-tête 11 octets (`+0` version 0, `+1`, `+2`, `+3` nombre d'entrées,
    `+7` graine du hash = 0), enregistrements de **0x1D** triés par hash :
    `+0` hash (u32), `+4` taille compressée, `+0xC` taille, `+0x14`
    décalage dans le `.dat`, `+0x1C` drapeau. Pas de fichier `.str` (noms)
    en version publique. Recherche `FUN_140e51fa0(lecteur, sortie, nom)` :
    minuscules, **MurmurHash2** `FUN_140e53a40` (m = **0x5bd1e995**, passé
    au montage par `FUN_140ff3060`), dichotomie. Montage :
    `FUN_140e0e290` → `FUN_140e0a260` (`FileSystem::ContainerMountPoint`) →
    `FUN_140e0a600` (`.dat`, `.idx`, `.str`).
    Hors du jeu, le hash de `ui/tiles/icons.dds` (et variantes, et même
    `common/project.config` dans `common.idx`) ne retrouve **rien** : la
    forme exacte du nom hashé est inconnue. → Hook `InstallTraceArchives`
    (mod_common, au démarrage) : log `[ARCH]` des 20 premiers noms cherchés
    et de tous ceux qui contiennent « icons » ou « tiles », avec hash,
    taille et décalage. Script hors jeu :
    `scratchpad\hash_textures.ps1` (hash → numéro d'entrée).
  - **Résultat `[ARCH]` (2026-10-28)** : les noms cherchés sont de la forme
    **`/UI/Tiles/Icons.texture`** (barre initiale sautée, extension
    `.texture`, minuscules) → hash de `ui/tiles/icons.texture` = `0x8F231C35`
    = **entrée 2350** (DXT5 2048², 538 362 octets compressés). Vérifié :
    `tech_tree_atlas` = 1179, `buldings_dds` = 3037. Le script
    `hash_textures.ps1` retrouve désormais l'entrée de n'importe quel nom
    en `.texture`. Blob décompressé : en-tête propre au jeu (largeur,
    hauteur, `DXT5`, 12 mips…), **niveau 2048² en dernier** (4 Mo de fin).
    Le fichier `Nouveau dossier\02350_2048x2048.dds` de l'auteur est
    **tronqué** (5 570 544 octets). Atlas des bulles : 16 × 16 cases de
    128 px ; survivants = (14, 6) ; cases libres notamment ligne 1, col 7–12.
    → `SUBSTITUTIONS` : `{2350, "ap_icons_bulles.gz"}` ; `TechIcons` gagne
    `bulleCol/bulleRow` (vanilla (7,1), filler (8,1), important (9,1),
    progressive (10,1), trap (11,1), done (12,1)) ; la bulle du piège garde
    la texture d'origine et prend la case `icon_trap`. Reste : l'atlas
    modifié (fait par l'auteur).
  - **Format d'un blob de `textures-s3`** (comparaison de l'entrée 2680 et
    de `ap_icons_ressources.gz`) : gzip (7-Zip accepté, nom de fichier dans
    l'en-tête gzip) de : en-tête de 144 octets (largeur u32, hauteur u32,
    `DXT5`, nombre de mips u32, puis par mip `{l u16, h u16, taille u32}`
    du plus petit au plus grand, zéros), puis les mips du 1×1 au plus grand.
    → **`textures\bulles\construire_ap_icons_bulles.ps1`**, repris de la
    méthode de l'auteur (`Frost\Nouveau dossier\RemplacerBlocAtlas1179.txt`
    / `…3037.txt`, anciens chemins `D:\`) : blob d'origine décompressé, et
    **seuls les blocs DXT des cases modifiées** remplacés dans chaque mip
    par ceux de texconv (`BC3_UNORM`, 12 mips, `-dx9`) ; compression .NET
    ou 7-Zip (`-mx9 -mfb=258 -mpass=15`), le plus petit ; limite =
    min(place jusqu'au blob suivant, taille compressée de l'index) ;
    `-Deployer`. **Piège corrigé** : lire toute la « place » fait échouer le
    `GZipStream` .NET sur les octets suivants (exception avalée, fin du
    blob perdue : c'est pourquoi l'ancien `02350_2048x2048.dds` était
    tronqué) → on lit exactement la taille de l'index. Test sur l'atlas non
    modifié : 499 953 octets (7-Zip), marge 38 ko. Fichiers :
    `02350_origine.png` (à éditer → `02350_modifie.png`), `emplacements.png`.
  - **Atlas des bulles produit (2026-10-28)** : icônes de l'auteur
    (`textures\bulles\icones\<nom>.png`, 2034 × 2112, en couleur) placées
    par `composer_atlas.ps1` (recadrage sur l'alpha, 108 × 112 centré,
    marge 8 px, case vidée avant) → `02350_modifie.png`, `controle.png`,
    `planche.png` ; `.gz` de **520 685 octets** (marge 17 ko), **déployé**
    dans `mods\ap_icons_bulles.gz`. **VALIDÉ en jeu** : la bulle du piège
    affiche l'icône trap.
  - **Catalogue (2026-10-28, demande de l'auteur)** : `dilemmes_ap.h/.cpp`,
    séparé de la mécanique ; F6 = `DilemmeAPDemander("trap_coal_or_steel")`.
    Quantités désormais **signées** (−50 = perte). `DilemmeAPCreer` remplace
    `DilemmePiegeCreer` ; icône de la bulle choisie par son nom.
  - **Icône des groupes : VALIDÉE** (carte et liste à droite affichent la
    case de l'atlas des bulles ; aucune autre injection nécessaire). Trop
    grande → variante **`icon_vanilla_groupe`** (vanilla à 85 %) dans la
    case libre **(8, 4)** de l'atlas des bulles, défaut de
    `GroupeAP::icone`. `composer_atlas.ps1` prend désormais une liste de
    placements `{nom, fichier, col, lig, echelle}` ; le build compte 7 cases.
    `.gz` : 525 561 octets, **marge 12,8 ko** (attention aux prochains ajouts).
  - **Icône des groupes AP (1re version)** : `PoserIconeGroupe` (hook du
    composant ajouté, avant l'original) met dans les deux variantes d'icône
    de la copie du template (`+0x70`, `+0x88`) la tuile de l'atlas des
    bulles (`TuileAtlasBulles`), `GroupeAP::icone` (défaut `icon_vanilla`) ;
    tuiles d'origine (`+0x70`, `+0x88`, image `+0xA0`) journalisées. Cible :
    l'icône de la carte et celle de la liste à droite. Si la texture n'est
    pas affichée hors bulle, piste : injecter l'icône dans l'atlas du
    groupe (`UI\ExpeditionUI\Icons_Scouts_Activity`, entrée à trouver par
    `hash_textures.ps1` avec `.texture`).
  - **Icône du piège (1re version, fausse)** : `UITextureTile` (0x18,
    `FUN_141028100`) = `+0x00` texture, `+0x08` case (x, y), `+0x10`
    nombre de cases. Dilemme : `+0x108` image principale, `+0x120` icône de
    colonie, `+0x1F0`/`+0x210`/`+0x230` images optionnelles (tuile +
    drapeau à `+0x18`) avant-plan / fond / **marqueur**. `PoserIconePiege`
    prend la tuile d'un gabarit de bâtiment (`+0x170`, atlas des miniatures
    #3037) avec la case `icon_trap` (15, 5) et la pose en `+0x108` et
    `+0x230` (drapeau à 1) ; le journal décrit les images d'origine.
  - **Textes en jeu passés en anglais** (2026-10-28) : popups
    (`Technology received`, `Resource received`, `Check sent`,
    `Archipelago Trap`), groupe de test (`Archipelago Delivery`…), piège F6
    (`Lose 50 coal` / `Lose 50 steel`), `Archipelago Item`, textes de check
    (`[AP] Sending: …`, `(already sent)`).
  - **`evenements.cpp`** : hook de `FUN_1417b9b70`, journal
    `[EVT] invoque "<nom>" … contexte : <entité, classe, chemin>` + effets
    (`+0x58`, classes RTTI) pour les 300 premiers ; `hkGroupeArrive` appelle
    `EvenementsIgnorerEntite(groupe)` **avant** l'original → les événements
    dont le contexte est ce groupe sont ignorés (`[EVT] IGNORE`). Si l'appel
    est différé, ça marche quand même (liste persistante).
  - git : absent du PATH, mais disponible dans Visual Studio
    (`…\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team
    Explorer\Git\cmd\git.exe`).
- **Inventaire des groupes AP (2026-10-28, demande de l'auteur : « vrai
  inventaire », comme celui d'un scout)**. Décompilés : le panneau de
  sélection d'un groupe (`FrostpunkUISurvivorGroupSelection`,
  `FUN_141c01860`) n'affiche que la population. La ligne de la **liste de
  droite** est un `FrostpunkUIExpeditionControl`, le même pour un scout et
  un groupe (`+0x290` poignée de l'expédition, `+0x2A8` poignée du composant
  de groupe, `CARGO_LAYOUT` en `+0x310` / compte `+0x318`, liaison des
  enfants `FUN_141c2a490`). **`FUN_141c2b6e0(contrôle)` remplit
  l'inventaire** : scout → cargaison `+0x168/+0x180/+0x198`, groupe →
  survivants `comp +0x48`. Case : `FUN_1416b7350(*DAT_14408ec18,
  &DAT_1440af6f0, case à réutiliser|0, layout, &texte (FUN_141ce67f0 depuis
  définition +0x28), tuile (définition +0x98), {1, quantité}, -1,
  &DAT_142a22e20)` → case suivante ; `FUN_141a31660(suivante, &nom)` supprime
  les restes ; `FUN_141a316b0(layout)` finalise.
  → `GroupeAP::cargaison[]` (`{ressource, quantité}`) : affichée dans
  l'inventaire (hooks des 4 fonctions, cases ajoutées après les survivants
  avec la fonction des scouts) **et livrée à l'arrivée** (`ReceiveResource`,
  avant `surArrivee`). `Res_Definition(nom)` (resources). Test F3 : charbon 50
  + cœur de vapeur 1 dans la cargaison. Non testé.
  **1er test** : 2 cases en plus, mais aux valeurs par défaut du modèle
  (icône des rations, 100) : `EXCEPTION 0xC0000005` à **chaque**
  rafraîchissement, dans la case « coal » (la case est créée, puis faute avant
  l'icône et la quantité). Réflexion de `FrostpunkResourceEntry`
  (`FUN_1413ed200`, 0xD8) : `+0x08` GUID, `+0x18` Initial amount, `+0x28`
  **Display name**, `+0x30` Color, `+0x98` **icône** (`UITextureTile`),
  `+0xB0` Text icon name → offsets bons, définition = entrée (clé
  `EntryPtr` de la `Map` de l'économie). → relevé de l'adresse fautive et de
  l'étape (texte / ajout / libération), **une seule tentative par groupe**
  (`inventaireEchec`). En attente du journal.
  **Résultat** : faute à l'étape « texte du nom », `0x141CE5A51` =
  `FUN_141ce5a40` lit `*(def+0x28)` puis son 1er caractère (attend `@`) : le
  `+0x28` de l'objet du conteneur de l'économie n'est **pas** une clé (cet
  objet a le GUID en `+8`, mais n'est pas le `FrostpunkResourceEntry`).
  → `EntreeRessource(nom)` : GUID (définition de l'économie `+8`) → entrée
  par la **collection des ressources** `FUN_1413ebe70` = `&DAT_14406ad18`,
  méthode `+0x88(collection, &guid)` (comme le chemin « transport » de
  `FUN_141c2b6e0`), clé vérifiée (`@`). **Test : introuvable** (0 case).
  → recherche élargie (collection, objet de l'économie, pointeurs qu'il porte
  en `+0x00..+0x38` : même GUID et clé `@`) et relevé complet en cas
  d'échec (`[GRP] inventaire : "…" introuvable`, dump de l'objet).
  **Relevé** : l'entrée est à **`Res_Definition() + 8`** (= `resId`) :
  `+0x00` nom interne (« Coal »), `+0x08` GUID, `+0x18`/`+0x1C` quantité
  initiale / plafond (150 / 900), `+0x28` `@Resources/Names/Coal`. Le GUID
  était lu 8 octets trop tôt. `EntreeRessource` prend désormais `def + 8`
  (vérifié : GUID + clé `@`). Non testé.

### 2026-11-04 — Groupes AP : malades, pas gravement malades

- **Problème remonté (test grandeur nature)** : les groupes AP arrivent
  **gravement malades**. Leurs statistiques initiales sont copiées d'un site
  de la carte (`SitesEntreeSurvivants`, sites_contenu.cpp), ici celles des
  survivants de Deep Hollow / Ice Crevasse.
- **Ghidra** : `FrostpunkCitizenInitialStatisticsEntry` (0xA8, réflexion
  `FUN_141446850`) : `+0x18` Male probability, `+0x50` Families per citizen,
  `+0x54` Temperature level average, `+0x58` Work safety level average,
  `+0x7C` **Health level** (lien `FrostpunkCitizenHealthLevel`, défaut
  « (Max) »), `+0x8C` / `+0x94` Min / Max hope, `+0x9C` Set discontent to
  average.
- **Correctif (compilé, déployé, non testé)** : `StatsMalade` choisit dans
  la collection globale des statistiques (`base + 0x4072140`) une entrée
  dont le nom contient « sick » / « ill » sans marque de gravité (grav,
  serious, sever, critic…), de préférence préfixée par la classe ; sinon les
  statistiques du site restent. Les noms des entrées étant inconnus, la
  1re fois toutes sont journalisées : `[SCONT] stats <i> "<nom>" : niveau de
  sante <lien>`, puis `[SCONT] classe "…" : statistiques "…" (malade)
  retenues` ou `aucune statistique "malade" trouvee`.
- **Doublons** : aucun dans une même partie. Le journal montre Deep Hollow
  et Ice Crevasse livrés une fois par partie (GameId `5ABBDF3B…`,
  `06039B02…`, `BC7B1DE5…`) : un contenu de site est livré dans **chaque
  partie** de son scénario (option B, 2026-11-03). Les deux groupes se
  ressemblent : 12 ouvriers + 7 enfants contre 11 ouvriers + 7 enfants.
- **Relevé (même jour, 17 entrées)** : `Default`, `[Builders] Convicts`,
  `[Reconquest] Ex-Convicts`, `[Reconquest] Ex-Convicts sick`,
  `DefaultBuilders`, `Disabled-70%`, `Disabled-10%`, `Winterhome`,
  `Survivors`, `SurvivorsMaleOnly`, `AutomatonFormForstLand`,
  **`SurvivorsIll`** (santé `7D9B74C7…`), `SurvivorsGravelyIll` (santé
  `A942714D…`), `[Reconquest] Eruption`, `LordCraven`, `Effie`, `Phasmid`.
  Le 1er essai a retenu `[Reconquest] Ex-Convicts sick`, dont la santé est
  **la même** que `SurvivorsGravelyIll` : faux. Corrigé : `SurvivorsIll`
  retenue par son nom (compilé, déployé 19:32, non testé). Les groupes déjà
  partis gardent les statistiques avec lesquelles ils ont été créés.
- **2026-11-06 (auteur)** : avec `SurvivorsIll` pour tous, **tout** le groupe
  arrivait malade. Voulu : seuls les gravement malades deviennent malades,
  les autres gardent leur état. Correctif (compilé, déployé 20:31, non
  testé) : `StatsSansGravite` ne remplace les statistiques que si leur
  niveau de santé (`+0x7C`) est celui de `SurvivorsGravelyIll` ; et
  `SitesEntreeSurvivants(classe, entrée, site)` cherche d'abord le **site
  d'origine** de l'item (nouvelle table `AP_ITEMS_SITES` de
  `known_ap_ids.h`, générée ; `GroupeAP::site`) : composant `+0xA8` jusqu'à
  sa capacité, trésor `+0x70`, aléatoires `+0xD8`. Limite : une entrée par
  classe (si un site mélange des ouvriers sains et gravement malades, la
  1re entrée trouvée décide).
