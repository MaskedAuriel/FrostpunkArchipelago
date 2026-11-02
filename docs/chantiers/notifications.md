# Icone et couleur des messages (ex-section 10)

> Historique detaille, copie mot pour mot depuis CLAUDE.md le 2026-11-02.
> Le resume a jour est dans CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

> **Icône et couleur des popups (2026-10-28, non testé)** —
> `FrostpunkMessageEntry` (réflexion `FUN_1419e76e0`, 0xB8, entrées au pas
> de 0xE0) : `+0x18` nom, `+0x30` Enabled, `+0x38` Parameters, `+0x50`
> Title, `+0x58` Description, `+0x60` Image et **`+0x78` Icon**
> (`UITextureTile`), `+0x90` Cooldown, `+0x98` Category (lien GUID vers
> `FrostpunkMessageCategoryEntry` : `+0x18` type, `+0x1C` Important, `+0x20`
> Duration, `+0x24` World map), `+0xA8` Type (`eMessageType` : 0
> Notification, 1 Openable notification, 2 Modal, 3–7 Modal …, 8 Tutorial,
> 9 ResearchFinished), **`+0xAC` Important**, `+0xB0` Duration, `+0xB4`
> World map. **Aucun champ couleur.** Panneau (`FUN_141aab820`,
> `frostpunkuinotificationpanel.cpp`) : tuile de l'enfant `"ICON"` donnée
> par la notification (méthode `+0xF0`), texte `"TEXT"`, et **l'orange =
> état d'animation `IMPORTANT_NOTIFICATION`** joué
> (`FUN_141024090(bouton, nom, 0, 2)` → animateur `contrôle +0x1E8`) si
> l'entrée est importante (`+0xAC`, ou `+0x1C` de la catégorie si le lien
> se résout). Presets d'interface : `BASIC_NOTIFICATION`,
> `WORLD_MAP_NOTIFICATION` (`FUN_141aaa630`). → `EntreeDeNotre(icone)`
> (MessageManager) : **copie de CitizensDied par icône**, Important = 0,
> catégorie vidée, icône = `TuileAtlasBulles(icone)` ;
> `AfficherNotificationAP(titre, texte, icone = "icon_vanilla")`, les
> pièges passent leur icône. Une couleur arbitraire demanderait un autre
> état d'animation ou l'accès à la couleur du contrôle (à chercher).
> L'auteur ne veut **pas de couleur précise** (juste ne plus être celle du jeu).
> **1er test : non concluant.** Relevé : CitizensDied = type 1, importante 1,
> catégorie nulle, icône `"UI/Tiles/Icons.dds"` case (1, 9) sur (16, 16)
> (même atlas que les bulles, mais nom avec **barres obliques**, alors que le
> marqueur des bulles donne `"UI\Tiles\Icons.dds"`). Message : `+0x80` =
> entrée (`FUN_141aa9780`), icône = `FUN_141687ac0` → `entrée +0x78`, sauf si
> `message +0x20` (icône propre en `+0x28`) ; preset = `FUN_141aaa630`
> (BASIC / WORLD_MAP). Vtables de notification : `0x142036AD8`,
> `0x142036C08`, `0x1421910E0`. → Correction : la copie garde la texture
> d'origine et ne change que la case. Non testé.
> L'auteur : au 1er test **rien n'avait changé** (orange + icône du jeu),
> alors qu'une mauvaise texture aurait au moins vidé l'icône → l'affichage
> ne lit sans doute pas notre copie. Émission `FUN_1416896B0` : si la file
> est active et `entrée +0x30` (Enabled), `FUN_141686530(file, &message)` puis
> événement `FUN_141687f80(file + 0x30, entrée)`. → Hook de diagnostic
> `InstallTracePopups` (MessageManager) sur `FUN_141aab820` : log `[POPUP]`
> classe de notification, message, entrée lue (« NOTRE COPIE » ou non),
> Important, tuile d'icône lue.
> **Résultat** : nos popups = `FrostpunkModalMessageNotification`, entrée
> lue = **notre copie**, Important 0, icône lue (7, 1) / (11, 1)… et
> **aucun changement visuel**. La case d'origine (1, 9) est une bulle de
> dialogue générique (aussi `SteamOff`, `GeneratorDown`) : l'enfant `ICON`
> n'est pas le symbole visible du cercle orange, qui vient d'ailleurs
> (preset / classe modale ?). Autres classes vues : `FrostpunkNotification`
> (OutOfCoal), `FrostpunkTutorialMessageNotification` (icône propre au
> message, texture nulle).
> **Capture de l'auteur** : popup AP = anneau orange + **loupe** (case
> (12, 4) de `Icons.dds`) : symbole et anneau propres aux notifications
> **ouvrables** (`FrostpunkModalMessageNotification`, type 1, comme
> CitizensDied qui ouvre la liste des morts) ; l'icône de l'entrée y est
> ignorée. Les notifications simples (`FrostpunkNotification`, type 0, ex.
> OutOfCoal) affichent l'icône de leur entrée. → la copie passe en **type
> 0**. (Capture d'écran : Win + Alt + Impr écran, ou fenêtré sans bordure
> — la capture Windows échoue en plein écran.)
> **L'auteur veut garder l'ouverture du détail au clic** (texte long d'un
> check) → type 0 **suspendu** (commenté). Constat : la vtable modale est
> `0x142036C08` (COL `0x142324548`), la simple `0x142036AD8` ; elles ne
> diffèrent que par la réflexion et `+0x128` (renvoie la fiche
> `DAT_1440a7e68` / `DAT_1440a7e70`) ; `+0x118`/`+0x120` (clics) =
> bouchon vide `0x140D82740`. Clic : `FUN_141aad120(panneau, evt)` (bouton
> `evt +0x24` : 0x10000 gauche / 0x10001 droit) → événement `panneau
> +0xD0` (gauche) ou `+0x108` (droit). La loupe et l'ouverture sont donc
> décidées par du code d'interface qui teste la classe. → hook
> `hkClicPopup` : liste les écouteurs de ces événements (`[POPUP]`, adresses
> Ghidra). Pistes : (A) ouvrable + remplacer la loupe ; (B) simple + ouvrir
> nous-mêmes le détail.
> L'auteur préfère (A). Session suivante : **aucune ligne de clic** dans le
> journal (pas de clic, ou clic hors de `FUN_141aad120`). Chaînes vérifiées
> sans succès : `MESSAGE_ICON` (panneau d'expédition, `FUN_141ba2b20`),
> « Default notification icon » (réglage de `FrostpunkUIConfig`,
> `FUN_14130ef90`). → **Essai A/B** : entrées `icon_vanilla` (F7, F11,
> groupes) en **type 0**, entrées `icon_trap` (F6) en type 1 ; log
> `[MSG] essai A/B`. Non testé. **Exigence de l'auteur : tous les messages
> Archipelago restent ouvrables (type 1)** ; l'essai A/B est temporaire, à
> retirer après le test. Seule voie acceptée : (A) garder l'ouvrable et
> remplacer la loupe / la couleur.
> **Résultat de l'essai A/B** : message F7 en type 0 → classe
> `FrostpunkNotification`, notre copie lue, icône (7, 1)… **toujours la
> loupe**. La loupe ne vient ni du type, ni de la classe, ni de l'icône de
> l'entrée. Clic gauche : `[POPUP] clic (bouton 0x10000)`, **0 écouteur**
> sur `panneau +0xD0` et `+0x108` (l'ouverture du détail passe ailleurs).
> `FUN_1410260c0(contrôle, nom, tuile, x, y, nx, ny)` = poser une tuile sur
> l'enfant `nom` (s'il existe et est une image, sinon rien). Hypothèse : le
> gabarit du message n'a pas d'enfant `ICON` visible, la loupe est une
> image fixe du gabarit (case (12, 4) de `Icons.dds`). **Essai retiré** :
> tous les messages AP en type 1.
> L'auteur : **les icônes des messages du jeu varient** (ex. découverte d'un
> site d'exploration). Or **toutes** les entrées relevées (`SteamOff`,
> `GeneratorDown`, `CitizensDied`, `OutOfOtherResource`) ont la même icône
> (1, 9) : l'icône du cercle vient d'une **donnée du message** (paramètres /
> contexte ?), loupe par défaut. Méthodes de notification vues : `+0xE0`
> texte (`FUN_141687c40`), `+0xE8` description (`FUN_141687800` →
> `FUN_141aa9a80`), `+0xF0` icône d'entrée, `+0xF8` durée
> (`FUN_141aaa4f0`), `+0x100` preset. → `[POPUP]` vide maintenant chaque
> message (`msg+0x00..0xE8`, classe ou texte de chaque qword) pour comparer
> un message à loupe et un message à icône.
> **Résultat (capture « Scouts have reached… » = pioches)** :
> `TechnologyResearched` (`FrostpunkOpenPanelNotification`, ouvrable) porte
> une **icône propre au message** : `msg +0x20 = 1`, `msg +0x28` = tuile
> (`"UI/Tiles/tech_tree_atlas.dds"`, case (0, 2) sur (20, 20)) ;
> `SiteExplored` n'a ni icône propre ni icône d'entrée mais un **contexte**
> (`msg +0x40` poignée d'entité, `+0x50..+0x68` positions) ; nos messages
> n'ont ni l'un ni l'autre → **loupe par défaut**. Message (`FUN_141aa9780`,
> 0xF0) : `+0x20` drapeau icône propre, `+0x28` tuile, `+0x40` poignée de
> contexte, `+0x50`/`+0x60` positions, `+0x80` entrée, `+0x98` clé de titre,
> `+0xA0` clé de texte, `+0xA8`/`+0xC0` paramètres, `+0xD8` = −1.
> → `PoserIconeMessage` : après `CreerMessage`, copie de la texture de notre
> entrée dans `msg +0x28` par **`FUN_140e4e990(dest, src)`** (copie de
> NameString avec comptage, `+0xE4E990`), case et nombre de cases, puis
> `msg +0x20 = 1`.
> **Test : toujours la loupe**, alors que le message porte bien `+0x20 = 1`
> et la tuile `"UI/Tiles/Icons.dds"` (7, 1) — exactement la forme de
> `TechnologyResearched`. **`FUN_141688210(système de messages,
> notification)` = clic sur un message** : `OpenPanelNotification`
> (fiche `DAT_1440a7e60`) → ouvre le panneau `notification[0xC]`
> (`FUN_141961de0(*DAT_14409f938, id)`) ; `ModalMessageNotification`
> (`DAT_1440a7e68`) → recopie le message (`FUN_141aa95a0`, 0xF0), type
> `+0xD8 = 2` (modal), l'émet : **c'est la fenêtre de détail** ; type 1 de
> notification = tutoriel. Seule différence restante avec la techno : la
> texture. → essai 2 : icône propre = texture de l'**atlas de l'arbre**
> (`techno +0x78`, grille 20 × 20, copiée par `FUN_140e4e990`), case
> `techCol/techRow` (vanilla (0, 7), trap (0, 4)).
> **Test : toujours la loupe.** La loupe = case **(12, 4) de
> `UI/Tiles/Icons.dds`** (atlas #2350, case non modifiée), quelle que soit
> la tuile posée (entrée ou icône propre, Icons ou atlas de l'arbre) : le
> cercle n'utilise pas notre tuile. Hypothèse : **icône par défaut** de
> l'interface quand le message n'a pas de contexte. **`FrostpunkUIConfig`**
> = instance statique **`DAT_1440615C0`** (`base + 0x40615C0`, construite
> par `FUN_141308da0` depuis `FUN_14010ac40`, vtable `0x141EE75B0`,
> réflexion `FUN_14130ef90`) ; `+0x60` / `+0x68` durées courte / longue des
> messages, **`+0x258` « Default notification icon »**. Aucun accès direct
> à `0x144061818` (lu via un pointeur). → `hkAjouterPopup` journalise
> cette tuile et, **pour nos messages seulement**, remplace sa case par la
> nôtre le temps de l'ajout au panneau, puis la restaure.
> **Résultat** : l'icône par défaut vaut **(1, 9)** et non (12, 4). La
> « loupe » de la capture était en fait la **bulle de dialogue (1, 9)**
> (erreur d'identification) ; c'est aussi la case de toutes les entrées du
> jeu. Substitution faite (`(1,9) -> (7,1)`), **aucun changement visuel**.
> Aucun accès par adresse directe à `UIConfig +0x258..+0x268` (lu par
> réflexion, ou inutilisé). L'essai « atlas de l'arbre » n'a rien posé
> (`tuile de techno 0x0` : aucune techno avec 20 × 20 en `+0x88`).
> → Hook de `FUN_1410260c0` (pose d'une tuile sur un enfant), journalisé
> seulement pendant l'ajout de NOS messages (`[POPUP]   pose de tuile sur
> l'enfant "…"`) : on saura quelle tuile l'enfant `ICON` reçoit vraiment.
> **Résultat décisif** : `pose de tuile sur l'enfant "ICON" : texture
> "UI/Tiles/Icons.dds" case (7,1) -> controle 0x0` — notre tuile arrive bien,
> mais **le gabarit du message n'a pas d'enfant `ICON`** (retour nul :
> `FUN_141025ff0` ne trouve pas l'enfant, ou ce n'est pas une image) ;
> l'image est jetée. L'icône visible vient d'un autre enfant. → relevé de
> toutes les poses de tuile pendant l'ajout de **tous** les messages, pour
> voir quel enfant reçoit l'icône variable d'un message du jeu (pioches de
> `SiteExplored`).
> **Résultat** : la pose sur `ICON` échoue (`controle 0x0`) pour **tous**
> les messages, y compris ceux du jeu (tutoriels, SteamOff, CitizensDied,
> OutOfOtherResource) → l'icône du cercle **ne vient jamais de l'entrée ni
> du message** ; hypothèse : elle vient du **preset** (`BASIC_NOTIFICATION`
> = bulle de dialogue, `WORLD_MAP_NOTIFICATION` = pioches, choisi par
> `FUN_141aaa630` selon `FUN_141aaa6c0(message)`). Arbre des contrôles
> d'interface (`FUN_141025ff0`) : `+0x1D8` nom, `+0x220` premier enfant,
> `+0x240` frère suivant. Enregistrements du panneau : `panneau +0x190`
> (compte `+0x198`, pas 0x30 : `+0x00` contrôle, `+0x28` notification).
> → `DecrireControleDuMessage` journalise l'arbre (nom, classe RTTI) du
> contrôle de chaque message ajouté (12 premiers), pour trouver l'élément
> image du preset.
> **Arbre relevé (identique pour tous les messages)** : `NOTIFICATION` →
> `BUTTON` (UIButton) → `clip` → `move` → `bg_blur`, `bg_tutorial`
> (→ `gleam_texture`), `line_tutorial2/3`, **`TEXT`** (UITextBox), `CLOSE`
> (UIButton → `bg`, `bg`, `stroke`, `x`) ; et `BUTTON` →
> **`static_left_notification_circle`** → `highlight`, **`notification_icon`**
> → `blur`, `bg`, `decorative_highlight`, `small_bg_rotation` (→ `small_bg`,
> `small_bg_thin_frame`), **`icon`** (UIPicture), `outer_line`. Le panneau
> pose l'icône de l'entrée sur **`"ICON"`** (majuscules) : introuvable →
> l'image du cercle vient du preset. Les contrôles sont **recyclés** (même
> adresse pour TechnologyResearched puis SiteExplored).
> → `PoserIconeCercle` : après l'ajout d'un de NOS messages, pose la tuile
> de notre entrée sur **`"icon"`** par `FUN_1410260c0` (nom construit par
> `FUN_140e4fa20`, détruit par `FUN_140e4eab0`). Log `[POPUP]   icone du
> cercle posee sur "icon" … -> element 0x…`. À vérifier : pas de fuite de
> notre icône sur un message du jeu qui réutilise le contrôle.
> **Test : icône OK**, mais tout le rond (icône comprise) est teinté orange.
> **Couleur des éléments d'interface** (décompilés du 2026-10-28) :
> `UIElementRecipe` (réflexion `FUN_14110fc60`) : `Color` (HDRColor, recette
> `+0xF0`) et `Color mode` (`+0x108` : Static / Multiply alpha / Multiply
> color) ; élément (`FUN_141023250`, 0x280) : **couleur linéaire `+0x120`**
> (blanc par défaut), écrite par la tâche 0x18 du thread de rendu
> (`FUN_1410fe640`) ; **`FUN_1410297f0(élément, float[4])` = poser la
> couleur**, entrée en gamma (2,2 / 2,2 / 2,2 / 1), mise en file (thread du
> jeu). Presets / états (`FUN_1410240d0`) = transitions de l'animateur
> `élément +0x1E8`, appliquées récursivement aux enfants.
> → `ColorerCercle` (MessageManager) : dans `static_left_notification_circle`,
> `icon` en blanc opaque, éléments teintés (r, g, b différents) en bleu-vert
> `TEINTE_AP` (ou blanc + enfants teintés s'ils sont sur le chemin de `icon`),
> alpha gardé, éléments transparents ignorés ; à l'ajout puis 1,5 s après
> (`PompeNotifications`) ; relevé `[POPUP] couleurs du cercle`. Couleurs
> d'origine mémorisées et **rendues** quand un message du jeu réutilise le
> contrôle. Non testé.
> **Test** : contour bleu OK, mais avec un **délai**, et **contour jaune**
> sur un message de site ensuite. Relevé : pas de multiplication par le
> parent ; le **preset pose lui-même l'orange juste après l'ajout** sur 3
> éléments : `bg` (1, 0.619, 0, 0.698), `decorative_highlight` (1, 0.523,
> 0.058, 1), `icon` (1, 0.619, 0, …) ; à l'ajout tout est encore blanc (d'où
> le délai : seule la passe à 1,5 s agissait). La « restauration » rendait
> l'orange relevé à 1,5 s → contour jaune du message de site.
> → v2 : `COULEURS_AP` (bleu-vert pour `bg` / `decorative_highlight`, blanc
> pour `icon`) posées à l'ajout par le poseur du moteur, puis
> **`CouleursMessagesTick`** (boucle du mod, 16 ms, 2 s) les repose par
> écriture directe en `+0x120` si le preset les reprend (log `le preset a
> repris la main N ms apres l'ajout`) ; **avant** l'ajout d'un message du
> jeu, si le panneau va prendre (dernier contrôle libre, `panneau +0x1A8` /
> `+0x1B0`, tutoriels `+0x1F8` / `+0x200`) un contrôle colorié par nous, ses
> éléments repassent en blanc : son preset décide. Non testé.
