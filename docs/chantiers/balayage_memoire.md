# Balayage memoire et ancrages (ex-section 8)

> Historique detaille, copie mot pour mot depuis CLAUDE.md le 2026-11-02.
> Le resume a jour est dans CLAUDE.md ; les nouveaux resultats de test s'ajoutent ici.

## 8. Le balayage mémoire — pourquoi et à quel prix

> **Restructuration du 2026-10-24** : la résolution des références globales
> (systèmes, gabarits, entrées de message, technos AP) et la détection des
> chargements ont quitté `scooting.cpp` pour le **cycle de vie de la partie**
> dans `mod_common.cpp` (`PartieTick`, appelé avant `ScootingTick`). Une init
> réussie incrémente `PartieGeneration()` ; les modules comparent à la
> dernière valeur vue. Log : `[PARTIE] scn=… dif=… sel=… msgSys=… bat=…
> msgEnt=…` puis `[PARTIE] prete (generation N)`, et côté scouting
> `[SCOUT] ancres : sites=… city=… exp=… sel=…`. Partie « prête » = système
> de scénario lisible ; scouting « prêt » = au moins un site.
>
> **Depuis le 2026-10-15, `ScootingInit` ne balaye plus la mémoire.** Les 10
> classes sont obtenues par ancrages et captures (détail en fin de section,
> « Piste : remplacer le balayage… ») :
> - systèmes Scenario, Difficulty, Selection, MessageProducer : `SystemeParId` ;
> - sites, envois, ville, expéditions : `ComposantsDuSysteme` (liste `+0x2078`) ;
> - gabarits de bâtiment et entrées de message : `GabaritsBatCaptures` /
>   `EntreesMessageCaptures`, alimentés par les hooks des constructeurs
>   (`ListeCaptures`, verrou SRW, compactage des objets morts quand plein).
> `RecenserSites`, `RescanExpeditions` et `FindDifficultySystem` sont aussi
> convertis. `CollectObjectsByClasses` ne sert plus qu'aux diagnostics ; le
> texte ci-dessous reste comme historique.
> `g_sites` porte désormais `MAX_SITES = 256` (le nombre de sites dépend du
> scénario).

Le jeu ne tient **aucun annuaire** des objets accessible de l'extérieur. Pour
trouver une instance (`FrostpunkScenarioSystem`, les sites, les gabarits), on
balaye les régions de mémoire **privée** du processus, on lit chaque mot aligné
comme un vptr candidat, on vérifie qu'il pointe dans l'image du module, puis on
résout le RTTI.

`CollectObjectsByClasses` fait **un seul balayage pour les 10 classes** à la
fois, avec une table de hachage `vptr → verdict` :
- taille `131072`, sondage linéaire **borné à 32**, arrêt d'insertion aux 3/4
  (une table pleine à sondage non borné partait en boucle infinie) ;
- la table est **vidée si la liste de noms change** — le verdict mémorisé est un
  indice dans la liste, donc changer la liste rend les verdicts faux ;
- borne sur la taille des régions :

```cpp
if (lisible && mbi.RegionSize > 0x1000000) lisible = false;   // 16 Mo
```

Mesures sur la machine de l'auteur (2×16 Go DDR5, ~6,5 Go de jeu) :

| Borne | Durée | Résultat |
|---|---|---|
| 4 Mo | 781 ms | **sites = 0** (inutilisable) |
| **16 Mo** | **15,8 s** | tous les objets trouvés (42 sites, 124 gabarits, 510 entrées de message) |
| 64 Mo | 108 s | idem |

**Compromis retenu : 16 Mo.** À tester encore : borne à **8 Mo**, et suppression
du pas `d += 2`.

Note : une recherche **linéaire** dans un cache de 8192 entrées s'est révélée
plus coûteuse que le RTTI qu'elle évitait (12 s) — d'où la table de hachage.

### Classes balayées (ordre significatif, buffers de 1024)

```cpp
static const char* noms[10] = {
    "FrostpunkExpeditionSiteComponent", "FrostpunkExpeditionSendingComponent",
    "FrostpunkCityComponent",           "FrostpunkSelectionSystem",
    "FrostpunkExpeditionComponent",     "FrostpunkScenarioSystem",
    "FrostpunkDifficultySystem",        "FrostpunkBuildingComponentTemplate",
    "FrostpunkMessageProducerSystem",   "FrostpunkMessageEntry",
};
```

Puis, dans l'ordre : `ScenarioSetCaches`, `RemplirCacheGabarits`,
`ResoudreTechnosAP`, `MessageSetReferences`.

### Piste : remplacer le balayage par des ancrages fixes

Idée de l'auteur : faire comme pour les technologies (`g_selfTech` résolu par un
global fixe via `ResolveSelf`) et atteindre chaque objet par une chaîne de
pointeurs depuis un global de l'image.
- **Mesuré le 2026-10-15 (F8, `DumpSceneSystems`)** : la « scène » à
  `+0x2C31B20` (pas de RTTI) est un **annuaire des types**, entrées de 0x10 =
  `{ptr nom, id 32 bits + 4 octets de bruit}`, et **pas** `{nom, objet}`.
  `FindSceneSystem` renvoie donc un id, pas un objet. Ids utiles :
  Difficulty `0x03`, Scenario `0x0C`, MessageProducer `0x15`,
  Technology `0x41`, Selection `0x4C`, GameEntity `0x58`, WorldMap `0x60`,
  City `0x64`, ExpeditionSite `0x65`, Expedition `0x68`, ExpeditionHost `0x69`.
