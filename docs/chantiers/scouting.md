# Scouting : survol, landmarks, sauvegarde empoisonnee, debug des trajets

> Historique detaille, copie mot pour mot depuis CLAUDE.md le 2026-11-02.
> Le resume a jour est dans CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

- **Survol d'un scout** : identifier l'expédition sous le curseur.
  **Infobulle de la case AP dans la liste de droite (2026-10-30, non
  testé)** : elle n'affichait qu'un check sur deux. Cause : la case est
  remplie par `FUN_141c2b6e0` (`FrostpunkUIExpeditionControl`), le texte de
  l'infobulle est la traduction de `@Resources/Names/AP_Item` faite à ce
  moment (`FUN_1416b7350` → `FUN_141ce67f0`), gardée telle quelle (+1 réf.)
  par `FUN_1419636e0` dans `FrostpunkUISimpleTooltipPanelArgs +0x38`. Or la
  liste ne visait que l'expédition survolée / sélectionnée **sur la carte**
  (`ExpeditionVisee`) → aucune pendant le remplissage → repli sur la
  description du site (un seul check). → `hkUICargaison` (groupes.cpp) pose
  l'expédition du contrôle (`ctrl +0x290`, poignée → composant) par
  `ScootingExpeditionAffichee` autour de l'original ; `hkTranslate` prend
  `ScootingListePourAffichage` et une **chaîne propre à chaque expédition**
  (`ApChaine` slots 4..19 : une chaîne partagée donnerait à toutes les
  cases le texte de la dernière remplie).
- **Sites landmarks** : affichage du contenu.
- **Sauvegarde empoisonnée** : écrire un hook de sauvegarde qui vide les
  cargaisons avant sérialisation.
  **2026-10-29** : le porteur AP (`g_def`, tampon statique de la DLL) garde
  la même adresse d'un chargement à l'autre dans une session ; une cargaison
  sauvegardée n'est morte qu'au 1er chargement après un lancement.
  Corrections : (1) **`DefRessourceValide`** lisait l'économie mémorisée
  (`g_selfRessource`, parfois celle de la partie précédente juste après un
  chargement) → la **vraie définition du charbon** d'une cargaison vanilla
  était jugée invalide et la cargaison vidée (journal : `[REPAIR] exp 1 :
  definition invalide 0x183AD5940D0`) ; elle lit maintenant l'économie
  actuelle (`+0x40916D0`, vrai compte du conteneur). (2)
  **`ScootingApresChargement`** (sortie de `LoadGame`, seulement si la
  lecture a réussi) : cargaisons à définition morte vidées **avant le 1er
  tick**, puis, si le porteur existe déjà, items de `ap_pending_<GameId>`
  réinjectés (GameId lu sur le système de scénario `+0x110`) ; sinon le tick
  s'en charge après l'init, comme avant. Non testé.

- **Debug : trajets d'expédition de 15 min (2026-11-01, non testé)** —
  pour atteindre vite les Londoniens d'une partie neuve (trace des quêtes).
  `FUN_1418a0a60(système, entité, composant, cible)` = envoyer une
  expédition vers une cible (`frostpunkexpeditionsystem.cpp`, seul appelant
  de `FUN_1418853b0` côté expéditions ; les autres sont groupes et
  transports d'avant-poste) ; `FUN_141885250` pose `mouv +0x50` = maintenant,
  `+0x58` = maintenant + durée. Hook `hkEnvoyerExpedition` (scooting.cpp,
  `InstallHooksExpeditions`) : après l'original, arrivée ramenée à départ +
  0,25 h si plus longue. Log `[EXP] debug : trajet X h -> 0.25 h`.
  **`EXPEDITIONS_RAPIDES = true` à remettre à false hors debug.** Inconnu :
  le temps d'exploration sur le site (hors trajet) n'est pas touché.

### 2026-11-04 — Tests grandeur nature : survivants trouvés dans un site AP

- **Problème remonté** : au premier site d'A New Home exploré, des
  survivants. Règle de l'auteur : un site, quel que soit le scénario, ne
  contient **que** le check Archipelago.
- **Cause** : `SurveillerSites` ne remplaçait que les ressources du
  composant (`+0x48`, porteur AP). Le reste du contenu tiré à la génération
  restait : `+0x60` BonusResources, `+0x78` Technologies, `+0x90` Artifacts,
  `+0xA8` OutsiderSurvivors (relevé de `sites_contenu.cpp`).
- **Correctif (compilé, déployé, non testé)** : pour chaque site AP, compte
  de ces quatre tableaux remis à 0 dès qu'il est non nul (tableau laissé au
  moteur) ; journal `[SITE] <site> : <quoi> retire(s) (n entree(s))`. Le
  gabarit n'est pas touché (ses survivants servent de modèle aux groupes AP).
