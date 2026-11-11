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
- **Build de release** (2026-11-07) : `release\construire_release.ps1`
  (même commande + `/DAP_RELEASE /DNDEBUG`, sortie dans `release\build\`) →
  `release\Frostpunk_Archipelago_<version>.zip` (DLL, 4 atlas copiés du
  `mods\` du jeu, apworld, YAML d'exemple, README anglais, licences).
  `AP_RELEASE` : pas de console, journal dans `<jeu>\mods\logs\console.log`,
  F11 / relevé `[REFUG]` / `SchemaReflexionTick` / `RapportSubstitutions`
  neutralisés, icônes de test remplacées par `icon_vanilla`. `release\` est
  dans le `.gitignore`. Une seule DLL pour **Epic et Steam** (+
  `ap_buildings_steam.gz`, `textures\`).
- **Steam** : `C:\Program Files (x86)\Steam\steamapps\common\Frostpunk\`
  (déploiement autorisé pour le portage du 2026-11-07 ; autorisation
  permanente non donnée, à redemander). Epic se lance par le
  lanceur Epic (l'exe lancé seul s'arrête avant de lire les archives).
- Dépendance externe : **MinHook** (sources dans `minhook\`, lib statique).
- Fichiers annexes du mod dans un sous-dossier `mods\` du jeu (persistance).
- Outils utilisés hors build : **Ghidra** (analyse de `Frostpunk.exe`),
  **texconv** (`Texconv\texconv.exe`), **7-Zip**, scripts **PowerShell** pour
  les textures.

---

## 4. Architecture des fichiers

| Fichier | Rôle |
|---|---|
| `version.cpp` | Point d'entrée : proxy des 17 exports de `version.dll`, `DllMain`, `PayloadThread` (console + journal `logs\console.log`, le précédent gardé en `console_precedent.log` depuis le 2026-11-11, `MH_Initialize`, hooks des constructeurs de templates **en premier**, puis les autres hooks), boucle principale : **plus de touches de debug** (F3–F9 et F11 retirées le 2026-11-04 à la demande de l'auteur ; jamais F10 = menu des options) + tick d'une seconde (`EnsureTechSelf`, `EnsureResSelf`, **`PartieTick`**, `CaptureRunState`, `ScootingTick`, `SurveillerArbreTechno`) |
| `mod_common.h/.cpp` | Socle : `SafeRead`/`SafeWrite`, `GetClassNameFP` (RTTI), ancrages (`SystemeParId`, `ComposantsDuSysteme`, `IdSysteme::*`), captures (`ListeCaptures`), **cycle de vie de la partie** (`PartieTick`, `PartiePrete`, `PartieGeneration`, `PartieSelection`, `PartieRechargee`, garde-fou sur la liste des sites), **diagnostics d'ancrage** (`DiagAncrages`, `DiagGabaritsPrecoces`, `ChercherValeurDansTas`, `ChercherReferencesVtable`…), balayage mémoire historique (`CollectObjectsByClasses`), résolution des self par globaux, dumps (PropertyManager, vtables), hooks d'E/S (`NtReadFile`/`NtWriteFile`), allocateur du moteur, `ToucheAppuyee`, offsets (`GameOffsets`, `SetupOffsets`, `TECH_*`, `RES_*`), macro `CHRONO` |
| `hooks_loc.h/.cpp` | `hkTranslate` : cœur du détournement de **tous** les textes affichés |
| `hooks_research.h/.cpp` | Les 4 hooks de recherche : `CanResearch`, `ResearchStart`, `ResearchComplete` (location check), `CancelResearch` |
| `tech_state.h/.cpp` | Accès au tableau des technos du jeu (`g_selfTech + 0x2248`) : état par hash ou par pointeur, clés de localisation, parcours |
| `tech_brain.h/.cpp` | État runtime par techno `{researched, received}` ↔ état mémoire du jeu |
| `tech_runtime.h/.cpp` | `TechRuntime`, `RT_Get`, `RT_CanResearch` |
| `known_techs.h` | Table des technos par scénario AP (hash, nom, arbre, rang, palier, parent) — **générée** par `outils\technos_table.py` depuis le relevé F4 `[TECHNO]` et `outils\technos_arbres.tsv` (hash → arbre, à éditer à la main) |
| `known_quests.h` | Table des quêtes par scénario (GUID → nom interne / affiché, catégorie) — **générée** par `outils\quetes_excel.py` depuis le relevé F9 |
| `known_sites.h` | Table des sites d'exploration par scénario (nom interne = clé de `SITE_PAYLOADS`, nom affiché, contenu) — **générée** par `outils\quetes_excel.py` depuis le relevé F9 |
| `archipelago.h/.cpp` | **Surface de communication Archipelago ↔ jeu** (noms en anglais, demande de l'auteur). Serveur → jeu : `ReceiveProgressive(tree, player)` (toute techno reçue est une progressive, même un arbre à un niveau), `ReceiveResource(name, amount, player)`. Jeu → serveur : `SendResearchCheck(hash)`, `SendLocationCheck(id)` — **seuls** points de sortie (appelés par `AP_SendCheck` et par `ScootingTick`). Chaque fonction : partie prête → effet → popup. La future classe réseau n'appellera que ça |
| `resources.h/.cpp` | Lecture/modification des ressources (`Res_Get`, `Res_Add`, `Res_OublierContextes`) via le hook du setter `+0x173FE40`. Le hook capture les **6 arguments** du dernier appel du jeu par ressource ; `Res_Add` met un crédit en attente, appliqué **par le hook, sur le thread du jeu** (l'ancienne version, commentée, ne passait que 4 arguments : règle n° 5) |
| `ap_ressources.h/.cpp` | « Porteur » : définition de ressource clonée servant de support visuel aux items Archipelago (`ApPorteurInit`, `ApPorteurDef`), `EconomieValide` |
| `ap_data.h/.cpp` | Table des checks Archipelago (`ApCheck` : id, joueur, description) ; `ApChargerChecksDeTest` en attendant le serveur |
| `scooting.h/.cpp` | Expéditions, sites, checks portés par les scouts, persistance ; contient la table `SITE_PAYLOADS` (contenu de chaque site par scénario) et `ScootingInit`, qui ne lit **que** les objets du scouting (sites, ville, expéditions) dans les listes des systèmes. Refait son init quand `PartieGeneration()` change |
| `scenario.h/.cpp` | Scénario courant, difficultés, `g_run`, systèmes du jeu, `FindSceneSystem` |
| `reseau_ap.h/.cpp` | **Couche réseau Archipelago** (classe `ReseauAP`, thread dédié) : WebSocket fait main (Winsock, `ws://`), protocole AP (nlohmann/json), session par graine et slot, reconnexion. N'appelle que `archipelago.h` |
| `IconSubManager.h/.cpp` | Substitution des icônes (arbre des technos, bâtiments), atlas |
| `MessageManager.h/.cpp` | Émission de notifications (popup gauche de l'écran) — **fonctionnel** |
| `groupes.h/.cpp` | Groupes de survivants sur la carte du monde : création d'entité par `Creer` (`+0x1016250`) sur le thread du jeu (`GroupesPompe`) ; F3 = test de création |
| `evenements.h/.cpp` | Mécanique des événements et dilemmes du jeu : hooks de l'invocation d'événement `+0x17B9B70` (journal `[EVT]`), de la création `+0x17CDE10` (remplacement du dilemme d'arrivée des groupes AP) et de la résolution `+0x17D40C0` ; `DilemmeCloner`, `DilemmeAPCreer(def)`, `TuileAtlasBulles(icone)` |
| `lois.h/.cpp` | Lois du jeu (« sociotechs ») : `DiagLois` (F4), relevé des arbres, prérequis et réglages par scénario |
| `lois_ap.h/.cpp` | Lois Archipelago : signature = check(s) par rang dans l'arbre, effet retenu jusqu'à l'item progressif, sauvegarde locale `ap_lois_<GameId>.txt` ; F5 = item de test |
| `dilemmes_ap.h/.cpp` | **Catalogue des dilemmes Archipelago** (bulles en ville, pièges) : table `DILEMMES_AP` (id, titre, sous-titre, texte, icône, titre de popup, choix si ignoré, ≤ 4 choix `{texte, ressource, quantité signée, effetSpecial}`) ; `DilemmeAPDemander("id")` (tout thread, file) → `DilemmesPompe` (thread du jeu). **Pour ajouter un piège : une entrée dans la table** |
| `dilemmes_exploration.h/.cpp` | **Dilemmes d'exploration à plusieurs choix** : table `CHOIX_FORCES` (scénario, dilemme, choix retenu) ; à chaque partie, le tableau des choix de la définition (`+0x78`) est remplacé sur place par une copie du seul choix retenu, texte `@AP/Prevail` (« Archipelago Prevail », `ApLocPrevail`, hooks_loc.cpp) ; log `[DILEXP]` |
| `menu_ap.h/.cpp` | **Écran de connexion Archipelago du menu principal** : hook de l'init du panneau principal (`+0x1B28950`), clones d'`EXIT_BUTTON` (libellés Server / Slot name / Password, bouton Connect) et d'un `UITextInput` de panneau en jeu, masque du mot de passe, `mods\ap_connexion.txt` ; Connect → `RequestConnection` (archipelago) |
| `scenarios_ap.h/.cpp` | **Verrou des scénarios au menu** : hook de `FUN_141283ea0` (prérequis de profil) ; pour la liste de déblocage (`scénario +0x20`) d'un des 6 scénarios AP, la réponse = `ScenarioDebloque(nom)`, alimenté **uniquement** par `ReceiveScenarioUnlock` (archipelago) ; preset `LOCKED_<nom>` sauté (texte de la condition du jeu) ; log `[SCN]` |
| `victoire.h/.cpp` | **Fin de partie** : hook de `EndGame` (`+0x1301490`, journal `[FIN]` victoire / défaite) ; toute victoire dans un scénario AP = **événement du goal**, sans item (difficulté imposée, ironman sans effet) (`SendScenarioVictory`, archipelago : `mods\ap_victoires.txt`, goal = tous les scénarios du slot gagnés) ; modes de victoire supprimés le 2026-11-04 |
| `temperature.h/.cpp` | **Température / météo** : relevé F9 (`[TEMP]`, thread du jeu via `TemperaturePompe`) des niveaux, météos, chronologies météo, actes, modifications de difficulté et de la chronologie calculée de la partie ; **montées AP** : hook du recalcul `+0x1812C10`, montées retirées, `TemperatureRecevoirMontee` (← `ReceiveTemperatureRise`, archipelago), `mods\ap_temperature_<GameId>.txt` |
| `sauvegardes_ap.h/.cpp` | **Sauvegardes de la session AP seulement** : hook de l'écriture `FUN_1412892f0` (nom noté dans `<session>\ap_sauvegardes.txt`) et de l'énumération `FUN_141cdee60` (`"saves"`, `"*.save"` : les autres sont retirées de la liste) ; log `[SAV]` |
| `versions_jeu.h/.cpp` | **Versions de l'exe** (2026-11-07) : Epic (`0x611CFD2D`) et Steam (`0x68E665D9`) détectées par l'en-tête PE ; `Adr(rvaEpic)` traduit toute adresse (identité sur Epic, table `VERSION_STEAM` sur Steam) ; exe inconnu = aucun hook ; `FichierAtlasVersion` (atlas des bâtiments Steam). **Toute nouvelle adresse s'écrit `Adr(0x…)` en RVA Epic et entre dans `VERSION_STEAM`** (`outils\portage\`) |
| `schema_reflexion.h/.cpp` | **Big map** : relevé unique du PropertyManager (classes, propriétés dans l'ordre de sérialisation) → `logs\schema_reflexion.tsv`, pour décoder les scènes hors jeu (`outils\scenes.py`) |
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

> Toutes les adresses de ce fichier sont des **RVA de l'exe Epic** ; dans le
> code elles passent par `Adr(...)` (versions_jeu.h). Pour l'exe Steam :
> `outils\portage\correspondances_steam.tsv`.

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

## 8. Accès aux objets du jeu : ancrages et captures

Historique complet (balayage mémoire, mesures, piste des ancrages) :
`docs/chantiers/balayage_memoire.md`.

- **Plus aucun balayage mémoire en jeu** (ancien coût : 15,8 s ; validé le
  2026-10-15, `[ANCRES]` en 0 ms). `CollectObjectsByClasses` ne sert plus
  qu'aux diagnostics.
- **Systèmes** : global fixe `modBase + 0x2C30200 + id × 0x20`
  (`SystemeParId(id)`, `IdSysteme::*`). Ids : Difficulty `0x03`, Scenario
  `0x0C`, MessageProducer `0x15`, Technology `0x41`, Selection `0x4C`,
  GameEntity `0x58`, WorldMap `0x60`, City `0x64`, ExpeditionSite `0x65`,
  Expedition `0x68`, ExpeditionHost `0x69`, SurvivorGroup `0x6D`.
  L'annuaire `+0x2C31B20` donne `{nom, id}`, pas des objets.
- **Composants** : liste `+0x2078` du système (paires `{entité, composant}`
  au pas de 0x10, compte `+0x2080`) → `ComposantsDuSysteme`. Composant d'une
  entité par type : `*(entité + 0x2A0 + idType × 8)`.
- **Gabarits de bâtiment et entrées de message** : aucun ancrage ; capturés
  par hooks des constructeurs posés au démarrage (`ListeCaptures`,
  `InstallCaptureGabarits`, `InstallCaptureEntrees`). Toujours revalider une
  capture par `ClasseExacte` avant usage.
- **Cycle de vie** : `PartieTick` (mod_common), `PartieGeneration()`
  incrémentée à chaque init réussie ; garde-fou sur la liste des sites
  (réagit avant `[LOADING]`) ; `PartieRechargee` surveille l'économie **et**
  le système de technologies (l'économie peut être réallouée à la même
  adresse). `g_sites` : `MAX_SITES = 256`.
- **Chargement d'une sauvegarde** : synchrone sur le thread du jeu ;
  `LoadGame` = `FUN_141265df0`, hooks `[CHARG]` (`InstallTraceChargement`) :
  fin de lecture puis sortie de `LoadGame` = points de recollage des éléments
  AP (voir `docs/chantiers/sauvegarde.md`).
- **Si l'init échoue**, le message d'erreur doit lister précisément ce qui
  manque (`sites=0 send=0 city=1 …`), pas un simple « incomplet ».

---

## 9. État des chantiers

> **Organisation (2026-11-02)** : ce fichier ne garde qu'un **résumé** par
> chantier. L'historique détaillé (décompilés, essais, journaux de test) est
> dans **`docs/chantiers/<chantier>.md`**, copié mot pour mot. Le lire
> **seulement** quand on travaille sur ce chantier.
> **Règle** : chaque nouveau résultat de test, décompilé ou essai s'ajoute
> à la fin du fichier du chantier ; le résumé ci-dessous n'est mis à jour
> que si l'état change (validé, non testé, décision de l'auteur, nouvel
> offset clé). Un nouveau chantier = un nouveau fichier dans
> `docs/chantiers/` + un résumé ici.

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

### Règle générale des threads

**Rien qui touche un système du jeu ne doit s'exécuter sur le thread du
mod** (popups, crédits, créations d'entités). Les actions passent par une
file et une **pompe** appelée depuis le hook du setter de ressources (thread
du jeu) : `PompeNotifications`, `GroupesPompe`, `CarteAssurerAcces`,
`DilemmesPompe`, `LoisPompe`, `LoisAPPompe`, `VictoirePompe`… **Les pompes
ne tournent pas en pause** : il faut reprendre le jeu. `Translate` et
`ModifyResource` sont sur le même thread (le thread du jeu). Encore sur le
thread du mod, à surveiller : `OnItemReceived` (F7) et le tick de scouting.

### Résumés par chantier

- **Ressources et couche `archipelago.cpp`** — `docs/chantiers/ressources_et_ancrages.md`.
  **VALIDÉ le 2026-10-28** : `ReceiveProgressive` (F7), `ReceiveResource`
  (F11), écouteurs actifs, popups par la file. Ressources ancrées à l'init
  (`Res_InitDepuisEconomie` : conteneur `éco +0x2240`, entrées de 0x70) ;
  convention de clé du setter **déduite du premier appel du jeu**
  (définition + 0). `JournalImmediat` (version.cpp) pour toute ligne qui
  précède une opération risquée (le relais du journal perd les dernières
  lignes en cas de crash). Items reçus hors partie prête : ignorés (file à
  prévoir avec le réseau).

- **Groupes AP (livraison par groupe de survivants)** — `docs/chantiers/groupes.md`.
  **VALIDÉ le 2026-10-28** dans n'importe quel scénario : template chargé
  par chemin (`GameEntities/WorldMap/SurvivorGroups/…`, pool
  `DAT_142b2e710`, `FUN_141035dd0` puis `FUN_14111d100`), création
  `Creer` = `FUN_141016250(gestionnaire, template, parent, &transform, 0)`,
  copie du template (nom, description, icône `icon_vanilla_groupe`),
  survivants copiés d'un template donneur (capture pendant le chargement,
  cache), durée réglable (`LinearMovementComponent +0x50/+0x58`), arrivée
  hookée (`FUN_1418b2870`) → `Receive*` + `surArrivee`, bulle d'arrivée
  remplacée par la nôtre (`hkCreerDilemme`), accès à la carte forcé
  (`CarteAssurerAcces`). Inventaire du groupe dans la liste de droite
  (`FUN_141c2b6e0`) : entrée de ressource = `Res_Definition() + 8` ; dernier
  état « non testé » dans le fichier. F3 = groupe de test (F8 réglait les
  30 min restantes ; F8 est désormais l'item de température).

- **Dilemmes et pièges AP (bulles)** — `docs/chantiers/groupes.md` (fin) et
  `dilemmes_ap.cpp`. **VALIDÉ** : bulle sur le générateur (sujet global),
  choix et pertes, icône `icon_trap` de l'atlas des bulles. Création
  `FUN_1417cde10`, résolution `FUN_1417d40c0`. Pour ajouter un piège : une
  entrée dans `DILEMMES_AP`. F6 = piège de test.

- **Atlas des bulles `UI/Tiles/Icons.texture` = entrée 2350** —
  `docs/chantiers/groupes.md`. Noms hashés de la forme
  `ui/tiles/icons.texture` (MurmurHash2, `scratchpad\hash_textures.ps1`).
  Construction : `textures\bulles\composer_atlas.ps1` puis
  `construire_ap_icons_bulles.ps1` → `mods\ap_icons_bulles.gz`.
  **Marge restante ~12,8 ko** : attention aux prochains ajouts.
  Cases (`bulleCol/bulleRow`) : vanilla (7,1), filler (8,1), important
  (9,1), progressive (10,1), trap (11,1), done (12,1), vanilla_groupe (8,4).

- **Sauvegarde des éléments AP** — `docs/chantiers/sauvegarde.md`.
  Principe (décision de l'auteur) : **nos propres fichiers** dans `mods\`,
  recollés au chargement, jamais d'écriture dans la sauvegarde du jeu.
  **VALIDÉ le 2026-10-29** : groupes (`ap_groupes.txt`, clé = id 128 bits
  `entité +0x18/+0x20`, recollage dès la fin de la lecture), bulles en
  attente (`ap_dilemmes.txt`, `InscrireOccurrence` obligatoire sinon
  plantage, icône reposée par `hkMajTrackerDilemme`). Messages
  (`ap_messages.txt`, id unique croissant) : contenu et couleur OK, icône
  après rechargement non testée. Technos (`ap_technos_<GameId>.txt`,
  **cumulatif**) : non testé. **2026-11-04 (décision de l'auteur)** : avec
  le mod, seules les sauvegardes **écrites dans la session AP** (graine +
  slot) sont visibles, les autres sont masquées (`sauvegardes_ap.cpp`).

- **Scouting** — `docs/chantiers/scouting.md`. Infobulle de la case AP de
  la liste de droite (une chaîne par expédition) : non testé. Sites
  landmarks : affichage du contenu à faire. Sauvegarde empoisonnée :
  `ScootingApresChargement` (cargaisons mortes vidées avant le 1er tick) :
  non testé. Debug `EXPEDITIONS_RAPIDES` (trajets de 15 min) **retiré le
  2026-11-04** (hook de l'envoi d'expédition supprimé).

- **Lois (sociotechs)** — `docs/chantiers/lois.md`. Déblocage progressif par
  arbre (18 arbres ; Order + Faith = `Purpose`, Left + Right = `Politics`).
  Signer = état 2 + check(s) par **rang** dans l'arbre ; seuls
  modificateurs et effets (`FUN_1416fdbc0`) attendent l'item
  (`ReceiveLawProgressive`). Règles de checks de l'auteur : loi sans
  exclusion = 1 check, paire exclusive = 1 check, lois exclusives par leurs
  parents = 1 check, Ceremonial Funerals (Adaptation) = 2 checks (1 seul
  dans On the Edge, sans Organic Fertiliser : `Poids`, non testé). Table
  des checks par scénario : `outils\lois_table.tsv` (`outils\lois_table.py`,
  relevé F4 `[LOITAB]`). Bâtiments
  de loi bloqués par `hkEvaluerPrerequis` (validé). Icônes AP dans le livre
  **VALIDÉES le 2026-11-01** (copies de `circle_inside` sans animateur,
  `PAIRE_DECALAGE = (0,7, −1,9)`). Non testés : lois inaccessibles sans
  icône, `AssurerPartie` (partie neuve mise en pause). **2026-11-04** :
  nom et icône du nœud = item du scout (`ApScoutLoi`, `ApIconeDuCheck`),
  rang propre à chaque étape (`RangAffiche`, profondeur dans l'arbre) ;
  paire exclusive : texte sur le demi-rond gauche seulement (droit = un
  espace) — **centrage du texte sous le rond pas encore juste**
  (`CentrerNomPaire`, à reprendre plus tard). Persistance
  `ap_lois_<GameId>.txt`. Debug `LOIS_SANS_DELAI` **retiré le 2026-11-04**
  (délai du jeu entre deux lois rétabli). Lois par scénario : tableau F4 v5
  dans le fichier (The Arks et On the Edge n'ont pas Purpose). Relevé
  (`LoisDemanderReleve`) et item de test : plus de touche depuis le
  2026-11-04.

- **Écran de connexion au menu principal** — `docs/chantiers/menu_connexion.md`.
  **VALIDÉ le 2026-10-31** (rendu, saisie, centrage par remise en page de
  la colonne, mot de passe masqué). Non vérifiés : casse conservée, valeur
  du mot de passe après effacements. **2026-11-04** : libellé d'état de la
  connexion sous Connect (`ApEtatConnexion`, minuterie Windows sans fenêtre
  sur le thread de l'interface) : non testé. Menu sans sauvegarde : hook du
  rafraîchissement `FUN_141b28f10`, **SCENARIO_BUTTON toujours actif,
  NEWGAME_BUTTON (lance A New Home) grisé**.

- **Température** — `docs/chantiers/temperature.md` (tableaux complets des
  chronologies et des montées par scénario et difficulté). Décisions :
  Endless ignoré, montées de fin de partie exclues ; **plus de checks de
  montée** (décision de l'auteur, 2026-11-04 : frise masquée, impossibles
  à situer), seuls les items `Temperature Rise` restent. Étape 2 **VALIDÉE le 2026-11-01** (On the Edge) :
  montées retirées (hook du recalcul `FUN_141812c10`), décalage cumulé,
  `ReceiveTemperatureRise` (F8), frise et panneau masqués. Pas de borne
  haute du niveau. Pas encore vu : une vraie montée retirée en jeu, le
  rechargement. `ap_temperature_<GameId>.txt`.

- **Actes et quêtes** — `docs/chantiers/quetes.md`. Fil principal de
  chaque scénario relevé (F9 `[QUETE]`), export `outils\quetes_excel.py` →
  `quetes_par_scenario.xlsx` et `known_quests.h`. **Checks de fin de quête
  VALIDÉS le 2026-11-01** : succès d'une quête d'histoire du scénario
  courant → `SendQuestCheck`, sans doublon (`ap_quetes_<GameId>.txt`).
  Lancement d'une quête : `FUN_1416e8570` ; fin : `FUN_1416d22a0`. Actes de
  A New Home : Storm preparation = succès de 006 REFUGEES, Storm = début de
  `[ENDGAME] 004 Survive the storm` ; 005 LONDONERS ← dilemmes
  `![London]S: Londoners appear ORDER / FAITH`.

- **Sites d'exploration** — `docs/chantiers/sites.md`. **Tirage fixe VALIDÉ
  le 2026-11-01** (graine fixe + emplacements triés par position,
  `hkTirerTable`) : 211 sites sur la carte (`known_sites.h`, champ
  `surCarte`), cache `outils\carte_fixe.tsv`. Icône AP sur les marqueurs de
  la carte **VALIDÉE** (copie de `ICON` sans animateur, `icon_done` si
  exploré). Type d'icône TEMPORAIRE : `IconeCheckDeTest`.
  **Contenu réel (2026-11-03)** : F9 `[SCONT]` (`sites_contenu.cpp`),
  survivants = composant `+0xA8`, trésor du gabarit `+0x118` ; table
  `outils\sites_contenu.tsv` (`outils\sites_contenu.py`, cumulatif),
  **VALIDÉE le 2026-11-03**. Quantités sans lien avec la difficulté (totaux de la table du
  scénario répartis au hasard) : répartition figée par `hkRepartirContenu`
  (`+0x18EF2F0`), **VALIDÉE le 2026-11-03**.

- **Technos** — `docs/chantiers/technos.md`. Icônes posées seulement
  pendant la construction des nœuds (aucune écriture permanente),
  `TECH_IMAGE` forcée pour les technos AP (validé) ; cases de l'atlas
  #1179 corrigées : important (0,2), filler (0,3), done (0,4), trap (0,5),
  progressive (0,6), vanilla (0,7) (validé). Non testés : rafraîchissement
  d'un nœud après une recherche, nom du nœud = nom du check
  (+ ` [Researched]`), pas de renvoi d'un check déjà envoyé. Sauvegarde
  locale **cumulative** (décision de l'auteur).
  **Paliers / arbres (2026-11-03)** : relevé F4 `[TECHNO]` (config
  `base + 0x4074430`) validé ; palier = prérequis `TierUnlockN`, un seul
  parent par techno ; table par scénario générée (`known_techs.h`,
  `outils\technos_table.tsv`, tableau dans le fichier du chantier). Technos
  de départ (Winterhome, On the Edge) exclues : `TechEstUnCheck`. Mod
  adapté, **VALIDÉ le 2026-11-03** (Winterhome, F7) ; table transmise à
  l'apworld.

- **Scénarios verrouillés** — `docs/chantiers/scenarios_verrouilles.md`.
  Verrou **VALIDÉ** (hook de `FUN_141283ea0`, 6 scénarios AP), déblocage par
  `ReceiveScenarioUnlock` ; items de test au démarrage : DEFAULT et
  REFUGEES (TEMPORAIRE, demande de l'auteur du 2026-11-03). Régression des
  icônes des lois **corrigée et VALIDÉE le 2026-11-03** : ce n'était pas le
  verrou mais une **course au démarrage** (atlas #2350 lu avant le hook
  `NtReadFile`) → substitutions posées juste après les captures, **avant
  tous les autres hooks** (à respecter pour tout nouveau hook). Texte
  `UNLOCK_SCENARIO_TEXT` effacé : **VALIDÉ le 2026-11-03**.

- **Difficulté et victoire** — `docs/chantiers/victoire.md`. Détection de
  la difficulté conforme ; `ScenarioUsesGroup` / `ScenarioVictoryId` lisent
  `g_run.scenario` (non testé). Hook de `EndGame` (`FUN_141301490`).
  **2026-11-04 (apworld)** : plus de checks de victoire ni de modes
  (Complete / General / Detail supprimés) ; toute victoire dans un scénario
  AP est un **événement du goal** (difficulté imposée, ironman sans effet ;
  plus de lecture des curseurs) : `SendScenarioVictory`,
  `mods\ap_victoires.txt`, goal = tous les scénarios du slot gagnés
  (**validé le 2026-11-04**, A New Home).
  Victoire / défaite de test (`VictoireDemanderTest`) : plus de touche
  depuis le 2026-11-04.

- **Difficulté imposée** — `docs/chantiers/difficulte.md`. Décisions de
  l'auteur (2026-11-03) : difficulté du YAML **imposée**, curseurs
  **figés**, Customize = toutes les lignes au niveau cible, 6 scénarios AP,
  **ironman libre**. `scenarios_ap.cpp` : hook de `FUN_141b2e8c0` (après
  chaque changement de scénario / bascule ironman : curseurs posés puis
  désactivés — immobiles, pas grisés, **validé**) et filet
  `DifficulteImposerAuLancement`, appelé par le hook d'ApplyConfig de
  `scenario.cpp` (un seul hook par fonction ; niveaux `+0x314` réécrits).
  L'ironman ne change rien à la victoire (auteur, 2026-11-04 :
  `PartieIronman` retiré de `victoire.cpp`). Log `[DIFF]`. Cible
  `g_difficulteCible` (Hard par défaut), réglée par le `slot_data`
  (`goal_difficulty`, `DifficulteReglerCible`). **VALIDÉ le 2026-11-03** (Refugees : partie lancée en Hard,
  victoire → checks General Easy / Medium / Hard).

- **Icône et couleur des messages** — `docs/chantiers/notifications.md`.
  Tous les messages AP restent **ouvrables (type 1)** (exigence de
  l'auteur). L'icône du cercle se pose sur l'enfant **`icon`** (minuscules)
  de `static_left_notification_circle` après l'ajout (validé) ; la couleur
  orange est reposée par le preset juste après l'ajout →
  `CouleursMessagesTick` (bleu-vert) : non testé.

- **apworld (Python, côté Archipelago)** — `docs/chantiers/apworld.md`.
  **Démarré le 2026-11-02.** C'est là que vit la logique (items, locations,
  règles, goal, options du YAML) ; le mod doit faire respecter en jeu les
  verrous qu'elle suppose. Fichier = référence AP + format de dictée +
  questions à trancher + journal des décisions dictées par l'auteur.
  **Logique dictée et apworld écrit le 2026-11-03** (`apworld/frostpunk/`,
  **générations testées** avec les sources d'Archipelago 0.6.8 ; commit
  `47a2eba` avec le mod branché sur les ids) : une région par quête goulot,
  goulot = tous les `Progressive Tree` progression de sa région
  (2026-11-04, remplace les 80 %), goal =
  victoire dans tous les scénarios choisis à la difficulté du YAML (forcée
  par le mod), items par scénario. Données générées par
  `apworld/outils/generer.py` → `data.py`, `apworld/ids.tsv` (ids
  **stables à vie**) et **`known_ap_ids.h`** (table des ids pour le mod).

- **Réseau Archipelago** — `docs/chantiers/reseau.md`. **Démarré le
  2026-11-04** (demande de l'auteur : dernière couche). Classe `ReseauAP`
  (`reseau_ap.h/.cpp`), thread dédié, n'appelle que `archipelago.h`.
  WebSocket **fait main** sur Winsock, **TLS par Schannel** pour `wss://`
  (décision de l'auteur, 2026-11-04 : archipelago.gg n'accepte que
  `wss://` ; sans préfixe, `wss://` puis `ws://` ; **validé**), JSON **nlohmann/json 3.11.3** (`nlohmann\json.hpp`). Session =
  `mods\ap_<graine>_<slot>\` (index des items, checks, slot_data, items
  reçus / livrés, victoires) ; dernière session relue au démarrage
  (`mods\ap_session.txt`). PrintJSON en message : items qui concernent le
  slot seulement. **VALIDÉ le 2026-11-04** (serveur local 0.6.7) :
  connexion, réception, envoi des checks, goal (`StatusUpdate 30`),
  reconnexion sans doublon (`index 0 … 0 nouveau(x)`), icônes d'après les
  scouts. Menu principal sans connexion : Scenario, Continue, Load et New
  game inactifs (`AppliquerVerrouConnexion`, menu_ap.cpp) : validé.
  Scouts : `LocationScouts` à la connexion, `<session>\scouts.txt`,
  transmis par `ApNoterScout` (archipelago.h) ; nom du check = `"<joueur> -
  <item>"` (`ApNomDuScout` ; pas de `|`, il coupe le texte en jeu), dans la
  **description** des technos seulement : les nœuds de l'arbre gardent le
  nom de la techno (décision de l'auteur, 2026-11-04) ; lois : nœuds du
  livre en vanilla, nom du check dans le panneau d'info (`hkInfoLoi`) ;
  icônes par `ApIconeDuCheck` (technos, sites ; lois
  par la session des bugs), **selon les drapeaux** de l'item même pour un
  autre joueur (décision de l'auteur, 2026-11-04 : piège > progression >
  useful > filler) : lois vues en jeu le 2026-11-04. Expéditeur des items
  gardé par réception (`ap_items_recus.txt` = `<id>\t<joueur>`) : popups
  « from <joueur> » (technos, lois, température, convois, pièges).

- **Version Steam** — `docs/chantiers/steam.md`. **2026-11-07** : exe
  Steam = autre compilation (2025), pas de SteamStub. 184 adresses
  appariées (fonctions identiques via `.pdata`, références exactes aux
  globaux, cas douteux vérifiés dans Ghidra), `versions_jeu.cpp`, atlas par
  hash de nom (`SubDecl.hashNom`), atlas des bâtiments Steam refait. Test
  de fumée Steam et Epic : hooks OK, menu construit. **Non testé en
  partie.** Bouton `CUSTOM_SCENARIO_BUTTON` (Steam, panneau `+0xF0`)
  toujours inactif (décision de l'auteur, 2026-11-07).

- **Dilemmes d'exploration** — `docs/chantiers/dilemmes_exploration.md`.
  **2026-11-10** : les 49 dilemmes à plusieurs choix des 6 scénarios ont
  un seul choix « Archipelago Prevail » = effets d'un choix du jeu, sans
  modification. Règle de l'auteur : **gain maximal** (Escort > Send >
  deux ressources > ressource de base > rien ; issues aléatoires permises,
  mort des éclaireurs déjà neutralisée), table validée. Tâche des
  expéditions = entier `+0x18` de `OrderStationingExpeditionToPerformTask`
  (15 noms, table `0x141ede5f0`). `dilemmes_exploration.cpp` : **non
  testé**. `TOUS_DEBLOQUES_TEMPORAIRE` (scenarios_ap.cpp) encore à `true`.

- **Big map (nouveau scénario, hors Archipelago)** — `docs/chantiers/big_map.md`.
  Branche `Big-map` (2026-11-04). Cartes = scènes binaires de `scenes.dat`
  faites à la main (aucune génération pour les villes). **Format de haut
  niveau décodé et validé hors jeu** (`outils\scenes.py`, 38 scènes lues
  à l'octet près) : entités = instances de gabarits (nom, GUID, parent,
  GUID du gabarit, type, calque) + propriétés en format « solide » de la
  réflexion. Relevé du schéma de la réflexion (`schema_reflexion.cpp` →
  `logs\schema_reflexion.tsv`) : registre des classes `+0x2A12DC8` (trié
  par nom ; classe = `PropertyManager` de 0xC0, nom `+0x60` ;
  `0x408B740` n'est que le global d'une classe), **validé le 2026-11-09**
  (2617 classes). **Propriétés des entités décodées** (`scenes.py lire
  <scène> -v`, `valider`) : type exact par le nom RTTI décoré de la vtable
  de chaque propriété (lu dans l'exe) ; **29 868 entités et 38 racines
  consommées à l'octet près** (validé hors jeu le 2026-11-09).
  **Écriture** (`Encodeur`, `Scene.ecrire`, `scenes.py allerretour`) : les
  38 scènes décodées puis réencodées sont identiques octet pour octet
  (validé le 2026-11-09). Suite : charger une scène modifiée en jeu.

### Constantes et touches de debug

**Toutes retirées le 2026-11-04** (demande de l'auteur) : `EXPEDITIONS_RAPIDES`
(scooting.cpp), `LOIS_SANS_DELAI` (lois.cpp), touches F3–F9 et F11
(version.cpp). (`SITES_TIRAGE_FIXE = true` est voulu, ce n'est pas du
debug.) Icônes et noms des checks : scouts du serveur (`ApIconeDuCheck`,
`ApNomDuScout`) ; `IconeCheckDeTest` / `IconeDuCheckDeTest` ne servent plus
que de repli sans scout (`ScenariosRecevoirItemsDeTest` supprimé le
2026-11-04 : les déblocages viennent du serveur). Valeurs par défaut
remplacées par le `slot_data` (`ApAppliquerSlotData`, archipelago.cpp,
appelé par le réseau ; `mods\ap_slot_data.txt` supprimé le 2026-11-04) :
`g_difficulteCible` (scenarios_ap.cpp, Hard), `g_scenariosGoal` (A New
Home seul), `g_heuresLivraison` (24 h).

### Approche abandonnée

Ajouter une **12e entrée** au conteneur de ressources du jeu : crash systématique
au chargement (l'économie sérialisée contient 12 ressources pour 11 déclarées).
Ne pas y revenir.

---

## 10. Émission d'une notification (séquence exacte)

Historique de l'icône et de la couleur : `docs/chantiers/notifications.md`.

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

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- When graphify-out/graph.json exists, run `graphify query "<question>"` before your first grep, search or source-file read for a codebase question, and put this rule in every subagent prompt that explores code. Read raw files only for lines the graph or an earlier read already located. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
- A committed graphify-out/graph.json is rewritten whole on every build, so a large diff to it in a pull request is expected. Do not flag it as churn or suggest skipping the graph rebuild or its hooks.
