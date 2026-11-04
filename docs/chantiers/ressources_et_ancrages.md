# Ancrages, chargements, couche archipelago, ressources (ex-section 9, En cours)

> Historique detaille, copie mot pour mot depuis CLAUDE.md le 2026-11-02.
> Le resume a jour est dans CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

### En cours

- Suppression du balayage (15,8 s) au profit des ancrages et captures :
  **validée le 2026-10-15** — `[ANCRES]` en **0 ms**, mêmes nombres que le
  scan (42/3/1/1/2/1/1/124/1/510), y compris après rechargement.
- Observations des chargements (2026-10-15) :
  - Templates (gabarits, entrées) **non recréés** au rechargement, même vers
    un autre scénario (124 → 125 constructions) ; ScenarioSystem et
    DifficultySystem gardent leur adresse.
  - **Bug corrigé** : en passant à un autre scénario, l'économie
    (`+0x40916D0`) a été réallouée **à la même adresse** → `PartieRechargee`
    ne voyait rien, pas de ré-init, et `SurveillerSites` a écrit dans un site
    de l'ancienne partie (mémoire libérée). `PartieRechargee` surveille
    maintenant aussi le système de technologies (change à chaque chargement).
  - **Garde-fou ajouté** dans `ScootingTick` : si les listes `+0x2078` des
    systèmes de sites / d'envoi ne sont plus celles de l'init, on oublie la
    partie avant toute écriture (fenêtre entre le début du chargement et sa
    détection : recensement à 0 site, fausses arrivées « avant-poste »).
    **Validé le 2026-10-24** (réagit avant même `[LOADING]`). Seule la liste
    des **sites** est surveillée : celle d'envoi est réallouée en cours de
    partie quand un bâtiment d'envoi apparaît.
  - **Autre scénario (2026-10-24)** : 31 sites, `send=0`, `exp=0` — aucun
    bâtiment d'envoi encore. L'init exigeait un composant d'envoi → échec
    en boucle toutes les 5 s. Désormais `g_pret` ne dépend que des sites, et
    **`g_sending` est supprimé** : il ne servait qu'à compter les expéditions
    d'un seul des 3 `FrostpunkExpeditionSendingComponent` (bâtiments non
    identifiés). Le tick suit le compte de la liste du système `Expedition`
    (`CompteDuSysteme`, `g_compteListeExp`) et relit les expéditions quand il
    change ou qu'une des nôtres devient invalide.
  - **Validé le 2026-10-27**, après la restructuration (`PartieTick` dans
    `mod_common`) : partie de base (génération 1, 42 sites, 2 expéditions)
    puis autre scénario (génération 2, 31 sites, 1 expédition, expédition
    envoyée vers un site du scénario), sans boucle d'init ; chaque
    chargement est vu d'abord par le garde-fou de la liste des sites.

