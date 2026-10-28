# CLAUDE.md — Mod Archipelago pour Frostpunk

Ce fichier est le contexte de reprise du projet. Il est destiné à Claude Code et
suppose qu'aucune conversation antérieure n'est disponible.

---

## 1. Ce qu'est le projet

Un mod **Archipelago** (randomizer multiworld) pour **Frostpunk** (version Epic
Games), écrit en **C++**, injecté par **proxy DLL** (`version.dll`) + **MinHook**.

Le jeu n'expose **aucune API de modding**. Tout passe par :
- du reverse engineering (Ghidra, base de chargement `0x140000000`),
- des hooks MinHook sur des fonctions trouvées par décompilation,
- des lectures/écritures mémoire sur des structures identifiées à la main,
- un balayage de la mémoire privée du processus pour énumérer les objets.

### Philosophie assumée par l'auteur

> « Je cherche à faire un mod, pas la Joconde. Si la fonctionnalité finale est
> fonctionnelle et visuellement/mécaniquement on a ce qu'on veut sans dégradation
> des performances et crash, ça passe. »

On ne refactorise pas pour l'élégance. On ne propose pas d'abandonner un chantier
parce qu'il est difficile : l'auteur a refusé cette proposition à chaque fois.

---

## 2. Conventions de travail (impératives)

