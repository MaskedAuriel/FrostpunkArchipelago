# Chantier : historique de la session AP en jeu

Historique. Le résumé est dans .claude/CLAUDE.md (section 9).

## Origine et décisions de l'auteur (2026-11-21)

Retour des joueurs : voir en jeu ce qui a été reçu et envoyé dans la session.
Décisions de l'auteur :
- un **bouton** dans le HUD ouvre l'historique ;
- **panneau dédié** (clone d'éléments d'interface du jeu), pas un message ;
- contenu : **items reçus + checks envoyés**, le plus récent en haut ;
- portée : **toute la session** (graine + slot) ;
- **tout l'historique**, sans limite de lignes (d'où un défilement).

Données : dossier de session `mods\ap_<graine>_<slot>\` —
`ap_items_recus.txt` (`<id>\t<joueur>`, ordre d'arrivée), `checks.txt`
(locations envoyées, ordre d'envoi), `scouts.txt` (contenu de chaque check).
Aucune heure dans ces fichiers : historique ordonné, sans date.

## Étape 1 : relevé de l'interface en jeu (2026-11-21)

Ghidra :
- Liste des recettes `UI/` du pool (journal du menu, `ListerRep`) :
  `UI/MainScreen` (HUD), `UI/Panels/General/MessagePanel`, `QuestPanel`,
  `NotificationPanel`, `UI/Panels/City/TutorialSummaryPanel`,
  `UI/Panels/Debug/DebugMessagesListPanel`, etc.
- `FUN_14196b2d0` = entrée 37 de la vtable de
  `FrostpunkUIGameplayController` (vtable `0x1421088a0`, COL `0x142367a88`) :
  `FUN_141024ba0("UI/MainScreen", "FRAME", &DAT_142be0438)` → racine `FRAME`
  de l'écran en jeu. **Steam : `0x1807D20`** (seule fonction qui référence
  `"UI/MainScreen"` dans les deux exe ; script
  `chaine_vers_fonction.py`, parcours de `.pdata` sans capstone).
- Défilement du moteur : `UIScrollPane` (`UIScrollPaneRecipe`),
  `FrostpunkUIScrollBar` (`SCROLL_DECREASE_BUTTON`, `SCROLL_INCREASE_BUTTON`,
  `DRAG_ELEMENT`, `BAR`) ; noms d'éléments `SCROLL_PANE` / `SCROLL_BAR`,
  `GOAL_ENTRIES_SCROLL_PANE`, `DESCRIPTION_SCROLL_PANE`, `LIST_SCROLL_PANE`…

Code (`historique_ap.h/.cpp`, nouveau fichier) : hook de `FUN_14196b2d0`
(poignée sur la racine `FRAME`), puis 5 s après, sur le thread du jeu
(`HistoriquePompe`, pompe du setter de ressources), relevé `[HIST]` :
- arbre des éléments du HUD (nom, classe, position locale `+0x80`, taille
  `+0x210`, visible `+0x270`), profondeur 8 ;
- arbres des recettes `MessagePanel`, `QuestPanel`, `TutorialSummaryPanel`,
  `DebugMessagesListPanel` (nom, classe, position, ancre, mode), profondeur 12.
Relevé neutralisé en `AP_RELEASE`. DLL compilée, déployée sur Steam.
**À faire par l'auteur** : lancer une partie (Steam), laisser tourner 5 s
hors pause, renvoyer le journal.

## Légende des icônes AP : décisions et relevé (2026-11-21)

Décisions de l'auteur : **panneau de légende dédié** à côté du livre des
lois, de l'arbre des technos et de la carte du monde, icônes dessinées ;
texte **court** donnant le sens (anglais), par exemple « Archipelago:
progression item ». Même brique qu'ici (panneau cloné), donc le relevé
`[HIST]` couvre aussi `SociotechPanel`, `TechnologyPanel`,
`WorldMap/SelectionPanel` et `WorldMap/CompassPanel`. Les recettes d'interface
ne sont pas dans le schéma de réflexion (`logs\schema_reflexion.tsv` : aucune
classe `UI*Recipe`) : pas de décodage hors jeu possible. L'auteur testera
tout à la fin (2026-11-21).

## Étape 2 : bouton, panneau d'historique et légendes (2026-11-21)

Relevé de l'auteur (Steam, 16:46) :
- HUD : `FRAME` (1280 × 720, coordonnées absolues) > `HUD` > `TOP_LEFT_static` >
  `TOP_LEFT_BUTTONS` (10, 94) > `INGAME_MENU_BUTTON` (UIButton (-42, -35),
  37 × 36, image `icon` 32 × 32) ; `WORLD_MAP_HUD` (-1, 0) 1280 × 720 (carte).
  Les panneaux de debug cachés du HUD (`EXT_CHEAT_PANEL`…) ont titre,
  `CLOSE_BUTTON` et `UIScrollPane`, mais un aspect de debug.
- `TutorialSummaryPanel` = modèle retenu : `FRAME` (99, -1) > `ANIM` > cadre
  `Background` (coins, lignes, `CENTER`), `HEADER` > `COLUMN_HEADER` (`TEXT`,
  `DESCRIPTION_TITLE`), `tutorial_mask_tabs` > `LIST_SCROLL_PANE` >
  `SUMMARY_LAYOUT` > `SUMMARY_ROW` (UIButton, texte `TEXT`),
  `content_alfa_anim_container` > `DESCRIPTION_SCROLL_BAR` (FrostpunkUIScrollBar)
  et `DESCRIPTION_SCROLL_PANE` > `DESCRIPTION_LAYOUT` > `DESCRIPTION_TEXT` (+ images,
  vidéos et `CUSTOM_*` des tutoriels). Pas de bouton de fermeture.
- `MessagePanel` (`MESSAGE` : fenêtre modale centrée, `CLOSE_BUTTON`) : sans zone
  à défilement. `QuestPanel` = suivi des quêtes du HUD.
- Livre : `FRAME` > `ANIM` > `background`, `TREE_VIEW`, `CUSTOM_CLOSE_BUTTON`,
  `CATEGORIES_VIEW`. Arbre des technos : `FRAME` (640, 360) > `ANI` (633, 355)…

Ghidra :
- `FUN_141bcf2e0` = init du panneau des tutoriels : relie chaque barre à sa
  zone par `FUN_141516ec0(barre, &poignée_zone, 0, &nom_vide)` (barre +0x2E0 =
  poignée de la zone, état `SCROLLABLE` / `NOT_SCROLLABLE`). Steam `0x13B1710`
  (référence unique à `"ScrollPane is nullptr!"`).
- `FUN_141bbdf60` = init du livre (racine `**(panneau+0xA8)`, `TREE_VIEW`
  `**(panneau+0x190)`, `CATEGORIES_VIEW` +0x198). Steam `0x1A5ECA0`.
- `FUN_141bc6800` = init de l'arbre des technos (racine `**(panneau+0xA8)`,
  `TECHTREE` +0x188, barres liées par `FUN_141516ec0`). Steam `0x1A67540`.

Code (`historique_ap.cpp`, réécrit) :
- **Bouton** (`HistoriquePompe`, 1 s après la création du HUD) : copie
  d'`INGAME_MENU_BUTTON` dans `TOP_LEFT_BUTTONS`, décalée de (0, +44), icône
  `icon_progressive` (`IconeAPPoser`, lois_ap), textes « Archipelago
  history », clic → ouvre / ferme le panneau.
- **Panneau** (construit au premier clic) : copie du `FRAME` des tutoriels
  ajoutée à la racine du HUD ; enfants de `DESCRIPTION_LAYOUT` autres que
  `DESCRIPTION_TEXT` non instanciés (mode 1 le temps de l'appel) ; en-tête
  « ARCHIPELAGO » ; liste : `SUMMARY_ROW` + 2 copies = « Received items (n) »,
  « Sent checks (n) », « Close » ; titre de la description = onglet ; barre
  reliée à la zone (`LierBarre`). Texte (anglais, le plus récent en haut) :
  `<n>. <item>  -  from <joueur>` (`ap_items_recus.txt`, `ApNomItemParId`) ;
  `<n>. <location>` + `<joueur> - <item>` du scout (`checks.txt`,
  `ApNomLocationParId`, `ApNomDuCheckLocation`). UTF-8, sinon Latin-1.
- **Légendes** : fond `CENTER` redimensionné, titre « Archipelago icons »
  (`DESCRIPTION_TITLE`), 6 lignes image + texte (`small_deco_arrow` avec la
  case AP, `TEXT` de `SUMMARY_ROW`) : Progression item, Useful item, Filler
  item, Trap, Already checked, Check (contents unknown). Animateur d'état des
  copies retiré (comme les icônes des lois). Posées dans `TREE_VIEW` du livre
  (30, 430), dans la racine de l'arbre des technos (-610, 90 : **position
  devinée**, origine de la racine inconnue) et dans `WORLD_MAP_HUD` (20, 420).
  Réglages en tête du fichier (`BOUTON_*`, `LEG_*`).
- Relevé `[HIST]` gardé, coupé (`RELEVE_INTERFACE = false`).
DLL compilée, déployée sur Steam. **Non testé.**

## Test 1 de l'auteur (Steam, 2026-11-21 19:43) et corrections

Capture en ville + journal `[HIST]` : aucun plantage, tout construit.
- **Bouton** : posé, mais **exactement sur le bouton du menu** (rond violet
  en haut à gauche = notre icône AP) : l'animateur d'état copié impose la
  translation, notre `Placer` ne tient pas. Panneau jamais ouvert.
  → animateur de la copie retiré (`+0x1E8 = 0`), position de recette du
  bouton du menu + (45, 0) dans `TOP_LEFT_BUTTONS`.
- **Légende de la carte visible en ville** : la visibilité de `WORLD_MAP_HUD`
  ne s'applique pas à nos copies. → poignées des pièces gardées, montrées /
  cachées par `HistoriquePompe` selon le bit visible de `WORLD_MAP_HUD`
  (`SuivreCarte`, transitions journalisées : à vérifier sur la carte).
- **Textes** : `TEXT` de `SUMMARY_ROW` aligné à droite et en majuscules,
  `DESCRIPTION_TITLE` centré très à droite. → `TextePose` : alignement
  gauche (`UITextRecipe +0x214 = 0`) et majuscules (`+0x24C`) posés dans la
  recette le temps de l'instanciation ; titre = `TEXT` de `SUMMARY_ROW` en
  majuscules, lignes = `DESCRIPTION_TEXT` ; largeur imposée.
- **Icônes invisibles** (`small_deco_arrow`) → copies de l'image `icon` du
  bouton du menu (`IconeAPSurCopieDe`, lois_ap : case AP, blanc, montrée),
  26 × 26.
- Fond `CENTER` redimensionné : correct. Livre et arbre : pas encore vus.
DLL recompilée, déployée sur Steam.

## Test 2 de l'auteur (2026-11-21 19:48) et nouvelle demande

Journal : légende de la carte montrée / cachée selon `WORLD_MAP_HUD` (transition
« visible » vue), légendes du livre et des technos construites (technos en
(-610, 90) dans `EXT_TECHNOLOGY_PANEL` (640, 360) 900 × 600 : hors écran).
Retours de l'auteur (capture de l'arbre des technos) :
- **retirer** la légende affichée en permanence en bas à gauche ;
- **arbre des technos** : dans le vide en haut à droite (à gauche de CLOSE),
  deux entrées : logo « recherché » (Archipelago vert) + « Check Sended »,
  anneau bleu autour des technos + « Tech Received » ;
- le bouton de l'historique **ne fait rien** et **chevauche** celui du menu.

Corrections :
- Légendes du livre et de la carte coupées (`LEGENDE_LIVRE` / `LEGENDE_CARTE`
  = false ; contenu à redéfinir par l'auteur).
- `ConstruireLegendeTechnos` : à l'init de l'arbre, premier élément dont le
  nom contient `CLOSE` ; pièces ajoutées à son parent (grand-parent si
  `UILayout`), sur une ligne à gauche du bouton (`LEGT_*`) : copie de
  `icon` du bouton du menu avec `icon_done` + « Check Sended »,
  `thin_stroke` de `MESSAGE_MILESTONE` (MessagePanel) teinté (0,13 ; 0,78 ;
  0,84) + « Tech Received » (`DESCRIPTION_TEXT` aligné à gauche). Arbre du
  panneau journalisé une fois (profondeur 6) pour régler la position.
- Clic : `FUN_14105a1c0` garde l'objet de l'écouteur sous forme de
  **poignée** (`FUN_140e1a120`) ; l'objet nul passé jusqu'ici est la cause
  probable du clic muet → objet = le bouton lui-même.
- Bouton placé sous celui du menu : (0, +52).

## Test 3 (capture de l'arbre, 2026-11-21 19:57) : « pas terrible visuellement »

Ligne bien posée à gauche de `CLOSE_BUTTON` (UIButton (1006, 25) 164 × 37,
parent `top` (0, -40) du panneau). Défauts : textes ~14 plus bas que les
icônes (`DESCRIPTION_TEXT` haut de 1, le texte descend sous son origine),
« Tech Received » mord sur CLOSE, anneau `thin_stroke` presque invisible. Les
nœuds de l'arbre n'existent pas encore à l'init du panneau (relevé).
→ ligne décalée de 30 à gauche (`LEGT_LARGEUR` 330), textes remontés
(`LEGT_TEXTE_DY` = -24) ; anneau = copie d'une image du premier nœud construit
(panneau +0x1A8, `IconesTechnoPanneau` exporté par IconSubManager) dont le
nom évoque l'anneau, teinte bleue, posée par la pompe ; arbre d'un nœud
journalisé. Aucun clic du bouton d'historique dans ce journal.

## Test 4 (capture, 2026-11-21 20:01)

Textes alignés sur les icônes : OK. L'anneau choisi par le nom était
`highlight_circle` (point bleu plein, placé trop haut). Nœud de techno relevé :
`GOAL_ENTRY` (UIButton) > `visibility_control` > `highlight_circle`,
`selection_glow`, `entry_number_*`, **`researched_stroke`** (70 × 70, (-14, 8) :
anneau des technos recherchées, ancré haut-gauche, centré sur `TECH_IMAGE`),
`TECH_IMAGE` (7, 29) 29 × 29, `DESCRIPTION` (`TECH_NAME`), `color_controler` >
`cog_around_node`, `tech_number`, `progress`, `DetailsView` >
`selection_circle`. → anneau = `researched_stroke` par son nom exact, teinte
(0,13 ; 0,78 ; 0,84). Toujours aucun clic du bouton d'historique au journal.

## Test 5 (capture, 2026-11-21 20:04)

- Anneau absent : `LegendeTechnosAnneau` n'a rien journalisé (pompe arrêtée,
  jeu en pause pendant l'arbre). → appel aussi depuis `hkNoeudTechno`
  (IconSubManager.cpp, après chaque nœud construit : `HistoriqueNoeudTechnoConstruit`).
- Bouton « juste pas là » : sans animateur, la copie d'`INGAME_MENU_BUTTON` est
  invisible (ses images ne deviennent visibles que par l'état joué). →
  animateur gardé ; la copie est posée dans un **conteneur** (copie de
  `DESCRIPTION_VIDEO` des tutoriels, UIElement sans enfant, animateur retiré)
  décalé de (0, +52) dans `TOP_LEFT_BUTTONS`.

## Test 6 (captures, 2026-11-21 20:29)

- Bouton visible (sous celui du menu) et **clic qui marche** (journal :
  panneau construit, onglet reçus, fermé, ouvert). Auteur : doubler sa taille,
  le rendre moins transparent.
- **Grand carré blanc** = le conteneur (copie de `DESCRIPTION_VIDEO`, 704 × 208,
  dessiné en blanc et captant les clics : menus de gauche inaccessibles).
- Panneau ouvert mais **invisible** ; impossible de le fermer (le carré blanc
  couvrait aussi le bouton).
- Anneau : le 1er nœud n'avait pas `researched_stroke` ; repli par le nom →
  `selection_circle` (point bleu).
- Auteur : retirer la ligne « Close » du panneau (fermeture native).

Corrections : conteneur = copie de `LEFT` du HUD (UIElement sans enfant),
taille nulle ; bouton instancié avec l'échelle de recette (`+0xC0`) doublée ;
icône opaque (animateur de l'icône retiré, couleur (1, 1, 1, 1)) ; panneau
montré **avec animation** (`FUN_14105ac90(el, 1, 1, 1)`) et relevé (visibilité,
couleur `+0x120`) des premiers niveaux après ouverture ; liste à deux lignes ;
tout élément `CLOSE` du panneau branché sur la fermeture ; anneau cherché
dans **tous** les nœuds (`researched_stroke` seulement).

## Test 7 (capture, 2026-11-21 20:36) : « on approche »

Journal : **panneau d'historique fonctionnel** (onglets reçus / envoyés
cliqués plusieurs fois, `CLOSE_BUTTON` natif du panneau trouvé et branché :
fermeture OK, réouverture par le bouton OK). Anneau `researched_stroke` posé.
Échelle de recette du bouton du menu : 0,67 (la copie est à 1,33).
Retours : bouton trop haut, libellé déployé au survol trop gros, anneau de
l'arbre trop bas. → conteneur à (0, +72) ; police des textes de la recette du
bouton divisée par l'échelle le temps de l'instanciation (`ReduirePolices`,
`UITextRecipe +0x210`, type déduit et journalisé) ; anneau remonté de 17 et
décalé de 4 à gauche.

## Test 8 (2026-11-21) et nouvelles demandes

Retours : bouton encore trop haut ; ajouter au panneau une catégorie
« légendes » avec la légende complète (celle d'abord posée en bas à gauche) ;
livre des lois : comme l'arbre des technos, mais avec l'icône « signée » du
jeu à la place de l'anneau bleu.
- Bouton : conteneur à (0, +100).
- Panneau : 3e ligne « Legends » ; onglet 2 = texte vide, titre « Archipelago
  icons », six lignes icône + sens (`ConstruireLegendeListe`) dans le parent de
  `DESCRIPTION_SCROLL_PANE`, montrées seulement dans cet onglet (et cachées à
  la fermeture du panneau).
- Livre : `HistoriqueLivreConstruit` appelé par `hkConstruireLivre` (lois_ap.cpp,
  `FUN_141bbc320`, après la capture de `circle_inside`) ; une fois par
  panneau ; vérifie `+0x190` = `TREE_VIEW` ; pièces dans `TREE_VIEW`, à gauche
  de `CUSTOM_CLOSE_BUTTON` : `icon_done` + « Check Sended », copie de
  `circle_inside` (`LoisModeleSigne`, exporté par lois_ap) teinte bleue +
  « Signed ». Position à régler sur capture (`LEGL_*`).

## Test 9 (captures, 2026-11-21)

- Bouton collé en haut à gauche : le conteneur `LEFT` (ancré au bord par sa
  recette) ignore notre position → retour à `DESCRIPTION_VIDEO` (position
  respectée au test 6), taille nulle, décalage (0, +60).
- Onglet « Legends » : affiché (six icônes + sens) mais icônes sur la bordure
  de la liste et en haut du cadre → décalage +40 / +30.
- Livre : ligne « Check Sended » / « Signed » bien placée à gauche de CLOSE ;
  plume minuscule et ~20 trop bas → 40 × 40, remontée de 20.
- Reste dans le panneau : pied « You can always disable tutorials in the
  Settings » du panneau des tutoriels (non traité).

## Test 10 (capture du livre, 2026-11-21 21:03)

Livre : ligne bien placée ; plume visible mais petite. Demandes : plume plus
grosse, texte « Signed and effective » ; le jeu doit être **en pause** quand
le panneau d'historique est ouvert ; ouvert alors que le jeu est en pause,
l'onglet « Legends » n'affiche pas la légende (journal : onglet affiché,
pièces montrées : cause non comprise, état de la 1re icône et du 1er texte
désormais journalisé à chaque affichage de l'onglet).
- Plume 70 × 70, ligne décalée (`LEGL_LARGEUR` 400), texte 2 large de 200.
- **Pause** : décompilés `FUN_14196c7e0` / `FUN_14196c6e0` (ouverture /
  fermeture d'un panneau du jeu) : raison « UIPanelPause » posée par
  `FUN_14100ec50(*(base+0x2C30130), &raison)`, retirée par `FUN_14100f1a0` ;
  le minuteur garde un ensemble trié de **pointeurs** de raisons. Notre raison
  = structure statique `{ "ArchipelagoHistoryPause" }` (`PauseHistorique`),
  posée à l'ouverture, retirée à la fermeture (bouton du HUD ou CLOSE) et à la
  recréation du HUD si le minuteur n'a pas changé. Steam : minuteur
  `0x2B8D390`, pose `0xEA8840`, retrait `0xEA8D90` (script
  `appel_avec_ptr_chaine.py`, mêmes résultats que Ghidra sur Epic).

### Test 11 (2026-11-21)

Retour de l'auteur : presque bon ; après deux autres onglets, il faut
**cliquer deux fois** sur « Legends » ; retirer la ligne « Check (contents
unknown) » (inutile) ; ajouter des paragraphes sur les technos, les lois et
l'exploration ; descendre encore légèrement la légende. Pause : OK.
- Journal du 10e test : au premier clic raté, le bit visible (`+0x270`) des
  pièces est déjà levé → l'affichage du moteur (`FUN_14105ac90`) semble
  **cumuler les masquages** (deux onglets = deux masquages, il faut deux
  affichages). Les pièces de l'onglet restent désormais toujours visibles et
  sont cachées **hors de l'écran** (x − 20000, `MontrerLegendePanneau`,
  positions notées dans `g_posLegendePanneau`).
- Ligne `icon_vanilla` retirée (5 lignes) ; légende à `(52, 54)` (+12).
- Paragraphes (`LEGENDE_PARAGRAPHES`, une zone de texte de 640 de large sous
  la dernière ligne) : technos (check à la recherche, effet à la réception de
  la progressive de l'arbre), lois (check à la signature, effet à la réception
  → « signed and effective »), exploration (sites à icône AP, checks
  ramassés par les éclaireurs, envoyés à leur retour en ville).
- Compilé ; **non déployé** (jeu ouvert), non testé.

- 12e retour : paragraphe des lois precise (double loi = paire exclusive = un seul check, le meme des deux cotes, envoye par l'une ou l'autre).

### Test 12 (2026-11-22)

Retour de l'auteur : la molette sous le panneau fait bouger la vue en jeu ;
la liste des checks envoyés ne défile pas.
- Journal : `DESCRIPTION_SCROLL_BAR` de la copie = `UIElement` (pas une
  `FrostpunkUIScrollBar`) → jamais reliée (`FUN_141516ec0` écrit jusqu'à
  `+0x332` : interdit sur un `UIElement`). La zone ne défile donc pas seule.
- **Verrou de la caméra** : décompilés `FUN_14196c7e0` / `FUN_14196c6e0` :
  les panneaux du jeu font `++` / `--` sur `*(DAT_144065ae0 + 0x1F0)` ; la
  caméra ne zoome que s'il vaut 0 (`FUN_1413b9f50`). `VerrouCamera` fait de
  même. Steam `0x3FC2998` (`verrou_camera.py`, 4 sites identiques).
- **Molette** : crochet `WH_GETMESSAGE` sur le thread de l'interface (posé au
  clic du bouton, retiré à la fermeture et à la recréation du HUD) ;
  `WM_MOUSEWHEEL` avalé et converti en défilement par
  `FUN_1411fd0a0(zone, {0, crans × 60, 0, 0})` (position = `+0x380 + +0x390
  − delta`, bornée au contenu ; uiscrollpane.cpp). Pas dans l'onglet
  Legends. Steam `0x10964B0` (octets identiques, 1 seul site).
- `ModalHistorique(ouvert)` = pause + verrou caméra + crochet.
- Compilé, déployé sur Steam, **non testé**. Journal : `[HIST] camera
  verrouillee`, `[HIST] crochet de la molette pose (thread …)`.

### Test 13 (2026-11-22)

Retour : la molette ne fait rien. Journal : crochet posé, caméra verrouillée,
mais `EXCEPTION 0xC0000005 au defilement` à chaque cran.
- Décompilés `FUN_1411fd1d0` / `FUN_1411fd420` : ils travaillent sur l'objet
  **miroir** de la zone (côté rendu : enfants `+0x158`, frère `+0x178`,
  positions `+0xC0`) ; `FUN_1411fd0a0` n'est appelé que par les tâches de
  rendu (`FUN_1411fca90`) → AV avec l'élément.
- Côté élément (barre du jeu `FUN_1415170d0`) : position = `+0x330`
  (x, y, z, w), maximum y = `+0x324` (x : `+0x320`) ;
  `FUN_1411fc420(zone, &cible[4], durée)` met la tâche `0x4F` (défiler vers)
  dans la file du rendu (`FUN_1411fc350` = tâche `0x4E`). Le crochet calcule
  `y − crans × 60`, borné à `[0, max]`, durée 0,08 s. Steam `0x1095830`.
- Journal : 3 premières lignes `[HIST] molette : … position y / max`, et
  l'adresse de la faute en cas d'AV. Compilé, déployé sur Steam, non testé.

### Test 14 (2026-11-22)

Retour : la molette ne défile toujours pas. Journal : `molette : -1.00
cran(s), position 0 / 0` → le maximum de l'élément (`+0x324`) vaut 0 : il
n'est calculé que côté rendu (`FUN_1411fd420`, lancé par les tâches de
défilement) puis renvoyé. Plus de borne haute tant qu'il vaut 0 (le rendu
borne lui-même dans `FUN_1411fd0a0`) ; 8 lignes de journal pour voir si
position et maximum remontent.

### Test 15 (2026-11-22)

Retour : toujours rien. Journal : 8 × `position 0 / 0` : ni la position ni
le maximum ne remontent du rendu → le rendu borne à 0, donc pour lui le
contenu tient dans la zone. Hypothèse : le texte garde sa hauteur de 1
(`704 x 1` à la création, le texte déborde sans agrandir le layout).
Décompilé `FUN_141bcf2e0` (init du panneau des tutoriels) : la barre
`DESCRIPTION_SCROLL_BAR` y est cherchée par nom puis par **type**
(`FrostpunkUIScrollBar`, `DAT_144087488`) avant `FUN_141516ec0`.
Diagnostic au 1er cran : taille du texte, du layout et de la zone,
`+0x27C` / `+0x278` (garde des mises à jour, `FUN_14102ba60`), zone
`+0x320..+0x33C` ; `Relayout` du layout avant chaque cran.

### Test 16 (2026-11-22)

Relevé du 15e test : texte `704 x 1`, layout `622 x 798` en (5, 10), zone
`725 x 480`, `+0x27C` = 0 partout, zone `+0x320..+0x33C` = 0 : le rendu ne
renvoie jamais de bornes, rien ne défile. **Abandon de la zone du jeu** :
défilement fait main — le layout du texte (enfant de la zone, qui découpe)
est placé à `y d'origine − décalage` ; décalage borné à `[0, lignes ×
hauteur de ligne + 40 − hauteur de la zone]`, lignes = `\n` du texte de
l'onglet (+1), hauteur de ligne = police de la recette du texte
(`+0x1E8` → `+0x210`) × 1,25, 26 à défaut. Retour en haut à chaque
changement d'onglet. Journal : `[HIST] defilement d / max (n ligne(s),
ligne h, zone z)` (6 lignes). L'entrée Steam `0x11FC420` est retirée
(fonction plus utilisée). Compilé, déployé sur Steam, non testé.
