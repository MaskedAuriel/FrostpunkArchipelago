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
