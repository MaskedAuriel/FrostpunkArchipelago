# Chantier : apworld Frostpunk (côté Archipelago, Python)

Document de référence pour dicter la logique. Sources : documentation
officielle d'Archipelago (`docs/world api.md`, `docs/rule builder.md`,
`docs/options api.md`, `docs/apworld specification.md` du dépôt
`ArchipelagoMW/Archipelago`, branche `main`, lue le 2026-11-02).

---

## 1. Où vit quoi

| Couche | Rôle | Où |
|---|---|---|
| **apworld** (Python) | Déclare **ce qui existe** (items, locations), **la logique** (ce qu'il faut avoir pour atteindre chaque location), les options du YAML, la condition de victoire. Ne tourne **qu'à la génération** de la partie multiworld. | nouveau dossier `apworld/frostpunk/` |
| **serveur AP** | Reçoit les checks, envoie les items. Ne connaît **aucune** logique, juste la table « location X contient item Y pour joueur Z ». | Archipelago |
| **client** (le mod) | Transmet les checks et applique les items. Il doit **faire respecter en jeu** les verrous que la logique suppose. | `archipelago.cpp` + future classe réseau |

**Point essentiel.** La logique de l'apworld est une **promesse** faite au
générateur : « sans l'item X, le joueur ne peut pas atteindre la location
Y ». Elle n'est utile que si elle colle au jeu réel tel que le mod le
modifie :
- **Logique trop permissive** (on dit Y accessible alors que le jeu exige X) :
  le générateur peut cacher X derrière Y. Le joueur est **bloqué** pour de
  bon, c'est l'erreur grave.
- **Logique trop stricte** (on exige X alors que le jeu n'en a pas besoin) :
  pas de blocage. Le joueur peut faire des checks « hors logique ». C'est
  seulement moins intéressant.

En cas de doute, **exiger plus**.

---

## 2. Vocabulaire Archipelago

- **Item** : ce qu'on reçoit. Il a un nom, un id numérique unique dans le
  jeu, et une **classification** :
  - `progression` : peut débloquer une location. Seuls ces items comptent
    pour la logique.
  - `useful` : bon à avoir, ne débloque rien en logique.
  - `filler` : remplissage (ressources).
  - `trap` : piège (nos dilemmes pièges).
  - Variantes : `progression_skip_balancing` (progression exclue de
    l'équilibrage, pour les items nombreux à exiger en quantité),
    `progression_deprioritized` (progression que le générateur évite de
    placer sur les locations prioritaires), et
    `progression_deprioritized_skip_balancing`, qui cumule les deux.
- **Location** : un endroit qui contient un item, c'est-à-dire un **check**.
  Elle a un nom et un id unique. Type : `DEFAULT`, `PRIORITY` (reçoit de la
  progression de préférence) ou `EXCLUDED` (jamais de progression ni
  d'utile ; pour les checks pénibles ou longs).
- **Region** : un groupe de locations qui partagent un accès. Le départ est
  la région `Menu`.
- **Entrance** : un lien entre deux régions, avec une règle. En pratique :
  `Menu → A New Home` exige `Scenario Unlock A New Home`.
- **Rule** : une fonction de l'état (`state`) qui renvoie vrai ou faux. Elle
  se pose sur une location ou une entrance. Exemples : « a l'item X », « a
  3 exemplaires de Y », « a X ET (Y OU Z) ».
- **State** (`CollectionState`) : l'ensemble des items progression qu'on
  suppose possédés pendant la génération.
- **Event** : une pseudo-location sans id, qui contient un pseudo-item
  verrouillé. Elle sert à nommer un jalon logique (« Acte 2 atteint »,
  « Production d'acier ») et à le réutiliser dans d'autres règles. Elle
  n'est **jamais** envoyée au serveur.
- **Goal** (condition de victoire, `completion_condition`) : la règle qui
  dit quand le joueur a fini. Le client l'annonce au serveur par un
  `StatusUpdate` (côté réseau), pas par un check.
- **Sphères** : sphère 0 = ce qui est accessible sans rien ; sphère 1 = ce
  que débloquent les items trouvés en sphère 0 ; etc. Le spoiler log les
  liste.
- **Accessibility** (option du joueur) : `full` (toutes les locations
  doivent être atteignables) ou `minimal` (seule la victoire doit l'être).
- **Progression balancing** : avance les items progression des joueurs vers
  les premières sphères, pour qu'un joueur ne reste pas bloqué.
- **slot_data** : un dictionnaire envoyé au client à la connexion. Il porte
  les options utiles au mod (mode de victoire, scénarios actifs,
  quantités…).
- **Hint** : `!hint <item>` révèle où est un item. Les **groupes d'items**
  (`item_name_groups`) permettent `!hint Progressive Tree`, etc.

### Comment le générateur remplit (pourquoi la logique doit être juste)

Il fait un « remplissage supposé » (assumed fill). Il prend un item
progression, suppose qu'on a **tous les autres**, et le pose sur une
location encore atteignable sans lui. Il recommence jusqu'à épuisement.
Puis il met utiles, filler et pièges dans les trous. Conséquence : le
générateur **croit aveuglément** les règles. Il ne sait rien de Frostpunk.

---

## 3. Structure d'un apworld

```
apworld/frostpunk/
  archipelago.json   manifeste : "game", "minimum_ap_version", "world_version", "authors"
  __init__.py        classe FrostpunkWorld(World)
  items.py           table des items (nom -> id, classification, quantité)
  locations.py       table des locations (nom -> id, région)
  regions.py         régions et entrances
  rules.py           règles d'accès
  options.py         options du YAML
```

- Archive installable : un zip `frostpunk.apworld` (nom **en minuscules**),
  qui contient le dossier `frostpunk/`. Construit par le composant
  « Build APWorlds » du Launcher, quand on tourne depuis les sources. Il se
  dépose dans `lib/worlds/` de l'installation d'Archipelago.
- Imports **relatifs** dans le monde (`from .items import ...`), **absolus**
  pour le cœur d'AP (`from Options import Toggle`).
- **Ids** : entiers de 1 à 2^53−1, **stables à vie** (un id changé casse les
  parties en cours et le client). Convention prévue : une base (par exemple
  `base_id`) et des **plages par catégorie** (technos, lois, quêtes, sites,
  températures, victoires, scénarios, ressources, pièges). Le mod devra
  connaître la même table : on la générera dans un `.h` depuis le Python,
  comme `known_quests.h`.
- **Contrainte de remplissage** : le nombre d'items créés doit être
  **égal** au nombre de locations du joueur. On complète avec du filler
  (ressources) et des pièges.

### Étapes de génération (méthodes de `World`, dans l'ordre)

1. `generate_early` : lit les options, choisit les scénarios actifs.
2. `create_regions` : crée les régions, les locations et les entrances.
   Avec le rule builder, c'est **aussi** là qu'on pose les règles
   (`self.set_rule`).
3. `create_items` : remplit `multiworld.itempool`.
4. `set_rules` : règles d'accès (méthode classique) et condition de
   victoire.
5. `generate_basic`, `pre_fill`, `post_fill` : cas particuliers, par
   exemple placer d'office un item.
6. `fill_slot_data` : ce que le client recevra.

---

## 4. Écrire les règles

Deux styles existent ; on prendra le **rule builder**, plus lisible et
proche d'une dictée :

```python
from rule_builder.rules import Has, HasAll, HasAny, AtLeast, CanReachRegion, True_

# "Location X demande 2 Progressive Tree Steam, et Steelworks OU Coal Thumper"
self.set_rule(loc, Has("Progressive Tree Steam", 2)
                   & (Has("Steelworks") | Has("Coal Thumper")))
```

- `&` = ET, `|` = OU (jamais `and`/`or` Python).
- `Has(item, n)`, `HasAll(a, b, …)`, `HasAny(a, b, …)`, `HasGroup(groupe, n)`
  (n items quelconques d'un groupe), `AtLeast(n, règle1, règle2, …)` (au
  moins n des règles), `CanReachRegion("…")`, `CanReachLocation("…")`.
- `self.set_completion_rule(règle)` pour la victoire.

L'ancien style (lambdas : `set_rule(loc, lambda state: state.has("X",
player, 2))`) reste valable.

---

## 5. Inventaire Frostpunk existant (ce que le mod sait déjà faire)

Interface de `archipelago.h` :

| Côté | Fonction du mod | Item / location AP |
|---|---|---|
| Item | `ReceiveProgressive(tree)` | `Progressive Tree <arbre>` (technos) |
| Item | `ReceiveLawProgressive(tree)` | `Progressive Law <arbre>` (18 arbres) |
| Item | `ReceiveTemperatureRise(levels)` | `Temperature Rise` (piège ? à trancher) |
| Item | `ReceiveScenarioUnlock(nom)` | `Scenario Unlock <scénario>` (6) |
| Item | `ReceiveResource(nom, n)` | `<n> <ressource>` (filler) |
| Item | `DilemmeAPDemander("id")` | pièges de `DILEMMES_AP` (pas encore exposé dans `archipelago.h`) |
| Location | `SendResearchCheck(hash)` | une par techno (`known_techs.h`) |
| Location | `SendLawCheck(tree, rank)` | une par rang d'arbre de lois (règles de comptage dans `lois.md`) |
| Location | `SendQuestCheck` | fin de quête d'histoire (`known_quests.h`) |
| Location | `SendLocationCheck(id)` | sites d'exploration (`known_sites.h`, 211 sur la carte) |
| Location | montées de température retirées | une par montée (tableaux dans `temperature.md`) |
| Location | `SendVictoryCheck` | victoires (modes Complete / General / Detail) |

Les groupes de survivants (`groupes.cpp`) sont un **mode de livraison**, pas
un item à part.

---

## 6. Format de dictée

Pour aller vite et sans ambiguïté, dicter chaque règle sous l'une de ces
formes. Je traduis ensuite en Python.

```
LOCATION <nom ou famille>   : <règle>
REGION   <nom>              : <règle d'entrée>
EVENT    <nom du jalon>     : <règle>          (réutilisable ensuite par son nom)
GOAL                        : <règle>
ITEM     <nom>              : <classification>, <quantité dans le pool>
```

Règles :
- `item` ou `item ×n` ;
- `ET`, `OU`, parenthèses ;
- `n parmi (a, b, c)` ;
- `[jalon]` pour un event, `@région` pour « peut atteindre la région ».

Les familles évitent de dicter 300 lignes :

```
LOCATION Recherche, arbre Steam, rang k  : Progressive Tree Steam ×(k-1)
LOCATION Site <n'importe lequel>         : [Scouts] ET Progressive Tree Exploration ×1
EVENT    Scouts                          : Beacon OU ...
REGION   A New Home                      : Scenario Unlock A New Home
```

Je note chaque décision dans le **journal** en fin de fichier, avec la
date.

---

## 7. Questions à trancher

Pour l'auteur, dans cet ordre. Chacune change la structure.

1. **Granularité des scénarios.** Une région par scénario, ouverte par
   `Scenario Unlock <X>` ? Quels scénarios sont dans le multiworld (option
   YAML) ? Lequel est débloqué au départ ?
2. **Goal.** Victoire dans N scénarios ? Dans tous les scénarios choisis ?
   Une victoire précise ? Quel mode (Complete / General / Detail) ?
3. **Les items sont-ils globaux ou par scénario ?** Un
   `Progressive Tree Steam` vaut-il dans **tous** les scénarios, ou un par
   scénario (`A New Home - Progressive Tree Steam`) ? Même question pour
   les lois et `Temperature Rise`. Ça change le nombre d'items et la
   difficulté du remplissage.
4. **Ce que le jeu exige vraiment** pour chaque type de check, une fois le
   mod en place :
   - **technos** : il faut le rang précédent recherché (fait par le
     joueur) et des ressources (bois, acier, noyaux). Que faut-il recevoir
     en items AP pour pouvoir **produire** ces ressources ? Une techno
     recherchée sans son item débloque-t-elle la suivante ?
   - **lois** : délai entre deux signatures, prérequis de rang. Une loi
     signée sans son item débloque-t-elle la suivante ?
   - **sites** : il faut un poste d'éclaireurs (bâtiment de techno ? de
     loi ?) et des scouts. Distances : sites lointains = autre chose ?
   - **quêtes** : quels items sont nécessaires pour finir chaque quête
     (construire X, rechercher Y, atteindre l'acte Z) ?
   - **montées de température** : seulement survivre jusqu'au jour J ?
     Faut-il de quoi tenir le froid (générateur, chauffage) ?
   - **victoire** : ce qu'il faut pour survivre à la tempête ou à la fin du
     scénario.
5. **Économie de base.** Sans aucun item AP, que peut-on produire ? (Les
   ressources reçues en filler ne comptent **jamais** dans la logique.)
6. **Pièges.** Proportion ? `Temperature Rise` est-il un bonus (il rend un
   niveau retiré, donc il **réchauffe** ?) ou un piège ? À clarifier.
7. **Checks exclus ou prioritaires** : longs (fin de scénario) →
   `EXCLUDED` ? Importants → `PRIORITY` ?
8. **Options du YAML** : scénarios inclus, mode de victoire, difficulté
   minimale, inclure sites / lois / quêtes / températures (oui/non),
   proportion de pièges, quantité des ressources de filler.

---

## 8. Pièges classiques

- Une ressource **reçue** (filler) ne doit jamais apparaître dans une règle.
  Seule la **capacité à produire** compte : un bâtiment ou une techno reçus.
- Ne pas supposer que le joueur « peut grinder ». Si une location exige
  1000 d'acier, la règle doit exiger le moyen de produire de l'acier.
- Une règle qui dépend de la **difficulté** du scénario doit lire l'option
  correspondante (`OptionFilter` dans le rule builder, ou un `if` sur
  `self.options`).
- Un id ne se renumérote jamais. On ajoute en fin de plage.
- Sphère 0 trop petite (pas assez de checks atteignables sans rien) : la
  génération échoue avec peu de joueurs. Il faut **plusieurs** checks
  accessibles dès le départ.

---

## 9. Journal des décisions dictées

### 2026-11-02 — Q1 : granularité des scénarios

- **Correspondance des noms internes** (correction de l'auteur) :
  `DEFAULT` = A New Home, `TechHaven` = The Arks, `REFUGEES` = The
  Refugees, `WINTERHOME` = Fall of Winterhome, **`BUILDERS` = The Last
  Autumn**, `Reconquest` = On the Edge.
- **Principe** : la complétion de chaque quête est un **goulot**
  (chokepoint) pour son scénario. Le découpage s'appuie sur le relevé des
  quêtes (`known_quests.h`, `quetes_par_scenario.xlsx`).
- **a.** Pas une région par scénario : **une région par grosse quête, par
  scénario**.
- **b.** Les 6 scénarios sont possibles ; **le YAML choisit** lesquels sont
  dans le multiworld.
- **c.** Scénario de départ **fixe ou aléatoire**, au choix dans le YAML.

#### Découpage en régions (dicté le 2026-11-02)

Chaque scénario va de `Start` à `Finish` ; entre les deux, les **quêtes
goulots**. Une région s'ouvre quand la quête goulot **précédente est
finie** (confirmé, point b). Lecture : `Start` → (goulot 1 finie) →
région 2 → … → (dernier goulot fini) → `Finish`.

| Scénario | Goulots, dans l'ordre |
|---|---|
| A New Home | 004 FIND OTHER CITIES, 005 LONDONERS, 006 REFUGEES |
| The Arks | 001 FIND FACTORY PLANS, 004 PREPARE FOR THE STORM, 008 NEW MANCHESTER (- COAL) |
| The Refugees | 001 SAVE YOUR PEOPLE, 003 FACE THE LORDS, 004 NORMALIZE THE SITUATION (004a / 004b) |
| Fall of Winterhome | 001 RESTORE HOPE, 002 BROKEN GENERATOR, 003 PREPARE EVACUATION, 004A FINAL HOURS |
| The Last Autumn | 001 BASIC FACILITIES EXTENDED, 003 GENERATOR CONSTRUCTION SITE, 006 GENERATOR FINALIZATION |
| On the Edge | [1] Send first batch, [3] Food settlement, [7] Make Decision |

- **Quêtes parallèles = un seul check** (point c). Exemple : Refugees
  004a NORMALIZE THE SITUATION et 004b BUILD THE WALL. Le regroupement
  sera traité côté mod (l'une ou l'autre envoie le même check).

### 2026-11-02 — Q2 : goal

- **a.** Goal = **victoire dans tous les scénarios choisis** du YAML, **à la
  difficulté choisie** dans le YAML. Exemple : Refugees + A New Home, Hard.
- **b.** La difficulté du goal = la **difficulté générale** (Easy / Medium
  / Hard / Very Hard ; niveau général d'une difficulté personnalisée = le
  plus faible des lignes, cf. `victoire.md`), jamais le détail par ligne.
  Le mode des **checks de victoire** est une option du YAML ; pour le goal,
  seul compte le mode **Complete** au niveau général choisi. Lecture : la
  victoire est cumulative, donc gagner au-dessus de la difficulté choisie
  compte aussi.
- **c.** **Pas de difficulté minimale** en dehors de celle du goal.

### 2026-11-03 — Q3 : items globaux ou par scénario

- **Par scénario** : `Progressive Tree <arbre>`, `Progressive Law <arbre>`
  et `Temperature Rise` existent **en un exemplaire par scénario** (ex.
  `A New Home - Progressive Tree Steam`). Un item d'un scénario n'agit que
  dans ce scénario.
- Nombre de `Temperature Rise` par scénario = nombre de montées retirées de
  ce scénario, **à la difficulté choisie** (tableau de `temperature.md` :
  montées de fin de partie exclues, The Arks Hard / Frostbite et Last
  Autumn = 0, On the Edge = 8 quelle que soit la difficulté).
- **Filler commun** : une ressource reçue s'applique au **dernier scénario
  joué** (la partie en cours).
- Conséquence côté mod : `ReceiveProgressive`, `ReceiveLawProgressive` et
  `ReceiveTemperatureRise` devront savoir à quel scénario l'item est
  destiné (le stocker et ne l'appliquer que dans ce scénario).

### 2026-11-03 — Q4.1 : technos

- **a.** Dans un arbre progressif, la **recherche** du rang précédent suffit,
  sans son item. **Mais** une techno peut aussi être bloquée par le
  **palier** (tier) : il faut la techno de palier correspondante.
  Exemple de l'auteur : Charcoal Kiln est de palier 1 → il faut d'abord
  Tier 1 ; son évolution Charcoal Kiln Thermostat exige Charcoal Kiln,
  Tier 1 **et** Tier 2.
  Traduction proposée : `LOCATION techno T (arbre A, rang k, palier p) :
  @[A rang k−1] ET @[TierUnlock p]` (`@` = location atteignable, donc
  recherchée ; TierUnlock p implique déjà TierUnlock 1..p−1 par son arbre).
- **b.** **Pas de restriction** de ressources (bois, acier, noyaux) sur les
  technos.
- **c.** Technos accessibles **dès `Start`** ; l'auteur signalera les
  exceptions.

#### Données des technos (session « Technos : arbres et paliers », 2026-11-03)

Table par scénario **validée en jeu par l'auteur** :
`outils\technos_table.tsv` (générée par `outils\technos_table.py`, source
hash → arbre `outils\technos_arbres.tsv`). Colonnes : scénario, hash, nom
interne, nom court, onglet, **arbre** (= id de l'item progressif ; trous,
max 81), **rang** (0-based, propre au scénario), **palier** (N de
`0N_TierUnlockN`, 0 = aucun), **parent** (seule techno exigée par le jeu),
parent de départ, **condition d'affichage** (déblocage par l'histoire).
88 / 82 / 87 / 73 / 67 / 60 technos (DEFAULT, TechHaven, REFUGEES,
WINTERHOME, BUILDERS, Reconquest).
- Arbre 36 = `TierUnlock1..5`, présent dans les 6 scénarios (Winterhome :
  1–3 sont des technos de départ, seuls 4–5 restent).
- **Technos de départ** : ni check ni item (décision de l'auteur).
- **Condition d'affichage** : checks normaux, condition seulement notée
  (décision de l'auteur rapportée par l'autre session).
- Constat : **102** technos ont leur parent dans un **autre arbre** (ex.
  `RC Ability`, arbre 38 rang 0, parent `RepairStation` arbre 37), et 2 ont
  un rang > 0 **sans parent** (`GU GeneratorUpgrade2/3` de Last Autumn).
  La règle « rang k−1 » et la règle « parent du jeu » diffèrent donc.

#### Règle des technos (précisions de l'auteur, 2026-11-03)

- **Palier = item reçu** : il faut avoir **reçu** `<Scénario> -
  Progressive Tree Tier` (arbre 36) autant de fois que nécessaire pour
  atteindre `TierUnlock p`. Dans Winterhome, `TierUnlock1..3` sont des
  technos de départ : palier ≤ 3 → rien, palier 4 → ×1, palier 5 → ×2.
  Ailleurs : ×p.
- **Les deux contraintes d'arbre** s'appliquent : le **parent** du jeu
  **et** le rang précédent de l'arbre progressif (recherchés, pas reçus).
  Exemple de l'auteur : Coal Mining optimization exige Tier 2 **et** le
  premier niveau de l'arbre progressif de Coal Mining, qui est son parent.

```
LOCATION techno T (scénario S, arbre A, rang k, palier p, parent P) :
    @[T de S, arbre A, rang k−1]      (si k > 0)
  ET @[P]                              (si P existe et n'est pas une techno de départ)
  ET S - Progressive Tree Tier ×n(p)  (n(p) = p, moins les TierUnlock de départ)
  ET @[région de la condition d'affichage]   (si condition, voir ci-dessous)
```

- **Conditions d'affichage → région** :

| Scénario | Technos (condition) | Région (après le goulot) |
|---|---|---|
| The Arks | AutomatonScouts (`[TechHeaven] Quest3`) | après 001 FIND FACTORY PLANS |
| A New Home | Storm_research (`StormResearch`) | après 006 REFUGEES |
| Fall of Winterhome | RepairStation et ses améliorations (arbres 37–39) | après 002 BROKEN GENERATOR |
| The Last Autumn | onglet HeatTab (arbres 40–44) | après 006 GENERATOR FINALIZATION |
| On the Edge | Vault (`SteamCores`, arbres 73–77) | après [1] Send first batch |
| Fall of Winterhome | ExpeditionUpgradeWinterhome (`EvacuationUpgradeDescription`) | après 003 PREPARE EVACUATION |
| The Last Autumn | GU GeneratorUpgrade1/2/3 (`[Builder] Generator Upgrades`, arbre 53) | après 006 GENERATOR FINALIZATION |

### 2026-11-03 — Q4.2 : lois

- **a.** Même règle que les technos : un check de loi exige que la loi du
  **rang précédent** de son arbre **et** les **lois requises** par le jeu
  soient **signées** (location atteignable), sans leur item. **Pas de
  palier** (Tier) pour les lois.
- **b.** Les **lois de départ** (5 dans Winterhome) sont **exclues** (ni
  check ni item). Left / Right = un arbre complet, `Politics` (comme
  Order + Faith = `Purpose`), checks par rang dans la branche choisie
  (`lois.md`). Régions d'ouverture de Purpose (A New Home), Labour et
  Politics (Last Autumn) : à préciser.
- **c.** Le **délai** entre deux signatures ne compte pas en logique.
- **d.** Une **table des checks de lois par scénario** (sur le modèle de
  `technos_table.tsv`) est à générer par la session parente depuis le
  relevé F4.

#### Données des lois (session « Lois : table des checks par scénario », 2026-11-03)

Table **validée par l'auteur** : `outils\lois_table.tsv` (générée par
`outils\lois_table.py` depuis les lignes `[LOITAB]` du relevé F4 ; tableau
lisible en fin de `lois.md`). Colonnes : scénario, **arbre** (nom exact
côté mod, celui de `ReceiveLawProgressive` / `SendLawCheck` : `Purpose`,
`Politics`, ou nom interne de la 1re racine), **rang** (1..N), lois
(interne / affiché, ` | ` = exclusives), lois requises, **condition
d'ouverture de l'arbre**, condition des lois. Lignes finales `# exclues`.
- **Sémantique** : le check de rang r part à la **r-ième signature** dans
  l'arbre (poids compris), **quelle que soit la loi**. Signer ne demande
  aucun item ; `Progressive Law <arbre>` n'applique que l'effet.
- Ceremonial Funerals = 2 checks, **sauf On the Edge = 1** (Organic
  Fertiliser absent ; décision de l'auteur du 2026-11-03). Purpose = 11
  rangs, Politics = 10.
- Totaux : DEFAULT 29, TechHaven 16, REFUGEES 29, WINTERHOME 24, BUILDERS
  32, Reconquest 17 (**147**). Un item `<Scénario> - Progressive Law
  <arbre>` par rang.
- **Traduction en logique** : comme n'importe quelle loi compte, et qu'aucune
  signature n'exige d'item, `LOCATION loi (S, arbre A, rang r) : @[A rang
  r−1] ET @[région d'ouverture de A]`.

#### Ouverture des arbres de lois (dicté le 2026-11-03)

| Scénario | Arbre (condition) | Région |
|---|---|---|
| A New Home | Purpose (`IdeologyTree`) | après 005 LONDONERS |
| The Last Autumn | Labour (`SAFETY`) | après 001 BASIC FACILITIES EXTENDED |
| The Last Autumn | Politics (`STRIKE L` / `STRIKE R`) | après 003 GENERATOR CONSTRUCTION SITE |
| autres | arbres ouverts dès le départ | `Start` |

- Lecture « après le goulot fini » (prudente : exiger plus).

### 2026-11-03 — Q4.3 : sites d'exploration

- **a.** Envoyer une expédition exige le **Beacon** : item reçu
  `<Scénario> - Progressive Tree 7` ×1 (Beacon = arbre 7, rang 0) dans
  A New Home, The Arks, Refugees. Winterhome et On the Edge : Beacon
  techno de départ → rien. Last Autumn : **pas de Beacon** dans la table
  des technos → à préciser.
- **b.** **Pas** d'exigence de distance.
- **c.** Certains sites **n'apparaissent qu'après une quête** : à rattacher
  à leur région. Données absentes de `known_sites.h` → relevé confié à une
  session dédiée.
- Confirmé : les lois de départ de Winterhome sont exclues ; les lois de
  Purpose que Winterhome **retire en cours de partie** restent des checks
  normaux.

#### Données des sites (2026-11-03, validées par l'auteur)

- Table : `outils\sites_apparition.tsv` (générée par
  `outils\sites_apparition.py`), 211 sites `surCarte`. Colonnes :
  scenario | site (clé de `SITE_PAYLOADS`) | nom_affiche | condition
  (vide = visible au départ) | region (`Start` ou `apres <goulot>`).
- Régions : DEFAULT après 006 REFUGEES (12) ; TechHaven après 001 FIND
  FACTORY PLANS (21) ; REFUGEES après 001 SAVE YOUR PEOPLE (14), après
  003 FACE THE LORDS (8) ; WINTERHOME après 004A FINAL HOURS (2) ;
  BUILDERS après 003 GENERATOR CONSTRUCTION SITE (3), après 006 GENERATOR
  FINALIZATION (5) ; Reconquest après [3] Food settlement (31) ; Start 115
  (recompté dans le TSV ; le rapport de la session annonçait 125).
- Refugees : 3 **paires exclusives** (`[ref]Trail_Bend` / `-dead`,
  `[ref]Black_Spire` / `-dead`, `[ref]White_Pass` / `-dead`) : un seul
  apparaît → **un seul check par paire** (condition « PAIRE EXCLUSIVE »).
- Last Autumn : le rôle du Beacon est tenu par le **Forager Quarters**
  (techno `FF Foragers`, arbre 54, rang 0) → item `BUILDERS - Progressive
  Tree 54` ×1 pour envoyer une expédition.
- Last Autumn : variante de carte désormais **fixe** ; « 53. Goods
  Station » est sur la carte, « 49. EIC Train Station » ne l'est plus.

### 2026-11-03 — Q4.4 : quêtes

- **a.** **Toutes** les quêtes d'histoire sont des checks
  (`QueteCat::Histoire` de `known_quests.h`).
- **b.** **Aucun item** exigé par quête : une quête ne dépend que de sa
  région (`@[région]`). Ces quêtes demandent un traitement particulier
  côté mod : dans A New Home, on peut sauter 001 ACTIVATE GENERATOR,
  002 MEANS FOR SURVIVAL et 003 RESCUE SURVIVORS pour aller directement à
  la recherche de Winterhome (004 FIND OTHER CITIES) → le mod doit alors
  **envoyer les checks des quêtes 1, 2 et 3** (à faire côté mod).
- **c.** Les quêtes parallèles (ex. 004a / 004b de Refugees, [8] Helping
  NL / Taking NL population d'On the Edge) = **un seul check**.
- Confirmé : « aller directement à Winterhome » = 004 FIND OTHER CITIES.

### 2026-11-03 — Q4.5 : montées de température et victoire

- **a.** Les checks de montée **n'exigent aucun item**. Ils sont
  **répartis équitablement** entre les régions du scénario, dans l'ordre
  chronologique ; en cas de reste, les régions **du début** en reçoivent
  une de plus. Pour N montées et R régions : chaque région en a
  `N // R`, les `N % R` premières une de plus. Régions = `Start` +
  régions des goulots ; **rien dans `Finish`** (confirmé).
- **b.** Victoire (`Finish`) : **rien** de plus que la région du dernier
  goulot.
- **c.** Même règle pour **toutes les difficultés**.

### 2026-11-03 — Q5 : économie de base

- **a.** Sans aucun item AP, le joueur peut récolter de quoi faire les
  recherches, les quêtes et survivre un certain temps, dans **tous** les
  scénarios.
- **b.** Donc **aucune exigence économique** dans la logique (pas d'item
  « production » requis).
- **c.** Aucune exception par scénario.

### 2026-11-03 — Q6 : pièges et filler

- **a.** `Temperature Rise` = **progression** (le scénario redevient
  vanilla).
- **b.** Pièges : pour l'instant **un par ressource**, perte de **50** :
  `Wood Trap`, `Coal Trap`, `Steel Trap` (ids mod `trap_wood`,
  `trap_coal`, `trap_steel` dans `DILEMMES_AP`). Le piège de test
  `trap_coal_or_steel` est retiré. L'auteur fournira plus tard un
  ensemble d'événements.
- **c.** Proportion de pièges : **0 % par défaut**, option du YAML.
- **d.** Filler = les ressources des **sites d'exploration** qui ne
  contiennent **ni survivants ni noyaux de vapeur**. Rien d'autre.

### 2026-11-03 — Q7 : checks exclus ou prioritaires

- **a.** Option du YAML pour **exclure les arbres Purpose et Politics**
  (Left / Right) : les checks **restent**, mais en `EXCLUDED` (filler ou
  pièges seulement). Les items `Progressive Law Purpose / Politics`
  restent dans le pool.
- **b.** **Aucune** location `PRIORITY` pour l'instant.
- **c.** Les victoires **ne portent pas d'item** : ce sont des événements
  pour le goal, pas des locations.

### 2026-11-03 — Q8 : options du YAML

| Option | Valeurs | Défaut |
|---|---|---|
| `scenarios` | sous-ensemble des 6 scénarios | **A New Home** |
| `starting_scenario` | un scénario ou aléatoire | **A New Home** |
| `goal_difficulty` | Easy / Medium / Hard / Very Hard | **Medium** |
| `trap_percentage` | 0–100 % du filler | **0** |
| `exclude_purpose_politics` | oui / non | **non** |

- **b.** Option du mode des checks de victoire **retirée** : les victoires
  ne portent pas d'item, et le goal se juge en mode **Complete**.
- **c.** Pas d'option « inclure » : sites, lois, quêtes et montées sont
  **toujours tous inclus**.
- **d.** Le mod **bloque ou force** la création de la partie à la
  difficulté du YAML (traité dans une autre session, `difficulte-forcee`).
  Le nombre de `Temperature Rise` suit donc `goal_difficulty` :

  | Scénario | Easy | Medium | Hard | Very Hard |
  |---|---|---|---|---|
  | A New Home | 5 | 5 | 5 | 5 |
  | The Arks | 2 | 4 | 0 | 0 |
  | The Refugees | 1 | 3 | 3 | 3 |
  | Fall of Winterhome | 3 | 4 | 4 | 3 |
  | The Last Autumn | 0 | 0 | 0 | 0 |
  | On the Edge | 8 | 8 | 8 | 8 |

- Le **mode Detail** des difficultés est **à supprimer** (côté mod
  aussi) : seule la **difficulté principale** compte.

### 2026-11-03 — Régions des quêtes et nature des items

- Table des quêtes proposée, **validée** sauf : `[LondonerArc] Sign
  Evening pray law` et `Sign Public penance law` **retirées** (pas de
  check).
- Les technos ne sont **pas** des items de progression au sens
  Archipelago : leur fonctionnement est progressif, mais ce sont des items
  **importants** (`useful`).
- Répartition des items dans les régions (idée de l'auteur, sens à
  préciser) : technos = un palier par région, la techno d'unlock du palier
  dans le palier d'avant ; lois = les progressives de niveau 1 dans la
  1re région, de niveau 2 dans la 2e, etc. ; contenu d'un site = dans la
  région de son déblocage.
- **Sens : règle de logique** (pas de placement forcé). Régions numérotées
  **depuis 0** (`Start` = 0, goulot k fini → région k, `Finish` = N).
  Lecture retenue :
  - techno de palier p : région `min(max(p − 1, 0), N − 1)` (paliers 0–1
    à `Start`, … paliers 4–5 dans la dernière région) ; `TierUnlock N` :
    région du palier N − 1 ;
  - **Winterhome** (paliers 1–3 de départ) : paliers ≤ 3 à `Start`,
    palier 4 en région **N − 2**, palier 5 en région **N − 1** ;
    TierUnlock 4 à `Start`, TierUnlock 5 en région N − 2 ;
  - loi de rang r : région `min(ouverture + r − 1, N − 1)`, où ouverture =
    région d'ouverture de l'arbre **dans ce scénario** (Purpose : région 0
    dans Refugees, région 2 dans A New Home) ;
  - site : région de son apparition (`sites_apparition.tsv`) ;
  - classification : `progression` = `Progressive Tree Tier`, Beacon
    (arbre 7) / Forager Quarters (arbre 54 de Last Autumn), `Scenario
    Unlock` ; les autres technos, les lois et `Temperature Rise` =
    `useful`.

### 2026-11-03 — apworld écrit (v0.1.0)

- `apworld/frostpunk/` : `archipelago.json`, `__init__.py`, `options.py`,
  `items.py`, `locations.py`, `regions.py`, `rules.py`, `data.py`.
- `apworld/outils/generer.py` lit les tables du mod (`technos_table.tsv`,
  `lois_table.tsv`, `sites_apparition.tsv`, `known_sites.h`,
  `known_quests.h`) et écrit `data.py`, **`apworld/ids.tsv`** (ids
  **stables à vie** : une clé nouvelle prend le prochain id libre de sa
  plage, rien n'est jamais retiré) et **`known_ap_ids.h`** (table pour le
  mod : `AP_LOCATIONS`, `AP_ITEMS`). Les décisions qui ne sont dans
  aucune table (goulots, régions des quêtes, conditions d'affichage,
  ouverture des lois, montées) sont en tête du script.
- Ids : `7747000000 + plage × 100000 + n`. Locations : technos 1, lois 2,
  sites 3, quêtes 4, montées 5. Items : arbres 11, lois 12, montées 13,
  scénarios 14, contenu des sites 15, filler 16, pièges 17.
- Totaux : 886 locations, 377 items possibles. Par scénario (Medium) :
  A New Home 179 locations / 141 items, The Arks 143 / 125, Refugees
  157 / 141, Winterhome 131 / 114, Last Autumn 137 / 109, On the Edge
  139 / 107 ; le reste = filler de complément (`Wood / Coal / Steel / Raw
  Food Crate`).
- Pool : un `Progressive Tree` par techno, un `Progressive Law` par rang,
  `Temperature Rise` selon la difficulté, `Scenario Unlock` (celui de
  départ reçu d'office), contenu vanilla des sites (`<site> Supplies` :
  `useful` s'il contient des noyaux, sinon filler), complément, pièges
  (part du filler).
- Logique : les règles de technos (rang précédent, parent, condition
  d'affichage, palier) sont **cumulées** par le générateur en (région,
  paliers requis) ; les entrées de région = goulot précédent atteignable.
- **Testé le 2026-11-03** avec les sources d'Archipelago 0.6.8 (dans le
  scratchpad, Python 3.15 forcé : `ModuleUpdate` neutralisé, `test.general.
  setup_multiworld` + `distribute_items_restrictive`) : 4 jeux d'options ×
  5 graines, avec un 2e joueur Frostpunk, toutes les locations remplies et
  le goal atteignable. Bug corrigé : une option `Choice` ne peut pas
  s'appeler `random` (réservé) → `starting_scenario: random_included`.
- **Suppositions à vérifier** : quantités du filler de complément.

### 2026-11-03 — Règle des quêtes et contenu des sites

- **Quêtes** : une quête de la région k exige d'avoir **reçu 80 %** des
  items de technos **de la région k seulement** (valeur **fixe**, pas une
  option du YAML). Comptage par arbre : items reçus au-delà des technos des
  régions précédentes, plafonnés au nombre de technos de l'arbre dans la
  région k ; seuil = 80 % arrondi au-dessus. Les goulots étant des quêtes,
  l'entrée dans la région suivante en dépend aussi. Seuils (seuil / technos
  de la région, régions 0, 1, 2, 3[, 4]) : A New Home 17/21, 17/21,
  12/14, 26/32 ; The Arks 16/20, 16/19, 12/14, 24/29 ; Refugees 17/21,
  17/21, 12/14, 25/31 ; Winterhome 28/35, —, 3/3, 16/20, 12/15 ; Last
  Autumn 15/18, 8/10, 10/12, 22/27 ; On the Edge 12/14, 15/18, 9/11, 14/17.
- Conséquence : **tous** les `Progressive Tree` sont `progression` au sens
  Archipelago (une règle les compte) ; l'icône en jeu peut rester
  « important ».
- **Contenu des sites** : pris dans `outils\sites_contenu.tsv` (validé,
  session `sites-survivants`) : lignes `direct` + `bonus` ; les choix de
  dilemme sont ignorés. Survivants (`workers`, `engineers`, `children`,
  `automaton`), noyaux ou technologie → item `useful` ; ressources seules
  → filler. Clé de l'item dans `known_ap_ids.h` : `"wood 144;workers 27"`.
- Retesté dans Archipelago 0.6.8 : 20 générations, toutes réussies.
- **Correction de l'auteur (même jour)** : la règle ne vise **que les
  quêtes** (les vrais goulots), et compte **80 % des checks progressifs**
  de la région : technos **et lois**. Les `Progressive Law` passent donc
  aussi en `progression`. Seuils (seuil / checks progressifs de la région,
  régions 0, 1, 2, 3[, 4]) : A New Home 22/27, 21/26, 15/18, 37/46 ; The
  Arks 20/25, 19/23, 14/17, 27/33 ; Refugees 23/28, 22/27, 15/18, 35/43 ;
  Winterhome 33/41, 4/5, 5/6, 18/22, 19/23 ; Last Autumn 20/24, 16/20,
  15/18, 30/37 ; On the Edge 16/20, 19/23, 11/13, 17/21. Retesté : 20
  générations réussies.
- **Classification des arbres intégrée** (`outils\technos_classification.tsv`,
  validée par l'auteur, session `technos-classification`) : un arbre est
  `progression` s'il est parent d'un arbre du même scénario, plus Tier et
  les éclaireurs ; sinon `useful`. 74 arbres progression, 140 useful. La
  règle des quêtes ne compte **que** les items `progression` (arbres
  progression + lois) : un item useful n'est pas suivi par Archipelago.
  Seuils recalculés (régions 0, 1, 2, 3[, 4]) : A New Home 16/19, 10/12,
  9/11, 22/27 ; The Arks 14/17, 9/11, 8/10, 13/16 ; Refugees 16/20,
  11/13, 9/11, 20/25 ; Winterhome 13/16, 4/5, 4/5, 8/9, 9/11 ; Last
  Autumn 15/18, 13/16, 10/12, 16/20 ; On the Edge 13/16, 11/13, 5/6,
  10/12. Retesté : 20 générations réussies.
- **Pas de règle sur les items `useful`** (décision de l'auteur).
- Ressources propres à Last Autumn (`structuralprofiles`,
  `steelcomposite`, `steamexchanger`) dans un item commun reçu ailleurs :
  **converties** en ressources de base de leur fabrication, mêmes
  quantités (recettes relevées par la session `recettes-last-autumn` :
  `outils\recettes_last_autumn.tsv`, variante « cheaper »). **Devenu sans
  objet** avec l'option B (plus bas) : un `<site> Supplies` n'est livré
  que dans le scénario de son site, donc les ressources de Last Autumn ne
  sont reçues que dans Last Autumn ; table gardée, non intégrée.
  Quantités du filler de complément (30 / 30 /
  15 / 15) **validées**. Branchement côté mod confié à la session
  `mod-apworld`.
- **À faire côté mod** : envoyer les checks par id (`known_ap_ids.h`) ;
  `Receive*` par scénario ; quêtes 001–003 d'A New Home envoyées si on
  saute à 004 ; un seul check par quêtes parallèles et par paire de
  sites exclusive ; mode Detail des victoires à supprimer ; survivants
  des sites (le contenu relevé ne les voit pas : 105 sites « (rien) »).

### 2026-11-03 — Mod : point 1, checks et items par id (non testé)

- `archipelago.cpp` inclut `known_ap_ids.h` (seul fichier à l'inclure).
  Chaque `Send*` cherche sa location dans le **scénario en cours**
  (`ScenarioVictoryId()`) : techno = hash `%016llX`, loi = `<arbre>|<rang>`
  (rang 1..n de `Rang`), site = nom interne (`SendLocationCheck(id, site)`),
  quête = un des noms de la clé (`|`). Trouvée → `[AP CHECK] location <id>
  "<nom>"` (TODO réseau) + popup ; absente → journal « aucune location
  AP », **ni envoi ni popup** (ex. quêtes exclues).
- `ReceiveItem(id, joueur)` : arbre → `ReceiveProgressive`, loi →
  `ReceiveLawProgressive`, montée → `ReceiveTemperatureRise(1)`, scénario →
  `ReceiveScenarioUnlock`, site / filler → une `ReceiveResource` par
  ressource (survivants journalisés, pas livrés : point 3), piège →
  `DilemmeAPDemander`. Item d'un autre scénario : ignoré (point 2).
- Tests passés par les ids : F5 (loi), F7 (arbre), F8 (montée).
- **Test de l'auteur (Last Autumn)** : F7 → `item 7748100175 "The Last
  Autumn - Progressive Tree RD Docksv1"` → arbre 61 reçu (**validé**) ;
  F8 → « aucun item Temperature Rise » (attendu : 0 montée dans ce
  scénario) ; F5 → aucune loi signée. Pas encore vus : envoi d'un check
  (recherche, loi, quête, site) avec son id.
- **2e test (A New Home)** : check de loi `location 7747200012 "A New
  Home - Cemetery Law 1"`, F8 → `item 7748300001`, F5 → `item
  7748200002` : **point 1 validé** (l'envoi d'une recherche reste à voir).

### 2026-11-03 — Mod : point 2, items par scénario (non testé)

- Arbres, lois et montées (items d'**un** scénario) : `ReceiveItem` les
  **note** dans `mods\ap_items_recus.txt` (une ligne `<id>` par réception,
  tous scénarios), quel que soit le scénario en cours.
- `ApItemsPompe` (thread du jeu, après `LoisAPPompe` / `TemperaturePompe`) :
  pour la partie en cours (GameId lu sur le système de scénario, égal à
  `g_run.gameId`), compare les réceptions de son scénario au compte de
  chaque module (`TechProgressivesRecues`, `LoisAPItemsRecus`,
  `TemperatureItemsRecus`, déjà sauvegardés par GameId) et applique la
  différence. Changement de partie → rattrapage **silencieux** (une popup
  « Archipelago items ») ; ensuite popups normales. Une nouvelle partie du
  scénario reçoit tout ce qui a déjà été reçu.
- **Bonus de palier** (1 `Tier` toutes les 5 progressives) **désactivé**
  (`BONUS_ACTIF = false`) : les items `Progressive Tree Tier` sont dans le
  pool de l'apworld.
- Maj+F7 = item d'arbre d'un **autre** scénario (test).
- TODO réseau : un fichier par graine / slot, dédoublonné par index d'item.
- **Test de l'auteur (2026-11-03) : VALIDÉ.** F7 dans A New Home →
  `1 recu(s) pour "DEFAULT"` puis arbre 35 appliqué avec popup ; Maj+F7 →
  `The Last Autumn - Progressive Tree Tier` noté pour BUILDERS, rien
  appliqué ; nouvelle partie de Last Autumn → `[AP ITEMS] partie BUILDERS :
  1 item(s) deja recu(s) appliques` (TierUnlock1) ; rechargement de cette
  partie → technos relues, aucune réapplication en double.

### 2026-11-03 — Mod : point 3, items communs (non testé)

- **Décision de l'auteur** : **un groupe AP par item** (contenu de site et
  filler), durée du trajet = **réglage entier en heures** (slot_data, à
  ajouter à l'apworld ; 1 h par défaut, `ApReglerHeuresLivraison`).
- Contenu, filler et pièges notés dans `ap_items_recus.txt` ; livrés **une
  fois** par `ApItemsPompe` dans la partie en cours (n'importe quel
  scénario), un par passage, nouvel essai 5 s après un échec ; livraisons
  comptées dans `mods\ap_items_livres.txt`. Un item reçu hors partie attend
  la prochaine partie prête.
- Groupe : `SurvivorsFromExpeditionSite`, nom = nom de l'item AP, ressources
  en cargaison, **une `SurvivorsEntry` par classe** (`GroupeAP::classes`) :
  classe sociale et statistiques copiées d'un site de la carte
  (`SitesEntreeSurvivants` : composant `+0xA8`, trésor `+0x70`, survivants
  aléatoires `+0xD8`), cache par nom ; classe introuvable → ses survivants
  rejoignent l'entrée 0. `technology` ignorée. Pièges : `DilemmeAPDemander`.
- F3 = item commun de test (Lost Expedition = 3 classes, Children's
  Hideout, Bridge to Winterhome = ressources + automate, Wood Crate, Wood
  Trap), à tour de rôle.
- Limite : un groupe créé puis perdu par le rechargement d'une sauvegarde
  antérieure n'est pas relivré.
- **Test de l'auteur** : 5 groupes partis (Lost Expedition, Children's
  Hideout, Bridge to Winterhome, Wood Crate) + bulle du piège. Mais sur
  cette carte (31 sites), seule la classe `workers` existe dans les sites :
  `children`, `engineers`, `automaton` introuvables → Children's Hideout
  arrive **vide**, Lost Expedition arrive avec 36 ouvriers (enfants et
  ingénieurs reportés), l'automate est perdu.
- **Correctif (non testé)** : repli sur les collections globales de
  `FrostpunkPopulationConfig` (ctor `FUN_141440690`, Ghidra) : classes
  sociales `base + 0x4072078`, statistiques initiales `base + 0x4072140`
  (tableau supposé à `+0x20`, compte `+0x28`) ; entrée retenue par nom
  (statistiques : 5 premières lettres), lien vérifié par la recherche par
  lien du jeu (`vtable + 0x88`) ; journal `[SCONT] collection …` (liste des
  noms si absent, octets bruts si tableau illisible).

### 2026-11-03 — Décision de l'auteur : contenus de site liés à leur scénario (option B)

- Il y avait eu **incompréhension** : un item `<site> Supplies` n'est **pas**
  commun. Il appartient au **scénario de son site** et n'est livré que dans
  ce scénario (Last Autumn ne doit pas recevoir d'enfants d'A New Home).
- **apworld à changer** : `scn` = `origine` pour les items de contenu de
  site (au lieu de `None`) ; dans `known_ap_ids.h`, `AP_ITEMS.scenario`
  renseigné pour `ApItemGenre::Site`. Les items de site restent dans le pool
  de leur scénario (régions / logique comme les autres items du scénario).
  Conséquence : la conversion des ressources propres à Last Autumn dans un
  item commun devient inutile pour les sites. Filler et pièges restent
  communs (`None`).
- **Mod (non testé)** : contenu de site avec scénario → livré seulement
  dans une partie de ce scénario, **dans chaque partie** (comme les
  technos), livraisons comptées par partie (`mods\ap_items_livres_<GameId>.txt`) ;
  filler / pièges inchangés (une fois, `ap_items_livres.txt`). Tant que
  `known_ap_ids.h` n'est pas régénéré, les sites restent traités en communs.
- F3 : un contenu de site du scénario en cours avec survivants, un sans,
  Wood Crate, Wood Trap (avant régénération : Lost Expedition / Crash Site).

### 2026-11-03 — apworld : option B appliquée, option `delivery_hours`

- `generer.py` : les items `<site> Supplies` ont `scn` = scénario du site
  (plus de `None` ni de champ `origine` distinct) ; `known_ap_ids.h`
  régénéré : `AP_ITEMS.scenario` renseigné pour les 132 `ApItemGenre::Site`
  (DEFAULT 32, TechHaven 23, REFUGEES 28, WINTERHOME 15, BUILDERS 10,
  Reconquest 24). Filler et pièges restent `nullptr`.
- **Ids inchangés** : la clé était déjà `item_site|<scénario>|<site>`, donc
  un même site dans plusieurs scénarios avait déjà des ids distincts ;
  `ids.tsv` identique octet pour octet après régénération.
- Pool inchangé : il se construisait déjà par `origine` (= scénario du
  site) ; classification useful / filler inchangée ; totaux par scénario
  inchangés (886 locations, 406 items). Les ressources propres à Last
  Autumn (`structuralprofiles`, `steelcomposite`, `steamexchanger`)
  n'apparaissent que dans 4 items de BUILDERS : aucune conversion à
  brancher.
- **Décision de l'auteur** : noms des items de site **préfixés par le
  scénario** (`The Arks - Supply Depot Supplies`, au lieu de
  `Supply Depot Supplies (2)`), ids inchangés. Seul suffixe restant : les
  trois sites distincts `Lords' Camp` de The Refugees (`… (2)`, `… (3)`).
  Le mod nomme le groupe de livraison d'après ce nom.
- Option **`delivery_hours`** (`DeliveryHours`, `Range` 1 à 24, défaut 1 ;
  bornes à valider par l'auteur) : durée du trajet du groupe de livraison
  en heures de jeu, transmise dans `slot_data["delivery_hours"]` (le mod :
  `ApReglerHeuresLivraison`).
- Tests (Archipelago 0.6.8, 5 jeux d'options × 5 graines, dont tous les
  scénarios en hard avec 20 % de pièges et `delivery_hours: 24`) : toutes
  les locations remplies, goal atteignable, chaque item de site du pool
  appartient à un scénario choisi, `slot_data` conforme.
- Partie apworld confiée à la session `apworld-sites-scenario`.
- **Décision de l'auteur** : `delivery_hours` **24 h par défaut** (« Il
  faut une valeur par défaut à 24h ») ; bornes 1 à 24 gardées. Données
  régénérées (ids inchangés), tests relancés (défaut 24, et 1 / 3 / 6
  explicites) : tous OK.

### 2026-11-03 — Mod : point 4, checks de montée (non testé)

- `NoterChangement` (temperature.cpp, hook du recalcul) : à chaque montée
  retirée (une fois par date, cumulatif par partie), rang k = 1 + nombre
  de montées retirées plus anciennes → `SendTemperatureCheck(k)` →
  location `<Scénario> - Temperature Rise k` (popup « Temperature rise #k
  held back »).
- **Question ouverte** : plusieurs montées valent **2 niveaux** (A New Home
  J23 −60→−40 et J27 −70→−50, Refugees Medium J13 −50→−30, Winterhome
  Medium J10 −50→−30 et J22 −60→−40, On the Edge J14 −40→−20), alors
  qu'un item `Temperature Rise` rend **1 niveau** : recevoir tous les items
  ne rend pas la chronologie vanilla (Q6a).
- **Réponse de l'auteur** : pas de complication. Les montées du jeu sont
  retirées (déjà fait), un item = +1 niveau.
- **Précision de l'auteur (même jour)** : autant de checks que de
  **niveaux** retirés, pour que tous les items rendent le niveau normal
  (« si 5 montées, mais augmentation de 70°, on doit avoir 7 montées »).
  Mod : une montée de N niveaux envoie les checks k+1..k+N (k = niveaux des
  montées retirées plus anciennes). apworld (session
  `apworld-sites-scenario`) : locations et items `Temperature Rise` comptés
  en niveaux ; mon calcul (Easy / Medium / Hard / Very Hard) : A New Home
  6/7/7/7, The Arks 2/5/0/0, Refugees 1/4/3/3, Winterhome 4/6/5/5, Last
  Autumn 0, On the Edge 9, à vérifier par la session.
- Durée de livraison des groupes : **24 h par défaut** (auteur), dans le mod
  (`g_heuresLivraison`) et demandé à la session `apworld-sites-scenario`.

### 2026-11-03 — apworld : montées de température comptées en niveaux

- `generer.py`, `MONTEES` : un check et un item `Temperature Rise` par
  **niveau** retiré (décision de l'auteur). Recalculé depuis le tableau des
  montées de `docs/chantiers/temperature.md` (fin de partie exclue, +1 de
  Storm preparation pour A New Home) : **identique** au calcul de la
  session mod. Easy / Medium / Hard / Very Hard : A New Home 6/7/7/7
  (avant 5), The Arks 2/5/0/0, The Refugees 1/4/3/3, Fall of Winterhome
  4/6/5/5, The Last Autumn 0, On the Edge 9 (avant 8).
- Répartition inchangée (`regions_montees` : équirépartition dans l'ordre,
  le reste aux premières régions).
- Ids : aucun renuméroté ; 7 nouvelles locations : `Temperature Rise 6`
  et `7` (A New Home), `5` (The Arks), `4` (The Refugees), `5` et `6`
  (Fall of Winterhome), `9` (On the Edge), ids 7747500025 à 7747500031.
  Total : 893 locations (au lieu de 886), 406 items.
- Tests (Archipelago 0.6.8, 5 jeux d'options × 5 graines) : tous OK ; ex.
  tous les scénarios en Hard = 24 items `Temperature Rise`.

### 2026-11-03 — Mod : point 5, quêtes (non testé)

- `quetes.cpp`, `EnvoyerCheckQuete` : un check par partie **et par location
  AP** (`ApQueteLocation`) : la 2e quête d'une paire parallèle (Refugees
  004a / 004b, On the Edge [0] / [1] et leur [timeout], [8] Helping NL /
  Taking NL population) n'envoie rien (« deja envoye par la quete
  parallele »). Quête sans location (communes, `[LondonerArc]`,
  `[ENDGAME] 004`, `[Builders] 007`, `002 DOCK INFRASTRUCTURE EXTENDED`) :
  rien envoyé, rien noté.
- A New Home : au **succès** de `004 FIND OTHER CITIES`, checks de 001, 002
  et 003 envoyés d'abord s'ils ne sont pas déjà partis (choix du succès
  plutôt que du lancement : 004 peut démarrer au succès de 002 en vanilla).

### 2026-11-04 — Mod : point 6, paires de sites exclusives (non testé)

- `ApCleDeSite` (archipelago.cpp) : clé de location d'un site = nom interne
  sans le suffixe `-dead` (Refugees : `[ref]Trail_Bend`, `[ref]Black_Spire`,
  `[ref]White_Pass` et leur `-dead`). Utilisée par `SendLocationCheck` et
  par `PayloadDuSite` (scooting.cpp) : les deux sites d'une paire donnent le
  même contenu et la même location, donc un seul check.
- **Constat** : le scouting ne porte encore des checks que sur les 4 sites
  de test de `SITE_PAYLOADS` (ids de test 1001–1004, descriptions
  `ApChargerChecksDeTest`). Faire de **tous** les sites de `AP_LOCATIONS`
  du scénario des checks = changement du scouting à décider avec l'auteur.
- **Tous les sites deviennent des checks** (accord de l'auteur, 2026-11-04,
  non testé) : `SITE_PAYLOADS` n'est plus la table des 4 sites de test, mais
  toutes les locations de site de `known_ap_ids.h`, construite au démarrage
  par `ScootingPayloadsInit` (version.cpp, après `ApChargerChecksDeTest`).
  Id local stable = 100000 + numéro de la location AP (gardé dans
  `ap_pending_<GameId>.txt` ; les anciens ids 1001–1004 sont ignorés au
  rechargement). Description (survol) = « [AP] Check: <nom de la
  location> » en attendant le serveur ; plus de progressive simulée.
- **Test de l'auteur (2026-11-04, Last Autumn) : VALIDÉ.** 208 sites AP au
  démarrage ; un scout ramasse 3 locations (Reindeer Tracks, Hunting
  Grounds, Nesting Place), les envoie au retour en ville
  (`location 7747300133..135`), puis Bald Hill (`7747300151`). Persistance
  des scouts en attente OK. Paires `-dead` de Refugees pas encore vues.

### 2026-11-04 — Mod : point 7, victoire = événement du goal (non testé)

- `victoire.cpp` : modes Complete / General / Detail **supprimés** (code
  commenté du Detail retiré, `MODE_TEST`, `VictoireMode`, `ModeVictoire`,
  `SendVictoryCheck` retirés). Une victoire dans un scénario AP compte si
  le niveau général (le plus faible des lignes ; ironman = au moins la
  cible) atteint `DifficulteCible()` ; sinon popup « Victory not counted ».
- `SendScenarioVictory` (archipelago.cpp) : victoire notée une fois par
  scénario dans `mods\ap_victoires.txt` ; popup « Scenario completed (n/m
  scenarios) » ; quand tous les scénarios du goal sont gagnés : « Goal
  reached » (TODO réseau : StatusUpdate CLIENT_GOAL). Scénarios du goal :
  `ApReglerScenariosGoal` (slot_data, point 8), A New Home seul par défaut.
- **Test de l'auteur (2026-11-04) : VALIDÉ.** Maj+F9 dans A New Home →
  `victoire "DEFAULT" : 1/1`, `GOAL ATTEINT` ; victoire de Last Autumn
  notée sans effet sur le goal (liste par défaut = A New Home).
- **Correction de l'auteur** : la difficulté est forcée sur celle choisie et
  l'ironman ne change rien → plus aucune lecture de niveau ni règle ironman :
  toute victoire dans un scénario AP compte. `victoire.cpp` réécrit sans la
  lecture des curseurs (`LignesDuScenario`, `PartieIronman`… retirés).

### 2026-11-04 — Mod : point 8, slot_data (non testé)

- `ApAppliquerSlotData(ApSlotData)` (archipelago.cpp), à appeler par la
  future classe réseau : `scenarios` → `ApReglerScenariosGoal` (noms
  internes connus seulement) ; `goal_difficulty` (`easy` … `very_hard`) →
  `DifficulteReglerCible` (scenarios_ap.cpp : `DIFFICULTE_TEST` devient la
  variable `g_difficulteCible`, Hard par défaut) ; `delivery_hours` →
  `ApReglerHeuresLivraison` ; `starting_scenario` (son déblocage arrive
  comme item) et `exclude_purpose_politics` (génération seulement) : notés.
- Test sans réseau : `mods\ap_slot_data.txt` lu au démarrage s'il existe
  (`ApChargerSlotDataDeTest`, lignes `cle=valeur`).
- **Arrivées échelonnées (demande de l'auteur, 2026-11-04, non testé)** :
  le délai n'est plus simultané mais cumulatif. Arrivée d'un nouveau groupe
  = arrivée du dernier groupe AP encore en route + `delivery_hours`
  (`GroupesHeuresAvantDerniereArrivee`, groupes.cpp : date d'arrivée du
  `LinearMovementComponent` − date courante ; durée pas encore posée →
  celle prévue). Ex. 24 h, un groupe à 8 h → 32 h, puis 56 h. Journal
  `[AP ITEMS] … arrivee dans X h = Y h d'attente + 24 h`.
- **Test de l'auteur (2026-11-04, A New Home) : arrivées échelonnées
  VALIDÉES** (24, 48, 72, 96, 120, 144 h, puis 191,9 h). Survivants : enfants
  et automate trouvés dans les sites de la carte ; **collection globale des
  classes lue correctement** (tableau `+0x20`, compte `+0x28` confirmés) :
  Workers, [Builders] Convicts, Automaton, Specialists, [Reconquest]
  Administrators, Children. Les ingénieurs n'étaient pas trouvés : leur nom
  interne est **`Specialists`** → correspondance apworld → jeu ajoutée dans
  `SitesEntreeSurvivants` (non testé).

### 2026-11-04 — Tests grandeur nature : règle des goulots revue

- **Problème remonté** (graine 58501539204423774757, A New Home Hard,
  Purpose et Politics exclus) : `Progressive Law Purpose` en sphère 1 (sur
  `TierUnlock1`), alors que l'arbre Purpose n'ouvre qu'après 005 LONDONERS.
- **Cause** : la logique contraint les **locations**, jamais l'endroit où
  tombe un item. L'item était `progression` et la règle des 80 % le
  comptait pour la région 2 (rang 1 de Purpose), d'où sa présence dans le
  playthrough. Le placement ne contredisait aucune règle.
- **Décision de l'auteur** : remplacer la règle des 80 % par : **la quête
  goulot** de la région k exige d'avoir reçu **tous** (100 %) les items
  `Progressive Tree` **classés `progression`** de la région k (même
  rattachement qu'avant : région du check de chaque rang). Les **lois ne
  comptent plus**, mais restent `progression`. Les **autres quêtes**
  n'ont plus d'exigence (seulement leur région). Pas de contrainte de
  placement des items Purpose / Politics.
- **Intention de l'auteur** : centrer le passage des goulots sur ces
  technos (arbres `progression`) ; toute retouche future de la règle doit
  garder ce cap.
- `generer.py` : `QUETE_POURCENT_PROGRESSIFS = 100`, lois retirées du
  comptage ; `rules.py` : exigence posée sur les goulots seulement
  (`scenario["goulots"]`). Ids, `known_ap_ids.h` inchangés.
- Seuils (items requis par goulot, régions 0, 1, 2[, 3]) : A New Home 13,
  7, 7 ; The Arks 12, 7, 7 ; Refugees 13, 7, 7 ; Winterhome 10, —, 2, 7
  (aucun arbre progression en région 1) ; Last Autumn 12, 6, 6 ; On the
  Edge 10, 8, 4. La dernière région n'a pas de goulot (victoire sans règle).
- Tests (Archipelago 0.6.8) : banc habituel 5 jeux × 5 graines (2 joueurs)
  et 70 générations **solo** (options de l'auteur + chaque scénario seul en
  Hard) : toutes réussies.
- `frostpunk.apworld` reconstruit dans
  `C:\Users\Simon\Desktop\Archipelago\custom_worlds` (commit `54dc5b8`).

### 2026-11-04 — Tests grandeur nature : description de la Sawmill = « RW Sawmill » (mod)

- **Problème remonté** (graine 46672752910410918624, A New Home) : la
  description de la recherche de base **Sawmill** affiche « RW Sawmill »
  au lieu du check (`A New Home - Sawmill` porte `A New Home - Progressive
  Tree Tier`).
- **Cause (mod, pas la logique)** : `[TECHDESC] cle "Technologies/Sawmill/
  Name" -> techno 9B8662D8557103EF` = `[Builders] RW 0_Sawmill` de Last
  Autumn. Le tableau des technos du jeu contient aussi des technos d'autres
  scénarios (172 dans A New Home), qui **partagent la clé de localisation**
  ; `LookupLocKey` (tech_state.cpp) gardait la **première** trouvée. Sans
  scout pour cette techno hors scénario → repli sur le nom court. Même cas :
  `Technologies/SawmillUpgradeRange` → `9222D9DF836D090A` (RW Sawmill
  Upgrade - range).
- **Correctif (compilé, déployé, non testé)** : `BuildLocKeyTable` ne garde
  qu'une techno par clé, **celle du scénario en cours** (groupe de
  `known_techs.h`, `TechDuScenario`) si elle existe ; table reconstruite
  aussi quand le scénario change. Journal : `[LOC] cle "…" : techno …
  (hors scenario) remplacee par …` et le bilan `[LOC] table construite
  (<scénario>) : … cle(s) partagee(s), … remplacee(s)`.
- **Vu en jeu (2026-11-04)** : `[LOC] table construite (DEFAULT) : 462 cles
  pour 172 technos, 45 cle(s) partagee(s), 22 remplacee(s)` (Sawmill, Steam
  Sawmill, Boilers, Infirmary, Kiln…).

### 2026-11-04 — Tests grandeur nature : lois non exclusives au même check (mod)

- **Problème remonté** : Duelling Law et Public House (arbre Fightclub)
  montrent le même check.
- **Cause** : le mod envoyait le check de rang r à la **r-ième signature**
  de l'arbre (et l'affichage suivait), donc deux lois sœurs non exclusives
  montraient le même « prochain » check.
- **Décision de l'auteur** : deux lois **non exclusives** ont toujours des
  checks **différents** ; seules les lois exclusives (paires, lois
  exclusives par leurs parents) partagent un check.
- **Correctif (mod, compilé, déployé, non testé)** : chaque loi a un **rang
  fixe**, celui de `outils\lois_table.tsv` (déjà la référence de
  l'apworld : Fightclub = Fighting Arena 1, Duelling Law 2, Public House 3,
  House of Pleasure 4, Moonshine 5 ; Ceremonial Funerals = 2 rangs).
  `generer.py` écrit `AP_LOIS_RANGS` (scénario, arbre, loi interne, rang)
  dans `known_ap_ids.h` ; `ApRangsDeLoi` (archipelago.cpp) ; `lois_ap.cpp` :
  `RangsFixes`, `EnvoyerChecks` (rangs fixes, repli sur l'ordre des
  signatures avec journal si la loi manque à la table), `RangAffiche` et
  `PoserTexteNomCheck` (nom et icône = check fixe, signée ou non).
  L'**effet** suit toujours l'ordre des signatures (k-ième item reçu =
  effet de la k-ième loi signée).
- apworld inchangé (`data.py`, `ids.tsv` identiques) : la logique des lois
  n'exige aucun item, une région plus tardive pour un rang fixe ne peut que
  rendre la logique plus stricte. `frostpunk.apworld` n'est pas à
  reconstruire.
- Ouvert : demi-rond droit de Child Labour (Child Shelters) ni nommé ni
  signable (« juste un demi-cercle ») ; **les autres paires marchent**
  (auteur), donc pas l'icône de paire posée par-dessus. Journal antérieur :
  à la 2e construction du livre, le nœud de Child Shelters n'avait **aucun
  élément d'interface** (absent du relevé). Diagnostic ajouté (déployé) :
  `[LOIDIAG]` = arbre d'éléments des deux demi-ronds de Child Labour et de
  Food Additives (témoin sain), état et accessibilité de la loi ; `[LOIUI]
  … AUCUN element d'interface` pour un nœud sans élément.
- **Partie suivante (auteur)** : le défaut **ne s'est pas reproduit** ;
  `[LOIDIAG]` : Child Shelters état 0, accessible. Défaut **intermittent**,
  diagnostic laissé en place jusqu'à la prochaine occurrence.

### 2026-11-04 — Plus de « Tree » dans les noms affichés

- **Demande de l'auteur** : retirer le mot « Tree » des `Progressive Tree`
  dans tous les affichages.
- apworld : items renommés `<Scénario> - Progressive <arbre>` (ex. `A New
  Home - Progressive Tier`, `- Progressive Beacon`) ; **ids inchangés** (clé
  = numéro d'arbre), `known_ap_ids.h` régénéré, aucun doublon de nom.
  Tests (0.6.8) : banc 5 × 5 et 70 générations solo OK. `frostpunk.apworld`
  reconstruit dans `custom_worlds`.
- Mod (graines générées avant : le serveur garde les anciens noms) :
  `ApTexteSansTree` (archipelago.cpp) remplace « Progressive Tree » par
  « Progressive » dans toutes les popups (`AfficherNotificationAP`) et dans
  le nom du check d'après les scouts (`ApNomDuScout` : descriptions des
  technos, panneau d'info des lois, sites). Compilé, déployé, non testé.

### 2026-11-04 — Classification des arbres inversée

- **Constat de l'auteur** : Coal Thumper était `useful`. La règle du
  2026-11-03 (progression = un autre arbre dépend de lui) était **à
  l'envers** de son intention.
- **Règle de l'auteur** : les arbres **principaux** sont `progression` ; un
  arbre qui **dépend** d'un autre arbre (une de ses technos a son parent
  dans un autre arbre du scénario) est `useful`. Tier et l'arbre des
  éclaireurs restent forcés en progression. Parent de départ et condition
  d'affichage ne comptent toujours pas.
- `outils\technos_classification.py` réécrit (et noms d'items sans
  « Tree ») ; `technos_classification.tsv` et le tableau de `technos.md`
  régénérés. Progression / useful : A New Home 19 / 20, The Arks 17 / 18,
  Refugees 18 / 20, Winterhome 28 / 12, Last Autumn 16 / 16, On the Edge 14
  / 16.
- Items exigés par goulot (100 % des arbres progression de la région,
  goulots 1, 2, 3[, 4]) : A New Home 18, 11, 9 ; The Arks 17, 11, 9 ;
  Refugees 18, 11, 9 ; Winterhome 26, —, 2, 15 ; Last Autumn 11, 7, 6 ; On
  the Edge 12, 11, 5.
- Tests (0.6.8) : banc 5 × 5 et 70 générations solo OK ; ids inchangés ;
  `frostpunk.apworld` reconstruit. Commit `fdfa316`.

### 2026-11-04 — Messages des checks et des items de site (mod)

- **Problème remonté** : le message du check de quête (« Quest: FIGHT THE
  COLD ») ne dit pas ce qu'il envoie ; l'item reçu en retour (`A New Home -
  Children's Hideout Supplies`, trouvé par le slot lui-même) n'a donné
  **aucun message** (journal : `[AP RECU]` puis groupe créé, rien d'autre).
- **Causes** : `SendQuestCheck` (et `SendLawCheck`, `SendTemperatureCheck`)
  ne lisaient pas le scout ; un contenu de site / filler ne produisait
  qu'une bulle à l'arrivée du groupe, et le message réseau `ItemSend` n'est
  montré que pour les échanges avec un autre joueur (décision du
  2026-11-04).
- **Correctif (compilé, déployé, non testé)** : `TexteCheckEnvoye`
  (archipelago.cpp) → « Check sent » = `<check> -> <joueur> - <item>` pour
  les quêtes, lois et montées (comme les technos et les sites) ;
  `LivrerContenu` → message « Item received » `<item> from <joueur> -
  convoy arrives in N h` au départ du groupe.

### 2026-11-04 — Plus de locations de montée de température

- **Décision de l'auteur** : les montées ne peuvent pas être des
  locations : la frise de température est masquée, le joueur n'a plus rien
  pour situer ces checks.
- `generer.py` ne crée plus les locations `<Scénario> - Temperature Rise k`
  (ids `loc_temp|…` gardés dans `ids.tsv`, jamais retirés) ; `regions.py`
  ne les pose plus. Les **items** `Temperature Rise` restent dans le pool
  (le mod retire toujours les montées du jeu, chaque item rend un niveau) ;
  le pool s'ajuste en retirant du filler de complément. 862 locations au
  lieu de 893 ; A New Home seul : 174.
- Mod : `known_ap_ids.h` régénéré sans ces locations ; une montée retirée
  appelle encore `SendTemperatureCheck`, qui ne trouve plus de location →
  journal « aucune location AP », ni envoi ni message. Compilé, déployé.
- Tests (0.6.8) : banc 5 × 5 et 70 générations solo OK ; `frostpunk.apworld`
  reconstruit.
