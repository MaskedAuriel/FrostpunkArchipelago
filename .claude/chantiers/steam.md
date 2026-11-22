# Chantier : version Steam

## 2026-11-07 — Constat

Release Epic installée sur Frostpunk Steam : crash instantané. L'exe Steam est
une **autre compilation** :

| | Epic | Steam |
|---|---|---|
| Lien (TimeDateStamp) | 18/08/2021, `0x611CFD2D` | 08/10/2025, `0x68E665D9` |
| SizeOfImage | `0x045A5000` | `0x04508000` |
| Taille | 49 278 464 | 48 636 416 |
| `.text` | `0x1D7A64A` | `0x1CC686A` |
| DRM | — | **pas de SteamStub** (pas de `.bind`) |

Chemins d'assertion Steam : `d:\buildagent\work\frostpunk_steam_p4\bc_frostpunk_steam_source\…`.
Les adresses Epic tombent au milieu de fonctions Steam (ex. `+0xE27BF0` dans
`FUN_140e27ab0`) : MinHook écrit son saut en plein code → crash.

## Méthode (scripts dans `outils\portage\`)

1. `extraire.py` : adresses Epic présentes dans le code (hors commentaires)
   → `adresses_epic.tsv` (184 : 128 `.text`, 56 `.data`).
2. `empreintes.py` (capstone + pefile) : pour chaque fonction de `.pdata`,
   suite d'instructions normalisée (rip-relatif et adresses absolues
   masqués, petits immédiats / offsets de structure gardés). ~316 000
   entrées par exe.
3. `apparier.py` : paires exactes uniques (33 247), plus longue sous-suite
   croissante = ancres (33 222) ; fonction non identique → meilleure
   ressemblance (difflib) entre les deux ancres voisines.
4. `feuilles.py` : fonctions feuilles sans `.pdata` (8) par motif d'octets
   masqué autour de la position prédite.
5. Globaux : `globaux.py` / `fenetre.py` — références rip-relatives
   **exactes** depuis les fonctions identiques, alignées instruction par
   instruction. Objets de config référencés seulement par des fonctions de
   réflexion clones : base de l'objet + offset du champ.
6. `table.py` → `table_steam.inc` → `VERSION_STEAM[]` de `versions_jeu.cpp`.
   `correspondances_steam.tsv` garde méthode et score par adresse.
7. `convertir.py` : chaque littéral d'adresse du code enveloppé dans
   `Adr(...)`, `constexpr` → `static const` (`inline const` en en-tête).
   `verifier.py` : 0 adresse nue, 184/184 couvertes.

Vérifications Ghidra (exe Steam) :
- rafraîchissement du menu `0x1419C61C0` (score 0,53 : bannières FP2 /
  Alters / FP1886 ajoutées) : mêmes champs du panneau `+0xD0` Continue,
  `+0xD8` New game, `+0xE0` Load, `+0xE8` Scenario ;
- `ApplyConfig` `0x1419C9140` (0,68) : `+0x1E0`, `+0x1C8`, `+0x1D0`,
  niveau `+0x314`, pas `0x20` (filtrage par scénario ajouté) ;
- autres fonctions non identiques (0,90 à 0,98) : mêmes offsets de
  structure (`diffoffsets.py`), seules des constantes locales changent ;
- `0x408B740` (« PropertyManager ») = en fait l'enregistrement de
  réflexion de `FrostpunkCurrentScenarioParametersCheckExpression`
  (`FUN_14147c2d0`) → Steam `0x3FE8628`.
- Table des systèmes / annuaire : décalage constant `-0xA2DA0` sur toute la
  zone (`0x2C30128`..`0x2C31B88`) → même nombre d'emplacements, mêmes ids.

## Atlas

`textures-s3.idx` : `+0` hash du nom (u32), `+4` taille compressée. Les
numéros d'entrée changent (Epic 2680 / 3037 / 1179 / 2350 → Steam 2692 /
3055 / 1186 / 2361) → **substitutions repérées par hash de nom**
(`SubDecl.hashNom`, `OffsetDuBlob(hash)`).
- Ressources, recherche, bulles : blobs Steam **identiques** à Epic → mêmes
  `.gz`.
- Bâtiments (`0xB97BA46D`) : Steam ré-encodé + une icône nouvelle en
  (14, 5) ; colonne 15 (nos cases) libre. `atlas_steam.py` : blocs DXT que
  le mod change dans l'atlas Epic (32 766, colonne 15 lignes 0-5) recopiés
  dans l'atlas Steam (décompression .NET, zlib refuse la fin du flux) →
  `textures\ap_buildings_steam.gz` (7-Zip, 3 177 789 ≤ place 3 257 929).
  Vérifié visuellement (`dxt.py`).

## Code

- `versions_jeu.h/.cpp` : `VersionDuJeu()` (en-tête PE), `Adr(rvaEpic)`,
  `JournalVersionJeu()` (`[VERSION]`), `FichierAtlasVersion`. Exe inconnu :
  **aucun hook** (`PayloadThread`).
- Les adresses restent écrites en RVA Epic dans tout le code.
  **Toute nouvelle adresse doit entrer dans `VERSION_STEAM`** (sinon
  `Adr` renvoie 0 sur Steam, journal `[VERSION] ADRESSE ABSENTE`).

## Tests de fumée (Claude, 2026-11-07)

- Steam, release installée proprement (`game\*` copié) : `[VERSION] Steam`,
  tous les hooks OK, 4/4 atlas (entrées 2692 / 3055 / 1186 / 2361), menu
  principal et colonne Archipelago construits, jeu vivant après 45 s.
- Epic, build de dev, lancé par le lanceur Epic : `[VERSION] Epic`, menu,
  sauvegardes filtrées. (Lancé directement sans le lanceur, l'exe Epic
  s'arrête de lui-même avant de lire les archives : normal.)
- **Non testé** : connexion, partie, technos / lois / sites / groupes /
  dilemmes / température / victoire sur Steam ; tables de données (technos,
  lois, quêtes, sites) identiques entre les versions : à vérifier en jeu.
- Nouveauté du menu Steam : bouton `CUSTOM_SCENARIO_BUTTON`.

## 2026-11-07 — Bouton des scénarios personnalisés verrouillé

Décision de l'auteur : verrouillé. Init du panneau Steam `FUN_1419c5b80` :
`CONTINUE_BUTTON` +0xD0, `NEWGAME_BUTTON` +0xD8, `LOADGAME_BUTTON` +0xE0,
`SCENARIO_BUTTON` +0xE8, **`CUSTOM_SCENARIO_BUTTON` +0xF0**,
`ENDLESS_BUTTON` +0xF8, `DLC_BUTTON` +0x100, `sequel_panel` +0x110
(le rafraîchissement `FUN_1419c61c0` montre +0xF0 selon `DAT_142b90f72`).
`AppliquerVerrouConnexion` (menu_ap.cpp) : sur Steam seulement, +0xF0
toujours inactif. Test de fumée Steam (release) : journal
`[MENU] bouton CUSTOM_SCENARIO_BUTTON force inactif`, rien de KO. Rendu
à l'écran non regardé.