- **Tout en français** : échanges, commentaires de code, messages de log.
- **Exception : tout texte affiché EN JEU est en anglais** (popups, noms et
  descriptions de groupes, dilemmes, choix, textes d'items/checks…) —
  décision de l'auteur du 2026-10-28, pour partager la progression avec des
  gens qui ne parlent pas français. Les logs restent en français.
  (Seule exception connue : la blague `Desmond2` de `g_overrides` dans
  `hooks_loc.cpp`, laissée à l'auteur.)
- **Nouveaux fichiers `.cpp` autorisés** (règle « pas de nouveau `.cpp` »
  levée le 2026-10-27 : elle datait de l'époque sans Claude Code). Une
  fonctionnalité va de préférence dans le fichier qui traite déjà son
  domaine ; un nouveau domaine peut avoir son fichier. Tout nouveau `.cpp`
  doit être ajouté à `compilationVersion.cpp.txt`.
- **Fonctions entières.** Quand une fonction change, on la redonne complète,
  jamais sous forme de patch partiel ou de « remplace la ligne 42 par ».
- **Pas d'accents ni de caractères non-ASCII dans les `printf`** : la console
  du jeu les rend en mojibake (`MÞre` au lieu de `Mère`).
- **Vocabulaire (décision de l'auteur, 2026-10-28)** : **bulle** = le cercle
  en ville qui ouvre un dilemme (F6, arrivée d'un groupe ; `evenements.cpp`,
  `dilemmes_ap.cpp`) ; **message** = la popup à gauche de l'écran
  (`AfficherNotificationAP`, `MessageManager.cpp`).
- Les identifiants et noms de variables sont en français (`ViderCargaison`,
  `MettreAJourListePortee`, `ToucheAppuyee`).
- L'auteur teste lui-même, en jeu, et renvoie les logs. Ne pas présumer d'un
  résultat : attendre le retour.

---

## 3. Environnement et compilation

- Visual Studio, **x64 Native Tools Command Prompt**, MSVC.
  Solution : `FrostpunkArchopelago.sln` / `FrostpunkArchopelago.vcxproj`.
- Cible : DLL proxy nommée `version.dll`, déposée à côté de `Frostpunk.exe`.
- **La compilation se fait par `cl` en ligne de commande**, pas par le
  `.vcxproj` (configuré en `Application` et incomplet : il lui manque
  `IconSubManager.cpp` et `MessageManager.cpp`). La commande de référence est
  dans `compilationVersion.cpp.txt` — tout nouveau `.cpp` doit y être ajouté.
- Claude Code peut compiler et déployer lui-même (PowerShell, hors sandbox) :

  ```powershell
  $vc = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
  $cmd = (Get-Content .\compilationVersion.cpp.txt -Raw).Trim()
  cmd /c "`"$vc`" >nul && $cmd /nologo"
  ```

  Puis, **seulement si `Frostpunk.exe` ne tourne pas** (DLL verrouillée),
  copier `version.dll` dans `C:\Program Files\Epic Games\Frostpunk\`. Ce
  dossier est **autorisé d'office** ; tout **autre** emplacement hors du
  projet se demande à l'auteur, à chaque fois. **Pas de sauvegarde** de
  l'ancienne DLL (demande de l'auteur).
  Le message « 'vswhere.exe' n'est pas reconnu » de vcvars64 est sans effet.
- Dépendance externe : **MinHook** (sources dans `minhook\`, lib statique).
- Fichiers annexes du mod dans un sous-dossier `mods\` du jeu (persistance).
- Outils utilisés hors build : **Ghidra** (analyse de `Frostpunk.exe`),
  **texconv** (`Texconv\texconv.exe`), **7-Zip**, scripts **PowerShell** pour
  les textures.

---

## 4. Architecture des fichiers

| Fichier | Rôle |
|---|---|
| `version.cpp` | Point d'entrée : proxy des 17 exports de `version.dll`, `DllMain`, `PayloadThread` (console + journal `logs\console.log`, `MH_Initialize`, hooks des constructeurs de templates **en premier**, puis les autres hooks), boucle principale : touches de debug (F3–F9 ; F11 réservée aux tests demandés par l'auteur ; jamais F10 = menu des options ; F3 = création de groupe, F6 = dilemme piège de test (remplace l'ancienne fenêtre `OpenKeyLogWindow`), F7 = progressive de test, F8 = rapprocher le premier groupe de la ville, F9 = diagnostic des groupes, F11 = ressources de test) + tick d'une seconde (`EnsureTechSelf`, `EnsureResSelf`, **`PartieTick`**, `CaptureRunState`, `ScootingTick`, `SurveillerArbreTechno`) |
| `mod_common.h/.cpp` | Socle : `SafeRead`/`SafeWrite`, `GetClassNameFP` (RTTI), ancrages (`SystemeParId`, `ComposantsDuSysteme`, `IdSysteme::*`), captures (`ListeCaptures`), **cycle de vie de la partie** (`PartieTick`, `PartiePrete`, `PartieGeneration`, `PartieSelection`, `PartieRechargee`, garde-fou sur la liste des sites), **diagnostics d'ancrage** (`DiagAncrages`, `DiagGabaritsPrecoces`, `ChercherValeurDansTas`, `ChercherReferencesVtable`…), balayage mémoire historique (`CollectObjectsByClasses`), résolution des self par globaux, dumps (PropertyManager, vtables), hooks d'E/S (`NtReadFile`/`NtWriteFile`), allocateur du moteur, `ToucheAppuyee`, offsets (`GameOffsets`, `SetupOffsets`, `TECH_*`, `RES_*`), macro `CHRONO` |
| `hooks_loc.h/.cpp` | `hkTranslate` : cœur du détournement de **tous** les textes affichés |
| `hooks_research.h/.cpp` | Les 4 hooks de recherche : `CanResearch`, `ResearchStart`, `ResearchComplete` (location check), `CancelResearch` |
| `tech_state.h/.cpp` | Accès au tableau des technos du jeu (`g_selfTech + 0x2248`) : état par hash ou par pointeur, clés de localisation, parcours |
| `tech_brain.h/.cpp` | État runtime par techno `{researched, received}` ↔ état mémoire du jeu |
| `tech_runtime.h/.cpp` | `TechRuntime`, `RT_Get`, `RT_CanResearch` |
| `known_techs.h` | Table des technos (hash → nom) |
| `archipelago.h/.cpp` | **Surface de communication Archipelago ↔ jeu** (noms en anglais, demande de l'auteur). Serveur → jeu : `ReceiveProgressive(tree, player)` (toute techno reçue est une progressive, même un arbre à un niveau), `ReceiveResource(name, amount, player)`. Jeu → serveur : `SendResearchCheck(hash)`, `SendLocationCheck(id)` — **seuls** points de sortie (appelés par `AP_SendCheck` et par `ScootingTick`). Chaque fonction : partie prête → effet → popup. La future classe réseau n'appellera que ça |
| `resources.h/.cpp` | Lecture/modification des ressources (`Res_Get`, `Res_Add`, `Res_OublierContextes`) via le hook du setter `+0x173FE40`. Le hook capture les **6 arguments** du dernier appel du jeu par ressource ; `Res_Add` met un crédit en attente, appliqué **par le hook, sur le thread du jeu** (l'ancienne version, commentée, ne passait que 4 arguments : règle n° 5) |
| `ap_ressources.h/.cpp` | « Porteur » : définition de ressource clonée servant de support visuel aux items Archipelago (`ApPorteurInit`, `ApPorteurDef`), `EconomieValide` |
| `ap_data.h/.cpp` | Table des checks Archipelago (`ApCheck` : id, joueur, description) ; `ApChargerChecksDeTest` en attendant le serveur |
| `scooting.h/.cpp` | Expéditions, sites, checks portés par les scouts, persistance ; contient la table `SITE_PAYLOADS` (contenu de chaque site par scénario) et `ScootingInit`, qui ne lit **que** les objets du scouting (sites, ville, expéditions) dans les listes des systèmes. Refait son init quand `PartieGeneration()` change |
| `scenario.h/.cpp` | Scénario courant, difficultés, `g_run`, systèmes du jeu, `FindSceneSystem` |
| `IconSubManager.h/.cpp` | Substitution des icônes (arbre des technos, bâtiments), atlas |
| `MessageManager.h/.cpp` | Émission de notifications (popup gauche de l'écran) — **fonctionnel** |
| `groupes.h/.cpp` | Groupes de survivants sur la carte du monde : création d'entité par `Creer` (`+0x1016250`) sur le thread du jeu (`GroupesPompe`) ; F3 = test de création |
| `evenements.h/.cpp` | Mécanique des événements et dilemmes du jeu : hooks de l'invocation d'événement `+0x17B9B70` (journal `[EVT]`), de la création `+0x17CDE10` (remplacement du dilemme d'arrivée des groupes AP) et de la résolution `+0x17D40C0` ; `DilemmeCloner`, `DilemmeAPCreer(def)`, `TuileAtlasBulles(icone)` |
| `dilemmes_ap.h/.cpp` | **Catalogue des dilemmes Archipelago** (bulles en ville, pièges) : table `DILEMMES_AP` (id, titre, sous-titre, texte, icône, titre de popup, choix si ignoré, ≤ 4 choix `{texte, ressource, quantité signée, effetSpecial}`) ; `DilemmeAPDemander("id")` (tout thread, file) → `DilemmesPompe` (thread du jeu). **Pour ajouter un piège : une entrée dans la table** |

---

## 5. Les sept règles apprises à la dure

Ces règles sont issues de crashs réels. Les enfreindre casse le jeu.

1. **Ne jamais donner au jeu un tampon statique.**
   Toute chaîne ou structure remise au moteur doit être allouée par
   **l'allocateur du moteur** (`MoteurAlloc`, `modBase + 0xE27BF0`). Un tampon
   statique passé au jeu corrompt le tas dès que le moteur tente de le libérer.
   Le jeu n'utilise **pas** le tas Windows (`HeapValidate` négatif sur les 9 tas).

2. **Les chaînes du moteur sont à comptage de références.**
   En-tête de **0x14 octets** avant les caractères : self-pointeur à
   `chars − 0x0C`, compteur de références à `chars − 0x04`. Écrire dans un objet
   de traduction sans cet en-tête provoque un crash différé. On **clone** l'en-tête
   d'une traduction valide et on fige le compteur à `0x40000000` pour que le jeu
   ne libère jamais notre objet.

3. **Ne jamais écrire dans un objet de traduction partagé.**
   Quand une clé est absente, `Translate` renvoie un objet **statique partagé**
   (pointeur de texte nul). Écrire dedans fait crasher le survol suivant. On
   renvoie **notre propre objet**, jamais celui du jeu.

4. **Ne pas écraser ce qui touche Last Autumn.**
   « Le problème est, reste et sera toujours Last Autumn, on ne DOIT PAS écraser
   des choses qui l'impactent. » En particulier, `TexteExpedition` ne doit pas
   écrire dans `Expeditions/EffectBaseResources`.

5. **Signatures de fonctions : trop d'arguments ne nuit pas, en manquer si.**
   Une signature de hook incomplète corrompt la pile. En cas de doute, déclarer
   4 arguments `uintptr_t`.

6. **`extern` dans le `.h`, définition sans `extern` dans un seul `.cpp`.**
   Un `extern` avec initialisation dans le `.cpp` donne un symbole non résolu.

7. **Tous les buffers à la taille annoncée.**
   Passer `maxParClasse = 256` avec un `bufScn[16]` écrase la mémoire voisine.
   Les 10 buffers de `CollectObjectsByClasses` sont tous à **1024**.

---

## 6. Méthode de diagnostic

Ce qui a effectivement payé, dans l'ordre d'efficacité :

1. **Bissection fonctionnelle.** Réduire la fonction suspecte à un passe-plat
   (`return oOriginal(...)`), vérifier que le crash disparaît, puis réactiver
   **bloc par bloc**. C'est ainsi qu'on a trouvé que le crash du dilemme venait
   de l'écriture sans en-tête de comptage, et non du marqueur `{baseResources}`
   manquant comme on l'avait d'abord cru.

2. **Ghidra, remontée d'appelants.** Pour trouver une fonction (créateur de
   message, poseur de texte, allocateur), on part d'une chaîne ou d'un global
   connu et on remonte les références. **Attention aux conversions d'adresse** :
   on a perdu un chiffre (`0x67C580` au lieu de `0x167C580`) et posé un hook sur
   du code jamais appelé.

3. **`_ReturnAddress()` est inutilisable dans un hook MinHook** (adresse
   aberrante `+0x4C2366BBE`). Substitut : balayage de pile à la recherche de
   valeurs précédées d'un octet `E8` (opcode `call`).

4. **Dump RTTI par vtable.** `GetClassNameFP` lit le `CompleteObjectLocator` en
   `vptr − 8`, vérifie `sig == 1` (format x64), lit le `TypeDescriptor` à
   `base + tdRva + 0x10` et décode `.?AVFrostpunkTruc@@` → `FrostpunkTruc`.

5. **Dump du système de réflexion (PropertyManager).** Donne les noms et offsets
   de toutes les propriétés d'une classe. C'est la source de tous les offsets de
   gabarits listés plus bas.

---

## 7. Offsets, structures et constantes connus

### Globaux (relatifs à `modBase`, image chargée à `0x140000000` dans Ghidra)

| Offset | Contenu |
|---|---|
| `+0xE27BF0` | **Allocateur du moteur** — `uintptr_t (*)(size_t)` |
| `+0x167C580` | Création d'un message — `void (*)(uintptr_t, uintptr_t*, uintptr_t, uintptr_t)` |
| `+0x1AA9D70` | Pose du **titre** d'un message — `void (*)(uintptr_t, uintptr_t*)` |
| `+0x1AA9C00` | Pose du **texte** d'un message — même signature |
| `+0x16896B0` | **Émission** du message dans la file |
| `+0x40B1B30` | Paramètres de message |
| `+0x40916D0` | Global de l'économie (sert à détecter un rechargement de partie ; aussi candidat de `g_selfRessource`) |
| `+0x2C30A20` | `g_selfTech` (système de technologies) — second candidat `+0x4062BA8` |
| `+0x2C309E0` | Second candidat de `g_selfRessource` |
| `+0x2C31B20` | Annuaire des types de systèmes : table `{nom, id}` au pas de 0x10 (voir section 8) |
| `+0x2C31B48` | Global de scénario |
| `+0x1326A60` | `CanResearch(self, tech, ctx) -> char` |
| `+0x1329610` | `ResearchStart(self, tech)` |
| `+0x1327120` | `ResearchComplete(self)` |
| `+0x1326C80` | `CancelResearch` |
| `+0x173FE40` | Setter de ressources (`ModifyResource`) — Ghidra `0x14173FE40`, voir ci-dessous |
| `+0x0FEB340` | `Translate(cle) -> valeur` |
| `+0x1267C50` | `SetScenario(self, scenario, bool)` |

### Système de technologies (`g_selfTech`)

- `+0x2248` : base du tableau des technos — entrées de `0x10` octets
- `+0x2250` : nombre d'entrées
- Entrée : `+0x00` pointeur techno, `+0x08` état (0/1/2/3)
- Techno : `+0x20` **hash** (identifiant stable)
- `+0x2260` : techno en cours de recherche (remise à zéro par l'appel original,
  donc à lire **avant**)

États du jeu, pilotés par le mod :
| researched | received | état jeu |
|---|---|---|
| F | F | 0 (recherchable) |
| F | T | 0 (recherchable, effet à la recherche) |
| T | F | 3 (faite, effet en attente) |
| T | T | 2 (faite, effet appliqué) |

> À vérifier : le commentaire de `tech_state.h` indique que « l'état 3 n'est
> plus utilisé : la vérité logique vit dans tech_runtime ». Le tableau ci-dessus
> est peut-être en partie obsolète.

La recherchabilité ne dépend **pas** de l'état mais de `RT_CanResearch`
(`= !researched`), via le hook sur `+0x1326A60`.

### Setter de ressources `+0x173FE40` (décompilé le 2026-10-27)

`int ModifyResource(eco, cle, int quantite, p4, int p5, char p6)` — 6
paramètres confirmés. Renvoie la quantité **réellement** appliquée.
- `cle` (2e) : passée par valeur à `ResIdToIndex(eco+0x2240, &cle)`.
  **Ce n'est PAS `définition + 8`** (supposition fausse du 2026-10-27 : AV
  dans les statistiques à `Frostpunk.exe+174391D`, et aucun appel du jeu
  reconnu par le GUID lu à la clé). Probablement la définition elle-même ;
  la convention est **déduite du premier appel du jeu**
  (`DeduireConventionCle`, `g_decalageCle` = 0 ou 8, log
  `[RES] convention de cle du jeu : definition + N`).
- Une quantité **négative** est refusée si le bit 1 de
  `*(DAT_1440628f8 + 0x68)` est levé, **sauf** si `p6 != 0` (forçage).
- Le stock est seulement borné à `[0, INT_MAX]` : **pas** de plafond de
  stockage ici (la capacité, entrée `+0xC`, ne sert qu'aux notifications).
- `p4` et `p5` ne sont **jamais lus** par le setter : transmis à
  `FUN_141743880(eco, cle, delta, 0, p4, p5)` et aux écouteurs de
  l'événement (`eco+0x2108`, nombre `+0x2110`, pas 0x10 =
  `{objet, fonction}`), appelés `(objet, cle, ancien|nouveau stock, p4, p5)`.
- `FUN_141743880` = **statistiques** (décompilé le 2026-10-27). Attention
  au décalage : son `param_5` = notre `p4` (**inutilisé** : seuls les
  écouteurs le voient, provenance), son `param_6` = notre **`p5` =
  catégorie** :
  - `0` générique : gain → revenu (entrée `+0x34`, jour `+8`, historique
    `+0x40`) ; perte → consommation (`+0x38`, jour `+0xC`, historique `+0x58`) ;
  - `2` production : comme un gain + compteur `+0x30` ;
  - `1` retrait spécial (jour `+0x10`) ; `3`/`4` consommations ; autre : rien.
  Stats du jour : `*(entrée + 0x18) + jour × 0x14`, jour = `eco+0x225c`.
- **`Res_Add` crédite avec `p4 = 0, p5 = 0, p6 = 0`** : revenu générique,
  aucune provenance, notifications normales. Plus aucun argument emprunté.
- `p6 == 0` : notifications de seuil (`FUN_141687f80` sur `eco+0x2160` et
  `eco+0x2198` : stock plein / seuil). `p6 != 0` : silencieux.
- Compteur d'invocation de l'événement à `eco+0x2120` (assert « Invoking an
  event again while it is being handled! ») : `Res_Add` ne crédite que
  quand il vaut 0.

### `FrostpunkExpeditionComponent`

| Offset | Contenu |
|---|---|
| `+0x18` | Entité propriétaire |
| `+0x30` | Id (petit entier, 0..4096) |
| `+0x120` | Lieu courant |
| `+0x128` | Destination |
| `+0x130` | Troisième lieu |
| `+0x168` | Cargaison ressources (tableau dyn. : ptr, `+8` compte, `+12` capacité) |
| `+0x180` | Cargaison technologies |
| `+0x198` | Cargaison artefacts |

`CargaisonTotale` somme les **quantités**, pas le nombre d'éléments.

### `FrostpunkExpeditionSiteComponent`

- `+0x18` : entité → `+0x08` : bloc de localisation
- `+0x20` : gabarit
- `State +0x034` : passe de 0 à non-nul à l'exploration (sert à détecter
  l'exploration des **sites sans ressources en vanilla**)

### `FrostpunkExpeditionSiteComponentTemplate` (taille `0x438`)

Noms par état, à prendre dans l'ordre `0x270, 0x268, 0x278, 0x280` :
`Display name`, `Unvisited name`, `Explored name`, `Depleted name`.
Descriptions : `0x290`, `0x288`, `0x298`, `0x2A0`.
`Exploration dilemmas` : `+0x3D0`. `Exploration instant effects` : `+0x3E8`.
`Randomization tags` : `+0x418`.

### `FrostpunkExpeditionSendingComponent`

- `+0x60` : tableau des expéditions, `+0x68` : nombre déclaré

### `FrostpunkDifficultySystem`

- `+0x30` : tableau d'éléments de `0x10` = `{définition, niveau retenu}`
- Définition : `+0x00` nom, `+0x08`/`+0x10` GUID, `+0x18` liste des niveaux,
  `+0x20` nombre de niveaux

### `FrostpunkScenarioSystem`

- `+0x68` : objet scénario (→ `+0x00` pointeur vers le nom interne)
- `+0x78` : drapeau de victoire
- `+0x110`/`+0x118` : `GameId` (128 bits — sert de clé de persistance)

### `FrostpunkBuildingComponentTemplate` (taille `0x1D8`)

`Display name` `+0x138`, `Description` `+0x140`, `Icon tile` `+0x178`.

### Chaînes du moteur

```
chars − 0x14 : début de l'en-tête
chars − 0x0C : self-pointeur (doit valoir chars − 0x14)
chars − 0x04 : compteur de références (on fige à 0x40000000)
chars        : caractères UTF-16
```

`ModeleCleValide(modele)` vérifie `self == modele − 0x14` et un compteur
plausible. Les **poseurs de titre/texte attendent une clé 8 bits**, pas de
l'UTF-16 : sinon seule la première lettre s'affiche, puis crash.

---

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

## 9. État des chantiers

### Fonctionnels

- Survol de l'item Archipelago dans l'inventaire du scout (plus de crash).
- Un scout peut porter **plusieurs** checks ; le survol liste leurs descriptions
  (séparateur `L"\n"`).
- Les 4 étapes de l'expédition :
  1. aperçu du site → « Check Archipelago »
  2. survol du bouton de dilemme → contenu du check
  3. clic → crédit de l'inventaire
  4. survol de l'objet → liste des checks
- Sites **sans ressources en vanilla** : détection d'exploration par
  `State +0x034`, substitution du contenu, allocation du tableau par
  `MoteurAlloc(4 * 0x10)` s'il est absent.
- Persistance par fichier externe `mods\ap_pending_<GameId>.txt`
  (`ScootingSauverEnAttente` / `ScootingChargerEnAttente`). Le chargement se
  fait **dans le tick**, pas à l'init.
- Rechargement d'une sauvegarde antérieure, ou d'un autre scénario, sans
  fermer le jeu : `PartieTick` (mod_common) — garde-fou sur la liste des
  sites, puis `PartieRechargee()` (économie `+0x40916D0` **et** système de
  technologies, car l'économie peut être réallouée à la même adresse).
- Icônes de l'arbre des technologies : logo Archipelago pour un check destiné à
  un autre joueur, icône de l'item reçu sinon. Deux mécanismes (bâtiment et
  arbre), 3 atlas localisés, `SubDecl` par **numéro d'entrée** (2680, 3037, 1179).
- **Notifications** : `AfficherNotificationAP(titre, texte)` crée, remplit et
  émet un message **à la demande**, sans attendre qu'une notification du jeu ait
  eu lieu. C'était une exigence ferme : « J'ai vraiment besoin de ça », et le
  repli consistant à attendre un message du jeu pour en remplacer le contenu a
  été explicitement refusé — « Inacceptable ».
- Touches de debug (régression après changement de PC) : `GetAsyncKeyState & 1`
  était consommé par le jeu à haute fréquence d'images. Fix : `ToucheAppuyee(vk)`
  avec détection de front montant et tableau d'état `avant[256]`.

### En cours

- Suppression du balayage (15,8 s) au profit des ancrages et captures :
  **validée le 2026-10-15** — `[ANCRES]` en **0 ms**, mêmes nombres que le
  scan (42/3/1/1/2/1/1/124/1/510), y compris après rechargement.
- Observations des chargements (2026-10-15) :
  - Templates (gabarits, entrées) **non recréés** au rechargement, même vers
    un autre scénario (124 → 125 constructions) ; ScenarioSystem et
    DifficultySystem gardent leur adresse.
  - **Bug corrigé** : en passant à un autre scénario, l'économie
    (`+0x40916D0`) a été réallouée **à la même adresse** → `PartieRechargee`
    ne voyait rien, pas de ré-init, et `SurveillerSites` a écrit dans un site
    de l'ancienne partie (mémoire libérée). `PartieRechargee` surveille
    maintenant aussi le système de technologies (change à chaque chargement).
  - **Garde-fou ajouté** dans `ScootingTick` : si les listes `+0x2078` des
    systèmes de sites / d'envoi ne sont plus celles de l'init, on oublie la
    partie avant toute écriture (fenêtre entre le début du chargement et sa
    détection : recensement à 0 site, fausses arrivées « avant-poste »).
    **Validé le 2026-10-24** (réagit avant même `[LOADING]`). Seule la liste
    des **sites** est surveillée : celle d'envoi est réallouée en cours de
    partie quand un bâtiment d'envoi apparaît.
  - **Autre scénario (2026-10-24)** : 31 sites, `send=0`, `exp=0` — aucun
    bâtiment d'envoi encore. L'init exigeait un composant d'envoi → échec
    en boucle toutes les 5 s. Désormais `g_pret` ne dépend que des sites, et
    **`g_sending` est supprimé** : il ne servait qu'à compter les expéditions
    d'un seul des 3 `FrostpunkExpeditionSendingComponent` (bâtiments non
    identifiés). Le tick suit le compte de la liste du système `Expedition`
    (`CompteDuSysteme`, `g_compteListeExp`) et relit les expéditions quand il
    change ou qu'une des nôtres devient invalide.
  - **Validé le 2026-10-27**, après la restructuration (`PartieTick` dans
    `mod_common`) : partie de base (génération 1, 42 sites, 2 expéditions)
    puis autre scénario (génération 2, 31 sites, 1 expédition, expédition
    envoyée vers un site du scénario), sans boucle d'init ; chaque
    chargement est vu d'abord par le garde-fou de la liste des sites.

- **Couche `archipelago.cpp` (2026-10-27)**, `ReceiveProgressive` et
  `ReceiveResource` validés le 2026-10-28 (détail ci-dessous) : F7 →
  `ReceiveProgressive` sur un arbre au hasard, F11 → `ReceiveResource("coal",
  50)`, recherche terminée → `SendResearchCheck`, scout rentré en ville →
  `SendLocationCheck`. `Res_Add` réécrite : la capture de contexte avait
  disparu du hook (ni `Res_Get` ni `Res_Add` ne pouvaient fonctionner).
  **Ressources ancrées à l'init** (`Res_InitDepuisEconomie`, appelée par
  `PartieInit`) : conteneur de l'économie `+0x2240` (compte `+0x2248`,
  entrées de 0x70), `+0x00` de chaque entrée = définition, GUID à
  définition + 8 = **2e argument du setter** ; `self` = économie
  (`+0x40916D0`). Toute ressource est donc creditable sans attendre que le
  jeu la touche. Arguments 4..6 **fixés à 0** d'après les décompilés (voir
  section 7, setter de ressources) ; le hook relève encore chaque
  combinaison du jeu (`[RES] combinaison #n …`) : si l'une a `arg4=0`, c'est
  la preuve que les écouteurs acceptent une provenance nulle. Le crédit est
  appliqué au prochain appel de premier niveau du setter (en pause : au
  retour du jeu).
  Items reçus hors partie prête : **ignorés** pour l'instant (il faudra une
  file d'attente quand le réseau existera).
  - **2026-10-27** : F7 validé (popups « Technologie recue », effet OK).
    **F11 plante** : crédits mis en attente pendant la pause (2 popups OK),
    crash à la reprise, au premier crédit appliqué. Aucune trace : le relais
    du journal (tube) **perd les dernières lignes en cas de crash**.
    → `JournalImmediat` (version.cpp) écrit directement sur disque ; à
    utiliser pour toute ligne qui précède une opération risquée.
    → `AppliquerCredit` : vérifie la clé contre le conteneur (sans appeler
    `ResIdToIndex`), journalise avant/après, appelle le setter sous SEH
    (adresse fautive + remise à 0 du compteur d'événement `+0x2120`).
    Suspects : écouteur qui déréférence `p4 = 0`, ou clé fausse.
  - Résultat : clé trouvée dans le conteneur, mais **AV à
    `Frostpunk.exe+174391D`** = `FUN_141743880 + 0x9D` (statistiques, pas un
    écouteur) et **aucune** ligne `combinaison` alors que le charbon varie :
    la clé `définition + 8` est fausse. Corrigé par la déduction de la
    convention au premier appel du jeu ; `ApPorteurEstNotreGuid` accepte
    les deux conventions.
  - Test suivant : **convention = définition + 0** (confirmé). Combinaisons
    vues : `coal +2` → `arg4` = objet, `arg5 = 2` (production) ;
    `coal -1` → `arg4 = 0`, `arg5 = 1`. Puis **crash immédiat sur F11**,
    sans la ligne `appel du setter` : le journal était ouvert sans mode
    ajout, le relais a pu écraser la ligne immédiate. Corrigé
    (`FILE_APPEND_DATA`). **Bissection en cours** : `AppliquerCredit` liste
    les écouteurs (`eco+0x2108`, nombre `+0x2110`, `{objet, fonction}`) et
    les **coupe** pendant le crédit (`SANS_ECOUTEURS = true`, à remettre à
    false une fois le coupable trouvé).
  - **2026-10-28 : crédits validés** (écouteurs coupés) : charbon 212 → 262
    puis 260 → 310, cœur de vapeur 5 → 6. 11 écouteurs listés, objets sans
    RTTI, fonctions `+167BCC0, +1702E10, +1715D70, +1796960, +17E19F0,
    +18E23B0, +186E560, +166ABB0, +18CDF30, +16C9DF0, +17A9020`.
    Le **2e** F11 a planté **dans `EmettreMessage`** (entre « 5. texte
    pose » et « 6 ») pendant que le thread du jeu appliquait le crédit :
    **les popups étaient émises depuis le thread du mod**, en concurrence
    avec le jeu (qui émet aussi des notifications de seuil depuis le setter).
    → `AfficherNotificationAP` ne fait plus que **mettre en file** ;
    `PompeNotifications` émet, appelée depuis le hook du setter (thread du
    jeu, compteur d'événement à 0). **Conséquence : en pause, les popups
    attendent la reprise.** Relevé des threads (`NoterThread`, log
    `[THREAD]`) de `Translate`, `ModifyResource`, `CreerMessage`, pour
    trouver une pompe qui tourne aussi en pause.
    Écouteurs **réactivés** (`SANS_ECOUTEURS = false`) pour vérifier qu'ils
    n'étaient pas en cause.
  - **2026-10-28 : `ReceiveResource` VALIDÉ**, écouteurs actifs, popups
    par la file : 5 F11 + un crédit de 100, aucun plantage (charbon
    211 → 512, cœurs de vapeur 5 → 9). Les écouteurs n'étaient pas en cause.
    **`[THREAD]` : `Translate` et `ModifyResource` tournent sur le même
    thread** (le thread du jeu) → la pompe des popups peut passer dans
    `hkTranslate`, qui est appelé aussi en pause (à faire).
  - Règle générale qui en découle : **rien qui touche un système du jeu ne
    doit s'exécuter sur le thread du mod** (popups, crédits). Les écritures
    d'état de techno (`OnItemReceived`, F7) et le tick de scouting le font
    encore : à surveiller.

### Ouverts

- **Livraison des items par groupe de réfugiés** (demandé le 2026-10-28) :
  prendre le contrôle des groupes de survivants qui marchent vers la ville
  (scénario Refugees, entre autres) pour y faire **arriver** les items AP
  (citoyens surtout : trop de citoyens d'emblée déséquilibre un scénario),
  au lieu d'un crédit instantané. Étape 1 en cours : diagnostic **F9**
  (`DiagSystemesGroupes`, mod_common) sur `SurvivorGroup 0x6D`,
  `Settlement 0x6C`, `Outpost 0x6A`, `OutpostTransport 0x6B`,
  `ExpeditionHost 0x69`, `Expedition 0x68`, `AbsentCitizen 0xB7`,
  `WorldMapMarker 0x61`. Reste à trouver dans Ghidra la fonction qui
  **crée** un groupe (événement « des réfugiés approchent »).
  - **Exigences de l'auteur** : livraison possible pour les **citoyens ET
    les ressources** (les technos peut-être) ; **le joueur choisit**, par
    catégorie, entre arrivée immédiate et arrivée par groupe ; le groupe
    doit être **créable dans n'importe quel scénario, à n'importe quel
    moment** (pas seulement Refugees).
  - Conception visée : le groupe sert de **véhicule**. On mémorise les items
    qu'il porte ; à son **arrivée**, on applique les `Receive*` immédiats
    (ressources, technos), et pour les citoyens soit la charge native du
    groupe, soit un ajout direct. Il faut donc : (a) créer un groupe vers la
    ville, (b) détecter son arrivée, (c) neutraliser sa charge native quand
    elle ne sert pas.
  - **F9 du 2026-10-28** (Refugees, 2 groupes visibles) :
    `FrostpunkSurvivorGroupSystem` (`0x6D`) → liste `+0x2078` = **2 paires**
    `{CompoundEntity, FrostpunkSurvivorGroupComponent}`. Ce ne sont **pas**
    des expéditions (liste Expedition vide). Composant de **0xA0** octets
    (contigus) : `+0x18` entité, `+0x20`
    `FrostpunkSurvivorGroupComponentTemplate`, `+0x28 = 2` (état ?),
    `+0x30 = {1,1}`, `+0x48` et `+0x80` pointeurs sans RTTI, `+0x78` float
    0.0001 + 1. Entité nommée **`RefugeeScenarioSurvivors01`**, son
    `CompoundTemplate` à `+0x28` : gabarit **propre au scénario** — d'où la
    difficulté pour les autres scénarios. Settlement / Outpost /
    OutpostTransport / AbsentCitizen vides. F9 dumpe maintenant aussi le
    template du groupe et les **propriétés nommées** (réflexion) des deux
    classes.
  - **`FrostpunkSurvivorGroupComponentTemplate`** (réflexion, 2026-10-28) :
    `+0x028` **Survivors** (tableau dyn. d'objets intégrés : ptr, `+0x30`
    compte = 1 dans Refugees), `+0x040` Base movement speed (objet intégré ;
    dword `0x447DC000` = 1015.0), `+0x048` Initialization on world map
    unlock, `+0x050` Display name (`@Expeditions/SurvivorPanelHeader/
    [RefugeesScenario]OurPeople`), `+0x060` Description, `+0x070` Icon,
    `+0x0A0` Image, `+0x0D0` SFX, `+0x0E0` Agents from origin text
    (`@Expeditions/SurvivorGroupName`), `+0x0E8` Agents without origin text,
    `+0x0F0` Hint text, `+0x0F8` **City reached dilemma** (lien d'entrée :
    dilemme à l'arrivée), `+0x108` Can be escorted, `+0x10C` **Starting
    state** (énumération), `+0x110` Death rate. Le template lui-même est
    propre au scénario (textes `[RefugeesScenario]`).
  - La fiche de réflexion de `FrostpunkSurvivorGroupComponent` (le
    composant, pas le template) ne liste **aucune** propriété : ses champs
    d'exécution (état `+0x28`…) restent à nommer.
  - Recherche de fiche (`FindClassRecord`) : 81 s pour le template — à ne
    pas appeler en jeu hors diagnostic.
  - Classes RTTI « Survivor » (Ghidra, 2026-10-28) : `SurvivorGroupComponent`
    / `…Template` / `…System`, **`MakeSurvivorGroupKnownEffect`**,
    `SurvivorGroupReachedCityEventTrigger`, `SurvivorGroupsSelector`,
    `SurvivorsEntry`, `RandomizedSurvivorsEntry`,
    `ScenarioRandomizedSurvivorsEntry` / `…Modification`, `RescuedSurvivor`,
    `RescuedSurvivorsSelector`, `RescueSurvivorsQuestObjective`,
    `CitizenSurvivorCountAccumulator`, `ExpeditionSiteSurvivorsTextParameter`,
    `UISurvivorGroupSelection`. **Aucune classe de création** : en vanilla,
    les groupes sont **pré-posés** sur la carte, cachés, et un effet les rend
    **connus**. Pour un groupe dans n'importe quel scénario, il faudra
    **instancier l'entité nous-mêmes** (piste : le chemin de création des
    expéditions, créées en cours de partie).
  - Ghidra n'a pas étiqueté les `vftable` de ces classes →
    `VtablesParNomDeClasse` (mod_common) les retrouve par RTTI (chaîne →
    TypeDescriptor → COL → vtable). F9 affiche, pour 6 classes (dont
    `FrostpunkExpeditionComponent`), vtable, méthodes et constructeurs, avec
    les adresses Ghidra.
  - **Résultat F9 (2026-10-28)** — vtables et fonctions qui les chargent :

    | Classe | vtable (Ghidra) | fonction(s) qui la chargent |
    |---|---|---|
    | `SurvivorGroupComponent` | `0x1421490F8` | `0x141A0EB70` (0x93) |
    | `MakeSurvivorGroupKnownEffect` | `0x1420F7898` | `0x141928160` (2 lea), `0x14192E9B0` |
    | `SurvivorGroupReachedCityEventTrigger` | `0x141F34F60` | `0x14140D310` |
    | `SurvivorGroupSystem` | `0x1420C2E68` | `0x1418AFA20` |
    | `ExpeditionComponent` | `0x142137D48` | `0x1419E9560` (0x1FF) |
    | `SurvivorsEntry` | aucune (struct sans vtable) | — |

    Les entrées `[1,2,4..7,9..11]` sont **communes** à toutes ces classes
    (base de réflexion) : seules `[0]`, `[3]`, `[8]` sont propres. Les
    méthodes de gameplay (exécution de l'effet…) sont donc au-delà de `[11]`.
    Le groupe est une `CompoundEntity` instanciée depuis un `CompoundTemplate`
    (`entité +0x28`, chemin `@Generated/Templates/GameEntities/WorldMap/
    SurvivorGroups/…`), comme probablement les expéditions : la création
    générique « template → entité » est la cible.
  - Décompilés des constructeurs (2026-10-28) : `0x1419E9560`
    (ExpeditionComponent, ≥ 0x20D octets) et `0x141A0EB70`
    (SurvivorGroupComponent, 0xA0) ne font qu'initialiser des champs à 0
    après le constructeur de base `FUN_140e19e90(this, -1, 0, 0)`. Groupe :
    `+0x28` dword = 0 à la construction (2 en partie : posé ailleurs),
    `+0x38` = `DAT_142c2fca8`, `+0x68` 16 octets = `DAT_142a12558`, `+0x78`
    float 1.0, `+0x7C` = 1. La vtable de `MakeSurvivorGroupKnownEffect` a
    29 entrées : propres à la classe seulement `[0]`, `[3]` `0x141932210`,
    `[8]` `0x1419317F0`, `[28]` `0x140D9C650` ; `[18..27]` sont des
    `_guard_check_icall` (méthodes vides).
  - **Les classes de réflexion n'ont pas de code de gameplay** (2026-10-28) :
    `[8]` renvoie le nom de la classe, `[3]` sa fiche de réflexion
    (`DAT_14409e8a0` pour l'effet). `FUN_141928160` est la **fabrique** de
    l'effet (`this == 0` → `MoteurAlloc(0x18)` puis construction, sinon
    construction sur place) : l'effet ne fait que 0x18 octets, aucun champ.
    Son exécution est ailleurs (système qui reconnaît la classe).
    Appelants des constructeurs : composant d'expédition `0x1419E94CA`
    (`CALL`) / `0x1419E94EB` (`JMP`) = sa fabrique, et `0x1419E9D72` ;
    groupe `0x141A0E7BA` / `0x141A0E7DB` = sa fabrique, et `0x141A0F062`
    (dans la méthode `[0]`, probablement un clonage).
    → **Traceur** `InstallTraceCreationEntites` (mod_common, posé au
    démarrage) : hooks des deux constructeurs, balayage de pile des
    12 premières constructions (adresses de retour précédées d'un `call`),
    log `[TRACE]` avec adresses Ghidra. But : la fonction générique
    « template → entité », appelée quand on envoie des éclaireurs.
  - **Résultat du traceur (2026-10-28)** (adresses de retour Ghidra, de
    l'appelé vers l'appelant) :
    - Démarrage : 1 construction de chaque sur la **pile** (prototypes de
      réflexion), sans intérêt.
    - Chargement d'une sauvegarde Refugees, 2 groupes :
      `0x141A0F067` (FUN_141a0f020, création du composant par son
      template ?) ← `0x140E36565` (FUN_140e36460) ← `0x1410439D1` ←
      `0x1410E5B1F` ← `0x140E24106` ← `0x1410F0330` … (chargement de scène).
    - **Création d'une équipe d'éclaireurs en partie** (suivie d'un
      `wood -40`, catégorie 4 = coût) : `0x1419E9D77` (FUN_1419e9d30) ←
      `0x141170F03` (FUN_141170ef9, fragment) ← `0x141172462`
      (FUN_141171cf0) ← `0x1410EBB01` (FUN_1410ebab0) ← `0x1411F00F6`
      (FUN_1411f00e0) ← `0x141017F68` (FUN_141017f38) ← `0x141016413`
      (FUN_141016250) ← `0x1418A3021` (FUN_1418a2f20, zone des systèmes de
      gameplay) ← `0x1410584FF` … (boucle de mise à jour).
      La fonction générique « template → entité » est dans cette chaîne,
      entre `FUN_1418a2f20` (appelant de gameplay) et `FUN_1419e9d30`.
  - **`FUN_1418a2f20` décompilé (2026-10-28) = « créer une équipe
    d'éclaireurs »** `(hôte, bâtiment, template, coût, forcer, p6)` :
    - `forcer == 0` → `FUN_1418a2850()` (peut-on payer ?) sinon `return 0` ;
    - **création** : `entité = FUN_141016250(*(base+0x2B2DE98), template, 0,
      &transform)` — `transform` = 64 octets : 3 lignes de rotation copiées de
      `DAT_1429292d0` (48 octets, identité présumée) + position (vec4)
      calculée depuis le bâtiment (`bâtiment +0x140` = x, `+0x148` = z) ;
    - **composant d'une entité par type** : `*(entité + 0x2A0 + idType × 8)`,
      `idType = FUN_14148c0a0()` ;
    - `FUN_1418a49b0(hôte, entité, bâtiment)` : rattachement de l'expédition ;
    - coût : tableau d'entrées de 0x14 (de `coût +0x20` si `coût +0x18`,
      sinon `template du composant +0x40`) débité par
      `FUN_141740440(économie, &tableau, -1, bâtiment)` (le `wood -40`).
    → **`FUN_141016250` est la fonction de création générique** à appeler
    pour un groupe, avec le `CompoundTemplate` du groupe.
  - **`FUN_141016250` décompilé (2026-10-28)** — `entitymanager.cpp`,
    **5 paramètres** : `Creer(gestionnaire, template, parent, &transform,
    drapeaux)` (l'équipe d'éclaireurs passe `parent = 0`, `drapeaux = 0`) :
    - limite `DAT_142b2de10 < 0xFFFF` entités vivantes (assert sinon) ;
    - `entité = template->vtable[0x158](template, gestionnaire)` ;
    - `gestionnaire +0x178` += 1 ; `template +0xE1` → drapeau `0x40000000000` ;
    - `entité +0x24C` (calque/scène) = celui du `parent`, ou de
      `FUN_141017490(gestionnaire)` si `parent == 0` ;
    - `entité->vtable[0x160](entité, drapeaux)` (init) ;
    - si `!(drapeaux & 0x20)` : `FUN_1410ee9d0(entité, &transform)`
      (pose la transformation) ;
    - `FUN_1410eceb0(entité, parent, 0, 1)` (rattachement au parent) ;
    - `FUN_141018240(gestionnaire + 0x48, entité)` (enregistrement) ;
    - si `!(drapeaux & 4)` : `FUN_141017dd0(gestionnaire, entité)`
      (activation dans les systèmes).
  - **`FUN_1418a49b0` décompilé** = rattacher l'expédition à son bâtiment :
    `composant +0x40` = **poignée** vers le bâtiment (`FUN_140e1a120(obj)`
    = obtenir la poignée, `FUN_140e1a110` = +1 réf., `FUN_140e1a260` =
    −1 réf. ; `*poignée = objet`), liste des expéditions de l'hôte
    (`FUN_1418a3470(hôte) + 0x60`, compte `+0x68`), puis événement de
    l'hôte (écouteurs `+0x2228`, compte `+0x2230`, compteur `+0x2240`).
    Rien de propre aux groupes.
  - **Entité** (`entity.cpp`, décompilés `FUN_1410ee9d0` = poser la
    transformation, `FUN_1410eceb0` = `SetParent`) : `+0x40` **parent**
    (le `SceneEntity` vu dans F9), `+0xD0` transformation **locale** 4×4
    (64 octets, position à `+0x100`), `+0x110` transformation **globale**,
    `+0x210` drapeau remis à 0 à chaque pose, `+0x248` octet de calque,
    `+0x24C` scène, `+0x278` (`[0x4f]`) gestionnaire.
    `FUN_141017a40(gestionnaire, scène)` = racine de la scène.
  - **Test F3 (`groupes.cpp`, 2026-10-28 ; pas F10 = menu des options du jeu)** : copie du premier groupe
    existant — `Creer(gestionnaire, entité +0x28, entité +0x40,
    transformation décalée de +20 en x et z, 0)`, sur le **thread du jeu**
    (`GroupesPompe`, appelée par le hook du setter de ressources à côté de
    `PompeNotifications`), sous SEH. Log `[GRP]` : classes du template,
    du parent et du gestionnaire, position, nombre de groupes avant/après,
    état `+0x28` de chaque composant.
  - **VALIDÉ le 2026-10-28 (Refugees)** : F3 crée une **copie conforme**
    d'un groupe (emplacement, nom, contenu), 2 → 3 → 4 groupes dans le
    système, état 2 comme les originaux, aucun plantage. Gestionnaire =
    `GameEntityManager`. Coordonnées de la carte du monde de l'ordre du
    millier (groupe vers x = −1675, z = −546) : le décalage de +20 est
    invisible. Les groupes **marchent** (x −1675 → −1672 entre deux appuis).
  - Itération suivante de F3 : relève la chaîne de parents et la scène
    (`+0x24C`) du groupe, d'un **site** et de la **ville** (le parent du
    site est-il celui du groupe ?), et, dans un scénario **sans** groupe,
    pose le template mémorisé (`g_tplMemo`, vu plus tôt dans la session)
    sur le premier site, avec le parent du site. But : savoir si le
    template survit au changement de scénario et si la scène de la carte
    est commune.
  - **Décision de l'auteur (2026-10-28)** : peu importe que le template
    survive à un changement de scénario ; il faut **le trouver où qu'on
    soit**, y compris dans un scénario qui n'a jamais de groupe. Et pour
    tester l'arrivée, il faut pouvoir **régler le temps de trajet restant**
    (cible : 30 minutes de jeu).
    → `InstallCaptureTemplatesGroupes` (groupes.cpp, au démarrage) : vtable
    de `FrostpunkSurvivorGroupComponentTemplate` par RTTI, fonctions qui la
    chargent (`FonctionsChargeantVtable`, mod_common), hook de chacune
    (≤ 4), capture des objets de la bonne classe + pile des 3 premiers
    (`[CAPGRP]`, `[TRACE]`). Templates de composant **contigus au pas de
    0x140**.
    → **F9 = `DiagGroupes`** (l'ancien `DiagSystemesGroupes` n'est plus
    lié à une touche) : templates capturés (nom, chemin `@Generated/…`,
    vitesse `+0x40`, nombre de survivants `+0x30`, état de départ
    `+0x10C`), dump du `CompoundTemplate` du groupe (où sont rangés ses
    templates de composant ?), puis **instantanés** du premier groupe
    (composant, objets `+0x48` et `+0x80`, transformation) avec les dwords
    modifiés depuis l'appui précédent (`[DIFF]`).
  - **Résultats (2026-10-28)** :
    - Fonction capturée : `0x141A0EC10` (seule à charger la vtable du
      template). Autre scénario lancé d'abord : **0 template de groupe**.
      Au chargement de Refugees : 2 construits (vitesses 825 et 1015,
      1 entrée de survivants, état de départ 1). **Les templates de groupe
      ne sont chargés qu'avec leur scénario** → il faut provoquer leur
      chargement nous-mêmes.
    - Ils sont construits sur un **thread de chargement** (≠ thread du
      jeu), pile : `0x141A0F0BE` (FUN_141a0f080, fabrique) ←
      `0x140E36565` (FUN_140e36460) ← `0x141043D57` (FUN_141043c20) ←
      `0x141CA8B52` / `0x141CA86F6` (FUN_141ca8b32 / FUN_141ca7260,
      désérialisation récursive) ← `0x140E5392B` (FUN_140e53830) ←
      `0x140E1CB85` (FUN_140e1cb30) ← `0x140E252C2` (FUN_140e24fe0) ←
      `0x140E0AFB6` (FUN_140e0af40) ← … (boucle du thread de chargement).
    - `CompoundTemplate` : `+0x20` **chemin de ressource**
      `templates/GameEntities/WorldMap/SurvivorGroups/Refugees/Refugee…`,
      `+0x38` → `ResourceTemplatePool` (le gestionnaire de ces ressources).
    - Marche (2 F9 à 1–2 h de jeu d'écart) : position `x −366.8 → −291.9`,
      `z −3.75 → −3.38` (≈ 75 unités ; la ville semble à l'origine) ;
      composant `+0x38` (qword) `0x3_9DDE3BC0 → 0x6_0CCD5390` (horodatage ?) ;
      `comp+0x48` = tampon de texte sans rapport, `comp+0x80` inchangé.
    - Heures notées de mémoire par l'auteur : ~15h21 puis ~13h02 (le
      lendemain ?), soit ~21,7 h pour 75 unités : **non fiable**.
  - **Décompilés de la pile du chargeur : faux appelants.** `FUN_140e53830`
    = décompression zlib (`decompressioncontext.cpp`), `FUN_141ca7260` =
    `inflate`, `FUN_140e24fe0` = lecture de flux de fichier
    (`filesysteminputstream.cpp`), `FUN_140e1cb30` = copie de chaîne. Le
    balayage brut de pile ramasse des **adresses de retour périmées**
    (appels terminés plus profonds). Seuls les 3 premiers cadres étaient
    sûrs : `FUN_141a0f080` ← `FUN_140e36460` ← `FUN_141043c20`.
    → `TracerPile` déroule maintenant la pile par `.pdata`
    (`RtlCaptureStackBackTrace`, lignes `cadre N`) ; balayage brut en
    repli seulement.
  - **F8 = rapprocher le premier groupe** (`RapprocherPremierGroupe`,
    thread du jeu) : replace le groupe à 10 % de sa distance à la ville
    (position de l'entité de `FrostpunkCityComponent`) par le poseur de
    transformation du moteur `FUN_1410ee9d0` (`+0x10EE9D0`). Si le groupe
    revient en arrière, sa progression est stockée ailleurs. L'ancien F8
    (bilan des captures, validé) est retiré.
  - **Résultat F8 / F9 (2026-10-28)** : la pose de position **ne tient
    pas** (106,4 visé, puis −346,9 → −308,2 au F9 suivant) : la position
    est **calculée** à chaque tick. Ville (entité de
    `FrostpunkCityComponent`) en `156,7 / 27,1 / −1,2` sur la carte.
    Relevé de l'auteur : 21h58 → 23h05 (67 min), restant affiché
    14h39 → 13h31. **Temps restant = distance à la ville / (vitesse du
    template / 24)** : 503,1 / (825 / 24) = 14,6 h. Vitesse = unités par
    jour de jeu. Seul champ variable du composant : **qword `+0x38`**
    (+6,80e9 pour 38,7 unités = 1,126 h → ~6,04e9 par heure de jeu),
    supposé être le temps de marche écoulé.
    → F8 v2 : avance `+0x38` de `(restant − 0,5 h) × 6,04e9` (log
    `[GRP] compteur +0x38`).
  - **Pile exacte du chargement des templates** (déroulage, thread de
    chargement) : `FUN_141a0f080` (fabrique) ← `FUN_140e36460` ←
    `FUN_141043c20` ← `FUN_140e3ec2b` ← `FUN_140e3eb40` ← `FUN_140e3e7c0`
    ← `FUN_140e38280` (méthode `[7]` commune de réflexion) ←
    `FUN_140e38170` ← **`FUN_14112e3c0`** ← **`FUN_1411d0940`** ←
    **`FUN_141037da0`** ← **`FUN_141038960`** ← `FUN_140e23e30` ←
    `FUN_140e23b00` (entrée du thread ?). Les 4 en gras sont les candidats
    « charger la ressource ».
  - **Décompilés du chargeur (2026-10-28)** :
    - `FUN_141038960(gestionnaire, p2)` = **traitement des requêtes de
      chargement** : deux files de requêtes (`+0x1F8`/compte `+0x200`,
      `+0x210`/compte `+0x218`), chaque requête : `+0x08` nom (chemin),
      `+0x14` état (2 = chargée), `vtable[0]` = type (index dans la table
      `PTR_s_Texture_141e366c0`), `vtable[+0x28]` = charger. Appelle
      `FUN_141037da0(gestionnaire, requête)` (« Loading {0} resource: {1} »,
      puis état 2). Verrou `gestionnaire +0x180`.
    - `FUN_14112e3c0(res)` = lecture binaire d'un template
      (`entitytemplateserialization.cpp`) : `res +0x08` = chemin (ou `#…`
      pour le lecteur optimisé `DAT_142b2e6c0`), fichier ouvert comme
      « BinaryTemplate ». L'objet est à `res − 0x18` : **la ressource est
      l'interface à `+0x18` du `CompoundTemplate`** (d'où le chemin à
      `+0x20`).
    - `FUN_1411d0940(res)` = **chargement d'un `CompoundTemplate`** :
      éléments à `+0x120` (compte `+0x128`, pas 0xA0) ; pour chacun,
      `élément[4]` = référence du template de composant, résolue par
      **`FUN_141035dd0(&DAT_142b2e710, élément[4], 1)`** (pool global de
      templates, protégé par la section critique `DAT_142b2e710`) →
      `élément[0x11]`, puis instance `FUN_14111d100(tpl, 0)` →
      `élément[0x12]`. Messages « Compound template element '{0}' … ».
    → `FUN_141035dd0` est le candidat « trouver / charger un template par
    référence ».
  - **Chaîne de chargement d'un template par chemin (Ghidra via MCP,
    2026-10-28)** :
    - `DAT_142b2e710` = **pool des templates** : commence par une section
      critique (`FUN_140e22d80(cs, 1)` = `EnterCriticalSection`) ; `+0x28` =
      `unique_ptr` vers la **racine d'un arbre de répertoires**.
    - `FUN_141035dd0(pool, chemin, p3)` = **recherche pure** (aucun
      chargement) : `FUN_14112f330(racine, chemin, p3)` découpe le chemin
      sur `/` ou `\` ; répertoire : sous-répertoires triés à `+0x10`
      (compte `+0x18`, `sousRep[0]` = nom, recherche `FUN_14112eff0`),
      feuilles triées à `+0x28` (compte `+0x30`, `feuille +0x08` = nom),
      comparaison `_stricmp`. `p3` n'est pas utilisé. Renvoie la feuille ou 0.
    - Feuille = **`EntityTemplateStub`** (`entitytemplatestub.cpp`) :
      `[0]` répertoire, `[1]` nom, `[4]` dword, `[5]` chemin complet ou 0,
      octet `+0x33`, **`+0x38` = template chargé** (0 sinon).
    - `FUN_14111d100(stub, p2)` = **obtenir le template, en le chargeant au
      besoin** : déjà chargé → +1 réf. (`FUN_1410042c0`, compteur `res+0x18`
      sous la section critique `DAT_142a2d650`) et renvoie `stub +0x38` ;
      sinon `FUN_14111ce10(stub, !p2)` (`SetupTemplateResource` : chemin =
      répertoire + nom, `FUN_1410056f0(&DAT_142a2d4d0, 5, chemin, …)` crée la
      ressource de type 5, `stub +0x38 = ressource − 0x18`), puis
      `vtable[+0x10]`, +1 réf., et `FUN_141003160(res+0x18, p2)`.
    - `FUN_141003160(res, p2)` : `p2 == 0` → état `+0x14` passe à 1 puis
      **`FUN_141037da0(&DAT_142a2d4d0, res)` charge sur le thread
      appelant** (c'est le chemin pris par `FUN_1411d0940`) ; `p2 != 0` →
      passage par `FUN_1410bf7a0` / `FUN_141004480` (file ou attente, non
      décompilé). `DAT_142a2d4d0` = gestionnaire de ressources.
    - Le template obtenu est l'objet complet (ressource − 0x18), donc pour
      un `CompoundTemplate` c'est ce que `Creer` attend (= `entité +0x28`).
      Son chargement (`FUN_1411d0940`) résout lui-même ses éléments.
    - `FUN_141af9770` montre l'usage inverse : stub → chemin texte par
      `FUN_14111c4f0(stub, tampon, 0x104, 1)`.
    → Recette visée, sur le thread du jeu : `Enter(pool)` ;
    `stub = FUN_141035dd0(pool, chemin, 1)` ; `tpl = FUN_14111d100(stub, 0)` ;
    `Leave(pool)` ; `Creer(gestionnaire, tpl, parent, &transform, 0)`.
    **Inconnues** : forme exacte du chemin (préfixe `templates/` ?
    extension ?) et présence des stubs d'un autre scénario hors de ce
    scénario → à lever en parcourant l'arbre depuis `pool +0x28`.
  - **F9 (parcours de l'arbre, 2026-10-28)** : 628 répertoires, 6292 stubs.
    Chemin **relatif à la racine, sans préfixe ni extension** :
    `GameEntities/WorldMap/SurvivorGroups/Refugees/RefugeeScenarioSurvivors01`.
    **Tous** les groupes de tous les scénarios ont leur stub, chargés ou
    non (32 sous `SurvivorGroups` : Builders, Refugees, TechHaven,
    Winterhome, StoryModeRefugees1..3, `SurvivorsFromExpeditionSite`,
    `SurvivorsFromLostExpedition`, `AutomataFromExpeditionSite`). Dans
    Refugees, `stub +0x38` = `entité +0x28` du groupe (vérifié) ; seuls
    Survivors01 et 02 y sont chargés. Le stub est aussi à
    `CompoundTemplate +0x68`.
  - Premier essai de F3 : rien. La pompe (`GroupesPompe`, dans le hook du
    setter de ressources) **ne tourne pas en pause**, et une demande arrivée
    pendant un chargement était jetée. Corrigé : la demande attend que la
    partie soit prête ; il faut **reprendre le jeu** pour qu'elle s'exécute.
  - **F3 dans TechHaven (2026-10-28) : création validée sans groupe
    préexistant.** `AutomataFromExpeditionSite` (non chargé) chargé à la
    demande, `CompoundTemplate`, entité `AutomataFromExpeditionSite*5552`
    posée sur un site, parent `WorldMaps/WorldMapTechHaven` (le même pour
    les sites et la ville : c'est la scène de la carte), système 0 → 1
    groupe, état 2. Aucun plantage. Pas vu à l'écran : carte inaccessible
    sans balise.
  - **Accès à la carte du monde** (`FrostpunkWorldMapSystem`, id `0x60`,
    décompilés du 2026-10-28) : `+0x2280` Initialized, `+0x2281`
    **WorldMapUnlocked**, `+0x2284` **WorldMapAvailabilityBuildingsCount**
    (bâtiments d'accès : la balise), `+0x2288` état de transition,
    `+0x21B8`/`+0x21C0`/`+0x21D0` écouteurs / nombre / compteur de
    l'événement LockedUnlocked, `+0x2248` événement AvailabilityChanged.
    Bouton actif ssi `Unlocked && compte >= 1 && *(DAT_14408dd50 + 0xA8) != 2`
    (`FUN_1413b8e10(DAT_144065ae0, dispo)` pose `+0x21C` de l'interface).
    `FUN_141332cd0(sys)` = **déverrouiller** ; `FUN_141331940(sys, 2)` =
    recalculer bouton + événement ; `FUN_1413319f0` = balise posée/détruite
    (±1, ferme la carte si le compte tombe à 0) ; `FUN_141331600` = init
    (déverrouille si `DAT_142c343c5`, drapeau de debug jamais écrit) ;
    `FUN_1413302d0` = recomptage des balises au chargement d'anciennes
    sauvegardes.
    → `CarteAssurerAcces` (groupes.cpp, appelée par le hook du setter à
    côté de `GroupesPompe`) : déverrouille si besoin, relève le compte à 1
    s'il est nul, puis rafraîchit. Log `[CARTE] acces force …`.
    **Validé le 2026-10-28** : carte ouverte sans balise, groupe créé par F3
    visible.
  - **Personnalisation d'un groupe : décompilés du 2026-10-28.**
    - Template (`FUN_141a10070`, réflexion) : `+0x28` Survivors
      (`Dynarray<FrostpunkSurvivorsEntry>`), `+0x40` Base movement speed
      (`ChangeRate` : float `+0x40`, unité `+0x44`), `+0x48` Initialization
      on world map unlock (octet), `+0x50` Display name (**2 variantes**
      `+0x50`/`+0x58`, clés 8 bits), `+0x60` Description (2 variantes),
      `+0x70` Icon (**2 variantes de 0x18** : `+0x70`/`+0x88`), `+0xA0` Image,
      `+0xD0` GUID, `+0xE0`/`+0xE8`/`+0xF0` textes, `+0xF8` City reached
      dilemma, `+0x108` Can be escorted, `+0x10C` Starting state, `+0x110`
      Death rate (`ChangeRate`).
    - `FrostpunkSurvivorsEntry` (`FUN_141a18ac0`), **0x24 octets** : `+0x00`
      Count (int), `+0x04` Social class (`EntryLink<FrostpunkSocialClassEntry>`,
      16 octets), `+0x14` Initial statistics (`EntryLink<…InitialStatistics…>`).
    - Composant de groupe : `+0x20` template, `+0x30` index de variante
      (nom/icône), `+0x34` Initialized, `+0x35` ReachedCity, `+0x36` Escorted,
      **`+0x38` = minuteur des morts** (pas le temps de marche ! cf.
      `FUN_1418b2f70`, tick de mortalité : tue un survivant par
      `FUN_1418b18f0` au bout d'un délai aléatoire), **`+0x48` = contenu
      propre au groupe** (copie des Survivors, pas 0x24, compte `+0x50`),
      `+0x78` copie du Death rate.
    - Système de groupes (vtable `0x1420C2E68`) : **`FUN_1418b1cc0` =
      composant ajouté** : copie les survivants du template
      (`FUN_1418afb80(comp+0x48, tpl+0x28)`), copie le nom
      (`FUN_141a0f1c0` = `tpl + 0x50 + variante × 8`) et l'icône
      (`FUN_141a0f2a0` = `tpl + 0x70 + variante × 0x18`) dans un autre
      composant de l'entité (`+0x30` nom, `+0x40..+0x58` icône), puis, si le
      système est initialisé et (`!tpl+0x48` ou carte déverrouillée),
      **`FUN_1418b1620` = départ** : vitesse `FUN_1418b0d40` = (`DAT_142c343c8`
      × conversion d'unité × `tpl+0x40` + `sys+0x21D8`) × `sys+0x21F8`,
      destination `FUN_141884720`, puis `FUN_1418853b0` (durée = distance /
      vitesse) → `FUN_141885250`.
    - **`LinearMovementComponent`** (`frostpunklinearmovementsystem.cpp`,
      système `DAT_14409ac18`, composant par `FUN_141884960(sys, entité)`) :
      `+0x30` départ (vec4), `+0x40` destination, `+0x50` date de départ,
      **`+0x58` date d'arrivée**, `+0x60` mode. Date courante :
      `*(DAT_142c32a40 + 0xB0)`. Position de l'entité utilisée : `+0x140`.
    - **Temps** (`frostpunktime.cpp`) : date interne = secondes × 2³¹
      (`DAT_141eb5808` = 2³¹) ; une heure de jeu = `*(int*)DAT_142c38ff0`
      secondes (global d'exécution) ; unités 0 = secondes, 1 = jours, 2 = heures.
  - **Groupes Archipelago (groupes.cpp, 2026-10-28, non testé)** :
    `GroupeCreer(GroupeAP)` (thread du jeu) — template par chemin, posé sur
    le premier site ; hook de `FUN_1418b1cc0` qui remplace `comp +0x20` par
    une **copie du template** (`MoteurAlloc(0x140)`) dont le nom et la
    description (`+0x50/+0x58`, `+0x60/+0x68`) sont nos clés
    `@AP/Groupe/<slot>/N|D` (`CleMoteur`, traduites par `hkTranslate` →
    `GroupeTexte` → `ApLocGroupe`) ; contenu : `Count` de la 1re entrée de
    `comp +0x48`, les autres à 0 ; `sansMorts` : `comp +0x78` = 0 ; durée :
    `ReglerDuree` réécrit départ / date de départ / date d'arrivée du
    `LinearMovementComponent` et met la vitesse de la copie à l'échelle.
    16 slots, oubliés au changement de partie. F3 = groupe de test (5
    survivants, 1 h, `SurvivorsFromExpeditionSite`), F8 = 30 min restantes
    (l'ancien F8, qui avançait le minuteur des morts `+0x38`, est remplacé).
    F9 dumpe les icônes (`tpl +0x70..+0xB8`) et le contenu du premier groupe.
    `CarteAssurerAcces` passe désormais **avant** `GroupesPompe`.
  - **Test F3 du 2026-10-28 (TechHaven)** : groupe créé, hook du composant
    vu, **durée OK (1 h), F8 OK (30 min)**. Mais : nom resté « Survivors »
    (`tpl +0x50` et `+0x60` sont **nuls** : les clés sont en `+0x58` nom et
    `+0x68` description ; le modèle d'en-tête lu en `+0x50` était nul →
    aucune clé créée), et **0 survivant** : `SurvivorsFromExpeditionSite` n'a
    **aucune** `SurvivorsEntry` (en vanilla, le site remplit le groupe) →
    `comp +0x48` vide, rien à l'arrivée. Icône (`tpl +0x70`) : chemin
    `UI\ExpeditionUI\Icons_Scouts_Activity.dds` à `+0x18` de la structure.
  - **`FUN_1418afb80`** = copie des survivants du template vers le composant
    (memcpy des entrées de 0x24, `tpl +0x28` → `comp +0x48`), appelée par
    `FUN_1418b1cc0`. **`FUN_1418b2870(système, entité)` = arrivée en ville** :
    crée les citoyens du contenu (`FUN_1418b7c00`) au drapeau d'arrivée,
    `+0x35` ReachedCity = 1, secourus en `comp +0x88` (compte `+0x90`),
    contenu vidé, événement `système +0x2128`. Contenu vide → rien.
  - **Corrections (non testées)** : modèle de clé = première clé valide parmi
    `+0x50, +0x58, +0x60, +0x68, +0xE0, +0xE8` ; écriture seulement dans les
    champs qui portaient une clé valide (`+0xE0/+0xE8` = noms d'agents, peut-
    être la source de « Survivors »). Survivants : `PreparerSurvivants` charge
    un **template donneur** (`GroupeAP::cheminSurvivants`, sinon liste :
    BuildersScenarioWorkers, RefugeeScenarioSurvivors01,
    TechHavenManchesterSurvivor, WinterhomeEngineersSurvivors), trouve son
    template de composant dans les éléments du `CompoundTemplate`
    (`TemplateGroupeDe`), copie sa 1re entrée (classe sociale + stats) avec
    notre `Count`, et le hook pose ce tableau dans la copie **avant**
    l'original. **Hook de l'arrivée** (`hkGroupeArrive`) : log, puis
    `GroupeAP::surArrivee(slot)` ; le test F3 livre 50 charbon + 1 cœur de
    vapeur par `ReceiveResource`. Slot réutilisable après l'arrivée.
  - **Test du 2026-10-28 (suite)** : **nom OK** (« Livraison Archipelago » ;
    champs remplacés : `+0x58`, `+0x68`, `+0xE0` `@Expeditions/
    SurvivorGroupName`, `+0xE8` `@UI/WorldMapSelectionPanel/Expedition/
    CarriedSurvivors`), **hook d'arrivée OK, ressources livrées**. Mais
    0 citoyen : les 4 donneurs se chargent, pourtant `template 0x0`, car les
    éléments du `CompoundTemplate` ne sont pas en `+0x120`
    (`FUN_1411d0940` reçoit la ressource = compound `+0x18` → essai en
    `+0x138`), avec un repli sur les templates capturés dont la clé `+0x58`
    contient `/<chemin du donneur>/`.
  - Test suivant : éléments bien en **`+0x138`** (2 par `CompoundTemplate`,
    `+0x120` nul), mais **aucun pointeur direct** vers le template de
    groupe ; et la clé `+0x58` n'est pas un chemin (repli inopérant). En
    revanche **chaque chargement de donneur construit un template**
    (captures 2 → 5). → Le donneur est désormais **la capture apparue
    pendant `ObtenirTemplateParChemin`** (différence avant / après) ; repli
    sur les éléments, avec dump des 2 premiers (classe RTTI ou texte de
    chaque qword) pour comprendre leur structure.
  - **VALIDÉ (2026-10-28)** : donneur trouvé par capture, **5 citoyens
    arrivent en ville**, ressources livrées. Au 2e F3, le donneur déjà
    chargé n'est plus reconstruit → mauvais donneur (Refugees) : cache
    chemin → template ajouté (non testé). À l'arrivée, un **dilemme
    générique** se déclenche.
  - **Dilemme d'arrivée** (`FUN_141a0f440`, init du composant depuis son
    template) : `comp +0x80` = dilemme résolu depuis le lien
    `tpl +0xF8` (City reached dilemma, `FUN_140e1a930` = lien valide ?,
    puis `FUN_1413e4240()->vtable[0x88]`) ; log « Overriding city reached
    dilemma » si `+0x80` était déjà posé. Même fonction : `comp +0x78` =
    `tpl +0x110`, `comp +0x30` = `tpl +0x10C`. Le lancement du dilemme
    depuis `comp +0x80` n'est pas encore localisé.
    `FUN_140e1a930` = comparaison de GUID de 16 octets (avec le GUID nul
    `DAT_142a12558`). **Décision de l'auteur : supprimer le dilemme** des
    groupes AP → le hook efface `clone +0xF8` (16 octets) et `comp +0x80` ;
    log `dilemme d'arrivee apres Creer` pour voir si le jeu le repose.
    **Résultat** : `comp +0x80` valait **déjà 0** (template sans dilemme) ;
    la bulle reste. Cache du donneur **validé** (`template deja connu`).
    La bulle (cercle en ville qui ouvre un dilemme à l'expiration) est un
    **événement du jeu** branché sur l'arrivée.
  - **Système d'événements** (`FrostpunkGameEventSystem`, réflexion
    `FUN_1417c3400`, global `DAT_1440935c8`) : `+0x2128` répertoires,
    `+0x2140` événements enregistrés, `+0x2170` état d'exécution,
    `+0x21A0` événement courant, `+0x21A8` AllowEventInvokes, et une liste de
    déclencheurs par type (`+0x2D08` Escorted / **`+0x2D20` Unescorted
    SurvivorGroupReachedCity**, `+0x2918` ExpeditionSent…).
    `FUN_1417c0fd0` = enregistrer un événement, `FUN_1417c1220(sys, évt,
    déclencheur)` = enregistrer un déclencheur.
  - **Commande console `InvokeEvent`** (`FUN_1413a3ec0(nom, forcer)`) :
    répertoires d'événements `DAT_14406dda8` (compte `DAT_14406ddb0`),
    chacun `+0x18` entrées / `+0x20` compte ; `FrostpunkGameEventEntry` :
    `+0x00` nom, `+0x20` Enabled, `+0x28` déclencheurs (compte `+0x30`),
    **`+0x58` effets**. Contexte : `{poignée d'entité, position vec4,
    vec4, 0}` (entité sélectionnée ou défaut `DAT_1440b1b30`).
  - **`FUN_1417b9b70(sys, entrée, contexte, forcer)` = invoquer un
    événement** : `forcer` → conditions (`FUN_1417aebc0`) ; effets exécutés
    par `FUN_14169ae90(DAT_14408e4b8, entrée + 0x58, contexte, nom)` si
    AllowEventInvokes ; puis compteurs et écouteurs (`sys +0x2108`).
    → Voie visée pour **nos propres dilemmes** (demande de l'auteur, 2026-10-28) :
    cloner un `FrostpunkGameEventEntry` (et son dilemme) avec nos clés de
    texte, puis l'invoquer avec le groupe pour contexte.
  - **Test du 2026-10-28** : à l'arrivée d'un groupe AP, **6 événements**
    ont le groupe pour contexte (tous ignorés à ce test) : `[NM]
    ManchesterSurvivor` (SelectEffect), `[NM] SpawnQuest` et `Some Question
    Helping NM - CTA` (TimedEffect), `S: Automaton from Frostland`
    (**`FrostpunkCreateSurvivoursDilemmaEffect`**), **`S: Survivors -
    universal`** (SelectEffect, la bulle générique présumée), `S: Heal
    Incomers (if Cold Resistance)`. Les invocations ont `forcer = 1`
    (conditions vérifiées). Les autres classes d'effets vues :
    `TimedEffect`, `SwitchEffect`, `SelectEffect`, `RandomizedEffect`,
    `FinishQuestEffect`, `ModifyCitizenNeedEffect`,
    `ModifySocialAttitudesEffect`, `ProfileProgressEffect`.
  - **Décision de l'auteur (2026-10-28)** : **garder la bulle**, puis la
    **remplacer par la nôtre** (nos propres dilemmes : prochaine tâche).
    La suppression est retirée ; `EvenementsSuivreEntite` remplace
    `EvenementsIgnorerEntite` et fait un relevé détaillé (`[EVT] GROUPE AP`).
  - **`FrostpunkCreateSurvivoursDilemmaEffect`** (0x28, `FUN_141937a70`) :
    `+0x18` lien (GUID) vers un `FrostpunkDilemmaEntry`. Résolution d'un
    lien de dilemme : `(*DAT_144069970)[0x88](DAT_144069970, &guid)`
    (disassemblé dans `FUN_141a0f440`).
  - **`FrostpunkDilemmaEntry`** (0x250, réflexion `FUN_1419dbde0`) : `+0x18`
    Variables, `+0x30` Text parameters, `+0x48` Initial effects, `+0x60`
    Ignored effects, **`+0x78` Choices** (pointeurs de
    `FrostpunkDilemmaChoiceEntry`), `+0x90` Sequence flags, **`+0xD8`
    Displayed name, `+0xE0` Arc subtitle, `+0xE8` Description, `+0xF0` Type
    text**, `+0xF8` Chat settlement greeting, `+0x108` Main icon, `+0x120`
    Settlement icon, `+0x150` Template (lien `FrostpunkDilemmaTemplateEntry`),
    `+0x188` Subject type, `+0x1B0` Modal, `+0x1B4` Ignorable, `+0x1B8`
    Dilemma type, `+0x1CC` Max occurences, `+0x1D4` Countdown, `+0x1E0` Time
    to countdown, `+0x1F0`/`+0x210`/`+0x230` images.
  - Relevé détaillé (validé) : effets récursifs (textes, objets RTTI,
    tableaux), et pour un `CreateSurvivoursDilemmaEffect` le dilemme résolu :
    textes, choix (qwords texte / RTTI des 0x100 premiers octets).
  - **La bulle = dilemme `[Beacon]S: Survivors group`** (GUID `FF DC 2F 34
    B3 3E 65 9F 85 4F 08 02 53 01 0C 5A`), via `S: Survivors - universal` →
    `FrostpunkSelectEffect` (`+0x18` `FrostpunkRescuedSurvivorsSelector`,
    `+0x50` effets) → `CreateSurvivoursDilemmaEffect`. Titre
    `@Events/Survivors/Days10-16/Title_universal` (« New Arrivals »),
    description « Survivors reached the city », 1 choix « I see » (clé en
    `choix +0xA8`). **Pas de RTTI** sur `FrostpunkDilemmaEntry` ni sur les
    choix ; nom de l'entrée en `+0x00`. Autres dilemmes vus :
    `[Beacon]S: Automata group`, `FrostpunkCreateDilemmaEffect` (TechHaven).
  - **`FrostpunkDilemmaChoiceEntry`** (0x150, `FUN_1419daf40`) : `+0x18`
    Choice type, `+0x28` Default choice, `+0x48` Show conditions, `+0x60`
    Conditions, `+0x78` Effects, `+0x90` Outcomes, **`+0xA8` texte**,
    `+0xB0` Description, `+0xB8` Effect text, `+0xC0` Disabled text,
    `+0xC8` Arc subtitle, `+0x110`/`+0x118` texte / titre du résultat.
  - **Exécution de `CreateSurvivoursDilemmaEffect`** (`FUN_141697870`,
    désassemblé vers `0x1416984dd`) : composant de groupe du contexte ; **si
    `comp +0x80` != 0, c'est ce dilemme qui est créé**, sinon celui du lien
    `+0x18` (résolu par `DAT_144069970`). Création :
    **`FUN_1417cde10(DAT_144095a98, dilemme, contexte, p4)`** (`p4` =
    survivants `FUN_1418b0e10(DAT_14409b698, entité)` dans le 1er cas, 0
    sinon) — fonction à retenir pour lancer nos dilemmes ailleurs.
  - **Remplacement de la bulle (non testé)** : `DilemmeCloner`
    (evenements.cpp) copie l'entrée (0x250) et ses choix (0x150), titre
    `+0xD8`, description `+0xE8`, texte de chaque choix `+0xA8` remplacés
    par nos clés ; `hkGroupeArrive` pose la copie de « Survivors group » en
    `comp +0x80` **avant** l'original. Clés `@AP/Groupe/<slot>/T|X|C`
    (`GroupeTexte(slot, champ)` : N, D, T, X, C ; 5 objets de traduction
    par slot dans `ApLocGroupe`, 80 au total). `GroupeAP::dilemmeTitre /
    dilemmeTexte / dilemmeChoix` (défaut : nom, description, « Accueillir »).
    Copie superficielle : effets / conditions / résultats des choix
    partagés avec l'original. Inconnu : sérialisation d'un dilemme en
    attente (EntryPtr de notre copie) lors d'une sauvegarde.
  - F8 ne vise plus un groupe déjà arrivé (durée absurde de 171798662 h).
  - **Test** : copie créée et posée en `comp +0x80`, événements exécutés
    **pendant** l'arrivée (synchrones), mais **bulle d'origine**. Cause
    présumée : le `SelectEffect` à `RescuedSurvivorsSelector` exécute
    l'effet interne avec un **survivant secouru** pour contexte, pas le
    groupe → pas de composant de groupe → branche du lien `+0x18`.
  - **`FUN_1417cde10(sys, dilemme, contexte, p4)` décompilé** = créer un
    dilemme : max d'occurrences (état par entrée, `sys + 0x437*8`),
    `FUN_1419dad00(dilemme)` = son template, conditions `FUN_1417ceae0`,
    variables (`+0x18`), log « [Dilemma][{0}] Dilemma created », effets
    initiaux (`FUN_14169aee0(…, dilemme + 0x48, …)`) ; renvoie l'id ou -1.
    → **Hook** `hkCreerDilemme` (evenements.cpp) : pendant un événement dont
    le contexte **extérieur** est un groupe suivi (`g_suivieEnCours`, posé
    autour de l'original dans `hkInvoquerEvenement`), le dilemme
    `orig` est remplacé par sa copie (`EvenementsRemplacerDilemme(entité,
    orig, copie)`, appelé par `hkGroupeArrive`). Log `[DIL] creation du
    dilemme …` pour **toute** création. **VALIDÉ le 2026-10-28** : bulle et
    dilemme d'arrivée avec nos textes.
  - **Dilemmes pièges** (demande de l'auteur, 2026-10-28 : les pièges
    Archipelago doivent être visibles et inévitables, via des dilemmes à
    plusieurs choix). `FUN_1417d40c0(sys, id, choix, drapeaux)` = **résoudre
    un dilemme** (dilemmes actifs `sys +0x21A0`, enregistrements 0x90,
    `+0x10` = entrée ; choix −1 = aucun ; `drapeaux & 1` → effets ignorés
    `+0x60`, sinon effets du choix `+0x78` puis du résultat). Création sans
    entité de contexte : marqueur posé sur la 1re entité de
    `*(DAT_14408dc20 + 0x2208)` ; contexte par défaut `base + 0x40B1B30`
    (celui de la commande `InvokeEvent`). Système des dilemmes
    `*(base + 0x4095A98)`.
    → `DilemmePiegeCreer(titre, texte, choix[], n, choixSiIgnore)` (thread
    du jeu) : copie de « Survivors group » avec n choix (copies du 1er choix
    d'origine), **tous les effets du jeu vidés** (initiaux `+0x48`, ignorés
    `+0x60`, effets `+0x78` et résultats `+0x90` des choix), clés
    `@AP/Dil/<slot>/T|X|C<i>` (`DilemmePiegeTexte`, `ApLocDilemme` : 8 slots
    × 6), création par l'original de `FUN_1417cde10`. `hkResoudreDilemme` :
    pour un de nos ids, applique la perte du choix (`Res_Add(res, −q)`,
    popup « Piege Archipelago ») ; choix hors bornes (ignoré) →
    `choixSiIgnore`. **F6** = piège de test (perdre 50 charbon / 50 acier).
    Inconnus : position de la bulle sans entité, et un dilemme en attente
    lors d'une sauvegarde.
  - **1er test F6** : `FUN_1417cde10` renvoie **−1**. Cause :
    `FUN_1417ceae0(dilemme, contexte, sortie)` vérifie le **type de sujet**
    (`dilemme +0x188`, −1 → template `+0x18`) : **0 global** (contexte par
    défaut `DAT_1440b1b30`), **1 entité** (exige une entité vivante dans le
    contexte), **2 lieu** (exige un point). « Survivors group » = entité.
    → La copie du piège est posée en sujet **global** (0). Non testé.
    L'auteur : le générateur comme sujet convient (c'est a priori l'entité
    par défaut du marqueur d'un dilemme global).
  - **Test F6 du 2026-10-28** : bulle **sur le générateur**, choix et pertes
    OK ; icône fausse. Le relevé a montré que la tuile **optionnelle** du
    marqueur a son **drapeau en tête** (`+0x230`) et la tuile en `+0x238`
    (texture `+0x238`, case `+0x240` = (14, 6) pour « Survivors group »,
    nombre de cases `+0x248`) ; l'hypothèse « drapeau après la tuile » avait
    écrasé le nombre de cases. `+0x170` du gabarit de bâtiment n'est pas
    non plus une tuile complète (nombre de cases illisible). Textures des
    tuiles : pas un `char*` lisible directement (NameString). → Correction :
    on garde texture et nombre de cases du marqueur d'origine et on ne
    change que la case → (15, 5) `icon_trap`. Non testé ; si ce n'est pas
    l'atlas #3037, il faudra identifier la texture du marqueur.
  - **2e test** : texture du marqueur = **`UI\Tiles\Icons.dds`** (16 × 16
    cases, lisible dans la NameString), case (14, 6) → pas un atlas que
    nous avons modifié (le #2680 est l'atlas des ressources, 512², cases de
    tailles variées ; vérifié en le convertissant). → La tuile du marqueur
    reprend celle d'une **techno** de l'arbre (`techno +0x78` texture,
    `+0x80` case, `+0x88` nombre de cases ; miniatures (0,2) exclues), case
    `icon_trap` de l'atlas #1179 (0, 4). Et « New Arrivals » = sous-titre
    d'arc `+0xE0` : clé `@AP/Dil/<slot>/S` (« Archipelago »), 7 objets de
    traduction par slot dans `ApLocDilemme`. Non testé.
  - **3e test** : sous-titre OK ; **aucune icône** avec l'atlas de l'arbre
    (la bulle ne sait sans doute afficher que son atlas). **Décision de
    l'auteur** : il ajoutera nos icônes dans l'atlas des bulles
    `UI\Tiles\Icons.dds` (16 × 16) ; il faut son **numéro d'entrée** dans
    `textures-s3`.
  - **Index des archives** (`textures-s3.idx`, décompilés du 2026-10-28) :
    en-tête 11 octets (`+0` version 0, `+1`, `+2`, `+3` nombre d'entrées,
    `+7` graine du hash = 0), enregistrements de **0x1D** triés par hash :
    `+0` hash (u32), `+4` taille compressée, `+0xC` taille, `+0x14`
    décalage dans le `.dat`, `+0x1C` drapeau. Pas de fichier `.str` (noms)
    en version publique. Recherche `FUN_140e51fa0(lecteur, sortie, nom)` :
    minuscules, **MurmurHash2** `FUN_140e53a40` (m = **0x5bd1e995**, passé
    au montage par `FUN_140ff3060`), dichotomie. Montage :
    `FUN_140e0e290` → `FUN_140e0a260` (`FileSystem::ContainerMountPoint`) →
    `FUN_140e0a600` (`.dat`, `.idx`, `.str`).
    Hors du jeu, le hash de `ui/tiles/icons.dds` (et variantes, et même
    `common/project.config` dans `common.idx`) ne retrouve **rien** : la
    forme exacte du nom hashé est inconnue. → Hook `InstallTraceArchives`
    (mod_common, au démarrage) : log `[ARCH]` des 20 premiers noms cherchés
    et de tous ceux qui contiennent « icons » ou « tiles », avec hash,
    taille et décalage. Script hors jeu :
    `scratchpad\hash_textures.ps1` (hash → numéro d'entrée).
  - **Résultat `[ARCH]` (2026-10-28)** : les noms cherchés sont de la forme
    **`/UI/Tiles/Icons.texture`** (barre initiale sautée, extension
    `.texture`, minuscules) → hash de `ui/tiles/icons.texture` = `0x8F231C35`
    = **entrée 2350** (DXT5 2048², 538 362 octets compressés). Vérifié :
    `tech_tree_atlas` = 1179, `buldings_dds` = 3037. Le script
    `hash_textures.ps1` retrouve désormais l'entrée de n'importe quel nom
    en `.texture`. Blob décompressé : en-tête propre au jeu (largeur,
    hauteur, `DXT5`, 12 mips…), **niveau 2048² en dernier** (4 Mo de fin).
    Le fichier `Nouveau dossier\02350_2048x2048.dds` de l'auteur est
    **tronqué** (5 570 544 octets). Atlas des bulles : 16 × 16 cases de
    128 px ; survivants = (14, 6) ; cases libres notamment ligne 1, col 7–12.
    → `SUBSTITUTIONS` : `{2350, "ap_icons_bulles.gz"}` ; `TechIcons` gagne
    `bulleCol/bulleRow` (vanilla (7,1), filler (8,1), important (9,1),
    progressive (10,1), trap (11,1), done (12,1)) ; la bulle du piège garde
    la texture d'origine et prend la case `icon_trap`. Reste : l'atlas
    modifié (fait par l'auteur).
  - **Format d'un blob de `textures-s3`** (comparaison de l'entrée 2680 et
    de `ap_icons_ressources.gz`) : gzip (7-Zip accepté, nom de fichier dans
    l'en-tête gzip) de : en-tête de 144 octets (largeur u32, hauteur u32,
    `DXT5`, nombre de mips u32, puis par mip `{l u16, h u16, taille u32}`
    du plus petit au plus grand, zéros), puis les mips du 1×1 au plus grand.
    → **`textures\bulles\construire_ap_icons_bulles.ps1`**, repris de la
    méthode de l'auteur (`Frost\Nouveau dossier\RemplacerBlocAtlas1179.txt`
    / `…3037.txt`, anciens chemins `D:\`) : blob d'origine décompressé, et
    **seuls les blocs DXT des cases modifiées** remplacés dans chaque mip
    par ceux de texconv (`BC3_UNORM`, 12 mips, `-dx9`) ; compression .NET
    ou 7-Zip (`-mx9 -mfb=258 -mpass=15`), le plus petit ; limite =
    min(place jusqu'au blob suivant, taille compressée de l'index) ;
    `-Deployer`. **Piège corrigé** : lire toute la « place » fait échouer le
    `GZipStream` .NET sur les octets suivants (exception avalée, fin du
    blob perdue : c'est pourquoi l'ancien `02350_2048x2048.dds` était
    tronqué) → on lit exactement la taille de l'index. Test sur l'atlas non
    modifié : 499 953 octets (7-Zip), marge 38 ko. Fichiers :
    `02350_origine.png` (à éditer → `02350_modifie.png`), `emplacements.png`.
  - **Atlas des bulles produit (2026-10-28)** : icônes de l'auteur
    (`textures\bulles\icones\<nom>.png`, 2034 × 2112, en couleur) placées
    par `composer_atlas.ps1` (recadrage sur l'alpha, 108 × 112 centré,
    marge 8 px, case vidée avant) → `02350_modifie.png`, `controle.png`,
    `planche.png` ; `.gz` de **520 685 octets** (marge 17 ko), **déployé**
    dans `mods\ap_icons_bulles.gz`. **VALIDÉ en jeu** : la bulle du piège
    affiche l'icône trap.
  - **Catalogue (2026-10-28, demande de l'auteur)** : `dilemmes_ap.h/.cpp`,
    séparé de la mécanique ; F6 = `DilemmeAPDemander("trap_coal_or_steel")`.
    Quantités désormais **signées** (−50 = perte). `DilemmeAPCreer` remplace
    `DilemmePiegeCreer` ; icône de la bulle choisie par son nom.
  - **Icône des groupes : VALIDÉE** (carte et liste à droite affichent la
    case de l'atlas des bulles ; aucune autre injection nécessaire). Trop
    grande → variante **`icon_vanilla_groupe`** (vanilla à 85 %) dans la
    case libre **(8, 4)** de l'atlas des bulles, défaut de
    `GroupeAP::icone`. `composer_atlas.ps1` prend désormais une liste de
    placements `{nom, fichier, col, lig, echelle}` ; le build compte 7 cases.
    `.gz` : 525 561 octets, **marge 12,8 ko** (attention aux prochains ajouts).
  - **Icône des groupes AP (1re version)** : `PoserIconeGroupe` (hook du
    composant ajouté, avant l'original) met dans les deux variantes d'icône
    de la copie du template (`+0x70`, `+0x88`) la tuile de l'atlas des
    bulles (`TuileAtlasBulles`), `GroupeAP::icone` (défaut `icon_vanilla`) ;
    tuiles d'origine (`+0x70`, `+0x88`, image `+0xA0`) journalisées. Cible :
    l'icône de la carte et celle de la liste à droite. Si la texture n'est
    pas affichée hors bulle, piste : injecter l'icône dans l'atlas du
    groupe (`UI\ExpeditionUI\Icons_Scouts_Activity`, entrée à trouver par
    `hash_textures.ps1` avec `.texture`).
  - **Icône du piège (1re version, fausse)** : `UITextureTile` (0x18,
    `FUN_141028100`) = `+0x00` texture, `+0x08` case (x, y), `+0x10`
    nombre de cases. Dilemme : `+0x108` image principale, `+0x120` icône de
    colonie, `+0x1F0`/`+0x210`/`+0x230` images optionnelles (tuile +
    drapeau à `+0x18`) avant-plan / fond / **marqueur**. `PoserIconePiege`
    prend la tuile d'un gabarit de bâtiment (`+0x170`, atlas des miniatures
    #3037) avec la case `icon_trap` (15, 5) et la pose en `+0x108` et
    `+0x230` (drapeau à 1) ; le journal décrit les images d'origine.
  - **Textes en jeu passés en anglais** (2026-10-28) : popups
    (`Technology received`, `Resource received`, `Check sent`,
    `Archipelago Trap`), groupe de test (`Archipelago Delivery`…), piège F6
    (`Lose 50 coal` / `Lose 50 steel`), `Archipelago Item`, textes de check
    (`[AP] Sending: …`, `(already sent)`).
  - **`evenements.cpp`** : hook de `FUN_1417b9b70`, journal
    `[EVT] invoque "<nom>" … contexte : <entité, classe, chemin>` + effets
    (`+0x58`, classes RTTI) pour les 300 premiers ; `hkGroupeArrive` appelle
    `EvenementsIgnorerEntite(groupe)` **avant** l'original → les événements
    dont le contexte est ce groupe sont ignorés (`[EVT] IGNORE`). Si l'appel
    est différé, ça marche quand même (liste persistante).
  - git : absent du PATH, mais disponible dans Visual Studio
    (`…\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team
    Explorer\Git\cmd\git.exe`).
- **Inventaire des groupes AP (2026-10-28, demande de l'auteur : « vrai
  inventaire », comme celui d'un scout)**. Décompilés : le panneau de
  sélection d'un groupe (`FrostpunkUISurvivorGroupSelection`,
  `FUN_141c01860`) n'affiche que la population. La ligne de la **liste de
  droite** est un `FrostpunkUIExpeditionControl`, le même pour un scout et
  un groupe (`+0x290` poignée de l'expédition, `+0x2A8` poignée du composant
  de groupe, `CARGO_LAYOUT` en `+0x310` / compte `+0x318`, liaison des
  enfants `FUN_141c2a490`). **`FUN_141c2b6e0(contrôle)` remplit
  l'inventaire** : scout → cargaison `+0x168/+0x180/+0x198`, groupe →
  survivants `comp +0x48`. Case : `FUN_1416b7350(*DAT_14408ec18,
  &DAT_1440af6f0, case à réutiliser|0, layout, &texte (FUN_141ce67f0 depuis
  définition +0x28), tuile (définition +0x98), {1, quantité}, -1,
  &DAT_142a22e20)` → case suivante ; `FUN_141a31660(suivante, &nom)` supprime
  les restes ; `FUN_141a316b0(layout)` finalise.
  → `GroupeAP::cargaison[]` (`{ressource, quantité}`) : affichée dans
  l'inventaire (hooks des 4 fonctions, cases ajoutées après les survivants
  avec la fonction des scouts) **et livrée à l'arrivée** (`ReceiveResource`,
  avant `surArrivee`). `Res_Definition(nom)` (resources). Test F3 : charbon 50
  + cœur de vapeur 1 dans la cargaison. Non testé.
  **1er test** : 2 cases en plus, mais aux valeurs par défaut du modèle
  (icône des rations, 100) : `EXCEPTION 0xC0000005` à **chaque**
  rafraîchissement, dans la case « coal » (la case est créée, puis faute avant
  l'icône et la quantité). Réflexion de `FrostpunkResourceEntry`
  (`FUN_1413ed200`, 0xD8) : `+0x08` GUID, `+0x18` Initial amount, `+0x28`
  **Display name**, `+0x30` Color, `+0x98` **icône** (`UITextureTile`),
  `+0xB0` Text icon name → offsets bons, définition = entrée (clé
  `EntryPtr` de la `Map` de l'économie). → relevé de l'adresse fautive et de
  l'étape (texte / ajout / libération), **une seule tentative par groupe**
  (`inventaireEchec`). En attente du journal.
  **Résultat** : faute à l'étape « texte du nom », `0x141CE5A51` =
  `FUN_141ce5a40` lit `*(def+0x28)` puis son 1er caractère (attend `@`) : le
  `+0x28` de l'objet du conteneur de l'économie n'est **pas** une clé (cet
  objet a le GUID en `+8`, mais n'est pas le `FrostpunkResourceEntry`).
  → `EntreeRessource(nom)` : GUID (définition de l'économie `+8`) → entrée
  par la **collection des ressources** `FUN_1413ebe70` = `&DAT_14406ad18`,
  méthode `+0x88(collection, &guid)` (comme le chemin « transport » de
  `FUN_141c2b6e0`), clé vérifiée (`@`). **Test : introuvable** (0 case).
  → recherche élargie (collection, objet de l'économie, pointeurs qu'il porte
  en `+0x00..+0x38` : même GUID et clé `@`) et relevé complet en cas
  d'échec (`[GRP] inventaire : "…" introuvable`, dump de l'objet).
  **Relevé** : l'entrée est à **`Res_Definition() + 8`** (= `resId`) :
  `+0x00` nom interne (« Coal »), `+0x08` GUID, `+0x18`/`+0x1C` quantité
  initiale / plafond (150 / 900), `+0x28` `@Resources/Names/Coal`. Le GUID
  était lu 8 octets trop tôt. `EntreeRessource` prend désormais `def + 8`
  (vérifié : GUID + clé `@`). Non testé.
- **Survol d'un scout** : identifier l'expédition sous le curseur.
- **Sites landmarks** : affichage du contenu.
- **Sauvegarde empoisonnée** : écrire un hook de sauvegarde qui vide les
  cargaisons avant sérialisation.
- **Lois.**
- **Réseau Archipelago** : à n'aborder que sur demande explicite. L'auteur
  prévoit « une classe spécifique pour toutes les interactions avec le serveur
  qui s'occupera de manager les notifications et leur véritable envoi ».

### Approche abandonnée

Ajouter une **12e entrée** au conteneur de ressources du jeu : crash systématique
au chargement (l'économie sérialisée contient 12 ressources pour 11 déclarées).
Ne pas y revenir.

---

## 10. Émission d'une notification (séquence exacte)

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

```cpp
typedef void (*CreerMsg_t)(uintptr_t, uintptr_t*, uintptr_t, uintptr_t);  // +0x167C580
typedef void (*PoseTexte_t)(uintptr_t, uintptr_t*);  // +0x1AA9D70 titre, +0x1AA9C00 texte
typedef void (*EmettreMsg_t)(uintptr_t, uintptr_t*); // +0x16896B0
```

1. `MessageSetReferences(systemes, nSys, entrees, nEnt)` : le producteur est
   `systemes[0]` ; l'entrée est celle nommée **`CitizensDied`** ; les paramètres
   sont à `base + 0x40B1B30`.
2. Créer le message.
3. Lire le **modèle de clé** dans l'entrée à `+0x50`.
4. Poser les clés `@AP/Notif/<id>/T` et `@AP/Notif/<id>/D` (clés 8 bits avec
   en-tête de comptage, via `ApCle`, `AP_CLES = 16`).
5. **Transférer la propriété** avant l'émission — sinon crash :

```cpp
uintptr_t transfert = message;
message = 0;
EmettreMessage(file, &transfert);
```

6. `hooks_loc.cpp` intercepte la branche `AP/Notif/<id>/T|D` et renvoie le texte
   via `ApLocNotif(slot, texte)` (16 objets/chaînes dédiés).
   `ApNotifTexte(id, titre)` est exportée par `MessageManager` pour ce besoin —
   penser à la déclarer, sous peine de `error C3861`.

---

## 11. Substitution de textures

Format propriétaire : archives `.dat` + index `.idx`, blobs **gzip**, textures
**DXT5** avec les **mips du plus petit au plus grand**.

- `OffsetDuBlob(index)` lit le `.idx` pour localiser un blob.
- Hook sur **`ntdll!NtReadFile`** pour substituer à la volée : réentrance par
  `t_dansHook`, position obtenue par `SetFilePointerEx(FILE_CURRENT)`,
  substitution, puis `EnregistrerLecture`. Chaque hook (`NtReadFile`,
  `NtWriteFile`) est installé **indépendamment**.
- Niveau de base d'une texture : `144 + (taille − w × h)`.
- Scripts PowerShell : extraction d'un blob (`$cible`), substitution
  multi-cases (`$cases`, `$nCols`, `$nRows`) via texconv + 7-Zip, et dichotomie
  par patch transparent pour localiser une case dans un atlas.

### Atlas d'icônes

```cpp
struct TechIcons { const char* nom; int32_t batCol, batRow; int32_t techCol, techRow; };
static const TechIcons ARCHIPELAGO_ICONS_TECH[] = {
    { "icon_vanilla",     15, 1,  11, 5 },
    { "icon_filler",      14, 6,  11, 2 },
    { "icon_important",   14, 5,  11, 1 },
    { "icon_progressive", 15, 0,  11, 4 },
    { "icon_trap",        14, 7,  11, 3 },
};
```

**Contrainte de taille** : le blob substitué doit tenir dans la place du blob
d'origine. `AppliquerIconeBatiment` doit se servir de `TechIcons` (remarque
explicite de l'auteur).

Résolution du nom de bâtiment depuis une clé (`NomBatimentDepuisCle`) : deux
formes à gérer, courte `@Buildings/<Nom>/…` et longue
`@Generated/.../<Nom>/Components/...`.

---

## 12. Rappel sur le ton

L'auteur connaît son code et son jeu. Il relève les incohérences
(« pourquoi tu ne te sers pas de `TechIcons` dans `AppliquerIconeBatiment` dont
on a parlé juste avant ? »). Il attend qu'on soit **précis** — « ça fait un
moment que je n'ai pas touché au projet » — et qu'on **ne propose pas de replis**
qui vident la fonctionnalité de son sens.