- Non couverts : choix des **dilemmes d'exploration** (`+0x3D0` du gabarit)
  et **effets instantanés** (`+0x3E8`), qui peuvent aussi donner des
  ressources ou des survivants.
- **Décision de l'auteur (même jour)** : dilemmes d'exploration et effets
  instantanés **laissés vanilla**. D'après le relevé du 2026-11-03
  (`sites.md`), ils ne donnent ni ressources ni survivants ; seul
  « 63. Convoy in Distress » retire des ressources (gardé tel quel).

### 2026-11-06 — Éclaireurs tués par un dilemme d'exploration (Tesla City)

- **Demande de l'auteur** : un événement d'exploration ne doit jamais tuer
  les éclaireurs ; l'issue est forcément positive.
- **Ghidra** : la mort passe par `FrostpunkDestroyStationingExpeditionEffect`,
  exécuté par **`FUN_14168dc50`(exécuteur, effet, contexte)** : entité du
  contexte = site ou groupe de survivants ; la 1re expédition qui y
  stationne (filtre de gabarit `effet +0x28`) est détruite par
  `FUN_141895580`.
- **Correctif (compilé, déployé 20:36, non testé)** : hook
  `hkDetruireExpedition` (scooting.cpp, `InstallHooksExpeditions`) : l'effet
  n'est plus exécuté, journal `[SCOUT] effet "Destroy stationing expedition"
  ignore`. Limite : les autres effets du choix (textes, pertes de
  ressources de l'expédition) restent ; si le texte du dilemme annonce la
  mort, il ne sera pas changé.

## 2026-11-10 — Infobulle de la case AP au retour en ville

Retour de l'auteur : au retour d'un scout en ville, la liste de ce qu'il
ramène montre la case AP, mais son survol affichait le **premier check de
la partie** (repli `ApDescriptionW(g_locationAffichee)` de hkTranslate),
car à l'arrivée les checks sont envoyés, `nEnAttente` remis à 0 et la
cargaison vidée. Correctif (scooting.cpp) : `ExpCache.retour[] / nRetour`
= checks du dernier retour, copiés à l'arrivée ; `ConstruireListePortee`
les liste quand l'expédition n'a plus rien en route ; oubliés au départ
suivant de la ville (et à l'init). Compilé, non testé.

## 2026-11-10 — Pas de 2e check sur un site déjà coché

Question de l'auteur (check vu sur l'équipe en démontant le centre
d'expédition) : un site déjà visité redonne-t-il un check ? Le
ramassage ne vérifiait que la file de l'expédition (`DejaEnAttente`) ; le
serveur refusait déjà le doublon (`deja cochee : rien envoye`), mais
l'éclaireur pouvait reporter l'item AP. Correctif : `ApSiteDejaCoche`
(archipelago.cpp, location du site cochée sur le serveur,
`ReseauLocationFaite`) dans les deux chemins de ramassage de scooting.cpp
(exploration et hausse de cargaison) → rien ramassé, journal `[SCOUT] exp N
: location … deja cochee sur le serveur, rien ramasse`. Le check vu au
démontage : items en attente relus et réinjectés sur les expéditions
restantes à la relecture de la liste (`[PERSIST] Restaured this check`),
comportement existant. Compilé, déployé Epic + Steam, non testé.

### 2026-11-10 — The Arks : expédition de secours bloquée à New Manchester

Retour de l'auteur : quêtes 006 / 007 / 008 (expéditions de secours vers
New Manchester), étape 1 faite, mais l'expédition de l'étape 2 (Food, exp 5,
cargaison 1200) ne disparaît jamais une fois arrivée. Journal :
- `[NM]NewManchester` est un site AP, coché à l'exploration (l. 5156).
- 006 SHELTER réussie à l'envoi de l'exp 4 (l. 6483) ; puis, à **chaque
  chargement**, `[SITE] [NM]NewManchester : contenu substitue`
  (l. 6862, 8029, 9218) : le site avait de nouveau un contenu vanilla.
  New Manchester sert de **dépôt** aux expéditions de secours : leurs
  ressources livrées étaient remplacées par l'item AP.
- exp 5 arrive (l. 10327) ; aucune fin de 007 ensuite.

**Correctif** (`SurveillerSites`) : un site déjà coché sur le serveur
(`ApSiteDejaCoche`) n'est plus touché, ni substitution du contenu, ni
retrait du reste du contenu vanilla. Hypothèse forte, pas certitude : il
faudra repartir d'une sauvegarde **antérieure à l'arrivée** de l'exp 5
(en mémoire, le contenu était déjà substitué). Compilé ; non déployé (jeu
lancé) ; non testé.

**Retour de l'auteur (DLL de 22:06, sans sauvegarde antérieure)** : 007 et
008 se terminent (`[QUETEAP] FIN … 007` l. 3809, `… 008` l. 4333), plus de
`contenu substitue` sur New Manchester ; les automates arrivent. Les
expéditions de secours (exp 5, 6, 7) **restent** sur le site :
probablement vanilla (la quête suivante `004 PREPARE FOR THE STORM`
demande « Disband all expeditions »), non vérifié.
Second défaut vu : à l'arrivée de l'exp 5, `location … deja cochee …, rien
ramasse` puis `InjecterPorteur(c, def, 1)` quand même → cargaison de
secours (1200) remplacée par un item AP. **Correctif** : site déjà coché et
aucun check porté → cargaison non touchée. Compilé ; non déployé (jeu
lancé).

À voir (pas corrigé) : à l'arrivée d'un groupe AP en ville, le journal
`[EVT] GROUPE AP` liste des événements `[NM] ManchesterSurvivor`,
`[NM] SpawnQuest`, « Some Question Helping NM - CTA » invoqués avec le
groupe pour contexte (gabarit `SurvivorsFromExpeditionSite`).

## 2026-11-11 — Site déjà coché, re-exploré dans une sauvegarde antérieure

- **Test de l'auteur** : un site exploré dans une partie, re-exploré
  après le chargement d'une sauvegarde antérieure, a rendu ses ressources
  de base vanilla au lieu de rien.
- **Cause** : depuis le correctif New Manchester du 2026-11-10,
  `SurveillerSites` ne touche plus le contenu d'un site dont la location
  est déjà cochée sur le serveur (`ApSiteDejaCoche`) : ni substitution par
  l'item AP, ni retrait du reste. Le site non encore exploré de la vieille
  sauvegarde gardait donc tout son contenu vanilla, en double avec l'item
  du site déjà dans le multiworld.
- **Correctif** (scooting.cpp) : site déjà coché **et** pas encore exploré
  (`State +0x34` à 0) → comptes mis à 0 pour les ressources (`+0x48`),
  ressources bonus, technologies, artefacts et survivants ; journal
  `[SITE] <site> (deja coche, non explore) : <quoi> retire(s)`. Un site
  déjà exploré n'est toujours pas touché (New Manchester reste un dépôt).
- Compilé, déployé Epic + Steam (10:42, remplace la DLL déployée à 10:04
  sur Steam). Non testé.

### 2026-11-15 — Last Autumn : checks perdus sur un site de nourriture

- **Vu en jeu** (Steam, release) : l'expédition 3, porteuse de 8 checks,
  arrive à « 04. [food] Moose Hunting Grounds » ; le jeu la **détruit**
  (`0 dans la liste du jeu (avant 1), une des notres invalide`) et en
  **crée une autre** (expédition 4, autre composant, cargaison vide).
  `ScootingChargerEnAttente` ne retrouvait aucune expédition d'id 3
  (`0 item(s) restaure(s)`), puis la sauvegarde du ramassage suivant a
  réécrit `ap_pending_<GameId>.txt` avec le seul check de l'expédition 4 :
  les 8 checks étaient perdus pour de bon.
- **Correctif** (scooting.cpp) : **checks orphelins**. Une ligne du fichier
  dont l'expédition n'existe plus devient un orphelin (`g_orphelins`),
  réécrit dans le fichier sous son ancien id (`ScootingSauverEnAttente`)
  et rendu à la première **nouvelle** expédition sans check porté
  (`ExpCache.nouvelle`, posé par `RescanExpeditions` pour un composant
  inconnu à la relecture précédente ; faux pour les expéditions présentes
  au chargement). Le fichier est alors réécrit sous le nouvel id, et
  l'item porteur est réinjecté comme après un rechargement. Journal :
  `[PERSIST] N check(s) orphelin(s) ... repris par la nouvelle expedition K`
  ou `... en attente d'une nouvelle expedition`.
