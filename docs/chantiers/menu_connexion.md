# Ecran de connexion du menu principal

> Historique detaille, copie mot pour mot depuis CLAUDE.md le 2026-11-02.
> Le resume a jour est dans CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

- **Écran de connexion au menu principal (2026-10-31, approche 1 choisie par
  l'auteur, VALIDÉ visuellement le 2026-10-31)** : 3 champs (serveur, slot,
  mot de passe) + bouton « Connect » dans une colonne propre à droite de la
  tache d'encre (voir les tests ci-dessous pour l'historique).
  Décompilés (adresses Ghidra) :
  - `FUN_141b28950` = init de `FrostpunkUIMainMenuMainPanel` (racine
    poignée `+0xA8`, boutons `CONTINUE_BUTTON` … `EXIT_BUTTON`) ;
    `FUN_141a324b0(racine, &nom, objet, fn, p5)` →
    `FUN_14105a1c0(bouton, objet, fn, 0, p5, 1)` (écouteurs de clic
    `bouton +0x258`, rappel `fn(objet, …)`).
  - **Tout élément garde sa recette en `+0x1E8` et sa ressource en `+0x1F0`**
    (`FUN_14102a5f0`) ; `FUN_14110c780(recette, ressource, drapeaux,
    &DAT_142be0438)` instancie une copie complète (comme les curseurs du
    panneau Endless, `FUN_141b3a420`). Recette : `+0x30` enfants, `+0x38`
    compte, `+0x48` nom, `+0x114` mode (0 toujours, 1 jamais, 2 si drapeau 1).
    `FUN_141024ba0(chemin, &nom, ctx)` = instancier un nœud nommé d'une
    recette par chemin ; `FUN_141035c80(pool, chemin, 1)` = ressource (chargée
    au besoin), `FUN_141119140(ressource)` = recette racine.
  - `FUN_1410266a0(parent, enfant, avant)` insère ; `FUN_141023d70` ajoute.
  - **`UITextInput`** (moteur, 0x6E0, fiche `DAT_142bfd7b0`) : `+0x3E8` texte
    affiché, `+0x678` tampon d'édition / `+0x680` longueur / `+0x684` max,
    `+0x6C8` en édition, `+0x6B0` caractères interdits, `+0x658` écouteurs de
    fin d'édition ; `FUN_14111bef0(texte, wchar*, &DAT_142bfd628)` pose un
    texte brut (tout UIText) ; `FUN_14112a920` = début d'édition (recopie
    `+0x3E8` dans le tampon) ; `FUN_141030e90(écran, input)` = focus
    (`UIScreen +0x328`) ; `FUN_1410261e0` = écran d'un élément. Pas de masque
    de mot de passe dans la recette.
  - Champs connus : `SAVE_NAME_INPUT` (panneau de sauvegarde, `FUN_141b7edf0`),
    `ENTITY_NAME_INPUT` (`FrostpunkUINamingCityMessagePanel`, la popup « nommer
    la ville » d'Endless, `FUN_141ba2580`), `NAME_INPUT` (recherche). Le menu
    n'en a aucun ; leur recette n'est pas connue du code.
  - Implémentation (`menu_ap.cpp`) : source du champ cherchée dans
    `UI/MainScreen` (recherche récursive qui ignore les modes), sinon liste
    des recettes `UI/` du pool et essai de celles dont le nom évoque un champ
    (`[MENU] recette …`) ; libellés = clones d'`EXIT_BUTTON` (clic = focus du
    champ) ; mot de passe : hooks de `FUN_14111bef0` (étoiles affichées,
    valeur retenue) et `FUN_14112a920` (valeur réelle remise dans le
    tampon). Inconnus : focus par clic direct sur le champ, saisie clavier au
    menu, longueur maximale du champ cloné, fond du champ.
  - **1er test** : source trouvée (`UI/Misc/SaveGamePanel` → `SAVE_NAME_INPUT`,
    mode 0, colonne = `MENU_LAYOUT` UILayout), focus appelé, mais champs
    **non interactifs et presque invisibles en bas**. Décompilés : clavier
    `FUN_140fdabc0` → `FUN_14100fab0` (écrans visibles `vtable+0x218` avec
    `+0x2D4`, bloquants `+0x2D0`/`+0x2D2`) → `FUN_14102f410(écran, vk)` →
    `FUN_141129c90` (Entrée / Échap seulement si `input +0x69C`) ; caractères
    `FUN_140fda9e0` → `FUN_14100f840` → `FUN_14102f300` →
    **`FUN_14112ac10`** (insertion : caractères interdits `+0x6B0`, refus si
    curseur `+0x688` ou longueur ≥ max `+0x684`, refus si le texte dépasse la
    largeur quand **« Limit to width » `+0x6CC`** est posé) ; clic souris
    `FUN_14102f720` (élément survolé `écran +0x380`, focus si
    `vtable+0x108` et `+0x228`, sinon fin d'édition si `input +0x6CA`).
    `FUN_14102b170(élément, l, h)` = taille (`+0x210`/`+0x214`). Causes
    présumées : champ cloné à 15 caractères (l'URL par défaut en fait 20),
    limite de largeur active, aucune taille hors de son panneau. →
    `PreparerChamp` : taille du bouton modèle, tampon de 128 caractères
    (`MoteurAlloc`), `+0x6CC` à 0 ; bloc déplacé **en tête** de la colonne.
  - **2e test** : saisie OK. Relevé : champ d'origine 264 × 40, limite de
    largeur déjà à 0 (seul le maximum de 15 bloquait), bouton 135,7 × 32 —
    la mise à la taille du bouton rétrécissait le champ (retirée). Remarques
    de l'auteur : les options du jeu sont repoussées trop bas ; les intitulés
    ne doivent **pas** être cliquables.
    → Réflexion de `UIElementRecipe` (`FUN_14110fc60`) : `+0x48` nom,
    `+0x80` Position, `+0x90` Anchor, `+0xA0` Dock, `+0xB0` Rotation, `+0xC0`
    Scale, `+0xF0` Color, `+0x108` Color mode, `+0x114` Disabled (= le
    « mode »), `+0x118` Modal, `+0x120` FreezedLayout, `+0x122` Hidden,
    `+0x123` Ignore layout, `+0x125` Focusable with mouse, `+0x134` Anchor by,
    `+0x138` Auto layout, `+0x140…` événements OnShow… .
    → **Colonne propre** : copie de `MENU_LAYOUT` instanciée avec, le temps
    de l'appel, 0 enfant et une position décalée en x dans sa recette
    (`CreerColonne`), ajoutée au parent de `MENU_LAYOUT` ; intitulés = copies
    du **texte** d'`EXIT_BUTTON` seul ; focus par clic direct : hook du clic
    de l'écran `FUN_14102f720` (survolé `écran +0x380`, focus `+0x328`).
  - **3e test** : tout est créé (journal : recette de `MENU_LAYOUT` position
    (638, 369), ancre (0, 0), dock 0, 18 enfants, parent `UITemplateView`,
    taille 350 × 370 ; colonne décalée de +410 en x ; texte modèle
    `MENU_OPTION_TEXT` UIText) mais **rien n'est visible**. Hypothèses :
    découpage par le parent, ou colonne jamais montrée.
    `FUN_14105ac90(élément, visible, animé, forcer)` = montrer / cacher
    (`élément +0x270` bit 0). → colonne forcée visible, et relevé
    `DecrireChaine` (translation locale `élément +0x80`, taille, drapeaux
    `+0x270`, couleur `+0x120`, clip / caché de la recette) de `MENU_LAYOUT`
    et de la colonne, à la pose puis au 1er clic 3 s après.
  - **4e test (capture de l'auteur)** : seul « ARCH… » (texte du champ
    serveur) apparaît en filigrane au bord droit de la tache d'encre. Chaîne :
    `MENU_LAYOUT` (638, 369) ← **`SFX_TXT_MASK`** (`UITemplateView`, 500 ×
    500, en (0, 0)) ← **`INK_PANEL`** (250, 250) ← `EXT_MAIN_MENU_PANEL`
    (1280 × 720) ← `FRAME` ← `UIScreen`. `SFX_TXT_MASK` est le **masque en
    tache d'encre** : ses enfants ne sont visibles que dans la zone noire
    (l'auteur l'avait deviné). Échelle relevée : ~1,09 px d'écran 1920 par
    unité. → colonne rattachée à `INK_PANEL` (mêmes coordonnées, ajoutée en
    dernier).
  - **5e test** : colonne visible, mais éléments qui **se chevauchent un
    peu**, et une **barre blanche** au début de chaque champ = le curseur
    `SAVE_NAME_INPUT_CURSOR`, enfant du champ dans la recette, copié avec lui
    et jamais lié (le panneau de sauvegarde le lie par `FUN_14112a1d0(input,
    curseur)`, qui l'ajoute en `input +0x6A0` ; `FUN_141129940` = cacher le
    curseur `+0x6A0` en fin d'édition). → curseur copié détruit (`vtable
    +0xE0`) ; au focus, `FUN_14112a920` crée le sien. Réflexion de
    `UILayoutRecipe` (`FUN_141111850`, 0x240) : `+0x200` Layout Type, `+0x204`
    Orientation, `+0x208` Alignment, `+0x20C` Cell alignment, `+0x210` (bool),
    `+0x211` Use content size, `+0x212` Ignore hidden children, `+0x214..+0x220`
    Padding (gauche, droite, haut, bas), `+0x224`/`+0x228` **Cell margin**
    horizontale / verticale, `+0x22C`/`+0x230` décalages personnalisés. →
    marge verticale +12 dans la recette le temps de l'instanciation de notre
    colonne, et relevé des enfants (translation, taille) 3 s après.
  - **6e test** : curseurs copiés détruits, espacement OK (layout de
    `MENU_LAYOUT` : type 0, orientation 0, alignement 3 / 3, padding nul ;
    groupes au pas de ~66 : intitulé `MENU_OPTION_TEXT` 18 de haut, champ 40).
    Demande de l'auteur : un **trait pointillé** entre chaque groupe, comme
    entre les boutons du jeu. → `TrouverSeparateur` : modèle = 1er enfant de
    `MENU_LAYOUT` qui n'est pas un `UIButton` (liste des enfants
    journalisée), cloné après chaque champ.
  - **7e test : rendu validé par l'auteur** (« c'est nice »). Reste : un champ
    vide affichait le texte par défaut du champ de sauvegarde (« New save »)
    — un texte vide posé par `FUN_14111bef0` ne le remplace pas. → slot par
    défaut « Player 1 » (demande de l'auteur), valeur vide affichée comme un
    espace (mot de passe vide compris), espaces de tête / fin retirés à la
    lecture (`LireChamp`).
  - **8e test (capture)** : « Player 1 » / espace OK, mais le texte des champs
    est **aligné à gauche** dans leur boîte de 264 (le slot déborde à droite,
    les étoiles du mot de passe sont décalées), alors qu'intitulés et traits
    sont centrés. Réflexion de `UITextRecipe` (`FUN_1411c4960`) : `+0x200`
    police, `+0x210` Font size, `+0x214` **Horizontal alignment** (0 Left,
    1 Right, 2 Center), `+0x218` Vertical alignment, `+0x220` ombre, `+0x240`
    contour, `+0x24C` Upper case, `+0x24D` Localized, `+0x258` Wrap text,
    `+0x25B`/`+0x25C` Adjust width / height. → `InstancierChampCentre` :
    `+0x214` = 2 dans la recette de `SAVE_NAME_INPUT` le temps de
    l'instanciation (recette partagée avec le panneau de sauvegarde du jeu).
    Non testé ; si l'élément relit la recette plus tard, l'alignement
    reviendra à gauche.
  - **9e test** : serveur et slot centrés (l'alignement est bien copié dans
    l'élément : `input +0x321`, lu par `FUN_14112a360`), mais pas le mot de
    passe. Décompilés : `FUN_14112a360(input, pos)` place le curseur en
    mesurant le **préfixe du tampon** `+0x678` (largeur affichée `+0x4D0`) ;
    `FUN_1411293b0` = effacer la sélection (`+0x690` début, `+0x694`
    longueur, `+0x6C9` sélection permise) ; `+0x688` curseur. Le tampon
    tenait le vrai mot de passe et l'affichage des étoiles → incohérence. Et
    **« Upper case » (`UITextRecipe +0x24C`) est actif** sur le champ de
    sauvegarde : `FUN_14112ac10` et `FUN_14112a920` convertissent le
    **tampon** en majuscules (slot et mot de passe auraient été envoyés en
    majuscules). → tampon du mot de passe = étoiles seulement, vraie valeur
    tenue en miroir (hooks `FUN_14112ac10` insertion, où l'on insère `*`, et
    `FUN_141129c90` retour arrière / suppression / sélection) ; « Upper
    case » coupé dans la recette le temps de ces appels et de
    l'instanciation, pour nos champs seulement.
  - **10e test (capture)** : texte toujours décalé à droite, d'autant plus
    que le texte de départ était court (étoiles très à droite ; au 8e test,
    texte à gauche, il partait du centre). Explication cohérente avec les
    translations relevées (serveur x = 57,8, autres 120) : la colonne place
    chaque champ selon la **largeur de son texte au moment de la mise en
    page** (une seule fois, à l'ouverture du menu), puis le texte part de
    cette position (à gauche) ou se centre dans la boîte de 264 (« Center »).
    → alignement d'origine (gauche) gardé, et **remise en page de la
    colonne** (`FUN_1411fd800`) après chaque insertion / effacement dans un
    de nos champs (`RecalculerColonne`). **VALIDÉ le 2026-10-31** par
    l'auteur (champs centrés ; relevé : champ x = 175 − largeur du texte / 2).
    Chantier mis en pause par l'auteur : il y reviendra pour la vraie
    connexion au serveur (`RequestConnection` ne fait que journaliser).
    Restent non vérifiés en jeu : la casse conservée à la saisie (« Upper
    case » coupé) et la valeur du mot de passe envoyée après effacements.


### 2026-11-04 — Libellé d'état de la connexion (bug du premier test réseau)

Au menu, aucun message ne s'affiche (pas de partie, pompes arrêtées).
Décision de l'auteur : un **libellé d'état sous Connect** (clone du texte
d'`EXIT_BUTTON`, `AjouterLibelle`). États (anglais, courts) : `Not connected`,
`Connecting...` (`RequestConnection`), `Connected` / `Reconnected`,
`Refused: <erreurs>`, `Disconnected, retrying...`,
`Server unreachable, retrying...`, `Failed: use ws://`. Source :
`ApEtatConnexion` (archipelago.cpp), alimenté par `ApMessageReseau` d'après
le titre du message ; `reseau_ap.*` non modifié.
Rafraîchissement : aucun hook ne tourne à chaque image au menu
(`FUN_14102f720` = événements souris seulement, vérifié dans Ghidra) →
**minuterie Windows sans fenêtre** (`SetTimer(nullptr, 0, 250, …)`) armée
dans `Construire` : le rappel est exécuté par la boucle de messages du
thread qui l'a armée (thread de l'interface). Garde : le rappel compare son
thread à celui de `Construire` (journal `[MENU] minuterie d'etat : thread …`),
l'intitulé est relu par poignée (panneau détruit = rien). Non testé.

### 2026-11-04 — Menu sans sauvegarde : Scenario actif, New game grisé

Retour de l'auteur : sans sauvegarde (liste filtrée par `sauvegardes_ap.cpp`),
impossible de choisir le scénario ; seul reste le bouton qui lance A New Home.
Décompilé : `FUN_141b28f10` = rafraîchissement du panneau principal (vtable
+0x08). Profil `FUN_14125aed0`, nombre de sauvegardes `+0x1B8` ; sans
sauvegarde : `CONTINUE_BUTTON` (`+0xD0`) caché, `NEWGAME_BUTTON` (`+0xD8`,
lance A New Home) montré, `SCENARIO_BUTTON` (`+0xE8`) désactivé sauf scénario
« à débloquer » passé (`FUN_141283ea0`). Hook `hkRafraichirPanneau` : après
l'original, Scenario forcé actif, New game inactif
(`FUN_14105a3a0(élément, actif, 1)` + état d'animation `ENABLED` / `DISABLED`
par `FUN_1410240d0(élément, &nom, 1, 0, 0, 0, 1, 0, 0)`). Déployé à 11:18.
Non testé.