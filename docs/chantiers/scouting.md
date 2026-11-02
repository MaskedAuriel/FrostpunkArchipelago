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