- **Couche `archipelago.cpp` (2026-10-27)**, `ReceiveProgressive` et
  `ReceiveResource` validés le 2026-10-28 (détail ci-dessous) : F7 →
  `ReceiveProgressive` sur un arbre au hasard, F11 → `ReceiveResource("coal",
  50)`, recherche terminée → `SendResearchCheck`, scout rentré en ville →
  `SendLocationCheck`. `Res_Add` réécrite : la capture de contexte avait
  disparu du hook (ni `Res_Get` ni `Res_Add` ne pouvaient fonctionner).
  **Ressources ancrées à l'init** (`Res_InitDepuisEconomie`, appelée par
  `PartieInit`) : conteneur de l'économie `+0x2240` (compte `+0x2248`,
  entrées de 0x70), `+0x00` de chaque entrée = définition, GUID à
  définition + 8 = **2e argument du setter** ; `self` = économie
  (`+0x40916D0`). Toute ressource est donc creditable sans attendre que le
  jeu la touche. Arguments 4..6 **fixés à 0** d'après les décompilés (voir
  section 7, setter de ressources) ; le hook relève encore chaque
  combinaison du jeu (`[RES] combinaison #n …`) : si l'une a `arg4=0`, c'est
  la preuve que les écouteurs acceptent une provenance nulle. Le crédit est
  appliqué au prochain appel de premier niveau du setter (en pause : au
  retour du jeu).
  Items reçus hors partie prête : **ignorés** pour l'instant (il faudra une
  file d'attente quand le réseau existera).
  - **2026-10-27** : F7 validé (popups « Technologie recue », effet OK).
    **F11 plante** : crédits mis en attente pendant la pause (2 popups OK),
    crash à la reprise, au premier crédit appliqué. Aucune trace : le relais
    du journal (tube) **perd les dernières lignes en cas de crash**.
    → `JournalImmediat` (version.cpp) écrit directement sur disque ; à
    utiliser pour toute ligne qui précède une opération risquée.
    → `AppliquerCredit` : vérifie la clé contre le conteneur (sans appeler
    `ResIdToIndex`), journalise avant/après, appelle le setter sous SEH
    (adresse fautive + remise à 0 du compteur d'événement `+0x2120`).
    Suspects : écouteur qui déréférence `p4 = 0`, ou clé fausse.
  - Résultat : clé trouvée dans le conteneur, mais **AV à
    `Frostpunk.exe+174391D`** = `FUN_141743880 + 0x9D` (statistiques, pas un
    écouteur) et **aucune** ligne `combinaison` alors que le charbon varie :
    la clé `définition + 8` est fausse. Corrigé par la déduction de la
    convention au premier appel du jeu ; `ApPorteurEstNotreGuid` accepte
    les deux conventions.
  - Test suivant : **convention = définition + 0** (confirmé). Combinaisons
    vues : `coal +2` → `arg4` = objet, `arg5 = 2` (production) ;
    `coal -1` → `arg4 = 0`, `arg5 = 1`. Puis **crash immédiat sur F11**,
    sans la ligne `appel du setter` : le journal était ouvert sans mode
    ajout, le relais a pu écraser la ligne immédiate. Corrigé
    (`FILE_APPEND_DATA`). **Bissection en cours** : `AppliquerCredit` liste
    les écouteurs (`eco+0x2108`, nombre `+0x2110`, `{objet, fonction}`) et
    les **coupe** pendant le crédit (`SANS_ECOUTEURS = true`, à remettre à
    false une fois le coupable trouvé).
  - **2026-10-28 : crédits validés** (écouteurs coupés) : charbon 212 → 262
    puis 260 → 310, cœur de vapeur 5 → 6. 11 écouteurs listés, objets sans
    RTTI, fonctions `+167BCC0, +1702E10, +1715D70, +1796960, +17E19F0,
    +18E23B0, +186E560, +166ABB0, +18CDF30, +16C9DF0, +17A9020`.
    Le **2e** F11 a planté **dans `EmettreMessage`** (entre « 5. texte
    pose » et « 6 ») pendant que le thread du jeu appliquait le crédit :
    **les popups étaient émises depuis le thread du mod**, en concurrence
    avec le jeu (qui émet aussi des notifications de seuil depuis le setter).
    → `AfficherNotificationAP` ne fait plus que **mettre en file** ;
    `PompeNotifications` émet, appelée depuis le hook du setter (thread du
    jeu, compteur d'événement à 0). **Conséquence : en pause, les popups
    attendent la reprise.** Relevé des threads (`NoterThread`, log
    `[THREAD]`) de `Translate`, `ModifyResource`, `CreerMessage`, pour
    trouver une pompe qui tourne aussi en pause.
    Écouteurs **réactivés** (`SANS_ECOUTEURS = false`) pour vérifier qu'ils
    n'étaient pas en cause.
  - **2026-10-28 : `ReceiveResource` VALIDÉ**, écouteurs actifs, popups
    par la file : 5 F11 + un crédit de 100, aucun plantage (charbon
    211 → 512, cœurs de vapeur 5 → 9). Les écouteurs n'étaient pas en cause.
    **`[THREAD]` : `Translate` et `ModifyResource` tournent sur le même
    thread** (le thread du jeu) → la pompe des popups peut passer dans
    `hkTranslate`, qui est appelé aussi en pause (à faire).
  - Règle générale qui en découle : **rien qui touche un système du jeu ne
    doit s'exécuter sur le thread du mod** (popups, crédits). Les écritures
    d'état de techno (`OnItemReceived`, F7) et le tick de scouting le font
    encore : à surveiller.


---

## Recettes de Last Autumn (2026-11-03)

Demande de la session apworld : un item de contenu de site de Last Autumn
reçu dans un autre scénario est converti en ressources de base, au coût de
fabrication (`structuralprofiles`, `steelcomposite`, `steamexchanger`).

Décompilés (Ghidra) :
- `FrostpunkBlueprintsConfig` (200 octets, `FUN_1414af330`), instance
  statique **`base + 0x407BE20`** (`FUN_1401e6d20`) : `+0x78` Production
  blueprints (Dynarray de pointeurs), `+0xB0` Extraction blueprints.
- `FrostpunkBlueprintEntry` (`FUN_1414aef40`) : `+0x00` nom, `+0x08` GUID,
  `+0x18` Requirements, `+0x30` Display name, `+0x38` Description.
- `FrostpunkProductionBlueprintEntry` (0xF0, `FUN_1414af8f0`) : `+0x70`
  Processing resources, `+0x78` Products (polymorphes), `+0x90` Production
  effects, **`+0xA8` Production cost** (`FrostpunkResourceAmount` 0x14 :
  `+0x00` lien, `+0x10` Amount), `+0xC0` Base production time, `+0xC8`
  Upgrades blueprints (liens), `+0xE0` Resource upfront consumption mode.
- `FrostpunkResourceProduct` (0x38, `FUN_141a20020`) : `+0x20` lien de
  ressource, `+0x30` Amount. Autres produits : GatherableResource,
  FoodRation, Agent.
- Il existe `FrostpunkProductionBlueprintDifficultyModification` (« Base
  production time modification ») : durée seulement, a priori sans effet
  sur le coût.

Relevé F9 ajouté : `recettes.cpp` (`RecettesReleve`, appelé par
`QuetesPompe` après `SitesContenuReleve`), lignes `[RECETTE]` B / O / I / U.
À lancer **dans The Last Autumn** (noms des ressources résolus par
l'économie de la partie). Non testé. Premier build bloqué par un symbole
d'une autre session en cours (`SendLocationCheck`, signature changée dans
`archipelago.cpp`, pas dans `scooting.cpp`).

### Relevé F9 `[RECETTE]` (2026-11-03, BUILDERS) — VALIDÉ (45 recettes lues)

Coût par unité produite (1 unité par cycle) :

| Ressource | Recette | Entrées | Durée |
|---|---|---|---|
| Wooden Materials (`structuralprofiles`) | `[Builders] Manufacture Scaffoldings` | wood 25 | 1,15 |
| | `… - cheaper` | wood 20 | 0,57 |
| Steel Plates (`steelcomposite`) | `[Builders] ManufactureSteelPlate` | steel 25, coal 50 | 2,30 |
| | `… - cheaper` | steel 20, coal 40 | 1,15 |
| Steam Tools (`steamexchanger`) | `[Builders] Factory Pipe` | steel 20 | 2,30 |
| | `… -cheaper` | steel 16 | 1,15 |

Les variantes « cheaper » sont des recettes séparées, sans lien
d'amélioration. Table : `outils\recettes_last_autumn.tsv` (les deux
variantes, en attente du choix de l'auteur). Autres relevés au passage :
CharcoalKiln 7 wood → 17 / 23 coal ; Automatons 100 steel + 100 wood +
50 coal + 1 steamcore (variantes moins chères) ; prothèses 10 / 5 steel.
Défaut connu du relevé : les noms de ressource lus en `définition +0x00`
sont les noms affichés (« Steel Plates »), pas les noms internes ; les
produits `GatherableResource` ne sont pas décodés.

**Décision de l'auteur (2026-11-03)** : la conversion retient la **variante
« cheaper »** (wood 20 ; steel 20 + coal 40 ; steel 16 par unité).
`outils\recettes_last_autumn.tsv` ne garde que ces trois lignes : **VALIDÉE**.


### 2026-11-04 — Expéditeur des popups (bug du premier test réseau)

Popup « Technology received (from Archipelago) » au lieu de « from Simon » :
`ReceiveItem` notait la réception par un simple compte, et `ApItemsPompe`
rappelait les `Receive*` avec `"Archipelago"` en dur. Correctif :
`ap_items_recus.txt` = `<id>\t<joueur>` (copie, lignes anciennes sans joueur
acceptées), `g_expediteurs[item]` dans l'ordre des réceptions ; la k-ième
application d'un item dans une partie (ou sa k-ième livraison) reprend le
k-ième expéditeur (`ExpediteurDe`). Couvert : technos, lois, température
(pompe), scénarios (direct), contenus livrés par groupe (« Heading to the
city: … (from X) », bulle « An Archipelago convoy from X … »), pièges
(sous-titre « Archipelago - from X », copié dans la file de `dilemmes_ap`,
gardé dans `ap_dilemmes.txt` pour le rechargement). `ReceiveResource` n'est
appelé que par F11. Non testé.
Complément : popups « Resource received » de l'arrivée d'un convoi = joueur
(`GroupeAP.expediteur`, gardé en champ 8 de `ap_groupes.txt`), et non plus le
nom du groupe. Compilé et déployé à 11:06. Non testé.