- Le « crash » à la fin du même journal (`+0x1C31259` Steam, lecture de
  `0x28`) était un Alt+F4 de l'auteur : sans objet.
- Compilé, déployé (Steam release, Epic dev, 17:33). **Non testé.**
- 2026-11-15 (18:28) : la reprise des orphelins **a marché** en jeu
  (`6 check(s) orphelin(s) de l'expedition 5 repris par la nouvelle
  expedition 7`, puis livrés en ville).

### 2026-11-11 / 2026-11-15 — Last Autumn : crash après un retour d'éclaireur chargé

- **Deux fois le même crash** : `Frostpunk.exe+0x16801E0` (Steam) =
  `FUN_1417e5160` + 0x80 (Epic, octets identiques), `add edi,[rcx+8]` :
  somme des quantités de la **file de production** (`+0x38`, nombre
  `+0x40`, pas 0x10) d'un `FrostpunkProductionComponent` ; le tableau
  pointe vers de la mémoire libérée. La fonction fait partie des alertes
  de bâtiment (`BuildingEmptyProductionQueue`,
  `BuildingChangeBlueprintDelay`), appelée par une vtable (`0x14440b8d0`).
- Contexte commun : retour d'un éclaireur chargé → rafale d'items reçus,
  dont **« Progressive Scaffolding Manufacture » 2/3 = `GS
  ManufactureCheaperMaterials`** les deux fois (effets appliqués la 1re ;
  la 2e, techno déjà recherchée : seul l'état 2 posé), et des effets de
  lois appliqués (Bath House / Public House). Le 11/11, l'auteur
  construisait une fabrique d'échafaudages.
- Cause **non prouvée**. Hypothèse : nos pompes appliquent les effets dans
  le hook du setter de ressources, au milieu d'une boucle du jeu.
- **Diagnostic ajouté** (`ReleveCrash`, version.cpp) : à cette adresse,
  classe du composant, entité (classe + gabarit), `+0x30`, file, compte,
  `+0x60`, et entité de l'entrée d'état. Déployé 18:31.
- Ghidra : `FrostpunkProductionComponent` `+0x30` ActiveBlueprint, `+0x38`
  ProductionQueue (`Dynarray<Pair<EntryPtr<Blueprint>, int>>`), `+0x58`
  Progress ; gabarit `+0x53` « Switch to next blueprint after availability
  change » ; système des plans : événement
  `EventBlueprintsAvailabilityChanged`. L'auteur confirme une **fabrique
  d'échafaudages en chantier** au moment du crash.

### 2026-11-15 — Crash en sélectionnant un site déjà coché après un chargement

- **Vu en jeu** : `Frostpunk.exe+0x1C31259` (Steam), lecture de `0x28`, en
  sélectionnant le site « 16. [food] Stream Head » où un éclaireur venait
  d'arriver (même signature que le crash pris pour un Alt+F4 plus tôt dans
  la journée). Fonction Epic `FUN_141ce5a40` (texte depuis une clé),
  appelée par `FUN_141bfe6c0` = panneau de sélection d'un site : pour
  chaque ressource `{définition, quantité}` du site (`+0x48`), nom lu à
  `définition + 0x28` → définition **nulle**.
