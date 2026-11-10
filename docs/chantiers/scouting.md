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
