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
| `version.cpp` | Point d'entrée : proxy des 17 exports de `version.dll`, `DllMain`, `PayloadThread` (console + journal `logs\console.log`, `MH_Initialize`, hooks des constructeurs de templates **en premier**, puis les autres hooks), boucle principale : touches de debug (F3–F9 ; F11 réservée aux tests demandés par l'auteur ; jamais F10 = menu des options ; F3 = création de groupe, F4 = relevé des lois, F5 = item de loi de test, F6 = dilemme piège de test (remplace l'ancienne fenêtre `OpenKeyLogWindow`), F7 = progressive de test, F8 = item « Temperature rise » de test (+1 niveau ; l'ancien rapprochement du premier groupe n'est plus lié), F9 = relevé des quêtes (`quetes.cpp` ; le relevé de température et l'ancien `DiagGroupes` ne sont plus liés), F11 = ressources de test) + tick d'une seconde (`EnsureTechSelf`, `EnsureResSelf`, **`PartieTick`**, `CaptureRunState`, `ScootingTick`, `SurveillerArbreTechno`) |
| `mod_common.h/.cpp` | Socle : `SafeRead`/`SafeWrite`, `GetClassNameFP` (RTTI), ancrages (`SystemeParId`, `ComposantsDuSysteme`, `IdSysteme::*`), captures (`ListeCaptures`), **cycle de vie de la partie** (`PartieTick`, `PartiePrete`, `PartieGeneration`, `PartieSelection`, `PartieRechargee`, garde-fou sur la liste des sites), **diagnostics d'ancrage** (`DiagAncrages`, `DiagGabaritsPrecoces`, `ChercherValeurDansTas`, `ChercherReferencesVtable`…), balayage mémoire historique (`CollectObjectsByClasses`), résolution des self par globaux, dumps (PropertyManager, vtables), hooks d'E/S (`NtReadFile`/`NtWriteFile`), allocateur du moteur, `ToucheAppuyee`, offsets (`GameOffsets`, `SetupOffsets`, `TECH_*`, `RES_*`), macro `CHRONO` |
| `hooks_loc.h/.cpp` | `hkTranslate` : cœur du détournement de **tous** les textes affichés |
| `hooks_research.h/.cpp` | Les 4 hooks de recherche : `CanResearch`, `ResearchStart`, `ResearchComplete` (location check), `CancelResearch` |
| `tech_state.h/.cpp` | Accès au tableau des technos du jeu (`g_selfTech + 0x2248`) : état par hash ou par pointeur, clés de localisation, parcours |
| `tech_brain.h/.cpp` | État runtime par techno `{researched, received}` ↔ état mémoire du jeu |
| `tech_runtime.h/.cpp` | `TechRuntime`, `RT_Get`, `RT_CanResearch` |
| `known_techs.h` | Table des technos (hash → nom) |
| `known_quests.h` | Table des quêtes par scénario (GUID → nom interne / affiché, catégorie) — **générée** par `outils\quetes_excel.py` depuis le relevé F9 |
| `known_sites.h` | Table des sites d'exploration par scénario (nom interne = clé de `SITE_PAYLOADS`, nom affiché, contenu) — **générée** par `outils\quetes_excel.py` depuis le relevé F9 |
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
| `lois.h/.cpp` | Lois du jeu (« sociotechs ») : `DiagLois` (F4), relevé des arbres, prérequis et réglages par scénario |
| `lois_ap.h/.cpp` | Lois Archipelago : signature = check(s) par rang dans l'arbre, effet retenu jusqu'à l'item progressif, sauvegarde locale `ap_lois_<GameId>.txt` ; F5 = item de test |
| `dilemmes_ap.h/.cpp` | **Catalogue des dilemmes Archipelago** (bulles en ville, pièges) : table `DILEMMES_AP` (id, titre, sous-titre, texte, icône, titre de popup, choix si ignoré, ≤ 4 choix `{texte, ressource, quantité signée, effetSpecial}`) ; `DilemmeAPDemander("id")` (tout thread, file) → `DilemmesPompe` (thread du jeu). **Pour ajouter un piège : une entrée dans la table** |
| `menu_ap.h/.cpp` | **Écran de connexion Archipelago du menu principal** : hook de l'init du panneau principal (`+0x1B28950`), clones d'`EXIT_BUTTON` (libellés Server / Slot name / Password, bouton Connect) et d'un `UITextInput` de panneau en jeu, masque du mot de passe, `mods\ap_connexion.txt` ; Connect → `RequestConnection` (archipelago) |
| `temperature.h/.cpp` | **Température / météo** : relevé F9 (`[TEMP]`, thread du jeu via `TemperaturePompe`) des niveaux, météos, chronologies météo, actes, modifications de difficulté et de la chronologie calculée de la partie ; **montées AP** : hook du recalcul `+0x1812C10`, montées retirées, `TemperatureRecevoirMontee` (← `ReceiveTemperatureRise`, archipelago), `mods\ap_temperature_<GameId>.txt` |
| `quetes.h/.cpp` | **Quêtes** : relevé F9 (`[QUETE]`, thread du jeu via `QuetesPompe`) des répertoires, quêtes, objectifs, effets, et des événements portant un effet de quête ou d'acte |

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
- **Sauvegarde des éléments AP (2026-10-29)** : au rechargement d'une
  sauvegarde, un groupe AP perd logo et contenu (la copie du template n'est
  pas sauvegardée : la sauvegarde référence le template d'origine).
  **Décision de l'auteur : notre propre sauvegarde persistante** (comme
  `mods\ap_pending_<GameId>.txt` des scouts), recollée au chargement — pas
  d'écriture dans la sauvegarde du jeu. Difficultés : clé stable d'un groupe
  après chargement (adresses changeantes), et chargement d'une sauvegarde
  **plus ancienne** (garder un enregistrement par groupe, ne recoller que les
  groupes présents). À recoller : copie du template (nom, description,
  icône), cargaison, durée, remplacement de la bulle d'arrivée ; plus tard :
  bulles de piège et messages en attente.
  Étape 1 : relevé `[SAV]` (`ReleverUnGroupe` / `ReleverGroupes`,
  groupes.cpp) à la création (« cree »), au 1er passage après un changement
  de partie (« apres chargement ») et sur F9 : template (notre copie ?),
  clé du nom, GUID `tpl +0xD0`, nom d'entité, qwords `entité +0x08..+0x40`,
  contenu, ReachedCity, dates du mouvement. Non testé.
  **Moment du recollage** (idée de l'auteur : agir pendant le chargement,
  avant le 1er tick). Décompilés du 2026-10-29 : `FUN_141265df0(jeu, nom)` =
  **`FrostpunkGame::LoadGame`** (`frostpunkgame.cpp`, drapeau `jeu +0x489`) :
  `FUN_141286840` (« Loading game save… » / « …successfully loaded. » ; lit
  le `.save` par `FUN_141286580` et le désérialise par
  `FUN_14101f110(gestionnaire d'entités, données, …)`), puis si succès
  `FUN_141264b10(jeu, 1, …)` (état `InFrostpunkGame`, écouteurs `jeu +0x358`
  / compte `+0x360`) et `FUN_141266390(jeu)`. `FUN_14126c9f0` = vérification
  « The game should not be paused after load » du système de temps (appelée
  depuis `0x141269500`, hors fonction connue). → **`InstallTraceChargement`**
  (mod_common) : `[CHARG]` à l'entrée / sortie de `LoadGame`, à la fin de la
  lecture, à la vérification après chargement (thread, nombre de groupes,
  temps). But : savoir si les groupes existent déjà à la sortie de
  `LoadGame` (chargement synchrone) et sur quel thread. Non testé.
  **Résultat (2026-10-29)** : tout le chargement est **synchrone sur le
  thread du jeu** (29924 = celui de `Translate` / `ModifyResource`) ; les
  composants de groupe sont reconstruits dans la lecture (pile `[TRACE]` :
  `FUN_14101f110` ← `FUN_141286580` ← `FUN_141286840` ← `LoadGame`),
  **0 groupe** inscrit à la fin de la lecture, **4/4** à la vérification
  après chargement et à la **sortie de `LoadGame`** → **point de
  recollage = sortie de `LoadGame`** (`GroupesApresChargement`, appelé par
  le hook). `LoadGame` est aussi appelé une 2e fois juste après, sans effet
  (entrée/sortie à t+0). La partie redevient « prête » (`PartiePrete`) bien
  plus tard : F9 juste après le chargement → `partie pas prete`. Relevé
  avant sauvegarde : notre groupe `SurvivorsFromExpeditionSite*6986`,
  `entité +0x18/+0x20` = 128 bits aléatoires (GUID d'entité ?), `+0x28` =
  `CompoundTemplate`, `+0x30` = `0x2000000005`. Relevé `[SAV]` désormais
  fait à la sortie de `LoadGame`. Non testé.
  **Relevé après chargement (2026-10-29)** : nom d'entité (`…*6986`),
  identifiant `entité +0x18/+0x20` (`9A92A21ED66D5C94 / 86EEAD4EAA9B43E4`),
  survivants (5) et **dates du trajet** identiques avant / après ; template
  = celui du jeu (nom, icône perdus). → **clé = identifiant 128 bits de
  l'entité** ; durée et survivants : natifs, rien à recoller.
  **Implémenté (non testé)** : fichier **`mods\ap_groupes.txt`** (un pour
  toutes les parties ; ligne `G <id 32 hex>\t<sansMorts>\t<icône>\t<nom>\t
  <description>\t<titre bulle>\t<texte>\t<choix>\t<res=q;res=q>`, ajoutée à
  la création par `SauverGroupe`, jamais effacée, la dernière ligne d'un id
  l'emporte). **Recollage dans le hook de `FUN_1418b1cc0`** (composant
  ajouté, appelé aussi pendant le chargement) : entre `GroupesAvantChargement`
  (entrée de `LoadGame`) et `GroupesApresChargement` (sortie), chaque groupe
  dont l'id est dans le fichier reçoit un slot, sa copie du template
  (`PoserCopieTemplate`, avant l'original : le jeu recopie lui-même nom et
  icône dans le composant de carte `entité +0x2A0 + type × 8` : `+0x30` nom,
  `+0x40..+0x50` icône), cargaison et textes de bulle ; slots remis à zéro au
  1er groupe du chargement ; taux de mort remis à 0 à la sortie si
  `sansMorts`. `SuivreGroupes` ne vide plus les slots au changement de
  génération : il garde ceux dont le composant, l'entité et l'id
  correspondent (adresses réutilisées d'un chargement à l'autre).
  Limite : `surArrivee` (pointeur de fonction) n'est pas sauvegardé.
  **Test** : groupe bien enregistré (`BFD163B16C8C39CF…`), mais **aucun
  appel du hook de `FUN_1418b1cc0` pendant `LoadGame`** : les groupes
  chargés ne passent pas par « composant ajouté », le composant de carte
  garde nom et icône désérialisés. → **recollage à la sortie de
  `LoadGame`** (seulement si la lecture a réussi, `GroupesLectureFaite`
  appelée par le hook de lecture) sur tous les groupes non arrivés :
  `RecollerGroupe` (slot + copie du template) puis **`MettreAJourCarte`** :
  composant de carte `*(entité + 0x2A0 + FUN_1412ec200() × 8)`, nom
  `*FUN_141a0f1c0(comp)` (+1 réf. `FUN_140e1ce20`, ancien relâché
  `FUN_140e1d180`) en `+0x30`, icône `FUN_141a0f2a0(comp)` copiée par
  `FUN_140e4f9a0` en `+0x40` + case / nombre de cases `+0x48 / +0x50`.
  (`FUN_141776ba0` = drapeau de sélection, sans rapport.) Non testé.
  **Test** : recollé, **liste de droite OK** (nom, logo), mais **icône de
  la carte vanilla** et **inventaire vidé** au rafraîchissement suivant.
  Causes : (1) le marqueur de carte (`FrostpunkWorldMapMarkerComponent`,
  0x38 octets, sans icône propre) lit l'icône à l'activation des groupes,
  **entre la fin de la lecture et la sortie de `LoadGame`** → trop tard ;
  (2) `Res_Definition` renvoie l'entrée − 8 tant que le contexte vient d'un
  appel du jeu, **l'entrée elle-même après `Res_InitDepuisEconomie`**
  (deux ancrages incohérents). → recollage **dès la fin de la lecture**
  (`GroupesLectureFaite`) sur les `SurvivorGroupComponent` **construits
  pendant `LoadGame`** (`GroupesComposantConstruit`, appelé par le hook du
  constructeur `+0x1A0EB70` de mod_common), si entité et template sont déjà
  là ; filet à la sortie de `LoadGame` (`RecollerSiNotre`, sans doublon) ;
  `EntreeRessource` : GUID de la table (`Res_Guid`), candidats `def`,
  `def + 8`, `def − 8`. **VALIDÉ le 2026-10-29** (carte, liste, inventaire
  après rechargement).
  **Messages AP (2026-10-29)** : après rechargement, seul le message
  « ouvrable » restait, sans contenu, icône et couleur du jeu. Causes : textes
  seulement en mémoire (8 places, id recyclé) ; entrée sauvegardée = notre
  copie relue comme `CitizensDied` vanilla (importante → orange, icône du jeu),
  or tout le code d'icône / couleur reconnaissait un message AP à notre copie.
  → **id unique croissant** (suite du plus grand id de
  **`mods\ap_messages.txt`**, ligne `M <id>\t<icône>\t<titre>\t<texte>`,
  `SauverMessage` à l'émission) ; `ApNotifTexte` passe par un cache de 64
  (`NotifParId`) qui relit le fichier pour un id inconnu (absence mémorisée) ;
  clés allouées par message (`CleMoteur`, plus de blocs statiques recyclés
  `ApCle`) ; dans `hkAjouterPopup`, un message dont la clé de titre (`msg
  +0x98`) est `@AP/Notif/<id>/…` et dont l'entrée n'est pas notre copie
  reçoit **avant l'ajout** `EntreeDeNotre(icône de l'id)` en `msg +0x80`
  (référence `CitizensDied` prise sur le message si `PartieTick` ne l'a pas
  encore posée). **Test : contenu et couleur OK, logo du jeu.** Les
  messages rechargés sont ajoutés **pendant `LoadGame`** ; l'icône posée sur
  `icon` y est perdue. → `ReposerIconesMessages` (contrôle + entrée gardés
  dans `ControleColore`) à la sortie de `LoadGame` (`MessagesApresChargement`)
  et au 1er passage de `PompeNotifications`. Non testé.
  **Bulles AP en attente (2026-10-29)** : sauvegardées par le jeu avec le
  GUID de l'original (« Survivors group ») → rechargées comme l'original
  (arrivée de groupe vanilla ; un piège perdrait sa perte). Décompilé de
  `FUN_1417cde10` : dilemmes actifs `système +0x21A0` (compte `+0x21A8`),
  enregistrements de 0x90 : `+0x00` id (compteur `système +0x2198`),
  `+0x10` entrée, `+0x20` contexte, `+0x78` **expiration**, `+0x80`
  marqueur. → **`mods\ap_dilemmes.txt`** : `P <id> <expiration>\t<id du
  catalogue>` (`DilemmeAPCreer`) et `G <id> <expiration>\t<titre>\t<texte>\t
  <choix>` (`hkCreerDilemme`, bulle d'arrivée d'un groupe AP, textes via
  `GroupeTexte`). **`RecollerBulles`** à la fin de la lecture puis à la
  sortie de `LoadGame` (`DilemmesAPChargement(0|1|2)`, appelée par les hooks
  de mod_common) : pour chaque enregistrement dont (id, expiration) est dans
  le fichier, slot de piège, copie (piège : `ConstruirePiege` depuis le
  catalogue ; groupe : `DilemmeCloner` avec clés `@AP/Dil/<slot>/T|X|C0`),
  posée en `enregistrement +0x10`. Bulle de groupe rechargée : `def` nul, pas
  d'effet AP à la résolution.
  **Test** : recollage seulement à la sortie de `LoadGame` (à la fin de la
  lecture, le système des dilemmes n'est pas lisible) ; **plantage au clic
  sur un choix du piège rechargé**. Cause (décompilé `FUN_1417d40c0`,
  résolution) : table des occurrences **`système +0x21B8`** (hachage indexé
  par l'adresse de l'entrée, éléments de 0x38 en `+0x21C0`) ; la résolution y
  cherche l'entrée et lit `*(+0x21C0) + 0x18 + index × 0x38` **sans vérifier
  l'index** ; la table du chargement porte l'entrée d'origine, pas notre
  copie → index −1 → lecture hors du tableau. → `InscrireOccurrence` :
  `FUN_1417cfc40(table, &entrée)` (chercher) puis `FUN_1417d16d0(table,
  &{entrée, DAT_142c2fca8, 0, 0, 0, 0})` (insérer, comme `FUN_1417cde10`),
  **avant** de poser la copie dans l'enregistrement. Autre structure indexée
  par l'entrée : `système +0x2218` (cherchée, ignorée si absente).
  **VALIDÉ le 2026-10-29** (plus de plantage, choix et pertes OK), sauf
  l'**icône de la bulle** : `FrostpunkUIDillemaTracker` (vtable
  `0x142217550`), mise à jour **`FUN_141c19460(tracker)`**, ne pose l'icône
  **qu'une fois** (drapeau `tracker +0x80`) : marqueur `*(tracker +0x28)` →
  indice `FUN_1417cfbf0(système, marqueur)` → entrée `FUN_1417cfd70(système,
  indice)` → tuile `entrée +0x238` (si `+0x230`) posée sur l'image
  `**(tracker +0x60)` par `FUN_141a33780(image, tuile)` ; au chargement, c'est
  fait avec l'entrée d'origine, avant le recollage. → hook
  `hkMajTrackerDilemme` : une fois par tracker dont l'entrée est une de nos
  copies de piège, repose la tuile avec `FUN_141a33780`. **VALIDÉ le
  2026-10-29** (tous les visuels après rechargement).
  **Technologies (2026-10-29)** : demande de l'auteur, **sauvegarde locale
  prioritaire**, le serveur viendra compléter. État du mod jusqu'ici en
  mémoire seulement, et jamais remis à zéro d'une partie à l'autre :
  `TechRuntime {researched, received}` par hash, `g_progressive[arbre]`,
  `g_sinceBonus`. → **`mods\ap_technos_<GameId>.txt`** (`T <hash> <r> <c>`,
  `A <arbre> <n>`, `B <n>`), réécrit en entier (fichier temporaire puis
  `MoveFileEx`) par `TechSauver` après `OnResearchComplete` et
  `OnProgressiveReceived` ; **cumulatif** (charger une sauvegarde plus
  ancienne garde checks envoyés et items reçus, comme le serveur AP).
  `TechPersistanceTick` (tick d'une seconde, après `CaptureRunState`) : à
  chaque nouvelle génération ou nouveau `GameId`, `RT_Reset`, compteurs à 0,
  relecture, puis `SyncGameState` de chaque techno connue ; `TechSauver`
  n'écrit rien avant cette relecture (sinon un chargement écraserait le
  fichier par un état vide). Non testé.
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
- **Lois** (demande de l'auteur, 2026-10-30 : déblocage **progressif** par
  arbre, comme les technologies). En interne, une loi = **sociotech**
  (« NewLawIntroduced » est émis avec la catégorie `"Sociotech"`).
  Décompilés du 2026-10-30 :
  - `FrostpunkSocietyConfig` = instance statique **`base + 0x4070460`** :
    `+0x78` catégories (= arbres), `+0xB0` **toutes les lois** (pointeurs,
    compte `+0xB8`, `DAT_144070510`).
  - `FrostpunkSociotechEntry` (0x240), base `FrostpunkProgressTreeEntry`
    (comme les technos) : `+0x18` nom, `+0x20` GUID, `+0x30` Enabled,
    `+0x38` Requirements (compte `+0x40`), `+0x68` Display name, `+0x88`
    Grid position, `+0x94` Signable, `+0xD0` Activation dilemma, `+0xE0`
    Effects, `+0x140` Exclusion group, **`+0x148` Category** (lien GUID),
    `+0x1C8` Special node.
  - `FrostpunkSociotechCategoryEntry` (0x90) : `+0x00` nom, `+0x08` GUID,
    `+0x18` Display name, `+0x78` Requirements.
  - `FrostpunkSociotechRequirement` (0x40) : `+0x30` lien vers la loi requise.
  - `FrostpunkScenariosConfig` = instance statique **`base + 0x2C34490`** :
    `+0x88` scénarios (`FrostpunkScenarioEntry` : `+0x00` nom, `+0x1B0`
    liens « Sociotech setups »), `+0x210` réglages
    (`FrostpunkScenarioSociotechSetupEntry` : `+0x18` lois de départ,
    `+0x30` lois verrouillées, liens de 0x10), `+0x4F8` scénarios de debug.
  - Système des lois : `*(base + 0x408FBD0)` ; `+0xD8` Map `{loi,
    SociotechData}` au pas de 0x18 (compte `+0xE0`), état à `+0x10` de
    l'élément (2 = posée d'office par le réglage de départ, cf.
    `FUN_1416ffbe0`). `FUN_1416ff990(sys, loi)` = active ?,
    `FUN_1416fd350(sys, loi)` = activer. Réglage courant :
    `*(DAT_144062870 + 0xE0)`. Collection des lois : `FUN_141437900()`,
    méthode `+0x88(coll, &guid)`.
  - **F4 = `DiagLois`** (lois.cpp ; remplace `ResetSeenKeys`) : liste par
    catégorie (nom, clé d'affichage, grille, prérequis), réglages et
    scénarios, état courant. **Validé le 2026-10-30.**
  - **Relevé F4 (2026-10-30)** : **112 lois, 4 catégories**.
    - `Purpose` (22) : branches `Order` (11, niveaux 1→5, fin
      `DESPOTIZM`) et `Faith` (11, niveaux 1→6, fin `FANATIZM`).
    - `Adaptation` (30) : branche `Adaptation` (16), branche `People` (11),
      et 3 lois `[… - Endless]` (Cocaine, Field pharmacy, Bathhouse).
    - `Labour` (37, préfixe `Builders` = Last Autumn) : sous-branches
      `Labour` (14), `Left` (12, fin `TERROR`), `Right` (11, fin
      `SERVITUDE`). **8 lois désactivées** (Enabled = 0).
    - `ADMINISTRATION` (23, `Builders [LifeConditions …]`).
    - Paires exclusives par **groupe d'exclusion** (`+0x140`) : Burials,
      Sick, Sick2, Soup, kids1, kids2, Work, Housing, Disabled.
    - Types de prérequis vus : `SociotechRequirement` (loi requise),
      `SociotechExclusionRequirement`, `CompositeRequirement`,
      `UnlockRequirement` (contenu des deux derniers **non relevé**).
  - **6 réglages, 2 seulement rattachés à un scénario** : `WINTERHOME` →
    `Winterhome` (5 lois de départ) ; `BUILDERS` → `Builders` (verrouille
    **toutes** les lois `Purpose` et `Adaptation`, hors Endless).
    `Faith Unlock`, `Order Unlock`, `Benchmark`, `AvgPlayer` : rattachés à
    aucun scénario (debug / tests). 14 scénarios : DEFAULT, Endless Canyon /
    Default / Points / Bridges / Largest Circle, TechHaven, REFUGEES,
    WINTERHOME, Debug_Minimal, Scenario, Debug_Builders, BUILDERS,
    Reconquest. Inconnu : ce qui **masque** `Labour` / `ADMINISTRATION`
    hors Last Autumn (probablement les prérequis des catégories, `cat
    +0x78`, non relevés).
  - État en partie : `Kids work in light jobs = 2` (loi signée → état 2).
  - **Prérequis (décompilés du 2026-10-30)** : base
    `FrostpunkRequirementEntry` (0x30 : `+0x18` lien d'erreur, `+0x28`
    texte d'erreur) ; `CompositeRequirement` (0x50 : `+0x30` Operator
    `eBooleanOperator`, `+0x38` Requirements) ; `SociotechExclusionRequirement`
    (0x38 : `+0x30` ExclusionGroup) ; `UnlockRequirement` (0x40 : `+0x30`
    lien vers `FrostpunkUnlockEntry` : `+0x00` nom, `+0x08` GUID, `+0x18`
    Display name, rangées dans `FrostpunkUnlocksConfig +0x78`, instance
    cherchée par RTTI autour de son gestionnaire `base + 0x4082EC0`).
    → F4 décrit les prérequis récursivement (`op<N>(…)`) et ceux des
    catégories. **Validé le 2026-10-30.** `FrostpunkUnlocksConfig` à
    **`base + 0x4082F10`**. `eBooleanOperator` (déduit) : **0 = ET, 1 = OU,
    2 = NON**.
  - **Visibilité des arbres** (prérequis des catégories) : `Purpose` ←
    unlock `IdeologyTree` ; `Adaptation` ← **NON** unlock `[Builders]Builders
    Scenario` ; `Labour` ← unlock `[Builders] SOCIOTECH Labour tree -
    SAFETY` ; `ADMINISTRATION` ← unlock `[Builders]Builders Scenario`. Donc
    Purpose + Adaptation partout sauf Last Autumn, Labour + ADMINISTRATION
    seulement dans Last Autumn. Les 3 lois « Endless » n'ont **aucune**
    restriction de scénario (Cocaine ← OU soupe, Field pharmacy ← OU soins,
    Bathhouse ← Care House).
  - Autres conditions : Neighborhood watch / House of Prayer ← OU de deux
    `PolicyRequirement` (choix de l'objectif, contenu non relevé) ; DESPOTIZM
    ← OU(Prison beating, Propaganda ability) ; Kids light jobs / Child
    shelters ← NON unlock `[TechHeaven] Quest1` ; Augmentation / Medical
    Repatriation (Last Autumn) ← NON unlock `[Builders] Act 3 - The End` ;
    Right (Factory Inspectorate) ← unlock `STRIKE R` + exclusion `Labour`,
    Union delegates (désactivée) ← `STRIKE L` : Left / Right se décide par
    l'histoire de Last Autumn.
  - **Arbres d'Adaptation (hors Last Autumn)** :
    Burials [Cemetery | Put them in the snow] → [Burial Ceremony |
    Transplantation] → Corps Processing (côté neige seulement) ;
    Sick [RadicalTreatment | Life Sustain] → OU → [Double Ration | People on
    the floor] (Sick2) → Triage, et → Care House → Augmentation, Bathhouse,
    et → Field pharmacy ; Soup [Poor Soup | Sawdust] → OU → Alternative Food
    Source, Cocaine ; kids1 [light jobs | Child shelters] → hard jobs (light)
    | [Med Kids | EngineerKids] (shelters) ; Fightclub → PublicHouses →
    Prostitutes, Moonshine ; Fightclub → Dueling Law ; Emergancy shift →
    Hard work rigor.
  - **ADMINISTRATION (Last Autumn)** : Burials [Cemetery | Morgue] →
    [Burial Ceremony | Steel coffins] ; Soup [Gruel | Better food] → Cocaine,
    Alternative Food Source ; Sick [Radical | Life] → [People on the floor |
    Double Ration], Care House → [Augmentation | Medical Repatriation] ;
    Bathhouse → [Crowded | Comfortable Quarters] ; Pub → Prostitutes,
    Bare-Knuckle Bouts ; Chapel → Evening of Prayers. **Labour** : Work
    [Hard work rigor | Double Shift] → [compensations | optimisation] ;
    Safety Procedures → After-hours ; Light Work rigor → upgrade ; Field
    pharmacy.
  - **Correction de l'auteur (2026-10-30)** : les arbres ci-dessus mélangent
    **tous les scénarios** ; il faut raisonner **scénario par scénario**.
    Capture du livre de Refugees : Fighting Arena (→ Public House, Duelling
    Law), Emergency Shift (→ Extended Shift), Child Labour Safe Jobs (→ All
    Jobs) / Medic | Engineer Apprentices, Soup | Food Additives, Cemetery |
    Corpse Disposal (→ Ceremonial Funerals | Organ Transplants), **Sustain
    Life seule** (→ Extra Rations | Overcrowding) ; **pas** de Radical
    Treatment, Corps Processing, House of Healing, prothèses, etc. Cause
    présumée : les **prérequis d'affichage** (`ProgressTreeEntry +0x50`
    « Show requirements », non relevés jusque-là) et les unlocks du scénario.
    → **F4 v3** : liste aussi `affichage : …` par loi, et une section
    **« scenario courant … : lois presentes »** : catégorie et affichage
    évalués avec les **unlocks actifs** (`FrostpunkUnlocksSystem`, trouvé
    par l'annuaire, `+0x68` = Set de pointeurs d'entrées, compte `+8` ;
    propman `DAT_14408fd10`, réflexion `FUN_1417018c0`) et le réglage
    courant (`*(*(base + 0x4062870) + 0xE0)`, lois verrouillées). Statuts :
    DESACTIVEE, VERROUILLEE, arbre absent, CACHEE, ?, PRESENTE (bloquée ou
    non par un unlock). Prérequis de loi / d'exclusion tenus pour vrais ;
    types inconnus (`PolicyRequirement`…) → `?`. Limite : les unlocks
    **ajoutés plus tard** dans le scénario ne sont pas connus (photo de
    l'instant). Non testé.
  - **1er relevé v3 (Refugees)** : 4 unlocks actifs (IdeologyTree,
    SociotechButton, TechTreeButton, TechnologyPanelOpened). Les lois
    absentes de la capture (Radical Treatment, Care House, Triage,
    Augmentation, Moonshine…) n'ont **aucun** prérequis d'affichage : la
    capture ne montrait qu'une partie du livre (grille jusqu'à y ≈ 5,
    |x| ≈ 6 ; Care House est en (0, 7), Moonshine en (−11, 0)) et une paire
    signée n'affiche que la loi choisie. Prérequis d'affichage réels :
    `ScenarioRequirement` (Corps Processing) et `PolicyRequirement`
    (Census Station, Propaganda, Mechanical Agitator, DESPOTIZM, Temple,
    Captains message, Righteous Denunciation, Public conversion, FANATIZM,
    Alternative Food Source, et 2 chacune pour les 3 lois Endless), NON
    `[TechHeaven] Quest1` (lois des enfants). Décompilés :
    `FrostpunkScenarioRequirement` (0x48 : `+0x30` lien scénario, `+0x40`
    Negate) ; `FrostpunkPolicyRequirement` (0x40 : `+0x30` lien
    `FrostpunkPolicyEntry`, `+0x00` nom, `+0x08` GUID, rangées dans les
    enjeux `FrostpunkSocietyConfig +0x150` → `FrostpunkPolicyIssueEntry
    +0x18`) ; politiques en vigueur : `FrostpunkPolicySystem +0x68` (Map
    `{enjeu, GUID}` au pas de 0x18). → F4 v4 les décrit et les évalue
    (scénario = `ScenarioSystem +0x68 +0x08`). **Validé le 2026-10-30.**
  - **Relevé v4 (Refugees)** : 96 politiques en vigueur (enjeux de tous les
    scénarios, ex. `[Refugees Scenario] Epidemy`, `[SOCIOTECH]Path chosen=
    Both`, `[CustomScenario]IsEndless=No`). Prérequis d'affichage décodés :
    - 9 lois de Purpose ← `[Scenario Fall of Winterhome] Sociotechs
      blocker = Avalible` (vrai hors Winterhome ; Winterhome les retire en
      cours de partie) ;
    - Alternative Food Source ← `[Sociotech] Alternative food source
      avalible = Avalible` (Refugees : `Hidden` → cachée ; politique qui
      peut changer en cours de partie) ;
    - Corps Processing ← **PAS** scénario `Reconquest` (= On the Edge,
      enjeux `[aDLC3]` / `[Reconquest]`) → **présente** dans Refugees selon
      les données, alors que l'auteur dit ne pas y avoir accès : à vérifier
      (case (8, 6), sous Organ Transplants, hors de la capture) ;
    - Cocaine, Field pharmacy, Bathhouse ← ET(`[CustomScenario]
      IsEndlessBuilders = Yes`, `[TechHaven] Manchester Quest =
      ManchesterQuestFinished`) : conditions de deux modes différents,
      donc a priori **jamais** visibles ;
    - lois des enfants ← NON unlock `[TechHeaven] Quest1`.
    Statut Refugees : Purpose 22 présentes, Adaptation 26 présentes
    (cachées : les 3 Endless, Alternative Food Source), Labour et
    ADMINISTRATION absents. **Limite** : politiques et unlocks évoluent en
    cours de partie, un relevé ne vaut que pour l'instant.
  - **2e capture de Refugees (aucune loi de soins signée)** : visibles
    (estompées) Soup | Food Additives, Radical Treatment | Sustain Life,
    Extra Rations | Overcrowding, **Care House**, Cemetery | Corpse Disposal,
    Ceremonial Funerals, Organ Transplants, Medic | Engineer Apprentices ;
    **absentes** Triage, Augmentation, Corps Processing (rien sous Organ
    Transplants). `FrostpunkSociotechCategoryDisplayData` (cat `+0x60`,
    `FUN_1419f1030`) n'est pas un filtre (Modifier, 2 images, Root node
    name). **Hypothèse** cohérente avec les 2 captures : le livre ne montre
    une loi que si ses lois requises sont **signées ou sans prérequis**
    (dévoilement à 2 niveaux) : Care House (← racines Radical / Sustain)
    visible, Triage (← paire Sick2, non signée) et Corps Processing (←
    Organ Transplants, non signée) cachées, Duelling Law / Public House
    (← Fighting Arena, racine) visibles. Donc Corps Processing **existe**
    dans Refugees mais n'apparaît qu'après Organ Transplants.
    **CONFIRMÉ le 2026-10-30** (captures après signatures en chaîne) :
    Organ Transplants signée → **Organic Fertiliser** (= Corps Processing)
    apparaît ; Care House signée → **Prosthetics** (= Augmentation) ;
    Sustain Life signée → Triage (estompée) sous Extra Rations | Overcrowding ;
    Fighting Arena signée → **House of Pleasure** (= Prostitutes) et
    **Moonshine** (estompées) derrière Public House. Règle : une loi est
    affichée quand ses lois requises sont **signées ou signables**. Les
    données (F4 v4) donnent donc bien la liste par scénario : Refugees =
    Purpose 22 + Adaptation 26.
    Noms affichés relevés : Kids work in light / hard jobs = Child Labour –
    Safe Jobs / All Jobs, Child shelters (non vue), Med Kids / EngineerKids =
    Medic / Engineer Apprentices, Fightclub = Fighting Arena, PublicHouses
    unlock = Public House, Dueling Law = Duelling Law, Emergancy shift =
    Emergency Shift, Hard work rigor = Extended Shift, Poor Soup / Sawdust =
    Soup / Food Additives, Put them in the snow = Corpse Disposal, Burial
    Ceremony = Ceremonial Funerals, Transplantation = Organ Transplants, Life
    Sustain = Sustain Life, Double Ration / People on the floor = Extra
    Rations for the Ill / Overcrowding.
  - **Noms affichés dans F4 (2026-10-30, non testé)** : le nom interne
    (`+0x18`, ex. `Corps Processing - fertilizer`) diffère du nom affiché
    (« Organic Fertiliser », clé `+0x68`). `hkTranslate` appelle
    `LoisNoterTraduction(cle, valeur)` pour chaque traduction ; les clés
    `Generated/SocietyConfig/Sociotechs/…/Display name` sont mémorisées
    (256 max, texte ASCII), et la section « lois presentes » de F4 affiche
    le nom vu, ou « (nom affiche non vu) » si le jeu ne l'a pas encore
    traduit (ouvrir le livre de lois avant F4).
    **Validé le 2026-10-30** (A New Home, Last Autumn). Relevés :
    - **A New Home (`DEFAULT`)** : Adaptation 26 présentes (mêmes que
      Refugees) ; Purpose **fermé au moment du relevé** (unlock
      `IdeologyTree` donné plus tard par l'histoire).
    - **Last Autumn (`BUILDERS`)** : Purpose et Adaptation verrouillées par
      le réglage ; ADMINISTRATION 22 présentes (Alternative Food Source
      cachée), noms affichés : Pub = Public House, Prostitutes = House of
      Pleasure, Evening of Prayers = Evening Service, Cemetery = On-Site
      Burials, Morgue = Repatriation of Bodies, Steel coffins = Company
      Funerals, Better food = Hearty Meals, Bathhouse = Bath House, Cocaine
      = Cocaine Pills, Augmentation = Prostheses Shipments ; Labour **fermé
      au moment du relevé** (unlock `SOCIOTECH Labour tree - SAFETY`).
    - Noms de Purpose non vus (jamais traduits dans ces parties).
    → F4 v5 : un arbre fermé n'efface plus ses lois (statut `PRESENTE si
    l'arbre s'ouvre`, prérequis de l'arbre en clair) ; `LireTexte` lit par
    blocs de 16 (un nom interne sortait `?` près d'une fin de page).
    Réponse à l'auteur : **pas besoin** de tout débloquer avant F4 ; ouvrir
    le livre suffit pour les noms affichés (globaux, une fois par session).
  - **Relevé F4 v5 de tous les scénarios (2026-10-30)** — lois existantes
    par scénario (hors désactivées, hors cachées) :
    | Scénario (interne) | Purpose | Adaptation | Labour | ADMINISTRATION |
    |---|---|---|---|---|
    | A New Home (`DEFAULT`) | 22, arbre ouvert plus tard (`IdeologyTree`) | 26 | — | — |
    | The Arks (`TechHaven`) | **aucune** (vérifié par l'auteur) | 21 : **sans les 5 lois des enfants** (unlock `[TechHeaven] Quest1` actif dès le départ) | — | — |
    | Refugees (`REFUGEES`) | 22 (ouvert) | 26 | — | — |
    | Fall of Winterhome (`WINTERHOME`) | 22 (ouvert) | 26, réglage `Winterhome` (5 lois de départ) | — | — |
    | Last Autumn (`BUILDERS`) | verrouillé | verrouillé | 9 (`Labour`) + Left / Right selon l'histoire (`STRIKE L` / `STRIKE R`) ; arbre ouvert plus tard (`SAFETY`) | 22 (Alternative Food Source cachée) |
    | On the Edge (`Reconquest`) | **aucune** (vérifié par l'auteur) | 25 : **sans Organic Fertiliser** | — | — |
    Toujours cachées : les 3 lois Endless, Alternative Food Source (sauf
    changement de politique en cours de partie). Labour / ADMINISTRATION
    hors Last Autumn : leurs unlocks n'existent que dans Last Autumn.
    Un 7e relevé (scénario `?`, système des lois illisible) a été fait hors
    partie : à ignorer. **Inconnu** : quels scénarios donnent réellement
    `IdeologyTree` (The Arks, On the Edge) ; il est donné par l'histoire
    (effet `FrostpunkAddUnlockEffect`), pas par les données du scénario.
    Noms affichés de Purpose relevés : Neighbourhood Watch, Morning
    Gathering, Patrol, Guard Stations, Foreman, Propaganda Centre (Census
    Station), Prison, Pledge of Loyalty (Propaganda ability), Forceful
    Persuasion (Prison beating), Agitator, NEW ORDER (DESPOTIZM), Shrines,
    House of Healing, The Temple, Protector of the Truth (Captains message),
    Righteous Denunciation, Public Penance (Public conversion), Field
    Kitchens, NEW FAITH (FANATIZM), House of Prayer, Evening Prayers, Faith
    Keepers.
  - **Qui donne les unlocks (F4 v6, 2026-10-30, non testé)** : question de
    l'auteur « The Arks / On the Edge ont-ils Purpose ? ». Les unlocks sont
    donnés par des **données** (effets d'événements et de dilemmes), pas par
    le code → F4 les parcourt en mémoire. Décompilés :
    `FrostpunkAddUnlockEffect` (0x28, réflexion `FUN_141934300`) : `+0x18`
    lien unlock ; `FrostpunkScenarioEntry` : `+0xE8` liens « Events
    setups », `+0x100` liens « Unlocks setups », `+0x1B0` « Sociotech
    setups » ; config des scénarios `+0xC0` réglages d'événements
    (`FrostpunkScenarioGameEventsSetupEntry`, `FUN_14160ed60` : `+0x18`
    « Event directories ») et `+0x248` réglages d'unlocks
    (`FrostpunkScenarioUnlocksSetupEntry`, `FUN_1416103b0` : `+0x18`
    « Starting unlocks »). Parcours (`DiagUnlocks`, lois.cpp) : tous les
    répertoires d'événements (`base + 0x406DDA8`), effets `+0x58`,
    récursion générique dans les tableaux d'objets « Frostpunk… » des
    0x100 premiers octets de chaque effet, et dilemmes des effets
    `…DilemmaEffect` (lien `+0x18`, résolu par
    `(*DAT_144069970)->vtable[0x88]`, effets initiaux / ignorés, effets et
    résultats des choix). Sortie `[UNLK] <unlock> <- evenement "…"
    (repertoire …)`, puis par scénario ses réglages d'événements / d'unlocks
    et le détail des réglages. **F4 s'exécute désormais sur le thread du
    jeu** (`LoisDemanderReleve` → `LoisPompe`) : jeu **hors pause**.
    **1er test : plantage** juste après `52 repertoires d'evenements`.
    Cause : `ResoudreDilemme` lisait `*(base + 0x4069970)` comme pointeur
    de collection, alors que **`DAT_144069970` est la collection
    elle-même** (`FUN_1413e4240()` renvoie `&DAT_144069970` ; vtable en
    `+0`) → appel d'une adresse prise dans le code. Corrigé : collection =
    `base + 0x4069970`, vtable et fonction bornées à l'image, appel sous
    SEH (`AppelerTrouverDilemme`). Log `[UNLK] resolution des dilemmes :
    vtable +…, fonction +…`.
    **2e test (2026-10-30) : pas de plantage**, 510 effets AddUnlock, mais
    **aucun dilemme suivi** (ligne de résolution absente) et ensemble des
    objets vus saturé (8192). Réglages : `DEFAULT` et les Endless →
    événements `Default` (Main Scenario, Londoners, Beacon, Endgame…) ;
    `TechHaven` → TechHeaven Quest, Londoners, Beacon ; `REFUGEES` →
    Refugees Quest + **unlocks de départ `Winterhome` = IdeologyTree,
    TechTreeButton** ; `WINTERHOME` → idem ; `BUILDERS` → Builders Quest /
    Consequences / Utility / Debug ; `Reconquest` → Beacon, Reconquest
    Quest / Utility / Improvements, unlocks de départ vides. Qui donne :
    - **IdeologyTree** : réglages d'unlocks Winterhome (donc Refugees et
      Winterhome dès le départ), **« U: Sick from the dead - start - first
      dead » (répertoire `Health`)**, « Endless Start » (×3,
      `CustomScenarioEvents`), et des événements de debug ;
    - `SOCIOTECH Labour tree - SAFETY` : « M: [Crisis] 1 - First Work
      Accident (Safety Unlock) » (Builders Quest) ;
    - `STRIKE L` / `STRIKE R` : seulement des événements de **debug**
      trouvés (le vrai déblocage passe sans doute par un dilemme, non suivi) ;
    - `[TechHeaven] Quest1` : « U: Intro Arks » (dès le départ des Arks) ;
    - `[Builders]Builders Scenario` : « U: Initial Statistic ».
    Les répertoires `Health`, `Sequences`, `Utility - General`… ne sont
    rattachés à **aucun** réglage de scénario : chargés partout ? → F4 v7 :
    compteurs des effets de dilemme (lien nul / non résolu / résolu),
    ensemble des vus en table de hachage (65536), et liste des
    **répertoires actifs** du système d'événements (`*(base + 0x40935C8)
    +0x2128`, format supposé : tableau de pointeurs). **Validé le
    2026-10-30** (The Arks, On the Edge) : collection des dilemmes à vtable
    `+0x1F22468`, fonction de recherche **`+0xFF8F90`** ; 683 effets de
    dilemme, tous résolus ; répertoires actifs lus correctement (32 dans
    The Arks, 33 dans On the Edge). **`Health` est actif dans les deux**
    (avec `Sequences`, `Utility - SocioTech`, `Hope Related Events`…,
    `Londoners` aux Arks seulement, les répertoires `Scenario Reconquest …`
    à On the Edge, et `Scenario Reconquest Endless` dans les deux).
    Défaut : un effet partagé n'était attribué qu'au **premier** événement
    qui l'atteignait (« Sick from the dead » disparu du relevé, IdeologyTree
    attribué à « E: No alive people in the city » de Winterhome). → F4 v8 :
    ensemble des vus **vidé à chaque événement** (`ViderVus`, cases
    mémorisées), et pour tout événement qui donne IdeologyTree, état activé
    (`+0x20`) et tableaux d'objets de l'entrée (déclencheurs `+0x28`,
    conditions…) décrits comme des prérequis (`DecrireEvenement`).
    **Test v8 : attributions fausses** (événements de debug, `[Hint]` de
    Winterhome « donnant » IdeologyTree ; « Sick from the dead » absent).
    Deux causes : (1) le parcours générique lisait 0x100 octets dans des
    effets de 0x28 → débordement dans les objets voisins ; (2)
    `FrostpunkGameEventEntry` fait **0x90 octets, entrées contiguës** :
    `+0xB8…` = l'événement suivant. Disposition confirmée par le relevé :
    `+0x20` Enabled, `+0x28` déclencheurs, `+0x40` conditions (expressions,
    ex. `CurrentScenarioParametersCheckExpression`, `UnlockCheckExpression`),
    `+0x58` effets. → F4 v9 : taille réelle de chaque objet par sa **fiche
    de réflexion** (méthode `[3]` = vtable `+0x18`, fiche `+0x88` = taille,
    `FUN_140e4e520` ; appel sous SEH), parcours générique borné à cette
    taille ; `DecrireEvenement` borné à 0x90. Non testé.
    **Recherche close (2026-10-30)** : l'auteur a vérifié ailleurs que
    **The Arks et On the Edge n'ont pas l'arbre Purpose**. `DiagUnlocks`
    reste dans F4 (relevé lourd, à couper si gênant).
  - **Debug : pas de délai entre deux lois (2026-10-30, VALIDÉ)** :
    commande console `ResetSociotechCooldown` = `FUN_1413a64c0` →
    `FUN_141700760(système des lois)` : `système +0xF8`
    (SociotechActivationCooldown) = `DAT_142c2fca8` (temps nul), puis
    événement « cooldown expiré » (`système +0xA0`) s'il a changé ;
    `+0xF0` = LastSociotechActivationTime. → `LoisPompe` (lois.cpp,
    appelée par le hook du setter de ressources après `DilemmesPompe`,
    thread du jeu) l'appelle dès que `+0xF8` diffère du temps nul, tant que
    `LOIS_SANS_DELAI = true` (**à remettre à false** hors debug). Log
    `[LOIS] debug : delai … remis a zero` (10 premiers). Comme les autres
    pompes, ne tourne pas en pause.
  - **Modèle de checks voulu par l'auteur (2026-10-30)** : dans un petit
    arbre d'Adaptation, la loi exclusive (l'une OU l'autre) = **un seul
    check**, la loi qui en découle (quelle que soit la branche) = **un
    seul check** ; l'item **progressif** de l'arbre débloque l'**effet** de
    la loi effectivement signée à ce niveau (signer = check, effet en
    attente de l'item, comme `{researched, received}` des technos). Cas
    hors modèle à trancher : 3e niveau d'un seul côté (Corps Processing),
    exclusion au 2e niveau (Housing, Disabled, kids2), branches sans
    exclusion, Purpose. Lois désactivées : à exclure.
    **Règles tranchées par l'auteur (2026-10-30)** : (1) une loi **sans
    exclusion** = **un check à elle seule** (on peut tout prendre) ;
    (2) une **paire exclusive**, quel que soit son niveau, = **un seul
    check** (ex. Medic | Engineer Apprentices). Les « suites communes » ne
    sont pas exclusives entre elles (aucun groupe d'exclusion : Care House,
    Triage, Prosthetics, Alternative Food Source, Cocaine…) → règle (1).
    (3) Des lois **exclusives par leurs parents** (chacune n'est
    accessible que par une branche différente d'une même paire) forment
    aussi **un seul check** : Child Labour – All Jobs | Medic Apprentices |
    Engineer Apprentices (← Safe Jobs | Child Shelters) ; Ceremonial
    Funerals | Organ Transplants ; Last Autumn : Ceremonial Funerals |
    Company Funerals, compensations | optimisation (travail). Organic
    Fertiliser (côté Corpse Disposal seulement) reste un check à part.
    (4) **Décision de l'auteur** : côté Cemetery, **Ceremonial Funerals
    vaut 2 checks** (l'équivalent d'Organ Transplants + Organic
    Fertiliser), pour que les deux côtés envoient autant de checks.
    **Order / Faith et Left / Right** (branches entières exclusives) :
    même nombre de lois (11 / 11 ; 10 / 10 hors désactivées) mais
    **formes différentes**. Order : NW → {MG → Foreman ; GS → {Census →
    {Pledge, Agitator} ; Prison → Forceful Persuasion ; Patrol}} → NEW
    ORDER (← OU Pledge, Forceful), profondeur 5. Faith : HoP → {Shrines →
    {House of Healing, Field Kitchens} ; Evening Prayers → {Temple →
    Righteous Denunciation ; Faith Keepers → Public Penance → Protector of
    the Truth → NEW FAITH}}, profondeur 6. Left : LU → {CB ; WC → OD →
    {SS ; Prop ; Abolition → Militia → {Show of force, TERROR}}} ; Right :
    FI → {IP ; Overseers → SO → {Class Seg., Strike Busting, Penal Colony
    → {Rationalised HC ; Round-ups → SERVITUDE}}} (identiques jusqu'à la
    profondeur 4, différents ensuite). **Retenu par l'auteur** : checks
    **par rang** (la N-ième loi signée dans la branche choisie envoie le
    check N ; 11 pour Purpose, 10 pour Left / Right).
  - **Signer sans effet / effet sans signer (décompilés du 2026-10-30)** :
    `FUN_1416fd350(sys, loi)` = signer : frustration (`loi +0x90`, sur les
    entités de `DAT_144098320`), map `+0x08` date / `+0x10` état 2,
    `sys +0xF0` / `+0xF8` délai (`loi +0xC8`), **`FUN_1416fdbc0(sys, loi)`**,
    espoir (`FUN_1418d8440`, `loi +0xB0/+0xB4`), alignement (`loi +0x98`,
    `FUN_1418fa5e0`), commentaires (`loi +0x1D0`), événement `sys +0x30`,
    `FUN_1416ffe70` (rafraîchit le livre). `FUN_1416fdbc0` = modificateurs
    globaux `+0xF8` / d'entité `+0x110` / de lieu `+0x128` inscrits **une
    fois** dans le système de modificateurs (`DAT_144093180`, liste
    `+0x2128`), puis effets `+0xE0` (`FUN_14169ae90`, contexte par défaut).
    Aussi appelée par `FUN_1416ffbe0` (lois de départ du réglage, état 2
    posé à la main) : la séparation état / effet existe déjà dans le jeu.
    Mais l'état est **aussi lu en continu** : `FUN_1416ff990` (« active ? »)
    a 29 appelants, dont l'expression « Sociotech is researched »
    (`FUN_1415d5ce0`, conditions d'événements), le panneau du refuge
    d'enfants (Medic / Engineer, `FUN_14152bb30`), le système des groupes
    (`FUN_1418b3d50`), les prérequis des lois et le livre.
    **Décision de l'auteur (option A)** : signer = état 2 tout de suite
    (le jeu réagit à l'état, événements compris) ; seuls les
    modificateurs et effets (`FUN_1416fdbc0`) attendent l'item.
    Espoir (`FUN_1418d8440` = `frostpunkcitizenhopesystem.cpp`) et
    « alignements » (`FUN_1418fa5e0`, système `DAT_14409d2e0` : jauges
    0..1 par alignement, liste `DAT_14407fea8`, commande console
    `SetAlignment`) : **à la signature** (décision de l'auteur,
    2026-10-30), comme la frustration, le délai et les commentaires.
    Donc à la signature : tout `FUN_1416fd350` **sauf** `FUN_1416fdbc0`,
    appelé à la réception de l'item (ou à la signature si l'item est
    déjà reçu).
  - **Ce que la sauvegarde du jeu garde (décompilés du 2026-10-30)** :
    `SociotechData` (`FUN_141700520`) = `ActivationTime` + `State`
    (`eSociotechState`, 1er nom « Inactive ») → l'état 2 d'une loi signée
    est sauvegardé. Le système de modificateurs (`FUN_1417a0220`)
    sauvegarde **lui-même** ses modificateurs (`GlobalModifierHost`,
    `ModifierExpireQueue`, `ActiveModifiersWithConditions`,
    `EntitiesOfTypeModifiers`, `BuildingsOfCategoryModifiers`) : ils ne
    sont **pas** recalculés depuis l'état des lois au chargement
    (`FUN_1416fdbc0` n'est appelée que par la signature et par le réglage
    de départ, qui saute les lois déjà à l'état 2). Une loi « signée, effet
    en attente » est donc rechargée telle quelle par le jeu ; seul le mod
    doit se souvenir de ce qui est en attente.
  - **Implémenté (2026-10-30, non testé) : `lois_ap.h/.cpp`.** Hooks de
    `FUN_1416fd350` (signature) et `FUN_1416fdbc0` (effets : retenus
    seulement s'ils sont appelés **dans** une signature et que l'arbre n'a
    pas assez d'items ; le réglage de départ passe toujours). **Arbres**
    construits une fois depuis les données : composantes des
    `SociotechRequirement` (composites compris) et des groupes d'exclusion
    (`+0x140`, par catégorie) ; Order + Faith = `Purpose`, Left + Right =
    `Politics` ; autres arbres nommés par leur 1re loi (nom interne) ; lois
    désactivées exclues. Liste dans le journal `[LOIAP]`. **Rang** = 1 +
    poids des lois signées avant dans l'arbre (date d'activation) ;
    Ceremonial Funerals d'Adaptation pèse 2 ; lois de départ (date
    `DAT_14293fde8`) ignorées. Une loi de rang r, poids p envoie les checks
    r..r+p−1 (`SendLawCheck(arbre, rang, nom, effetEnAttente)`,
    popup « Check sent ») ; effet appliqué quand l'arbre a ≥ r items
    (`ReceiveLawProgressive(arbre, joueur)`, popup « Law received » ;
    application par `LoisAPPompe`, thread du jeu, hook du setter).
    **Persistance** `mods\ap_lois_<GameId>.txt` (A items / C checks /
    E guid + date d'activation + date d'application), cumulative ; relue au
    changement de partie et après `LoadGame` (`LoisAPApresChargement` : date
    de la sauvegarde) ; effet présent ssi même date d'activation et
    application ≤ date de la sauvegarde, sinon réappliqué si l'item est là.
    **F5** = un item pour l'arbre de la loi en attente la plus ancienne
    (remplace `BaselineKeys`). Les lois signées par un événement (appelants
    `FUN_14168c160`, `FUN_14186b190`) passent aussi par le hook.
    **1er test (2026-10-31)** : 104 lois, 18 arbres (Politics = Left + Right
    seuls, Labour découpé en petits arbres : comme voulu). Cemetery signée →
    check + effet en attente ; F5 → effet appliqué ; Ceremonial Funerals →
    checks 2 et 3. **Bug** : « Child Labour – Safe Jobs », signée **avant** le
    mod dans cette partie, comptait comme en attente → F5 lui a appliqué son
    effet une **2e fois**. Corrigé : ligne `P <guid> <date>` à chaque effet
    retenu ; à la relecture, une loi signée sans E ni P de la même date
    d'activation est « signée hors suivi » → effet tenu pour présent (E
    écrit) ; checks manquants rattrapés. Arbres nommés par leur 1re racine
    (loi sans loi en prérequis, hors Endless). Tirets typographiques des
    noms affichés → `-`. Non testé.
    **2e test (partie neuve, 2026-10-31)** : Cemetery signée, effet bien
    retenu (journal), espoir OK, mais **cimetière (bâtiment) débloqué quand
    même** : le bâtiment a son propre prérequis « loi signée »
    (`FrostpunkSociotechRequirement`) évalué par **`FUN_1416a71f0`**
    (`frostpunkrequirementsystem.cpp`, évaluateur de tous les prérequis ;
    cas 4 techno, 5, 6, **7 loi** → `FUN_1416ff990`, 12–15 scénario…).
    → hook `hkEvaluerPrerequis` : un SociotechRequirement (vtable relevée)
    qui **n'est pas** un prérequis de loi (ensemble trié des objets de
    `+0x38` et `+0x50` de toutes les lois, composites compris) répond 0 si
    sa loi est `retenue` (signée, effet en attente). Le livre garde donc
    l'état signé. Log `[LOIAP] prerequis … refuse`. Inconnu : le menu de
    construction se rafraîchit-il seul quand l'item arrive ? Non testé.
    **3e test** : un refus journalisé à la signature (prérequis hors lois de
    Cemetery), mais **bâtiment toujours débloqué**. Tout prérequis passe
    par `FUN_1416a71f0` (appelants : `FUN_1416a79e0` liste,
    `FUN_1416a62d0` composite, `FUN_1416a79d0`). Système des bâtiments
    (`DAT_142c3c408`, réflexion `FUN_1412f0980`) : cache non sauvegardé
    **`Ignored_LockedBuildings` `+0x22B8`** (éléments 0x18), événements
    `BuildingsUnlocked` `+0x2280`, `BuildingTypeConstructibleCheck`
    `+0x2130`. Écartés (pas le bâtiment) : `FUN_1416d5650` (objectifs de
    quête), `FUN_1416fec10` (date d'activation), `FUN_141700de0`,
    `FUN_1416ffe70` (livre), `FUN_141bc09f0` (UI du livre), `FUN_1415d66b0`
    (« temps depuis l'activation »), `FUN_14152e990` / `FUN_14152bb30`
    (refuge d'enfants). → **diagnostic** : chaque évaluation d'un prérequis
    d'une loi retenue (60, pile des 8 premiers refus) et hook de
    `FUN_1416ff990` qui journalise les « active ? » sur une loi retenue
    (30, pile des 8 premiers), sans changer le résultat.
    **Résultat (2026-10-31)** : le blocage **fonctionne** dans une partie
    déjà suivie (le menu de construction évalue bien ce prérequis :
    `FUN_1412f07c0`, système des bâtiments, appelé par l'événement de
    signature, et l'UI `FUN_141b81f40`, tous refusés). Le déblocage observé
    venait d'une **partie neuve mise en pause dès le début** : la pompe ne
    tourne pas en pause, le nouveau GameId n'était pas lu, la signature a été
    traitée avec l'état de la partie précédente (« check #1 déjà envoyé »),
    puis à la reprise la loi a été prise pour « signée hors suivi » → effet
    tenu pour présent, bâtiment débloqué, **effet jamais appliqué**. →
    `AssurerPartie(exclure)` : relecture au changement de GameId faite aussi
    **depuis la signature** (hkEffets), la loi en cours étant exclue de la
    vérification. Hook de diagnostic de `FUN_1416ff990` retiré. Non testé.
  - **Icônes des lois = type du check (2026-10-31, non testé)**, comme les
    technos. Réflexion `FUN_14145bdd0` (`FrostpunkProgressTreeEntry`) :
    `+0x80` **Icon tile = case (x, y) seule** ; la texture est commune à
    toutes les lois : `FrostpunkSocietyConfig` (réflexion `FUN_141439ef0`)
    `+0x328` « Sociotech icons texture », `+0x330` nombre de cases, `+0x338`
    / `+0x340` / `+0x348` ornements. `RafraichirIcones` (lois_ap.cpp, après
    chaque signature et chaque relecture de partie) : loi active →
    `icon_done` ; sinon icône du **prochain check de son arbre** (rang =
    1 + poids des lois signées), donc la même pour toutes les lois non
    signées d'un arbre. Type du check : `IconeDuCheckDeTest(arbre, rang)`
    (TEMPORAIRE, hachage stable) en attendant le serveur.
    **1er relevé** : texture = **`UI/Tiles/Icons.dds`** 16 × 16 = l'**atlas
    des bulles (#2350)**, où nos icônes sont déjà → cases
    **`bulleCol/bulleRow`** (`techCol/techRow` si c'était `tech_tree_atlas`).
    Toutes les lois sont en **(2,13)** (flèche jaune ronde), sauf Strike
    Busting (3, −8). Le livre (`FUN_141bbc320`, image `TECH_IMAGE` de chaque
    nœud `TECH_ENTRY` / `LEFT_OR_ENTRY` / `RIGHT_OR_ENTRY`) pose la texture
    `+0x328` (`FUN_141107140`) et la case `loi +0x80` (`FUN_141bb2d30` →
    `FUN_141107360`) à la **construction** de l'arbre.
    **Test (capture de l'auteur)** : rien de visible. Loi non signée = rond
    vide, loi signée = **plume** (case (4,4) d'`Icons.dds`) dans un rond
    bleu : `TECH_IMAGE` n'est pas l'image affichée. → diagnostic
    `hkConstruireLivre` (hook de `FUN_141bbc320`, 4 premières constructions) :
    nœuds du livre (`livre +0x1B0`, compte `+0x1B8`, pas 0x80 : `+0x00` GUID
    de la loi, `+0x10` poignée de l'élément), arbre d'éléments d'un nœud
    signé et d'un nœud non signé (`[LOIUI]` : nom, classe, visible
    `+0x270` bit 0, couleur `+0x120`, textures trouvées entre `+0x280` et
    `+0x600`).
    **Relevé** : le nœud (`TECH_ENTRY` / `LEFT_OR_ENTRY` / `RIGHT_OR_ENTRY`,
    UIButton → `STANDARD_NODE` / `SPECIAL_NODE` → `STATE_ELEM` → `light`,
    `ROTATING` (`ORNAMENT`…), `graphic`, `active`, `selected`,
    `highlighted` (cercles), **`circle_inside`** (UIPicture), `signed_text`,
    et `TECH_NAME`) n'a **aucun `TECH_IMAGE`** : la case `+0x80` n'est
    jamais affichée. La plume d'une loi signée = **`circle_inside`**
    (visible si signée, caché sinon, teinte (0.13, 0.78, 0.84)). Aucune
    texture lisible en clair dans les éléments (scan `+0x280..+0x600` vide).
    → `PoserIconeNoeud` (à chaque construction du livre, tous les nœuds) :
    tuile = texture config `+0x328` + case `bulleCol/bulleRow` posée sur
    `circle_inside` par `FUN_1410260c0`, élément montré (`FUN_14105ac90`) et
    remis en blanc (`FUN_1410297f0`).
    **Test (capture)** : icônes AP **visibles** (done sur Cemetery, autres
    selon l'arbre), mais **~3 fois trop grandes**, et **paires exclusives
    inchangées** (`LEFT_OR_ENTRY` / `RIGHT_OR_ENTRY` : pas de
    `circle_inside`, élément 0x0). → taille d'origine relevée sur la 1re
    image, puis `FUN_14102b170(image, l / 3, h / 3)` (`ICONE_REDUCTION`,
    `+0x210/+0x214`) ; arbre complet du 1er nœud de chaque sorte sans
    `circle_inside`.
    **Test** : taille OK, mais icônes décalées en haut à gauche (la position
    est celle du coin haut-gauche ; `circle_inside` d'origine 87 × 87 en
    (−13, −28)). → recentrage par **`FUN_14102a3e0(élément, vec4)`**
    (translation, mise en file) sur le centre (−13 + 43,5) **en x comme en
    y** (la plume d'origine n'est pas centrée en y ; décalage mesuré sur la
    capture ≈ 30 / 45). **Paires** : relevé de `LEFT_OR_ENTRY` /
    `RIGHT_OR_ENTRY` = demi-ronds dans un `Clip` (`STATE_ELEM` → `defoult`,
    `active`…), sans image intérieure ; le demi-rond **gauche** a, hors du
    `Clip`, une image vide **`BlurNotAvaiable`** (alpha 0) → l'icône de la
    paire y est posée (`IconeDePaire` : `icon_done` si l'une des deux lois
    est active), taille des nœuds simples, centrée sur sa propre géométrie
    (journalisée). Rien sur le demi-rond droit.
    **Test** : nœuds simples presque centrés (un peu bas → `ICONE_DECALAGE_Y`
    = −4) ; paires : tuile bien posée sur `BlurNotAvaiable` (263 × 163 en
    (−80, −25)) mais **invisible** (flou « indisponible », alpha remis à 0
    par l'état). → `PoserIconePaire` : **copie de `circle_inside`** d'un
    nœud simple (recette `+0x1E8` / ressource `+0x1F0` mémorisées,
    `FUN_14110c780`), ajoutée au demi-rond gauche (`FUN_141023d70`), tuile,
    taille 29, centrée sur le rond de la paire (translations cumulées
    `Clip` → `STATE_ELEM` → `defoult` → `circle_bg` + demi-taille). Paires
    traitées après les nœuds simples.
    **4e test** : paires visibles mais décalées d'environ une demi-taille de
    rond (~48 en bas à droite) → la translation de `circle_bg` est déjà son
    **centre** : plus de demi-taille (chaîne journalisée une fois) ; nœuds
    simples trop hauts à −4 → `ICONE_DECALAGE_Y = −2`. Loi **signée** :
    icône verdâtre, car l'animation d'état reteint `circle_inside` en
    (0.13, 0.78, 0.84) après notre blanc. → nœuds simples aussi sur une
    **copie** (`IconeSurCopie`, ajoutée au même parent `STATE_ELEM`, non
    liée aux animations ; réutilisée si déjà là), original réduit à taille
    nulle (plume cachée) ; tuile posée **directement** sur la copie
    (`FUN_141107140` texture + `FUN_141107360` case, corps de
    `FUN_1410260c0` sans la recherche par nom). Non testé.
    **5e test** : toujours teintée (paire signée bleue) et paire décentrée.
    Chaîne relevée : `Clip` (−6, −55) 107 × 200, `STATE_ELEM` (105, 96,7)
    59 × 59, `defoult` (0, 0) 64 × 64, `circle_bg` (−13, −28) 87 × 87 : les
    translations cumulées ne donnent pas le centre (ancres / échelle
    inconnues). → centre **mesuré** sur deux captures (point rouge) et
    interpolé : `PAIRE_CENTRE = (103, 32)`. Couleur : hook de
    `FUN_1410297f0` (poseur de couleur) qui force le blanc (alpha gardé) sur
    nos copies (`g_copies`, vidée à chaque construction du livre) et
    journalise qui les teinte ; couleur / mode de la recette de
    `circle_inside` journalisés. Si aucun appel n'est journalisé, la teinte
    passe par un autre chemin (animateur). Non testé.
    **6e test** : aucun appel du poseur de couleur sur nos copies, et la
    « recette » lue en `+0x1E8` donne des flottants absurdes : **`élément
    +0x1E8` = animateur d'état** (`FUN_1410240d0` : `animateur->vtable
    [0x138]` puis récursion sur les enfants, pointeur nul = ignoré). La
    copie rejouait l'état du nœud avec l'animateur de `circle_inside`. →
    `NouvelleCopie` met `copie +0x1E8` à 0 ; hook du poseur de couleur
    retiré. Paire : icône **sous le demi-rond droit** (dessiné après) →
    copie ajoutée **en dernier au parent commun** des nœuds (`racine
    +0x230` ; `FUN_141023e70` : `+0x228` dernier enfant, `+0x238`/`+0x240`
    frères), à `translation de la racine + (101, 35)` (moyenne des 3
    mesures) ; anciennes copies détruites (vtable `+0xE0`) à la
    construction suivante si leur parent a survécu.
    **7e test** : parent commun = `TREE_PANEL` (UIElement), racines de
    paire en (1500, 1100), (1180, 1450)… (`+0xB0` = 100, 100 ; `+0x8C` = 1) ;
    icône « plus du tout centrée ». Hypothèse : la translation d'une racine
    est son **centre** → position = `tr − taille / 2 + (101, 35)` ; tailles
    de la racine et du panneau journalisées.
    **8e test** : couleur OK (animateur vidé), au-dessus du demi-rond droit
    OK ; icône à (+84, −10) px du centre (racine 100 × 100, `TREE_PANEL`
    2600 × 2400, ~1,22 px / unité) → centre = **translation de la racine
    gauche + (−18, −7)** (`PAIRE_DECALAGE`).
    **9e test** : presque centrée, ~16 px trop à gauche, ~2 px trop bas →
    `PAIRE_DECALAGE = (−5, −9)`.
    **10e test** : un peu trop haut. Remarque de l'auteur : une fois la loi
    d'une paire signée, la **plume du jeu** s'affiche au centre exact du
    rond (sous notre icône) → s'en servir comme référence. Relevé de
    l'arbre (avec translations et tailles) du demi-rond de la loi signée,
    une fois par côté (`arbre du demi-rond SIGNE`).
    **Journal** : aucun demi-rond signé — une loi signée d'une paire devient
    un **nœud simple** (`TECH_ENTRY`, ex. « Child Labour - Safe Jobs » état 2,
    icône done déjà centrée par la logique des nœuds simples) et l'autre
    reste en demi-rond (`LEFT_OR_ENTRY` « Child Shelters »). Le « logo de
    signature » vu par l'auteur = cette icône du nœud simple ; la nôtre
    faisait doublon. → plus d'icône de paire quand l'autre loi est signée ;
    translation de la racine de chaque nœud journalisée (`racine en (x,
    y)`) pour caler le centre des paires non signées sur celui du nœud
    simple qui les remplace.
    **11e test** : doublon disparu ; paire encore ~10 px à gauche, ~3 px
    haut (capture à ~1,55 px / unité) → `PAIRE_DECALAGE = (2, −7)`.
    **12e test** : ~4 px trop haut, et ~2 px trop à droite selon l'auteur
    → `PAIRE_DECALAGE = (0,7, −5)`.
    **13e test** : 2 px plus bas demandés → `(0,7, −3,7)`.
    **14e test** (plein écran 1920, ~1,65 px / unité) : 3 px plus bas →
    `(0,7, −1,9)`. **VALIDÉ le 2026-11-01** (placement bon selon l'auteur).
    **Lois inaccessibles (2026-11-01, demande de l'auteur)** : une loi qu'on
    ne pourra plus signer perd son icône AP (`Accessible`, lois_ap.cpp) :
    non active et (une autre loi de son groupe d'exclusion, même catégorie,
    est active) ou (prérequis insatisfaisables : loi requise inaccessible,
    composites ET / OU évalués, NON et autres types tenus pour
    satisfaisables). Ex. Cemetery signée → Corpse Disposal, Organ
    Transplants, Organic Fertiliser sans icône. Non testé.
    Question de l'auteur : l'icône des lois non signées change quand on
    signe dans l'arbre (Organ / Ceremonial Funerals : trap → important) —
    **normal dans le modèle par rang** : toutes les lois non signées d'un
    arbre montrent le prochain check (rang = 1 + poids signés ; Ceremonial
    Funerals pèse 2), tiré au hasard stable par `IconeDuCheckDeTest`.
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
- **Température (2026-11-01, demande de l'auteur)** : but final = retirer
  toutes les variations de température scriptées et en faire des checks /
  items AP. **Étape 1 (en cours) : cartographie** par scénario et difficulté,
  sans rien retirer. Décompilés du 2026-11-01 — la température vient de la
  **météo**, pilotée par des **données** :
  - `FrostpunkEnvironmentConfig` = instance statique **`base + 0x406B4D0`**
    (réflexion `FUN_1413f5320`) : `+0x58` niveau de température par défaut
    (lien), `+0x68` degrés par défaut (float, −10), `+0x6C` degrés par niveau
    (float, 10), `+0x90` niveaux (`FrostpunkTemperatureLevelEntry`, 0x68 :
    `+0x00` nom, `+0x08` GUID, `+0x40` Display name), `+0xF8` **météos**.
  - `FrostpunkWeatherEntry` (`FUN_1413f6dc0`) : `+0x00` nom, `+0x08` GUID,
    `+0x24` **Storm**, **`+0x54` Temperature offset from default level**
    (int, en niveaux), `+0xB8` Display name.
  - Chronologies : `FrostpunkScenarioWeatherTimelineEntry` (0x40, polymorphe :
    `+0x18` nom, `+0x20` GUID), rangées dans `FrostpunkScenariosConfig +0x1A0`.
    `…SimpleWeatherTimeline` (0x58) : `+0x40` changements
    (`FrostpunkWeatherChangeEntry`, pas 0x20 : `+0x00` jour u16, `+0x02`
    heure, `+0x03` minute, `+0x04` lien météo, `+0x18` marqueur de carte) ;
    `…CompositeWeatherTimeline` (0x60) : `+0x40` composantes (pas 0x18 :
    liens de chronologies), `+0x58` Loop last component.
  - Rattachement : `FrostpunkScenarioEntry +0x118` Weather timelines (liens) ;
    `+0xC8` Game acts setup → `FrostpunkScenarioGameActsSetupEntry` (0x38 :
    `+0x18` actes, `+0x30` 1er acte au départ) → `FrostpunkGameActEntry`
    (0x30 : `+0x18` Weather timelines). Un acte démarre par
    `FrostpunkStartGameActEffect` (`+0x18` lien d'acte) dans un événement.
  - Difficulté : `FrostpunkDifficultyEntry` (0x38) `+0x20` Modifications
    (`FrostpunkDifficultyModificationEntry`, 0x50 : `+0x30` scénario requis,
    `+0x40` interdit) ; météo : `…GameActDifficultyModification` (`+0x50`
    acte, `+0x60` chronologies de remplacement),
    `…ScenarioDifficultyModification` (`+0x50` scénario, `+0x60` idem),
    `…TemperatureLevelDifficultyModification` (`+0x50` niveau, `+0x60`
    chance de maladie : **pas** une variation de température, réglage
    « Health »).
  - Partie : `FrostpunkWeatherSystem` = `*(base + 0x4062D48)` (réflexion
    `FUN_14132de80`) : `+0x118` chronologie calculée {date, météo} (pas 0x10),
    `+0x130` chronologies retenues, `+0x148` index, `+0x150` météo courante,
    `+0x158` précédente. `FUN_14132f260(sys, météo)` = changer de météo
    (commande console `SetWeather`).
  - **F9 = relevé `[TEMP]`** (`temperature.cpp`) : config, toutes les
    chronologies, scénarios / actes, modifications de difficulté (via le
    système des difficultés : **en partie**), événements portant un
    `StartGameActEffect` ou un effet « Weather/Temperature »
    (`LoisParcourirEvenements`, lois.cpp), chronologie calculée de la partie.
  - **Relevé F9 du 2026-11-01** (partie Refugees, Medium). Échelle :
    degrés par défaut **−20 °C** (la donnée remplace le −10 du
    constructeur), 10 °C par niveau, niveau par défaut **Chilly** (indice 3 ;
    niveaux 0 Freezing, 1 Very cold, 2 Cold, 3 Chilly, 4 Liveable,
    5 Comfortable). Une météo porte son décalage en niveaux : le nom donne
    la température (`-40` = −2 niveaux = −40 °C ; variantes `_NoFog`,
    `_DLC3`, `_Endgame` = même décalage, autre visuel ; `10_Builders` = +3,
    `00_Builders_with_snow` = +2, `-00` = −1 malgré son nom). 52 météos,
    66 chronologies. **Aucune météo n'a `Storm` (+0x24) à 1**, pas même les
    `_Endgame` de la tempête.
    **Vérifié** : la chronologie calculée de la partie (12 changements)
    reproduit « Refugees » à l'identique ; `jour N 07:00` = 07:00 du N-ième
    jour affiché ; `jour 1 00:00` = météo de départ.
  - **Mécanisme par scénario** : chronologie du scénario (`+0x118`),
    **remplacée selon le réglage de difficulté « Weather »** (Easy / Hard /
    Frostbite : `…ScenarioDifficultyModification` par scénario ; Medium = la
    chronologie du scénario). Les autres réglages de difficulté n'ont aucune
    modification météo. Actes : A New Home, The Arks, Endless et scénarios de
    debug ont le réglage d'actes « Default » (Storm preparation → Storm),
    dont seul **Storm preparation** dépend de la difficulté ; Last Autumn :
    3 actes **sans** chronologie (Acte 1 à `U: Initial Statistic`, Acte 2 à
    `S: [Crisis] 3 - Breaking Point`, Acte 3 à `M: [Cut-off] 4 - Last
    delivery`) ; On the Edge : Acte 1 au départ, chronologie
    `Reconquest-normal`. Refugees et Winterhome : pas d'actes.
  - **Tableau complet (référence ; température en °C par jour de bascule, à
    07:00 sauf mention ; J1 = départ)** — seules les **montées** comptent
    (voir plus bas) :

    | Scénario | Difficulté « Weather » | Chronologie | Variations |
    |---|---|---|---|
    | A New Home | Medium | Default | J1 −20, J4 −40, J6 −30, J10 −40, J12 −30, J15 −40, J21 −60, J23 −40, J26 −70, J27 −50, J30 −60, J33 −70, J36 −80 |
    | A New Home | Easy | Default_easy | J1 −20, J5 −40, J6 −30, J11 −40, J12 −30, J16 −40, J22 −60, J23 −40, J26 −60, J27 −50, J31 −60, J34 −70, J37 −80 |
    | A New Home | Hard | Default_hard | J1 −20, J4 −40, J7 −30, J10 −40, J13 −30, J15 −40, J21 −60, J24 −40, J26 −70, J28 −50, J30 −60, J32 −70, J35 −80 |
    | A New Home | Frostbite | Default_frostbite | J1 −20, J4 −40, J8 −30, J10 −40, J14 −30, J15 −40, J21 −60, J25 −40, J26 −70, J29 −50, J30 −60, J32 −70, J34 −80 |
    | A New Home, acte Storm preparation (jours relatifs à l'acte) | Medium | Story - storm preparation | J1 −50, J3 −70, J5 −60, J8 −70, J9 −80, J10 −90 |
    | idem | Easy | …_easy | J1 −50, J4 −70, J5 −60, J9 −70, J10 −80, J10 23:00 −90 |
    | idem | Hard | …_hard | J1 −50, J2 −70, J5 −60, J7 −70, J9 −80, J10 −90 |
    | idem | Frostbite | …_frostbite | J1 −50, J2 −70, J5 −60, J6 −70, J8 −80, J10 −90 |
    | A New Home, acte Storm (toutes difficultés) | — | Story - storm | J1 −90, J2 −100, J3 −110, J4 −120, J6 02:00 −150, J6 07:00 −150 (outro), J6 07:45 −20 (`-00`, −1 niveau) |
    | The Arks | Medium | TechHaven-normal | J1 −20, J4 −30, J6 −40, J7 −30, J9 −40, J10 −30, J11 −40, J13 −60, J14 −40, J17 −50, J18 −60, J19 −50, J25 −60, J26 −70, J27 −80 |
    | The Arks | Easy | TechHaven-easy | J1 −20, J3 −30, J5 −20, J8 −30, J12 −40, J14 −50, J15 −40, J20 −50, J25 −60, J27 −70 |
    | The Arks | Hard | TechHaven-hard | J1 −20, J3 −30, J7 −40, J10 −50, J15 −60, J18 −70, J20 −80, J22 −90 |
    | The Arks | Frostbite | TechHaven-frostbite | J1 −20, J3 −30, J6 −40, J9 −50, J14 −60, J17 −70, J20 −80, J22 −90 |
    | Refugees | Medium | Refugees | J1 −20, J3 −30, J7 −40, J9 −30, J12 −50, J13 −30, J15 −40, J17 −50, J19 −60, J21 −50, J23 −60, J26 −70 |
    | Refugees | Easy | Refugees - easy | J1 −20, J7 −30, J12 −40, J13 −30, J15 −40, J19 −50, J21 −50 |
    | Refugees | Hard | Refugees - hard | J1 −20, J3 −30, J7 −50, J9 −40, J12 −50, J13 −40, J16 −50, J17 −60, J19 −70, J21 −60, J23 −70, J26 −80 |
    | Refugees | Frostbite | Refugees - frostbite | J1 −20, J3 −30, J6 −50, J9 −40, J11 −50, J13 −40, J16 −60, J19 −70, J22 −60, J23 −70, J26 −80 |
    | Fall of Winterhome | Medium | Winterhome-normal | J1 −30, J2 −40, J4 −30, J7 −50, J10 −30, J13 −40, J14 −50, J15 −60, J18 −50, J21 −60, J22 −40, J24 −50, J25 −60 |
    | Fall of Winterhome | Easy | Winterhome-easy | J1 −20, J2 −30, J3 −20, J7 −40, J10 −20, J13 −30, J14 −40, J18 −50, J22 −40, J25 −50 |
    | Fall of Winterhome | Hard | Winterhome-hard | J1 −40, J4 −30, J5 −40, J7 −50, J10 −40, J14 −50, J15 −60, J18 −50, J21 −70, J24 −50, J25 −70 |
    | Fall of Winterhome | Frostbite | Winterhome-frostbite | J1 −40, J6 −50, J10 −40, J13 −50, J15 −60, J17 −70, J18 −50, J21 −70, J24 −50, J25 −70 |
    | Last Autumn | toutes | Builders-* | J1 +10, puis pluie / sec à 05:00 (J3, J4, J8, J9, J11, J13, J14, J15, J18, J19, J23, J24 ; température inchangée), J27 0 (neige), J31 −10, J35 −20, J37 −30, J39 −40, … , J45 14:30 −20 |
    | Last Autumn | Medium | Builders-normal | … J39 −40, J42 −50, J45 14:30 −20 |
    | Last Autumn | Easy | Builders-easy | … J39 −40, J45 14:30 −20 |
    | Last Autumn | Hard | Builders-hard | … J39 −40, J42 −50, J43 −60, J45 14:30 −20 |
    | Last Autumn | Frostbite | Builders-frostbite | … J39 −40, J41 −50, J42 −60, J45 14:30 −20 |
    | On the Edge | Medium | Reconquest-normal | J1 −20, J3 −30, J5 −20, J7 −30, J9 −20, J10 −40, J11 −30, J12 −40, J14 −20, J15 −30, J17 −40, J20 −50, J21 −40, J23 −50, J26 −60, J27 −50, J30 −60, J33 −50, J35 −60, J36 −50, J37 −60 |
    | On the Edge | Easy | Reconquest-easy | J1 −20, J4 −30, J5 −20, J8 −30, J9 −20, J10 −40, J11 −30, J13 −40, J14 −20, J16 −30, J20 −40, J21 −30, J23 −40, J25 −50, J26 −60, J27 −40, J30 −50, J32 −60, J33 −40, J34 −50, J35 −60, J36 −50, J37 −60 |
    | On the Edge | Hard | Reconquest-hard | J1 −20, J3 −30, J8 −40, J10 −30, J12 −40, J16 −50, J17 −40, J19 −50, J20 −40, J21 −50, J22 −60, J23 −50, J26 −60, J29 −50, J30 −60, J33 −50, J34 −60, J36 −50, J37 −60 |
    | On the Edge | Frostbite | Reconquest-frostbite | J1 −20, J2 −30, J6 −40, J10 −30, J11 −40, J13 −50, J14 −40, J17 −50, J18 −60, J19 −50, J22 −60, J23 −50, J26 −60, J29 −50, J30 −60, J34 −50, J35 −60, J36 −50, J37 −60 |
    | Endless (5 cartes) | — | Endless_CompositeEasy (Hard / Builders : autres composites) | composantes tirées au hasard parmi 3 variantes A/B/C, dernière en boucle (détail dans le log) |

    Chronologies de test sans scénario : Always Clear0, BASIC_NO_STORMS,
    GAMESCOM, Default_DEBUG_ACT_2, Cold_Storm_prototype, EndgameTest_*,
    AvgPlayer, Endless_Test.
  - **Décision de l'auteur (2026-11-01)** : **Endless ignoré**, et **seules
    les montées de température deviennent des checks** (les baisses restent
    telles quelles). Montées par chronologie (calculées depuis le relevé ;
    « J9 −40→−30 » = au jour 9, 07:00, de −40 à −30 °C) :

    | Scénario | Difficulté | Montées |
    |---|---|---|
    | A New Home | Medium | 4 : J6 −40→−30, J12 −40→−30, J23 −60→−40, J27 −70→−50 |
    | A New Home | Easy | 4 : J6 −40→−30, J12 −40→−30, J23 −60→−40, J27 −60→−50 |
    | A New Home | Hard | 4 : J7 −40→−30, J13 −40→−30, J24 −60→−40, J28 −70→−50 |
    | A New Home | Frostbite | 4 : J8 −40→−30, J14 −40→−30, J25 −60→−40, J29 −70→−50 |
    | A New Home, acte Storm preparation | toutes | 1 : J5 de l'acte −70→−60 |
    | A New Home, acte Storm | toutes | 1 : J6 07:45 −150→−30 (fin de la tempête = fin de partie) |
    | The Arks | Medium | 4 : J7 −40→−30, J10 −40→−30, J14 −60→−40, J19 −60→−50 |
    | The Arks | Easy | 2 : J5 −30→−20, J15 −50→−40 |
    | The Arks | Hard / Frostbite | **0** (froid monotone) |
    | Refugees | Medium | 3 : J9 −40→−30, J13 −50→−30, J21 −60→−50 |
    | Refugees | Easy | 1 : J13 −40→−30 |
    | Refugees | Hard | 3 : J9 −50→−40, J13 −50→−40, J21 −70→−60 |
    | Refugees | Frostbite | 3 : J9 −50→−40, J13 −50→−40, J22 −70→−60 |
    | Fall of Winterhome | Medium | 4 : J4 −40→−30, J10 −50→−30, J18 −60→−50, J22 −60→−40 |
    | Fall of Winterhome | Easy | 3 : J3 −30→−20, J10 −40→−20, J22 −50→−40 |
    | Fall of Winterhome | Hard | 4 : J4 −40→−30, J10 −50→−40, J18 −60→−50, J24 −70→−50 |
    | Fall of Winterhome | Frostbite | 3 : J10 −50→−40, J18 −70→−50, J24 −70→−50 |
    | Last Autumn | toutes | 1 : J45 14:30 → −20 (depuis −50 / −40 / −60 / −60 selon Medium / Easy / Hard / Frostbite ; fin du scénario) |
    | On the Edge | **toutes** (l'Acte 1 impose `Reconquest-normal`) | 8 : J5 −30→−20, J9 −30→−20, J11 −40→−30, J14 −40→−20, J21 −50→−40, J27 −60→−50, J33 −60→−50, J36 −60→−50 |

    **On the Edge vérifié le 2026-11-01** (F9, difficulté « Weather » =
    Frostbite) : chronologie calculée = `Reconquest-normal` (21 changements,
    identiques à la donnée). L'Acte 1, démarré au lancement, **écrase** le
    remplacement de difficulté du scénario : les variantes
    `Reconquest-easy / -hard / -frostbite` ne servent jamais (sauf si un
    acte suivant les reprend : Actes 2 et 3 sans chronologie).

    **Exclues (décision de l'auteur, 2026-11-01)** : les montées de **fin de
    partie** (fin de la tempête de A New Home, J45 de Last Autumn) ne sont
    pas des checks. Donc Last Autumn n'a **aucune** montée, l'acte Storm
    non plus. The Arks Hard / Frostbite n'a aucune montée ; l'entrée dans un acte (ex. Storm preparation qui
    démarre à −50) n'est pas comptée comme un changement de la chronologie.
  - **Étape 2 (2026-11-01, non testé) : retrait des montées + item.**
    Décision de l'auteur : **toutes** les montées sont retirées, les baisses
    restent vanilla, donc le décalage se **cumule** (« on commence à −20, puis
    prochaine baisse −30, puis −50… ») ; un check reçu rend la montée.
    Décompilés : `FrostpunkTemperatureSystem` (réflexion `FUN_141813280`) :
    **`+0x21A0` CurrentTemperature** (int, en niveaux), `+0x20C0` événement
    GlobalTemperatureLevelChanged (écouteurs `+0x20D8`, compte `+0x20E0`) ;
    **`FUN_141812c10(sys)` = recalcul** : indice du niveau par défaut (config
    `+0x58` dans `+0x90`) + décalage de la météo courante (`+0x54`), écrit
    `+0x21A0` et diffuse si la valeur change ; seul appelant : l'écouteur
    `0x141811540` (objet `sys + 0x20A8`, inscrit par `FUN_141812500` sur
    l'événement « météo changée », `système météo +0x30`, déclenché par
    `FUN_14132f260` après la pose de `+0x150/+0x158/+0x160`). Température
    d'une entité (`FUN_141811e10`) = `+0x21A0` + chaleur locale.
    → `temperature.cpp` : hook de `FUN_141812c10`. À chaque changement de
    météo, si c'est une **montée** (décalage de la nouvelle > ancienne), elle
    est notée « retirée » (date `météo +0x160`, niveaux), sauf les montées de
    fin de partie (vers `-00` ; vers `-20_NoFog` dans `BUILDERS`). Décalage
    AP = niveaux reçus − montées retirées dont la date est passée ; il est
    ajouté à `météo courante +0x54` **le temps de l'appel original**, qui
    calcule et diffuse lui-même. Recalcul forcé (sans noter de changement) à
    la réception d'un item et au changement de génération de partie.
    Persistance **`mods\ap_temperature_<GameId>.txt`** (`A <reçus>`, `M <date
    hex> <niveaux>`), cumulative ; une montée rejouée après chargement d'une
    sauvegarde plus ancienne retombe sur la même date : pas comptée deux fois.
    Item : **`ReceiveTemperatureRise(levels, player)`** (archipelago, popup
    « Temperature rise »), **F8** = +1 niveau de test. Contrairement au
    tableau, une **entrée d'acte qui réchauffe** (changement de météo)
    est aussi retirée. Non traité : une partie commencée sans le mod (ses
    montées passées ne sont pas retirées). Journal `[TEMPAP]`.
    **Prévisions de l'interface (demande de l'auteur : retirées aussi)** —
    décompilés du 2026-11-01 : `FUN_141559730(panneau, i, &chrono[i],
    &chrono[i−1], p5)` = icône de la **frise météo** pour le changement i
    (`WEATHER_CHANGE_ICON`, case montée / baisse selon l'écart des décalages,
    infobulle `env +0x200 / +0x218`, icônes `+0x250 / +0x268`) ; écart nul →
    pas d'icône, renvoie 1 (boucles `FUN_141559f10`, `FUN_14155a620`,
    `FUN_14155acf0`, frise reconstruite sur l'événement « chronologie
    changée » `météo +0x68`). `FUN_141bcc1a0(panneau, météo à venir)` =
    icône du **panneau de température** (`TEMPERATURE_UP` / `_DOWN` /
    `STORM`, écart nul : rien), écouteur `0x141bcb540`. Le message
    « WeatherChangeApproaching » (n° 0x1B, `FUN_141680800`) n'est programmé
    que vers une météo plus haute dans la liste des météos (= plus froide) :
    rien à faire. → hooks : une montée retirée (même règle d'exclusion) est
    traitée comme un écart nul (frise : renvoie 1 ; panneau : appelé avec la
    météo courante).
    **Test du 2026-11-01 (On the Edge) : validé par l'auteur.** 3 hooks OK ;
    F8 : 8 appuis en pause regroupés à la reprise (`+8`, température 3 → 11
    niveaux), puis 3 → … 12 ; frise : montée du changement 2
    (`-30_DLC3 → -20_DLC3`) masquée. **Aucune borne** : le niveau monte
    au-delà de Comfortable (indice 5), à borner si nécessaire. Pas encore vu :
    une vraie montée retirée au changement de météo (`MONTEE … retiree`) et
    le rechargement d'une sauvegarde.
  - **Points ouverts** :
    - le **déclencheur réel des actes de A New Home** (Storm preparation,
      Storm) n'est dans aucun événement ni dilemme : seuls des événements
      de debug (commandes console `StartStormPreparation`, `StartStorm`,
      `changeAct`, `endGame`) portent un `StartGameActEffect` « Default ».
      Piste : quêtes, ou code (`StartGameAct`). The Arks a aussi le réglage
      « Default » : a-t-il réellement un acte de tempête ?
    - **Actes : prochaine grosse étape du mod** (décision de l'auteur,
      2026-11-01). La cartographie n'en dépend que pour la date réelle de la
      montée de Storm preparation (jour relatif au début de l'acte).
    - les `EntityTemperatureLevelComparisonExpression` (prison gelée,
      `[Act III] Cold steel`…) sont des **conditions** sur la température,
      pas des variations.
    Validation de l'auteur attendue.
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
- **Actes et quêtes (2026-11-01, prochaine grosse étape)** : les « actes »
  de données (`FrostpunkGameActEntry`) ne sont **pas** les actes du joueur.
  A New Home en a 4 selon l'auteur : (1) trouver Winterhome, (2) convaincre
  les Londoniens de rester, (3) recevoir les réfugiés en attendant la
  tempête, (4) survivre à la tempête ; (3) et (4) correspondent aux actes de
  données Storm preparation / Storm, (1) et (2) aux répertoires
  d'événements « Main Scenario » / « Londoners ». Première quête de A New
  Home (donnée par l'auteur) : **Fight the Cold** — Stockpile some Coal
  (0/200), Turn the Generator on.
  Décompilés du 2026-11-01 : **`FrostpunkQuestsConfig` = `base + 0x407AE80`**
  (`FUN_14148d280`), collection hiérarchique en `+0x58`, **répertoires en
  `+0x98`** (compte `+0xA0`) ; `EntryDirectory<FrostpunkQuestEntry>` (0x30) :
  `+0x00` nom, `+0x08` GUID, `+0x18` quêtes ; **`FrostpunkQuestEntry`**
  (0xF0, `FUN_1414a5930`) : `+0x00` nom, `+0x08` GUID, `+0x18` Timeout,
  `+0x30` **Objectives**, `+0x48` / `+0x60` / `+0x78` effets initiaux /
  succès / échec, `+0x90` Social problem, `+0xA0` Displayed name, `+0xA8`
  Description, `+0xB0` Time left text, `+0xD8` Quest type ;
  **`FrostpunkQuestObjectiveEntry`** (0x88, polymorphe, `FUN_1414a61e0`) :
  `+0x18` Conditions, `+0x31` Hide progress, `+0x32` Display remaining
  progress, `+0x33` Mandatory, `+0x38` icône, `+0x50` texte, `+0x58`
  Description, `+0x60` GUID, `+0x70` entités surlignées ; 61 sous-classes
  (`GatherResource`, `ActivateBuildingOfType`, `ConstructBuildingOfType`,
  `ResearchTechnology`, `ActivateSociotech`, `CompleteQuest`, `EndGame`,
  `GeneratorConstructionStepFinished`, `RescueSurvivors`…), champs propres
  au-delà de `+0x88`. Classe d'effet : `FrostpunkProgressCustomQuestObjectiveEffect`
  (+ effets `…Quest…` vus par le relevé). **F9 = relevé `[QUETE]`**
  (`quetes.cpp`) : chaque quête (nom, clés du nom / de la description, type,
  classes des effets), chaque objectif (classe, obligatoire, clés de texte,
  classes des conditions, champs propres en entiers / textes), puis les
  événements portant un effet de quête ou d'acte (`LoisParcourirEvenements`,
  lien présumé en `+0x18` résolu en nom de quête).
  **1er relevé (2026-11-01) : validé.** 8 répertoires : `Default` (47, A New
  Home), `Scenarios` (33 : TechHeaven, Refugees, Winterhome), `Simple Snops`
  (36), `MiniSnops` (10), `Achievements` (118, type 9), `CustomScenarios`
  (63, Endless), `Scenario Builders` (63), `Reconquest` (26). Clés de texte
  `@Generated/QuestsConfig/Quests/<répertoire>/Entries/<nom>/…`.
  **« Fight the Cold » = `[Main Narrative Line] 001 ACTIVATE GENERATOR`**
  (type 1) : `GatherResourceQuestObjective` + `ActivateBuildingOfCategory…`.
  Fil principal de A New Home (types 1 / 3 / 4 / 5) : 001 ACTIVATE
  GENERATOR, 002 MEANS FOR SURVIVAL, 003 RESCUE SURVIVORS, 004 FIND OTHER
  CITIES, **005 LONDONERS** (type 5), 006 REFUGEES, 007 CRUEL HORIZON /
  007 The End, `[ENDGAME]` 001 Research tech, 002 Prepare for the storm
  (type 3, sous-quêtes 002a FOOD / b SURVIVORS / c OUTPOSTS / d TEMPERATURE,
  type 4), 003 Final preparation (type 4), 004 Survive the storm.
  Correspondance avec les 4 actes de l'auteur (présumée) : (1) 001–004,
  (2) 005, (3) 006 + ENDGAME 001–003, (4) ENDGAME 004 / 007.
  Lancements trouvés dans les événements : 001 ← `U: Start first quest`
  (Main Scenario) ; 004 Survive the storm ← `U: Gameplay Storm Start`,
  fini par `End` (déclencheur `WeatherChangedEventTrigger`) ; 003 Final
  preparation ← `U: Final preparation quest trigger` ; 005 fini par
  `BREAKPOINT_order_dillema` / `BREAKPOINT_faith_dillema` (Sequences) ;
  006 progressé par `U: Refugees in the City1..3`. Effets vus :
  `StartQuestEffect`, `FinishQuestEffect`, `ProgressCustomQuestObjectiveEffect`.
  Les autres enchaînements passent par les **effets des quêtes** →
  F9 v2 : `LoisParcourirEffets` (lois.cpp) sur les effets initiaux / de
  succès / d'échec de chaque quête (lignes `-> … dans les effets …`), actes
  nommés par `TemperatureNomActe`.
  **Relevé v2 (2026-11-01) : validé.** Chaîne de A New Home :
  001 ← `U: Start first quest` (`GameStartedEventTrigger`) ; 001 succès →
  002 ; 002 → 003 **non trouvé** (succès = `SwitchEffect`, branches non
  parcourues) ; 003 succès → 004 ; 004 → 005 **non trouvé** (succès =
  `SelectEffect`) ; **005 succès ou échec → 006** ; **006 succès →
  acte « Storm preparation »** + `[ENDGAME] 001 Research tech` (échec →
  001 Research tech) ; 001 Research tech succès → 002 Prepare for the storm +
  002a FOOD / b SURVIVORS / c OUTPOSTS / d TEMPERATURE ; 003 Final
  preparation ← `U: Final preparation quest trigger` (4 ×
  `QuestFinishedEventTrigger`) ; 004 Survive the storm ←
  `U: Gameplay Storm Start` (`RandomTimeOfDayEventTrigger`) ; **004 Survive
  the storm, effets initiaux → acte « Storm »** + fin de toutes les quêtes
  ENDGAME 001–003 ; 004 fini par `End` (changement de météo) ; 007 CRUEL
  HORIZON → 007 The End (lancement de 007 : seulement un événement de
  debug trouvé). Le **déclencheur des actes de tempête est donc résolu** :
  Storm preparation = succès de 006 REFUGEES (fin de votre acte 3, début de
  l'attente de la tempête), Storm = début de 004 Survive the storm.
  Reste : parcourir les branches de `SwitchEffect` / `SelectEffect` pour
  les maillons 002 → 003, 004 → 005, et le lancement de 007.
  **Cause trouvée (décompilés du 2026-11-01)** : `FrostpunkSwitchEffect`
  (0x30, `FUN_141943d00`) : `+0x18` Cases = pointeurs vers des
  **`FrostpunkCaseEntry` sans vtable** (0x38, `FUN_141935030` : `+0x00`
  nom, `+0x08` conditions, `+0x20` effets) ; le parcours générique ne
  descendait que dans les objets à RTTI. `FrostpunkSelectEffect` (0x68,
  `FUN_1419408d0`) : `+0x18` Selector, `+0x20` Conditions, `+0x38`
  Probability, `+0x40` Max count, `+0x48` Max fraction, `+0x50` Effects.
  → `ParcourirObjet` (lois.cpp) fouille aussi les éléments **sans RTTI**
  d'un tableau de pointeurs (leurs tableaux d'objets « Frostpunk… » dans
  les 0x80 premiers octets). **F9 v3** (`quetes.cpp` réécrit) : graphe des
  liens (`S` lance, `F` termine, `A` acte ; source = événement + répertoire,
  ou quête + moment initiaux / succès / échec, sans doublon), détail de
  chaque quête avec ses liens entrants / sortants (hors Achievements et
  CustomScenarios), puis **par scénario** (hors Endless, Debug, `Scenario`) :
  répertoires d'événements = ceux de ses réglages (`scénario +0xE8` →
  config `+0xC0` → `+0x18`) **plus ceux qu'aucun réglage ne cite** (marqués
  `*`, supposés chargés partout), et **quêtes atteignables** (lancées par un
  événement d'un répertoire actif, puis fermeture par les liens quête →
  quête), avec leurs sources et les actes lancés.
  **Relevé v3 (2026-11-01)** : 396 quêtes, 52 répertoires d'événements,
  516 liens (368 depuis des événements, 148 depuis des quêtes) ; 002 → 003
  désormais trouvé. Défauts : les répertoires « communs » (`Sequences`,
  `Utility - General`…) portent des événements de **tous** les scénarios
  (A New Home « atteignait » des quêtes de Refugees, Builders, Winterhome,
  The Arks), `Debug` et `CustomScenarioEvents` (Endless) polluaient ; **005
  LONDONERS n'est lancée par aucun `StartQuestEffect`** (seulement terminée
  par `BREAKPOINT_order_dillema` / `BREAKPOINT_faith_dillema`, Sequences).
  Lancement d'une quête : **`FUN_1416e8570(système des quêtes
  DAT_14408f0e8, quête, &contexte) -> char`** (appelée par la commande
  console `StartQuest`, `FUN_1413abc00`). Classes d'effets de quête :
  `FrostpunkStartQuestEffect`, `FrostpunkFinishQuestEffect` seulement ;
  système de séquences : `FrostpunkSequenceSupportSystem`,
  `FrostpunkSequenceSignalEntry` / `…EventTrigger`, `FrostpunkSequenceFlagEntry`
  (non décompilé). **`FrostpunkCurrentScenarioCheckExpression`** (0x30,
  `FUN_1415e12f0`) : `+0x18` lien scénario, `+0x28` Negate.
  → **F9 v4** : sources exclues `Debug`, `CustomScenarioEvents`,
  `Achivments`, `Scenario Builders Debug` ; un événement n'est point
  d'entrée d'un scénario que si ses conditions (`+0x40`, récursivement dans
  les expressions imbriquées) ne l'excluent pas (cités sans Negate =
  autorisés, avec Negate = interdits). **Trace en jeu** :
  `InstallHooksQuetes` (hook de `FUN_1416e8570`) journalise chaque
  démarrage (`[QUETE] DEMARRAGE #n "nom" -> lancee / refusee ; evenement en
  cours : …`, `g_nomEvtEnCours` exposé par evenements.cpp) et la pile des 40
  premiers (`[TRACE]`).
  **Relevé v4 (2026-11-01)** : listes resserrées (A New Home 28, The Arks
  31, Refugees 22, Winterhome 23, Last Autumn 50, On the Edge 29) mais
  encore polluées par des événements de `Sequences` / `Utility - General`
  propres à un autre scénario **sans condition de scénario** (ex.
  `[Refugees-Scenario]Lords_are_coming_SpawnCTA`, `[Builders]GeneratorGet_
  resolve`, `[TH]_ManchesterSurvivor_CTA`, `U: Endlog-refugees2`).
  **Trace en jeu (1 démarrage)** : `[Sociotech Related] Burials` lancée
  **par les effets de la loi Cemetery** (pile `FUN_1416e8570` ←
  `FUN_141697870` ← `FUN_14169aee0` ← `FUN_14169ae90` ← `FUN_1416fdbc0`,
  effets de loi appliqués par lois_ap) : les quêtes `[Sociotech Related]`
  sont lancées par les **lois** (`loi +0xE0`).
  → **F9 v5** : (1) liens depuis les **effets de toutes les lois** (source
  « Lois », admise dans tous les scénarios, la quête visée filtrée par son
  étiquette) ; (2) **filtre par étiquette** de nom (`[Refugees`,
  `Refugees-Scenario`, `Endlog-refugees` → REFUGEES ; `[Builders` →
  BUILDERS ; `[Winterhome` → WINTERHOME ; `[TH]`, `[TH_`, `[TechHeaven`,
  `[NM]` → TechHaven ; `[Reconquest`, `[DLC3`, `[aDLC3` → Reconquest) sur
  l'événement source et sur la quête visée. Reste : le
  lancement de 005 LONDONERS (à tracer en jouant jusqu'aux Londoniens).
  **Relevé v5 (2026-11-01)** : 560 liens (dont **44 depuis des lois** :
  chaque quête `[Sociotech Related]` est lancée par sa loi, ex. Neighbourhood
  watchtower ← `[Order - level 1] Neighborhood watch`, Temple ←
  `[Faith - level 2] Temple`, Labour Union ← `Builders [Left - level 0]
  Labour Unions`). A New Home : intrus de Refugees / Builders / Winterhome /
  The Arks éliminés, mais les quêtes des lois de Last Autumn et Endless y
  apparaissaient → étiquettes `Builders [` (BUILDERS) et `- Endless]`
  (Endless) appliquées aussi au nom de la loi source.
  **Trace en jeu** : 72 démarrages (partie neuve jouée jusqu'aux
  Londoniens) : ~67 succès Steam au lancement, puis 001, 002, 004, Hope
  Fail Last chance, **005 LONDONERS** ; « événement en cours » **toujours
  vide**, y compris pour 001 (lancée par `U: Start first quest`) : les
  effets ne s'exécutent pas pendant l'invocation hookée (`FUN_1417b9b70`),
  effets différés. Pile non relevée pour 005 (limitée aux 40 premiers,
  tous des succès). → pile tracée pour **toute quête hors Achievements /
  CustomScenarios** (200 max), répertoire de la quête journalisé.
  **Trace du 2026-11-01 (suite)** : pile de 005 LONDONERS =
  `FUN_1416e8570` ← `FUN_141697870` (exécution d'un effet) ←
  `FUN_14169aee0` (liste d'effets) ← **`FUN_14169d890`** ← `FUN_141268a70`
  (mise à jour du jeu). Décompilé : `FUN_14169d890(exécuteur)` = **file
  des effets temporisés** (`exécuteur +0xA8` file à priorité, compte `+0xB0`,
  entrée : `+0x00` date, `+0x10` objet `TimedEffect`, `+0x20` poignée de
  contexte) ; à échéance, « Executing scheduled timed effect... » puis
  `FUN_14169aee0(exécuteur, TimedEffect + 0x40, &contexte, &sortie)`.
  005 est donc lancée par un **`TimedEffect`** programmé plus tôt.
  → hook de **`FUN_14169aee0`** (`hkExecuterEffets`, 6 arguments) : pile par
  thread des tableaux d'effets en cours ; à chaque démarrage de quête hors
  succès, chaque tableau est cherché dans les données (`ChercherProprietaire` :
  objet « Frostpunk… » dont un champ `+0x18..+0x60` est ce tableau, dans les
  événements, quêtes et lois ; effets directs `+0x58` d'événement, `+0x48 /
  +0x60 / +0x78` de quête, `+0xE0` de loi) : lignes `[QUETE] tableau
  d'effets = …`.
  **Test** : pour 005 LONDONERS et Hope Fail Last chance, la liste exécutée
  (`TimedEffect +0x40`) n'appartient à **aucune donnée parcourue** (copie
  ou structure non atteinte). → `NoterTimedEffects` : à chaque liste
  exécutée, chaque `FrostpunkTimedEffect` qu'elle contient est noté (512 max)
  avec l'événement en cours (`g_nomEvtEnCours`), la liste qui le porte et
  la liste la plus extérieure ; au démarrage d'une quête depuis un
  `TimedEffect`, son contexte de programmation est journalisé et ses listes
  cherchées dans les données.
  **Partie neuve du 2026-11-01 (jusqu'aux Londoniens)** : démarrages hors
  succès : **001** ← `TimedEffect` de `U: Start first quest` (Main
  Scenario) ; **002** ← `TimedEffect` des effets de 001 ; **003 jamais
  lancée** dans cette partie ; **004** ← `TimedEffect` de `[Act 1 Arc 1] U:
  Step 3 controller` (Main Scenario ; objet partagé avec `FirstRefugees`,
  Sequences) ; **005** ← `TimedEffect` d'une liste **sans propriétaire
  trouvé**, à 0x380 en mémoire de celui de 004 (même zone de données).
  « Événement en cours » toujours vide, y compris à la programmation : les
  effets de ces événements ne passent pas par l'invocation hookée.
  → repli : objet parcouru le plus proche **en dessous** du tableau
  (< 0x2000) avec son événement, et classes des effets du tableau.
  **Partie neuve du 2026-11-01 (trajets de 15 min) : lancement de 005
  trouvé** (ordre du journal) : éclaireurs sur `Site_007_City_Ruins`
  (Winterhome) → événement **`StartWinterhomeSequence`**
  (`FrostpunkStartSequenceEffect` : **séquence cinématique**, système
  `SequenceSystem` / `FrostpunkSequenceSupportSystem`, données propres
  « Sequences », non parcourues par le relevé) → événements
  `Winterhome_InitiateSocialComments`, `Winterhome_ModifyHope`,
  `Winterhome_SpawnCTA` → dilemmes **`[Scenario] M: Pick your way
  (protest)`** (choix Ordre / Foi ; le livre Purpose est construit juste
  après) et **`Winterhome decision`** → plus tard, dilemme **`![London]S:
  Londoners appear FAITH`** (variante selon la voie choisie ; 1 création
  réussie, 5 refusées : max d'occurrences) → message tutoriel **`27.
  Londoners`** → **005 LONDONERS** lancée par un `TimedEffect` dont la liste
  exécutée = `[CreateMessageEffect, TimedEffect]`. Présumé : cette liste =
  **effets initiaux du dilemme Londoners** (`dilemme +0x48`, exécutés par
  `FUN_1417cde10`) — `ChercherProprietaire` ne fouille pas les dilemmes,
  d'où « propriétaire introuvable » (le voisin `BREAKPOINT_faith_dillema`
  n'est qu'un repli d'adresse). Classes d'actions de séquence vues dans
  l'image : `SequenceActionCreateDilemma`, `SequenceActionEmitSignal`
  (→ `SequenceSignalEventTrigger` des événements `BREAKPOINT_*`),
  `SequenceActionStartSequence`… Donc l'acte 2 de l'auteur (Londoniens)
  commence à l'**exploration de Winterhome** par les éclaireurs.
  À vérifier : propriétaire exact de la liste (dilemmes à ajouter à la
  recherche) et qui crée le dilemme Londoners (choix de « Pick your way » /
  « Winterhome decision », ou action de séquence).
  Debug des trajets : **validé** (5 trajets de 10,8 à 15,2 h ramenés à
  0,25 h).
  **Export Excel (demande de l'auteur, 2026-11-01, non testé)** : F9 écrit
  aussi des lignes tabulées `[QXLS]` (Q quête : répertoire, type, nom et
  description **traduits** ; O objectif : nom interne = segment de clé après
  `/Objectives/`, classe, obligatoire, texte et description traduits ; S
  scénario : ordre, quête, genres des sources L loi / E événement / Q quête).
  Traduction = appel du jeu **`FUN_140feb760(*(base + 0x2A28D80), &clé,
  &DAT_142a25a30 (GUID nul = langue courante), 0, 1, 0)`** (comme
  `FUN_14116b850`), résultat : qword +0 → UTF-16 ; non-ASCII sorti en
  `\uXXXX`. Puis `python outils\quetes_excel.py` (depuis la racine du
  projet) → `quetes_par_scenario.xlsx` (une feuille par scénario hors
  Endless, catégories Histoire / Secondaire Simple Snops, MiniSnops / Lancée
  par une loi, + feuille « Sans scénario ») **et `known_quests.h`** (table
  `KNOWN_QUESTS` : scénario, GUID de la quête `+0x08`, nom interne, nom
  affiché, catégorie, nombre de tâches ; demande de l'auteur : à terme,
  terminer certaines quêtes donnera un check). Le texte est celui de la
  **langue du jeu**. 1er F9 : tout « (non traduit) » (212 quêtes) — la clé
  était passée avec son `@` ; le jeu le saute avant de traduire
  (`FUN_141ce5a40`) → corrigé, et GUID ajouté aux lignes `Q`.
  **Validé le 2026-11-01** : 212 quêtes, traductions OK sauf **40 textes**
  (« (non traduit) ») : textes à paramètres / variantes (noms des quêtes
  ENDGAME, objectifs `Custom…`, `RescueSurvivors…`, `Unload…`, `Health -
  facility…`, Builders 002–005), que le jeu remplit avec un contexte.
  Le script écarte les quêtes lancées seulement par des lois **absentes** du
  scénario (`LOIS_ABSENTES` : Purpose dans The Arks / On the Edge, lois des
  enfants aux Arks, Organic Fertiliser à On the Edge), que F9 ne filtrait
  pas. `known_quests.h` : A New Home 18 histoire + 13 lois, The Arks 22 + 3,
  Refugees 17 + 12, Winterhome 14 + 13, Last Autumn 43 + 32, On the Edge
  18 + 6 ; Simple Snops / MiniSnops rattachées à aucun scénario (feuille
  « Sans scénario »). L'en-tête compile mais n'est inclus nulle part.
  **v2 (2026-11-01, demande de l'auteur, non testé)** : onglet « Lois » à
  part (colonne A = scénarios) ; 86 quêtes restaient « Sans scénario », dont
  005 LONDONERS / 006 REFUGEES et toute leur suite ENDGAME (005 lancée par un
  dilemme créé par une séquence). Ajouts :
  (1) **tous les dilemmes comme sources** : `FrostpunkDilemmasConfig`
  (réflexion `FUN_1413e6a10`, 0x1E8 ; « Dilemma templates » `+0x78`,
  « Categories » `+0xB0`, **« Dilemmas » `+0x130`** = répertoires
  `EntryDirectory<FrostpunkDilemmaEntry>`, « Choice types » `+0x168`,
  « Preloaded dilemma directories » `+0x1D0`), instance statique
  **présumée `base + 0x4069880`** (collection `DAT_144069970` = config
  `+0xF0`, à vérifier : log `[QUETE] dilemmes : config +4069880, N
  repertoire(s)`) ; effets initiaux / ignorés / des choix / des résultats,
  et **`choix +0x38` = lien de quête lancée directement à la résolution**
  (`FUN_1417d40c0` → `FUN_14155e350` → `FUN_1416e8570`). Source
  `Dilemmes/<répertoire>`, active dans un scénario si un répertoire
  d'événements du **même nom** y est actif (log par répertoire de dilemmes :
  « repertoire d'evenements du meme nom : oui / NON »). Genre `D`.
  (2) **repli par répertoire de quête** dans le script : `Reconquest` → On
  the Edge, `Scenario Builders` ou `[Builders]` → Last Autumn, `Scenarios` +
  `[Refugees` / `[Winterhome` / `[TechHeaven` / `[TH` ; `CUT …` et
  `[Endless]…` écartées ; catégorie « Déduit du répertoire ».
  (4) **GUID sur les lignes O et S** (des noms internes existent dans deux
  répertoires) ; le script indexe par GUID et refuse un ancien relevé.
  Autres appelants de `FUN_1416e8570` : `FUN_141736590` / `FUN_141736730`
  (quêtes de répertoire au GUID `B66D8367-…`, succès probables), commande
  console. Simple Snops / MiniSnops : déclencheur non trouvé (système du
  jeu présumé, à chercher).
  **Relevé v2 (2026-11-01)** : config des dilemmes **confirmé à `base +
  0x4069880`** (49 répertoires, 411 liens depuis des dilemmes). **005
  LONDONERS ← dilemmes `![London]S: Londoners appear ORDER` / `FAITH`**
  (répertoire de dilemmes « Londoners »), 006 ← succès / échec de 005.
  Mais seuls 7 répertoires de dilemmes ont un homonyme côté événements
  (Debug, Sociotech Hints, Londoners, Endgame, Holiday Gimmick, SeasonPass)
  → **v3** : `ScenarioDuRepDilemmes` rattache par le nom (Refugees…,
  TechHaven…, Winterhome…, Builders…, Reconquest… ; Main Scenario +
  Endgame → A New Home ; écartés : Debug, CustomScenario, Reconquest
  Endless, Holiday Gimmick, SeasonPass, répertoires « unused / backup / WIP
  / TEMP / temp » ; les autres = communs, filtrés par étiquette), et
  étiquettes `[Main Narrative Line` / `[ENDGAME]` → A New Home (Londoners
  actif aussi aux Arks). Résultat du script avec le relevé v2 : 56 quêtes
  sans scénario (Simple Snops, MiniSnops, [Endless] Crysis…, 007 CRUEL
  HORIZON / The End…). **v3 validée le 2026-11-01** : **17 quêtes sans
  scénario** (6 `[Endless] Crysis…`, `CUT …`, 007 CRUEL HORIZON / The End,
  `[Story Randomized] … TEST`, Food Shortage, Use Public Penance / Round Up,
  2 MiniSnops, 2 Simple Snops) ; fil principal (001–006, ENDGAME) dans A New
  Home seulement ; Simple Snops (32, 33 à Last Autumn) et MiniSnops (4, 8 à
  Last Autumn) rattachées partout par les dilemmes « Simple Version Snops »
  / « miniSNOPs ». Le script réserve aussi `[LondonerArc]` à A New Home
  (`RESERVEES` : lois de Purpose, retirées des Arks et de Refugees).
  **Tri avec l'auteur (2026-11-01)** : « Burials et Snow Pit, c'est de la loi
  à 100 % » → catégorie Loi dès qu'**une** loi lance la quête (même si un
  événement ou un dilemme la lance aussi : « S: Death - First death »,
  « [Sociotech Hints]S: FirstDead »), et pour tout nom `[Sociotech Related]`
  (« Burials again » n'est relancée que par « U: Cemetery relocation »).
  Onglet Lois : 39 quêtes ; une quête de loi dont toutes les lois sources
  sont absentes du scénario reste écartée (Guardposts, Prison, Faith Keepers
  aux Arks malgré les dilemmes « Londoners »).
  Décision suivante : « Game Ending avec Last chance, c'est du sans scénario,
  comme c'est présent partout » → toute quête (hors lois) rattachée aux **6**
  scénarios passe dans l'onglet « Sans scenario », catégorie « Commune a
  tous les scenarios (…) », et dans `known_quests.h` avec `scenario = "*"`
  (40 : les 2 `[GAME ENDING] … Last Chance`, `[consequences] Augmentation`,
  `[Disability] Care House`, 32 Simple Snops, 4 MiniSnops). Les variantes
  `[Builders]` restent à Last Autumn. Une quête présente dans 5 scénarios
  sur 6 reste dans ses feuilles.
  Puis : les 14 quêtes Last Autumn au nom affiché « Pass "<loi>" »
  (`[Builders] Pass Workers Councils`, `… Safety Procedures`, `… Terror`,
  `… Servitude`…) sont des quêtes de loi → onglet Lois (règle sur le nom
  affiché, appliquée aussi au repli par répertoire). De même les quêtes
  `[Builders] … Build <bâtiment de loi>` (`[Cut-off] Build Peoples Militia`,
  `[Cut-off] Build Security Outposts`) ; règle limitée à `[Builders]`
  (`[ENDGAME] Build Coal Thumpers` et le générateur de Last Autumn ne sont
  pas des lois).
  **Tri manuel de Last Autumn** (`CLASSEMENT_MANUEL` du script, nom interne →
  catégorie, prioritaire) : en Lois `Pass First Safety Law`, `Open House of
  Pleasure`, `No more soup`, `Open Bath House`, `Open Public House`, `Open
  Care House`, `Dismantle guard posts`, `Better food` ; en nouvelle
  catégorie « Secondaire (hors quête principale) » (`QueteCat::Secondaire`)
  `[Crisis] Resolve strike`, `Heat construction site (research / no
  research)`, `[Discontent] General Strike`, `Find escaped convicts`, `Safe
  Construction Site`. Reste en histoire de Last Autumn : 001 / 002 / 003 /
  004 / 005 / 006 / 006a / 007, New Allerton (+ survivors).
  Demande suivante : les feuilles de scénario ne gardent **que l'histoire** ;
  toutes les catégories « Secondaire … » (hors quête principale, Simple
  Snops, MiniSnops propres à un scénario) vont dans un onglet
  **« Secondaires »** (colonne A = catégorie - scénarios). `known_quests.h`
  inchangé (catégories gardées).
  **Checks de fin de quête (2026-11-01, demande de l'auteur, non testé)** :
  seules les quêtes de l'**histoire** (catégories Histoire / Dilemme /
  Répertoire de `known_quests.h`, pour le **scénario courant** ; pas les
  lois, secondaires ni communes `"*"`) envoient un check, à leur **succès**.
  Décompilés : **`FUN_1416d22a0(système des quêtes, id d'instance, succès)`
  = terminer une quête** (16 appelants : objectifs remplis, effet
  `FinishQuestEffect` = `FUN_14168e090` → `FUN_1416d2710(sys, quête,
  succès)`, console, délai…) ; instances `système +0x1C0` (enregistrements
  0x90, indice par `FUN_1416c5ec0(système + 0x1C0, &id)`) : `+0x10` quête,
  `+0x18` état (−1 active, puis `!succès`), `+0x70` date de fin ; effets de
  succès `quête +0x60` / d'échec `+0x78` exécutés là. → hook
  `hkTerminerQuete` (quetes.cpp) : quête lue **avant** l'original, puis
  `QueteTerminee` : log `[QUETEAP] FIN "…" (scénario) : succes / echec
  [-> quete de l'histoire]` (100 premières hors histoire), check par
  **`SendQuestCheck(nom, nomAffiché)`** (archipelago, popup « Check sent » /
  « Quest: … »), sans doublon : **`mods\ap_quetes_<GameId>.txt`** (`Q
  <GUID>`, cumulatif). Scénario courant = `ScenarioSystem +0x68 → +0x00`.
  Quêtes d'histoire : A New Home 18, The Arks 11, Refugees 8, Winterhome
  14, Last Autumn 12, On the Edge 20.
  **Validé le 2026-11-01** (A New Home) : `[QUETEAP] FIN "[Main Narrative
  Line] 001 ACTIVATE GENERATOR" (DEFAULT) : succes -> quete de l'histoire`,
  puis `[AP CHECK] quete … ("FIGHT THE COLD")` et popup. Partie suivante :
  001, 002 (« SECURE FOOD »), 003 (« PEOPLE FROM OUR CONVOY ») → 3 checks.
  **Non-doublon validé** (2026-11-01) : 001 refaite après rechargement →
  `check deja envoye dans cette partie`, pas de 2e check. Chantier validé.
- **Liste des sites d'exploration (2026-11-01, demande de l'auteur : pour
  leur assigner les checks ; non testé)** : F9 fait aussi
  `ScootingReleverSites` (scooting.cpp) : `[SITES] scenario X : N site(s)`
  puis une ligne `[SXLS]` par `FrostpunkExpeditionSiteComponent` de la
  partie : nom interne (`NomDuGabarit`, clé de `SITE_PAYLOADS`), dossier du
  gabarit (`ExpeditionSites/<dossier>/` : Story, Refugees, _SHARED…), noms
  affichés Display / Unvisited / Explored / Depleted (gabarit `+0x270` /
  `+0x268` / `+0x278` / `+0x280`) et description `+0x290` traduits
  (`TraduireCleJeu`, exportée par quetes.cpp), état `+0x34`, contenu
  courant `comp +0x48` ({définition, quantité} au pas de 0x10 ; nom = qword
  à `définition + 8`, `AP` pour notre porteur), nombre de dilemmes
  d'exploration (`gabarit +0x3D8`). `outils\quetes_excel.py` en fait un
  onglet **« Sites - <scénario> »** par scénario relevé (un nouveau relevé
  du même scénario remplace l'ancien : faire F9 dans chaque scénario, le
  journal les cumule). Contenu = état de la partie au moment du F9
  (vanilla tant que le site n'est ni exploré ni configuré).
  **1er relevé (2026-11-01, A New Home)** : 42 sites (36 `Story`, 6
  `_SHARED`), noms et descriptions traduits OK (nom « exploré » souvent
  vide), 23 sans contenu ; noms des ressources « ? » (lus à `définition +
  8`) → v2 : nom essayé à `+0x00` puis `+0x08` de la définition.
  **Validé le 2026-11-01 (6 scénarios)** : 211 sites (A New Home 42, The
  Arks 31, Refugees 36, Winterhome 26, Last Autumn 31, On the Edge 45),
  ressources nommées (« Steam Cores 1, Steel 56 »…). Le script génère
  aussi **`known_sites.h`** (table `KNOWN_SITES` : scénario, nom interne =
  clé de `SITE_PAYLOADS`, dossier, nom affiché, contenu au relevé, nombre
  de dilemmes), comme `known_quests.h` ; il compile, inclus nulle part.
  **Limite relevée par l'auteur** : un F9 ne voit que les sites **tirés**
  dans la partie (The Arks : `resources01-04, 06`, pas `05`) : les sites
  `_SHARED` (`[t-plain]` / `[t-mountains]` × `resources` / `[landmark]` /
  `[steamcore]`) sont tirés au sort. Décompilés du 2026-11-01 : tables
  `FrostpunkScenarioWorldMapRandomizationTableEntry` (0x90, `FUN_1416108b0` :
  `+0x00` nom, `+0x08` GUID, **`+0x18` Templates** = GUID de gabarit d'entité
  au pas de 0x10, compte `+0x20`, `+0x30` / `+0x48` ressources aléatoires,
  `+0x60` survivants, `+0x78` conditions d'activation), rangées dans
  **`FrostpunkScenariosConfig +0x368`** (réflexion `FUN_14129e3e0`) ;
  résolution (système de tirage `FUN_1418f2a30`) : **`FUN_141035c50(&pool
  DAT_142b2e710, &guid, 1)` = stub par GUID**, `stub +0x08` = nom du
  gabarit, `FUN_14111c4f0(stub, tampon, taille, 1, 0)` = chemin ; puis
  `FUN_1411180b0(stub, type, 1)` = composant de site (conditions de tirage
  `+0x400`). Une variation de carte (`FrostpunkScenarioWorldMapScene
  VariationEntry`, 0x58, réflexion `FUN_141610cc0` : `+0x18` liens vers ses
  tables, `+0x30` entités dynamiques, `+0x48` nom affiché) appartient à un
  `FrostpunkScenarioWorldMapSetupEntry` (`FUN_141610ff0` : `+0x18` scène,
  `+0x20` variations) ; les paramètres d'une partie (`FUN_1412c67f0`) lient
  `+0x50` World map setup et `+0x60` World map scene variation.
  → F9 v3 : `ScootingReleverTablesDeSites` (scooting.cpp) : `[TXLS] T
  <table> <n>` puis `[TXLS] E <table> <site> <chemin>` pour **toutes** les
  tables ; onglet « Tables de tirage » du script.
  **1er test : jeu bloqué** au 1er site de la 1re table (« Refugees », 48
  gabarits, 36 tables) : `FUN_141035c50(pool, &guid, 1)` **charge** le
  gabarit et attend le thread de chargement (`FUN_14111c700` →
  `FUN_141004480`), verrou du pool tenu → interblocage. → recherche pure
  **`FUN_141035cf0(pool, &guid)`** (dichotomie sur les stubs triés par
  GUID : `pool +0x30`, compte `+0x38`, `stub +0x10` = GUID), sans verrou.
  **Validé le 2026-11-01** : 36 tables, tous les GUID résolus. Tables des
  scénarios : `Story Randomized` (A New Home, 44), `TechHaven` (36),
  `Refugees` (48), `Winterhome` (26), `Builders` (32), `Reconquest` (45) ;
  les autres sont Endless (`Endless_*`, `Custom_*`, `Refugees_Castaway`).
  **Une table = TOUS les sites possibles du scénario**, fixes compris (A New
  Home : les 42 vus + `resources03` / `06` ; aucun site vu hors table).
  Script : caches **`outils\sites_releves.tsv`** (relevés fusionnés d'une
  exécution à l'autre, amorcé depuis l'Excel) et **`outils\tables_tirage.tsv`**,
  liste complète = table ∪ relevés (`TABLE_DU_SCENARIO`), colonne « Vu en
  partie » ; `known_sites.h` : 231 sites, champ `vuEnPartie`. Jamais tirés :
  The Arks 5 (`landmark site01/02/05/06`, `resources05`), Refugees 12 (dont
  `[t-coast]Old_Storehouse`, 3 `[t-mountains]resources`), Last Autumn 1
  (`53. Goods Station`). Onglet « Tables de tirage » (toutes les tables).
  **Décision de l'auteur (2026-11-01) : option 2, tirage forcé** (un check
  par site ; la carte d'un scénario doit toujours être la même, « possible
  que les joueurs ne le remarquent même pas »). Décompilé :
  `FUN_1418f2a30(système, &emplacements, table)` = tirage d'une table
  (appelants `FUN_1418f1440`, `FUN_14172a770`) → **`FUN_1418f2290`** :
  mélange des emplacements et des candidats par groupes de priorité
  (`gabarit +0x430`), appariement `FUN_1418edc80` ; **seul hasard = le
  générateur global multiply-with-carry `DAT_142a12d60` / `DAT_142a12d64`**
  (`base + 0x2A12D60`, multiplicateurs `0x9069` / `18000`). → hook
  `hkTirerTable` (scooting.cpp, installé par `InstallHooksExpeditions`) :
  graine fixe (`GRAINE_SITES_A / _B`) le temps de l'appel, état d'origine
  rendu après ; `SITES_TIRAGE_FIXE = true`. Log `[SITES] tirage fixe :
  table "…", N emplacement(s)`. **Validé le 2026-11-01** (A New Home, 2
  parties neuves) : mêmes 42 sites (partagés : `resources01-04`, `landmark
  site09 / site10` ; les parties d'avant le hook variaient). Compte des
  emplacements lu désormais avant l'appel (le tirage vide la liste : le log
  affichait 0). Reste : un relevé par scénario sur partie neuve pour figer
  la carte de chacun dans `known_sites.h` (les sites jamais tirés y
  resteront possibles mais hors carte).
  Question de l'auteur : les **emplacements** sont-ils fixes aussi (un
  `resources01` proche de la ville dans une partie, à l'autre bout dans une
  autre) ? En principe oui (l'appariement emplacement ↔ gabarit est fait
  dans le même tirage), sauf si l'ordre des emplacements fournis varie.
  → F9 écrit aussi `[SPOS] <scénario> <site> <x> <z>` (entité `+0x140`).
  **Test (The Arks, 2 parties neuves)** : mêmes 31 sites mais **14
  échangent leurs emplacements** (sites partagés + `Snowcrawler`) : l'ordre
  des emplacements fournis au tirage varie. → `TrierEmplacements` (hook du
  tirage) : emplacements (enregistrements 0x10, 1er qword = entité ou
  poignée, les deux essayés) triés par position (x puis z) avant la graine.
  Log `… tries par position` / `NON tries` (+ classe du qword si
  l'entité est introuvable). **Validé le 2026-11-01** (The Arks, 3 parties
  neuves) : `tries par position` à chaque fois, mêmes 31 sites, **0 site
  déplacé** entre les relevés. Tirage fixe (sites + emplacements) acquis.
  **Cartes figées (2026-11-01)** : un F9 sur partie neuve dans chacun des 6
  scénarios (tous `tirage fixe … tries par position`). Le script retient
  comme **carte figée** le dernier relevé d'un scénario qui suit un
  `[SITES] tirage fixe` de sa table (cache **`outils\carte_fixe.tsv`**),
  colonne « Sur la carte (tirage fixe) » des onglets Sites, champ
  **`surCarte`** de `known_sites.h` : **211 sites sur la carte** (A New Home
  42 / 44 possibles, The Arks 31 / 36, Refugees 36 / 48, Winterhome 26 / 26,
  Last Autumn 31 / 32, On the Edge 45 / 45) = les sites qui porteront un
  check. Limite : un relevé fait après le chargement d'une sauvegarde
  antérieure au hook, sans nouvelle partie entre-temps, serait pris pour la
  carte figée. Corrigé au passage : plusieurs F9 dans un même journal
  dupliquaient les lignes `S` des quêtes (comptes ×6) ; dédoublonnées par
  (scénario, GUID).
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
