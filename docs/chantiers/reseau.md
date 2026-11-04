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