- **Cause** : un site déjà coché et exploré garde notre item AP dans ses
  ressources ; la sauvegarde l'emporte et, au chargement, le jeu ne
  retrouve pas notre définition fabriquée (entrée à définition nulle).
  `SurveillerSites` ne touche plus au contenu d'un site coché (dépôts de
  The Arks), donc rien ne réparait l'entrée.
- **Correctif** (scooting.cpp, `NettoyerRessourcesSites`) : sur **tout**
  site, retrait des seules entrées à définition nulle ou illisible
  (compactage du tableau, compte réduit), à la sortie de `LoadGame`
  (`ScootingApresChargement`) et à chaque tick (`SurveillerSites`) ; les
  entrées valides (dépôts) restent. Un site AP pas encore coché ni
  exploré dont le tableau est devenu vide reçoit de nouveau l'item AP.
  Journal : `[SITE] (<quand>) <site> : N entree(s) de ressource a
  definition nulle retiree(s)`. Déployé 18:38, **non testé**.

## 2026-11-22 — « Destroy stationing expedition » limité aux sites AP

- **Demande de l'auteur** (expédition bloquée à New London) : revoir la
  neutralisation de l'effet (`hkDetruireExpedition`, depuis le 2026-11-06).
- **Ghidra `FUN_14168dc50`** : entité = `*(*contexte)` ; elle doit être un
  site (`DAT_14409ad58`) ou un groupe de survivants (`DAT_14409b698`) ; la
  1re expédition qui y stationne (filtre de gabarit `effet +0x28`) est
  détruite par `FUN_141895580`. Le jeu s'en sert donc aussi pour ses
  propres expéditions et groupes.