- **Trouvé le 2026-10-15** : chaque système a son **global fixe** dans
  l'image, `modBase + 0x2C30200 + id × 0x20` (`SystemeParId(id)`,
  constantes `GameOffsets::SystemesGlobaux` et `IdSysteme::*`). Vérifié sur
  Difficulty `+0x2C30260`, Scenario `+0x2C30380`, Technology `+0x2C30A20`
  (= `g_selfTech`), Selection `+0x2C30B80` ; Economy `+0x2C309E0` (= second
  candidat de `g_selfRessource`). Chaque système a aussi une seconde copie
  ailleurs (`+0x4062870`, `+0x2C3CB58`, `+0x40924C0`, `+0x4062BA8`).
  `VerifierSystemesParId` contrôle la règle sur tout l'annuaire.
  Il n'existe **pas** de tableau `id -> système` dans le tas.
  Règle vérifiée sur **tout** l'annuaire : 196 ok, 0 ko, 5 globaux vides.
- **Composants — confirmé (2026-10-15)** : chaque système tient ses
  composants en paires `{entité, composant}` à `+0x2078` (pas de 0x10,
  compte à `+0x2080`) ; `+0x2060` = entités seules (compte `+0x2068`).
  Vérifié, une seule classe de composant par liste :
  ExpeditionSite → `FrostpunkExpeditionSiteComponent` (42 dans le scénario
  testé), Expedition → `FrostpunkExpeditionComponent` (2),
  ExpeditionHost → `FrostpunkExpeditionSendingComponent` (3 ; celui retenu
  par le scan est le 3e), City → `FrostpunkCityComponent` (1),
  Building → `FrostpunkBuildingComponent` (123 bâtiments posés).
- **Le nombre de sites dépend du scénario** (42 dans celui testé, davantage
  ailleurs). Attention au plafond `g_sites[64]` de `scooting.cpp`.
- **Gabarits de bâtiment et entrées de message** : aucune trace dans les
  systèmes ; aucun global ne pointe dessus. Les `FrostpunkMessageEntry` sont
  **contiguës, au pas de 0xE0** (CitizensDied est englobée par l'entrée
  précédente à −0xE0). Le gabarit n'a aucun pointeur RTTI ni chaîne dans
  ses 0x80 premiers octets. Mesures du 2026-10-15 :
  - `FrostpunkBuildingComponent +0x20` → son gabarit ; mais seuls les types
    **posés** sont atteints (20 gabarits distincts pour 123 bâtiments, sur
    123 en cache) : insuffisant pour l'arbre des technos.
  - Un tableau contigu de **353** entrées de message (sur 510 : il y en a
    donc plusieurs) ; **aucun pointeur** vers son début, ni dans le tas
    complet ni dans les systèmes de message.
  - Aucun pointeur structurel vers le gabarit, même dans tout le tas.
  - Recherche dans tout le tas sans borne : **~105 s**, inutilisable.
  Conclusion provisoire : pas d'ancrage mémoire simple pour les templates.
  - Test au menu principal (`ScootingTesterGabaritsPrecoces`) : balayage de
    ces 2 classes en **2,6 s** au menu (mémoire bien plus petite). Les **510
    entrées de message sont déjà là** et gardent la même adresse en partie
    (510/510). Mais **seuls 6 gabarits de bâtiment sur 124** existent au
    menu : les autres sont créés au chargement de la partie.
    Stabilité après rechargement d'une sauvegarde : non testée.
  - Piste en cours : **hooker les constructeurs** des templates pour capturer
    chaque instance à sa création (zéro balayage). F8 lance
    `ChercherReferencesVtable` : instructions `lea reg,[rip+disp]` qui
    chargent la vtable, avec la fonction englobante via `.pdata`.
  - Résultat : **une seule** référence par classe.
    `FrostpunkBuildingComponentTemplate` : vtable `+0x20E6710`, chargée
    dans la fonction **`+0x1916F90`** (taille 0x1BB, lea à +0xE).
    `FrostpunkMessageEntry` : vtable `+0x21363B0`, chargée dans
    **`+0x19E68B0`** (taille 0xF9, lea à +0x58). Constructeurs présumés.
  - **Validé (2026-10-15)** par hooks posés dès le démarrage
    (`InstallCaptureGabarits` dans `IconSubManager`, `InstallCaptureEntrees`
    dans `MessageManager`) :
    gabarits : 124 appels, 124 adresses, 124 de la bonne classe, 123/123 du
    cache du scan ; entrées : 582 appels, 582 adresses, **510** encore de la
    bonne classe (72 détruites depuis), CitizensDied capturée.
    → Toujours revalider une capture par `ClasseExacte` avant usage.
    Non testé : rechargement d'une autre sauvegarde (les captures
    s'accumulent ; tableaux bornés à 1024 / 2048).
- Les **composants** (sites, envoi, expéditions, ville) et les **gabarits**
  sont à atteindre depuis un système propriétaire ou depuis l'entité
  (`composant +0x18`) — chaîne à trouver par `FindStaticPointersTo` /
  remontée Ghidra.

### Diagnostic d'init incomplète

Si l'init échoue, le mod ne peut pas fonctionner : il **faut** un message
d'erreur qui liste précisément **ce qui manque** (`sites=0 send=0 city=1 …`),
pas un simple « incomplet ».

---

