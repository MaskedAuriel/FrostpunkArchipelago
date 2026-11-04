# Sauvegarde des elements AP (groupes, messages, bulles, technos)

> Historique detaille, copie mot pour mot depuis CLAUDE.md le 2026-11-02.
> Le resume a jour est dans CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

- **Sauvegarde des éléments AP (2026-10-29)** : au rechargement d'une
  sauvegarde, un groupe AP perd logo et contenu (la copie du template n'est
  pas sauvegardée : la sauvegarde référence le template d'origine).
  **Décision de l'auteur : notre propre sauvegarde persistante** (comme
  `mods\ap_pending_<GameId>.txt` des scouts), recollée au chargement — pas
  d'écriture dans la sauvegarde du jeu. Difficultés : clé stable d'un groupe
  après chargement (adresses changeantes), et chargement d'une sauvegarde
  **plus ancienne** (garder un enregistrement par groupe, ne recoller que les
  groupes présents). À recoller : copie du template (nom, description,
  icône), cargaison, durée, remplacement de la bulle d'arrivée ; plus tard :
  bulles de piège et messages en attente.
  Étape 1 : relevé `[SAV]` (`ReleverUnGroupe` / `ReleverGroupes`,
  groupes.cpp) à la création (« cree »), au 1er passage après un changement
  de partie (« apres chargement ») et sur F9 : template (notre copie ?),
  clé du nom, GUID `tpl +0xD0`, nom d'entité, qwords `entité +0x08..+0x40`,
  contenu, ReachedCity, dates du mouvement. Non testé.
  **Moment du recollage** (idée de l'auteur : agir pendant le chargement,
  avant le 1er tick). Décompilés du 2026-10-29 : `FUN_141265df0(jeu, nom)` =
  **`FrostpunkGame::LoadGame`** (`frostpunkgame.cpp`, drapeau `jeu +0x489`) :
  `FUN_141286840` (« Loading game save… » / « …successfully loaded. » ; lit
  le `.save` par `FUN_141286580` et le désérialise par
  `FUN_14101f110(gestionnaire d'entités, données, …)`), puis si succès
  `FUN_141264b10(jeu, 1, …)` (état `InFrostpunkGame`, écouteurs `jeu +0x358`
  / compte `+0x360`) et `FUN_141266390(jeu)`. `FUN_14126c9f0` = vérification
  « The game should not be paused after load » du système de temps (appelée
  depuis `0x141269500`, hors fonction connue). → **`InstallTraceChargement`**
  (mod_common) : `[CHARG]` à l'entrée / sortie de `LoadGame`, à la fin de la
  lecture, à la vérification après chargement (thread, nombre de groupes,
  temps). But : savoir si les groupes existent déjà à la sortie de
  `LoadGame` (chargement synchrone) et sur quel thread. Non testé.
  **Résultat (2026-10-29)** : tout le chargement est **synchrone sur le
  thread du jeu** (29924 = celui de `Translate` / `ModifyResource`) ; les
  composants de groupe sont reconstruits dans la lecture (pile `[TRACE]` :
  `FUN_14101f110` ← `FUN_141286580` ← `FUN_141286840` ← `LoadGame`),
  **0 groupe** inscrit à la fin de la lecture, **4/4** à la vérification
  après chargement et à la **sortie de `LoadGame`** → **point de
  recollage = sortie de `LoadGame`** (`GroupesApresChargement`, appelé par
  le hook). `LoadGame` est aussi appelé une 2e fois juste après, sans effet
  (entrée/sortie à t+0). La partie redevient « prête » (`PartiePrete`) bien
  plus tard : F9 juste après le chargement → `partie pas prete`. Relevé
  avant sauvegarde : notre groupe `SurvivorsFromExpeditionSite*6986`,
  `entité +0x18/+0x20` = 128 bits aléatoires (GUID d'entité ?), `+0x28` =
  `CompoundTemplate`, `+0x30` = `0x2000000005`. Relevé `[SAV]` désormais
  fait à la sortie de `LoadGame`. Non testé.
  **Relevé après chargement (2026-10-29)** : nom d'entité (`…*6986`),
  identifiant `entité +0x18/+0x20` (`9A92A21ED66D5C94 / 86EEAD4EAA9B43E4`),
  survivants (5) et **dates du trajet** identiques avant / après ; template
  = celui du jeu (nom, icône perdus). → **clé = identifiant 128 bits de
  l'entité** ; durée et survivants : natifs, rien à recoller.
  **Implémenté (non testé)** : fichier **`mods\ap_groupes.txt`** (un pour
  toutes les parties ; ligne `G <id 32 hex>\t<sansMorts>\t<icône>\t<nom>\t
  <description>\t<titre bulle>\t<texte>\t<choix>\t<res=q;res=q>`, ajoutée à
  la création par `SauverGroupe`, jamais effacée, la dernière ligne d'un id
  l'emporte). **Recollage dans le hook de `FUN_1418b1cc0`** (composant
  ajouté, appelé aussi pendant le chargement) : entre `GroupesAvantChargement`
  (entrée de `LoadGame`) et `GroupesApresChargement` (sortie), chaque groupe
  dont l'id est dans le fichier reçoit un slot, sa copie du template
  (`PoserCopieTemplate`, avant l'original : le jeu recopie lui-même nom et
  icône dans le composant de carte `entité +0x2A0 + type × 8` : `+0x30` nom,
  `+0x40..+0x50` icône), cargaison et textes de bulle ; slots remis à zéro au
  1er groupe du chargement ; taux de mort remis à 0 à la sortie si
  `sansMorts`. `SuivreGroupes` ne vide plus les slots au changement de
  génération : il garde ceux dont le composant, l'entité et l'id
  correspondent (adresses réutilisées d'un chargement à l'autre).
  Limite : `surArrivee` (pointeur de fonction) n'est pas sauvegardé.
  **Test** : groupe bien enregistré (`BFD163B16C8C39CF…`), mais **aucun
  appel du hook de `FUN_1418b1cc0` pendant `LoadGame`** : les groupes
  chargés ne passent pas par « composant ajouté », le composant de carte
  garde nom et icône désérialisés. → **recollage à la sortie de
  `LoadGame`** (seulement si la lecture a réussi, `GroupesLectureFaite`
  appelée par le hook de lecture) sur tous les groupes non arrivés :
  `RecollerGroupe` (slot + copie du template) puis **`MettreAJourCarte`** :
  composant de carte `*(entité + 0x2A0 + FUN_1412ec200() × 8)`, nom
  `*FUN_141a0f1c0(comp)` (+1 réf. `FUN_140e1ce20`, ancien relâché
  `FUN_140e1d180`) en `+0x30`, icône `FUN_141a0f2a0(comp)` copiée par
  `FUN_140e4f9a0` en `+0x40` + case / nombre de cases `+0x48 / +0x50`.
  (`FUN_141776ba0` = drapeau de sélection, sans rapport.) Non testé.
  **Test** : recollé, **liste de droite OK** (nom, logo), mais **icône de
  la carte vanilla** et **inventaire vidé** au rafraîchissement suivant.
  Causes : (1) le marqueur de carte (`FrostpunkWorldMapMarkerComponent`,
  0x38 octets, sans icône propre) lit l'icône à l'activation des groupes,
  **entre la fin de la lecture et la sortie de `LoadGame`** → trop tard ;
  (2) `Res_Definition` renvoie l'entrée − 8 tant que le contexte vient d'un
  appel du jeu, **l'entrée elle-même après `Res_InitDepuisEconomie`**
  (deux ancrages incohérents). → recollage **dès la fin de la lecture**
  (`GroupesLectureFaite`) sur les `SurvivorGroupComponent` **construits
  pendant `LoadGame`** (`GroupesComposantConstruit`, appelé par le hook du
  constructeur `+0x1A0EB70` de mod_common), si entité et template sont déjà
  là ; filet à la sortie de `LoadGame` (`RecollerSiNotre`, sans doublon) ;
  `EntreeRessource` : GUID de la table (`Res_Guid`), candidats `def`,
  `def + 8`, `def − 8`. **VALIDÉ le 2026-10-29** (carte, liste, inventaire
  après rechargement).
  **Messages AP (2026-10-29)** : après rechargement, seul le message
  « ouvrable » restait, sans contenu, icône et couleur du jeu. Causes : textes
  seulement en mémoire (8 places, id recyclé) ; entrée sauvegardée = notre
  copie relue comme `CitizensDied` vanilla (importante → orange, icône du jeu),
  or tout le code d'icône / couleur reconnaissait un message AP à notre copie.
  → **id unique croissant** (suite du plus grand id de
  **`mods\ap_messages.txt`**, ligne `M <id>\t<icône>\t<titre>\t<texte>`,
  `SauverMessage` à l'émission) ; `ApNotifTexte` passe par un cache de 64
  (`NotifParId`) qui relit le fichier pour un id inconnu (absence mémorisée) ;
  clés allouées par message (`CleMoteur`, plus de blocs statiques recyclés
  `ApCle`) ; dans `hkAjouterPopup`, un message dont la clé de titre (`msg
  +0x98`) est `@AP/Notif/<id>/…` et dont l'entrée n'est pas notre copie
  reçoit **avant l'ajout** `EntreeDeNotre(icône de l'id)` en `msg +0x80`
  (référence `CitizensDied` prise sur le message si `PartieTick` ne l'a pas
  encore posée). **Test : contenu et couleur OK, logo du jeu.** Les
  messages rechargés sont ajoutés **pendant `LoadGame`** ; l'icône posée sur
  `icon` y est perdue. → `ReposerIconesMessages` (contrôle + entrée gardés
  dans `ControleColore`) à la sortie de `LoadGame` (`MessagesApresChargement`)
  et au 1er passage de `PompeNotifications`. Non testé.
  **Bulles AP en attente (2026-10-29)** : sauvegardées par le jeu avec le
  GUID de l'original (« Survivors group ») → rechargées comme l'original
  (arrivée de groupe vanilla ; un piège perdrait sa perte). Décompilé de
  `FUN_1417cde10` : dilemmes actifs `système +0x21A0` (compte `+0x21A8`),
  enregistrements de 0x90 : `+0x00` id (compteur `système +0x2198`),
  `+0x10` entrée, `+0x20` contexte, `+0x78` **expiration**, `+0x80`
  marqueur. → **`mods\ap_dilemmes.txt`** : `P <id> <expiration>\t<id du
  catalogue>` (`DilemmeAPCreer`) et `G <id> <expiration>\t<titre>\t<texte>\t
  <choix>` (`hkCreerDilemme`, bulle d'arrivée d'un groupe AP, textes via
  `GroupeTexte`). **`RecollerBulles`** à la fin de la lecture puis à la
  sortie de `LoadGame` (`DilemmesAPChargement(0|1|2)`, appelée par les hooks
  de mod_common) : pour chaque enregistrement dont (id, expiration) est dans
  le fichier, slot de piège, copie (piège : `ConstruirePiege` depuis le
  catalogue ; groupe : `DilemmeCloner` avec clés `@AP/Dil/<slot>/T|X|C0`),
  posée en `enregistrement +0x10`. Bulle de groupe rechargée : `def` nul, pas
  d'effet AP à la résolution.
  **Test** : recollage seulement à la sortie de `LoadGame` (à la fin de la
  lecture, le système des dilemmes n'est pas lisible) ; **plantage au clic
  sur un choix du piège rechargé**. Cause (décompilé `FUN_1417d40c0`,
  résolution) : table des occurrences **`système +0x21B8`** (hachage indexé
  par l'adresse de l'entrée, éléments de 0x38 en `+0x21C0`) ; la résolution y
  cherche l'entrée et lit `*(+0x21C0) + 0x18 + index × 0x38` **sans vérifier
  l'index** ; la table du chargement porte l'entrée d'origine, pas notre
  copie → index −1 → lecture hors du tableau. → `InscrireOccurrence` :
  `FUN_1417cfc40(table, &entrée)` (chercher) puis `FUN_1417d16d0(table,
  &{entrée, DAT_142c2fca8, 0, 0, 0, 0})` (insérer, comme `FUN_1417cde10`),
  **avant** de poser la copie dans l'enregistrement. Autre structure indexée
  par l'entrée : `système +0x2218` (cherchée, ignorée si absente).
  **VALIDÉ le 2026-10-29** (plus de plantage, choix et pertes OK), sauf
  l'**icône de la bulle** : `FrostpunkUIDillemaTracker` (vtable
  `0x142217550`), mise à jour **`FUN_141c19460(tracker)`**, ne pose l'icône
  **qu'une fois** (drapeau `tracker +0x80`) : marqueur `*(tracker +0x28)` →
  indice `FUN_1417cfbf0(système, marqueur)` → entrée `FUN_1417cfd70(système,
  indice)` → tuile `entrée +0x238` (si `+0x230`) posée sur l'image
  `**(tracker +0x60)` par `FUN_141a33780(image, tuile)` ; au chargement, c'est
  fait avec l'entrée d'origine, avant le recollage. → hook
  `hkMajTrackerDilemme` : une fois par tracker dont l'entrée est une de nos
  copies de piège, repose la tuile avec `FUN_141a33780`. **VALIDÉ le
  2026-10-29** (tous les visuels après rechargement).
  **Technologies (2026-10-29)** : demande de l'auteur, **sauvegarde locale
  prioritaire**, le serveur viendra compléter. État du mod jusqu'ici en
  mémoire seulement, et jamais remis à zéro d'une partie à l'autre :
  `TechRuntime {researched, received}` par hash, `g_progressive[arbre]`,
  `g_sinceBonus`. → **`mods\ap_technos_<GameId>.txt`** (`T <hash> <r> <c>`,
  `A <arbre> <n>`, `B <n>`), réécrit en entier (fichier temporaire puis
  `MoveFileEx`) par `TechSauver` après `OnResearchComplete` et
  `OnProgressiveReceived` ; **cumulatif** (charger une sauvegarde plus
  ancienne garde checks envoyés et items reçus, comme le serveur AP).
  `TechPersistanceTick` (tick d'une seconde, après `CaptureRunState`) : à
  chaque nouvelle génération ou nouveau `GameId`, `RT_Reset`, compteurs à 0,
  relecture, puis `SyncGameState` de chaque techno connue ; `TechSauver`
  n'écrit rien avant cette relecture (sinon un chargement écraserait le
  fichier par un état vide). Non testé.


## 2026-11-04 — Sauvegardes de la session Archipelago seulement

Bug du premier test réseau : avec le mod, on ne doit pas voir les parties
faites sans lui. Décisions de l'auteur : **masquer** dans la liste (pas
refuser le chargement), et seulement les sauvegardes **de la session**
(graine + slot). `sauvegardes_ap.cpp` (journal `[SAV]`) :
- `FUN_1412892f0(profil, nom, p3, p4)` = écriture d'une sauvegarde (thread
  principal, « [GameSave] Game save created on the main thread ») ; `nom`
  = `wchar_t*` sans extension, fichier `<nom>.save` dans `saves`. Hook : nom
  ajouté à `<session>\ap_sauvegardes.txt` (UTF-8, sans doublon).
- `FUN_141284eb0` = liste des sauvegardes : `FUN_141cdee60(profil, &liste,
  L"saves", L"*.save")` (seul appelant), `liste = {+0 tableau de chaînes du
  moteur, +8 nombre, +0xC capacité, +0x10 allocateur}`, éléments =
  `"<nom>.save"`, libérés par `FUN_140e1d180(&élément)`. Hook : éléments
  absents de la session libérés et retirés, nombre réécrit.
Conséquence : sans session (jamais connecté), aucune sauvegarde visible ; une
sauvegarde faite avant ce correctif est masquée (la réécrire une fois). Non
testé (liste, Continue, sauvegarde automatique).