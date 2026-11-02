# Scenarios verrouilles par Archipelago

> Historique detaille, copie mot pour mot depuis CLAUDE.md le 2026-11-02.
> Le resume a jour est dans CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

- **Scénarios verrouillés par Archipelago (2026-11-01, non testé)** :
  au menu, un scénario non débloqué par le mod doit se comporter comme un
  scénario verrouillé par le jeu (jour 20 de A New Home…). Décompilés :
  - `FUN_141283ea0(profil, liste) -> char` = prérequis de profil satisfaits
    (liste de liens « profile progress », pas 0x10 ; `DAT_142c343cd` = tout
    débloqué, debug). `FrostpunkScenarioEntry` : `+0x00` nom interne,
    `+0x18` visible au menu, **`+0x20` « Scenario unlock profile
    progress »**, `+0x38` liste du conseil « Maybe play Main Scenario »,
    `+0x238` / `+0x240` titre / description, `+0x263` lien de DLC.
  - `FUN_141b2b1b0(panneau, état)` = `FrostpunkUIMainMenuScenariosPanel`,
    état 0 (liste) : débloqué = `FUN_141283ea0(profil, scn + 0x20)` ; seuls
    les débloqués entrent dans la liste sélectionnable (`panneau +0x198`,
    compte `+0x1A0` : flèches, détails) ; **`FUN_141b2cb40(panneau, scn,
    débloqué, indice)`** crée le bouton : `FUN_14105a3a0(bouton, débloqué,
    1)`, si verrouillé presets `LOCKED` et `LOCKED_<nom>` sans écouteur de
    clic, sinon clic → `FUN_141b2d910` (sélection → détails → lancement).
  - Autres appelants sur `scn + 0x20` : `FUN_141b2e010` (points de la vue
    de détails), `FUN_141b28f10` (bouton « Scenarios » du menu principal,
    actif s'il y a des sauvegardes ou un scénario débloqué ; presets
    `SCENARIO_UNLOCK`, `NEW_DLC_ON`), `FUN_14131e800` (un scénario autre que
    le principal est-il débloqué ?), `FUN_14131eca0` (notification « scénario
    débloqué » après un progrès de profil).
  → `scenarios_ap.cpp` : hook de `FUN_141283ea0` ; si la liste est le
  `+0x20` d'un des 6 scénarios AP (`DEFAULT`, `TechHaven`, `REFUGEES`,
  `WINTERHOME`, `BUILDERS`, `Reconquest`), la réponse est
  **`ScenarioDebloque(nom)`** seule (le verrou du jeu est remplacé : un
  scénario débloqué par AP se lance même sans le jour 20 ; l'achat d'un DLC
  reste vérifié à part, `+0x263`). Endless et debug non touchés. Tous les
  appelants voient le même verrou. Liste de test `DEBLOQUES_TEST` =
  DEFAULT, REFUGEES (TEMPORAIRE). Log `[SCN] "<nom>" : jeu …, Archipelago
  … -> LANCABLE / VERROUILLE` (une ligne par changement).
  **1er test (2026-11-01) : verrou OK** (DEFAULT / REFUGEES lançables, les
  autres verrouillés comme par le jeu), mais le bouton affiche la condition
  du jeu (« nécessite le jour 20 »). `FUN_1410240d0(élément, &nom, récursif,
  …)` (9 paramètres, renvoie un octet) joue un preset par l'animateur
  `élément +0x1E8 → vtable +0x138`, puis sur les enfants si `récursif`.
  → hooks de `FUN_141b2cb40` et `FUN_1410240d0` : pendant la création du
  bouton d'un scénario verrouillé **par le mod**, le preset `LOCKED_<nom>`
  (présumé porteur du texte) est sauté ; arbre du bouton journalisé
  (`[SCN] bouton verrouille de "…"` : nom, classe, visible `+0x270` bit 0,
  texte `+0x3E8`). Liste de test : **Reconquest seul**.
  **VALIDÉ le 2026-11-01** par l'auteur (texte de condition retiré, seul On
  the Edge lançable).
  **Déblocage = réception d'un item** (demande de l'auteur, 2026-11-01) :
  plus de liste en dur ; `ReceiveScenarioUnlock(scenario, joueur)`
  (archipelago, popup « Scenario unlocked » seulement si une partie est
  prête) → `ScenarioDebloquer(nom)` (scenarios_ap, état en mémoire, tout
  thread ; pas de fichier : le serveur renvoie les items à chaque
  connexion). Item de départ simulé (TEMPORAIRE) :
  `ScenariosRecevoirItemsDeTest()` (version.cpp, au démarrage) reçoit
  **REFUGEES**. Non testé.
  **Régression signalée (2026-11-01)** : plus d'icônes AP dans le livre des
  lois, alors que le journal `[LOIUI]` montre les copies posées comme avant.
  Suspect : le **hook global de `FUN_1410240d0`** (presets d'interface,
  passe-plat hors du bouton). Arbre du bouton relevé :
  `SCENARIO_BUTTON` > `MENU_BUTTON_LAYOUT` > `MENU_OPTION_CONTAINER` >
  `DISABLED_TEXT`, `TITLE_TEXT` (« SCENARIO LOCKED »), `MAIN_STORY_TEXT`,
  **`UNLOCK_SCENARIO_TEXT`** (« UNLOCKED ON DAY 20 OF A NEW HOME »).
  → hook des presets **retiré** ; après la création du bouton d'un
  scénario verrouillé par le mod (nouvel enfant de la colonne
  `**(panneau + 0xD8)`), le texte de `UNLOCK_SCENARIO_TEXT` est remplacé
  par un espace (`FUN_14111bef0`). Log `[SCN] "…" : condition du jeu
  effacee`. Non testé (icônes des lois et texte du bouton).
