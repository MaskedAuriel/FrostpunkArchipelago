# Chantier : couche réseau Archipelago

Démarré le 2026-11-04. La « classe spécifique pour toutes les interactions
avec le serveur » prévue par l'auteur : `reseau_ap.h/.cpp` (classe
`ReseauAP`). Elle n'appelle **que** la surface `archipelago.h`.

## Décisions de l'auteur (2026-11-04)

- **Transport : WebSocket fait main** sur Winsock (`ws2_32.lib`) — refus de
  WinHTTP. Conséquence : **`ws://` seulement** ; `wss://` demanderait
  Schannel (TLS), refusé proprement avec un message en jeu.
- **JSON : nlohmann/json 3.11.3** (`nlohmann\json.hpp`, release GitHub
  officielle, SHA-256 `9BEA4C80…103EA6`), inclus par `reseau_ap.cpp` seul.
- **PrintJSON en messages** : seulement les items qui concernent le slot
  (reçu d'un autre joueur : « Item received », trouvé ici pour un autre :
  « Item sent »). Tout le reste au journal `[AP MSG]`.

## Architecture

- Thread réseau dédié (`ReseauDemarrer`, appelé par `PayloadThread` à la
  place de `ScenariosRecevoirItemsDeTest` et `ApChargerSlotDataDeTest`,
  supprimés). Il ne touche aucun système du jeu.
- Appels du jeu (thread du jeu), sous verrou, sans attente :
  `ReseauConnecter` (← `RequestConnection`, bouton Connect),
  `ReseauEnvoyerLocation` (← `EnvoyerLocation` de tous les `Send*`),
  `ReseauEnvoyerGoal` (← `SendScenarioVictory` quand le goal est atteint).
- Appels du réseau vers `archipelago.h` : `ReceiveItem`,
  `ApAppliquerSlotData`, `ApDefinirSession`, `ApCheminSession`,
  `ApGoalAtteint`, `ApMessageReseau`.

### Protocole

1. `RoomInfo` → `GetDataPackage` (jeux sans cache) puis `Connect` (game
   `Frostpunk`, version 0.6.4, `items_handling = 7`, tags `["AP"]`,
   `slot_data = true`, uuid de `mods\ap_uuid.txt`).