- **Journal du 2026-11-21** : 2 effets ignorés (l. 6122, City of Rats,
  légitime ; l. 8180, entité inconnue, juste avant l'arrivée d'un groupe AP).
  Aucun effet ignoré après l'arrivée de l'exp 5 à New London : la cause de
  ce blocage est le contenu AP du site (voir sites.md), pas ce hook.
- **Correctif** : l'effet n'est plus neutralisé que si l'entité est un site
  AP (`SiteDeLEntite` + `PayloadDuSite`) ; ailleurs l'original s'exécute.
  Journal : `... ignore sur le site AP "<site>"` ou `... execute (pas un
  site AP) : <site> - <classe et gabarit de l'entite>`.
- Compilé, déployé Steam (00:15). Non testé.

## 2026-11-22 — Étapes de route d'On the Edge (TD1-3, TG1-6) : check jamais envoyé

- **Constat (auteur)** : un éclaireur qui arrive sur une étape ne déclenche
  rien ; une étape construite par l'équipe de construction ne crée pas la
  route et n'envoie pas le check.
- **Journal (`console.log`, l. 7382-7856)** : au lancement de
  `[TradeRoutes] Ark Springs`, TG1, TG2 et TD1 passent **de l'état 0 à 2**
  (`[SCOUT] site … explore (etat 0 -> 2)`), sans expédition sur place :
  rien n'est ramassé. L'arrivée de l'éclaireur (exp 1 : TG2 puis TG1, à
  vide) ne change plus l'état, donc ne fait plus rien. L'équipe de
  construction (exp 18) ouvre les dilemmes `TD1` (« The Lost Scouts »)
  puis `TG2` ; le journal s'arrête là (aucun effet « Destroy stationing
  expedition » neutralisé, aucune trace de la construction du camp).
- **Correctif (déployé sur Steam à 13:39)** : une expédition qui arrive **à vide** sur un
  site AP déjà visité (état ≠ 0), pas coché, dont le check n'est porté par
  aucune expédition, ramasse le check comme à l'exploration
  (`[SCOUT] exp N ramasse la location … a l'arrivee (site deja visite …)`),
  envoyé au retour en ville. Diagnostic : tout autre changement d'état d'un
  site est journalisé (`[SCOUT] site … : etat A -> B`).
- **Route non créée** : cause inconnue, il faut un journal qui couvre la
  construction d'une étape jusqu'au bout. Non testé.
- **Test de l'auteur (13:44)** : le ramassage à l'arrivée marche (TG1, TD1 ;
  `TG1 : etat 2 -> 3` quand le camp est construit). Rock Shelf (TG2) jamais
  ramassé : exp 18 y arrive en portant déjà le check de Gentle Descent
  (porte 1), exp 19 avec celui de Mountain Pass. Correctif (compilé à 13:44,
  déployé sur Steam à 13:46) : une cargaison faite de nos seuls items AP
  (`cargo == nEnAttente`) compte comme vide.