2. `Connected` → session `mods\ap_<graine>_<slot>\`, `slot_data` sauvé et
   appliqué, checks de la session absents de `checked_locations` renvoyés,
   goal renvoyé si `ApGoalAtteint`. Message « Connected / Reconnected to
   Archipelago ».
3. `ConnectionRefused` → message « Connection refused » + erreurs ; plus de
   reconnexion jusqu'au prochain clic sur Connect.
4. `ReceivedItems` : items d'index < `index.txt` sautés (connexion, Sync,
   rechargement) ; trou → `Sync`. Chaque item : `ReceiveItem(id, alias)`,
   puis `index.txt` réécrit.
5. Perte de connexion → « Disconnected… reconnecting », reprise après 5,
   10, 20 puis 30 s.

### Persistance

| Fichier | Contenu |
|---|---|
| `mods\ap_session.txt` | `dossier=ap_<graine>_<slot>` : dernière session, relue au démarrage |
| `mods\ap_uuid.txt` | uuid du client |
| `mods\ap_datapackage\<jeu>_<checksum>.json` | noms des items / locations par jeu |
| `<session>\index.txt` | prochain index d'item attendu |
| `<session>\checks.txt` | locations cochées (renvoyées à chaque connexion) |
| `<session>\slot_data.json` | dernier slot_data (réappliqué au démarrage) |
| `<session>\ap_items_recus.txt`, `ap_items_livres*.txt`, `ap_victoires.txt` | déplacés de `mods\` dans la session |

Les déblocages de scénarios (mémoire seulement) sont rejoués depuis
`ap_items_recus.txt` de la session à chaque `ApDefinirSession` : hors
connexion, le jeu redémarre avec les scénarios de la dernière session.
Les sauvegardes par `GameId` (technos, lois, température, quêtes) ne
bougent pas.

## Étapes

1. Connexion seule (RoomInfo / Connect / Connected / refus) — **écrit,
   non testé**.
2. Réception (ReceivedItems, index, Sync) — écrit, non testé.
3. Envoi (LocationChecks, hors connexion, goal) — écrit, non testé.
4. Scouts (`LocationScouts` / `LocationInfo`) → remplacer `IconeCheckDeTest`,
   `IconeDuCheckDeTest`, `AP_NomDuCheck` — à faire.

## Limites connues

- Au menu principal, aucun message ne s'affiche : l'état de la connexion
  n'est visible qu'au journal `[AP NET]` (les messages attendent la partie,
  file de 16).
- Un message reçu d'un autre joueur peut doubler la popup du jeu
  (« Technology received (from Archipelago) ») : à juger en test.

## Journal

### 2026-11-04 — Étapes 1 à 3 écrites, compilées, déployées (non testé)

### 2026-11-04 — Premier test de l'auteur (serveur 0.6.7)

Connexion, déblocage de A New Home (item #0), check de loi envoyé et item
Sawmill reçu et appliqué : OK au journal. Quatre bugs relevés par l'auteur
(message de connexion au menu, sauvegardes faites sans le mod, expéditeur
perdu avant la pompe, nom du check sur les nœuds de lois) : confiés à la
session « Premiers bugs du mod en Archipelago ». Le serveur avertit que le
client ne compresse pas le WebSocket (sans effet aujourd'hui).

### 2026-11-04 — Scouts, côté réseau (compilé, non branché)

Après `Connected` : `LocationScouts` de toutes les locations du slot
(`create_as_hint = 0`) ; `LocationInfo` gardé en mémoire et dans
`<session>\scouts.txt`, relu au changement de session (valable hors
connexion). Transmis au jeu par la surface : `ApNoterScout(location, item,
nomItem, nomJoueur, flags, pourMoi)` (archipelago.h), appelée à la
relecture de la session puis à chaque `LocationInfo`. Partage convenu avec
la session des bugs : réseau = cette session ; `ApNoterScout` et tout
l'affichage (lois, technos, sites, icônes) = session des bugs.
`ApNoterScout`, `ApScoutLoi`, `ApScoutTechno` et `ApNomDuScout` ont été écrits
par la session des bugs (noms des nœuds de lois et de technos).

### 2026-11-04 — Bugs : nom vanilla sur les nœuds de technos, format des noms

Constat de l'auteur : les nœuds de l'arbre des technos gardent le **nom
vanilla**. Décompilés : le nom est posé par `FUN_141bc5810` seulement
(`FUN_14111bd70` → `FUN_14111bd80`, clé `techno + 0x18`) ; `FUN_14111bef0`
(notre texte brut) prend bien `(texte, wchar_t*, &DAT_142bfd628)` ;
`FUN_141bc63b0` = panneau d'info, `FUN_141bbe840` = livre des lois. Pas de
cause trouvée en statique → journal `[TECHNOM]` (30 premiers nœuds : texte,
hash, techno AP ou non, nom posé). Non testé.
**Format décidé par l'auteur** : `"<joueur> | <item>"`, destinataire en
tête, même pour ce slot (`ApNomDuScout` ; lois, technos, popup « Check
sent »).
Test suivant : `[TECHNOM]` montre le nom complet posé
(`"Simon | A New Home - Progressive Tree Factory"`), mais le nœud
n'affiche que « Simon » : **le `|` coupe le texte en jeu** (hypothèse de
l'auteur) → séparateur ` - ` : `"Simon - A New Home - Progressive Tree
Factory"`. `AP_LocationText` (description de la techno, popup « Check
sent ») : plus de `[AP] Sending:` ni `(already sent)`, seulement le nom
du check (demande de l'auteur). Non testé.
Test suivant : séparateur ` - ` posé (journal), mais **nom vanilla** à
l'écran. Décompilé `FUN_140e339a0` : `FUN_14111bd70` passe la **clé**
`techno + 0x18` (char*), seulement élargie en UTF-16, **pas traduite** →
le texte d'un nœud est une clé traduite à l'affichage ; un texte brut
n'est pas affiché tel quel. → `PoserNomDuCheck` pose la clé
`@AP/TechNom/<hash>` ; `hkTranslate` (hooks_loc.cpp, modifié par script en
gardant l'encodage Windows-1252) la traduit en `AP_NomDuCheck` +
` [Researched]`, objet propre par techno (`ApLocTechNom`, 256). Non testé.
Si l'auteur lit `@AP/TechNom/…` sur les nœuds, l'hypothèse est fausse.
Test suivant (capture de l'onglet Heating) : **toujours le nom vanilla**
avec la clé ; descriptions correctes. `FUN_141bc3eb0` (construction) ne
réécrit pas le nom ; seul `FUN_141bc5810` appelle `FUN_14111bd70`.
Indice : avec `|`, le nœud affichait « Simon » (texte court) ; sans `|`
(texte long), le nom vanilla. Hypothèse : **un texte trop long pour le
cadre n'est pas affiché**. Test : texte brut, variante par techno
(`hash % 5`) : 0 « Simon », 1 16 caractères, 2 28 caractères, 3 joueur +
retour à la ligne + item, 4 nom complet ; journal `[TECHNOM] <techno> :
variante N`. Non testé.
Résultat : **aucune variation à l'écran** → l'élément `TECH_NAME` de
`FUN_141bc5810` (`sortie[1]`) n'est **pas** le label visible (le « Simon »
du test au `|` venait sans doute d'ailleurs). Variantes retirées (nom
complet remis). Relevé `[TECHUI]` : arborescence UI des 2 premiers nœuds
construits (nom, classe, visible, texte UTF-16 `+0x3E0` des `*Text*`),
avant et après notre pose, pour trouver l'élément qui porte le nom. Non
testé.
Résultat `[TECHUI]` : le nœud n'a qu'un texte, `TECH_NAME` (UITextBox),
qui contient avant nous la **clé** `"H 1_SteamHub"` et après nous notre
texte → c'est bien lui, mais l'affichage est recalculé ailleurs.
Décompilé `FUN_14111a680` (validation d'un texte) : si la clé `+0x3D8`
n'est pas vide, `FUN_140feba40` → `FUN_140feba60` → **Translate**
(`+0xFEB340`) → texte affiché `+0x3E8`, ce qui écrase le texte brut.
→ Hook de `FUN_14111a680` (`hkValiderTexte`, IconSubManager) : pour un
`TECH_NAME` de nœud noté à la construction (boîte → hash, 512), il signale
à `hkTranslate` (`TechNomEnCoursPrendre`) que la traduction en cours est
celle d'un nœud ; `hkTranslate` renvoie alors `AP_NomDuCheck` +
` [Researched]` (`ApLocTechNom`). Seul l'arbre change. Relevé `[TECHUI]`
retiré ; journal `[TECHNOM] validation du nom du noeud …` (20 premiers).
Non testé.
**Décision de l'auteur (2026-11-04) : on n'écrit plus sur les nœuds** —
le joueur doit voir le nom de la techno qu'il a reçue. Le nom du check
n'est que dans la **description** (`ApDescBuffer` → `AP_LocationText`).
Retirés : `PoserNomDuCheck`, `hkValiderTexte` (hook de `FUN_14111a680`),
`TechNomEnCoursPrendre`, la branche `AP/TechNom/` et `ApLocTechNom` de
hooks_loc.cpp ; `IconesTechnoRafraichirNoeud` repose la clé vanilla.

### 2026-11-04 — Bug : description d'une techno reçue = nom interne

Constat de l'auteur : techno **reçue mais pas recherchée** → la description
montre le nom interne de la techno au lieu du nom du check. Piste : une
techno reçue est mise à l'**état 2** du jeu (`SyncGameState`, « faite,
effet appliqué ») ; le panneau d'info affiche peut-être une autre clé
(courte, emplacement 2) ou une clé partagée qui pointe vers une autre
techno (table `LookupLocKey` : une clé → la première techno trouvée), dont
le scout manque → repli `AP_NomDuCheck` = nom interne. Journal
`[TECHDESC]` (hkTranslate) : clé, emplacement, techno, `researched` /
`received`, nom du check calculé, une ligne par cas nouveau. Non testé.
**Testé par l'auteur (2026-11-04) : « ça semble bon »** après le retrait
de l'écriture sur les nœuds ; `[TECHDESC]` donne bien `"Simon - …"` pour
les technos du scénario. (Le défaut venait probablement du code des nœuds
retiré dans la même version.) Le journal `[TECHDESC]` reste en place
(dédoublonné, 256 cas).

### 2026-11-04 — Icônes d'après les scouts (compilé, déployé, non testé)

`ApIconeDuCheck(genre, cle)` (fin d'archipelago.cpp) : règle unique
`IconeDuScout` = piège > progression > useful > filler, sans distinguer
le destinataire. **Décision de l'auteur (2026-11-04) : icône selon les
drapeaux, y compris pour un item d'un autre joueur** (pas de logo AP /
`icon_vanilla` réservé aux autres joueurs). nullptr sans scout → icône de
test.
Branché : technos (`IconeEffective`, IconSubManager.cpp), marqueurs de
site (scooting.cpp, journal `[SITEUI] … pas de scout`). Lois : session des
bugs.

Test proposé : `MultiServer` local sur un `.archipelago` généré avec
l'apworld, Connect avec `localhost:38281`. Attendu au journal :
`[AP NET] RoomInfo`, `Connect envoye`, `DataPackage …`, `Connected : …`,
`[AP SESSION] dossier mods\ap_…`, `ReceivedItems index 0, … nouveau(x)`,
puis les scénarios du slot débloqués au menu.

### 2026-11-04 — Validation de l'auteur, nettoyage du debug

Validés par l'auteur (et vus au journal) : goal (victoire hors connexion,
puis `envoye : 0 location(s) + goal (StatusUpdate 30)` à la connexion),
reconnexion sans doublon (`ReceivedItems index 0, 5 item(s), 0
nouveau(x)`), icônes d'après les scouts. Retirés à sa demande :
`EXPEDITIONS_RAPIDES` (et le hook de l'envoi d'expédition),
`LOIS_SANS_DELAI`, toutes les touches de debug (F3–F9, F11). Ajout : menu
principal verrouillé sans connexion (voir `menu_connexion.md`).

### 2026-11-04 — Avant le test grandeur nature

- Sites : le texte d'un check porté par un scout (survol du dilemme,
  inventaire du scout, popup « Check sent ») = nom du check du scout
  (`ApNomDuCheckLocation`, archipelago ; repris par `ApCheckById` /
  `ApDescriptionW`, ap_data.cpp), repli = nom de la location (plus de
  `[AP] Check:`).
- Retirés : `ApChargerChecksDeTest` (checks 1001-1004), le champ
  `arbreTest` et la simulation de réception dans `SendLocationCheck`.
- Constat (`checks.txt` de la session de test) : seuls des checks de lois
  sont allés jusqu'au serveur ; technos, sites, quêtes, montées de
  température à voir dans le test grandeur nature.
- `apworld\test\frostpunk.apworld` reconstruit (sources du 2026-11-03,
  inchangées).
