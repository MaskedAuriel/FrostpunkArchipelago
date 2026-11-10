# Dilemmes d'exploration à plusieurs choix

> Chantier ouvert le 2026-11-10 (demande de l'auteur) : remplacer les choix
> des dilemmes d'exploration par un seul « Archipelago Prevail », qui
> applique le choix retenu ci-dessous. Relevé automatique `[SCONT]`
> (sites_contenu.cpp) dans les 6 scénarios, scénarios débloqués
> temporairement (`TOUS_DEBLOQUES_TEMPORAIRE`, scenarios_ap.cpp, à remettre
> à false). Entre parenthèses : effets de chaque choix (classes du jeu).
> `DestroyStationingExpedition` (éclaireurs tués) est déjà neutralisé par le
> mod depuis le 2026-11-06 (`hkDetruireExpedition`). `OrderStationingExpedition
> ToPerformTask` = l'expédition récupère / escorte (le contenu du site).
>
> **Choix à forcer : à remplir par l'auteur** (numéro du choix pour chaque
> dilemme).
### A New Home (42 sites relevés)

  - **Site_058_C1** — dilemme `[Frostland] C1. Camp Compass`
    1. Escort the scientists to our city  _(OrderStationingExpeditionToPerformTask)_
    2. Direct the scientists to our city  _(OrderStationingExpeditionToPerformTask)_
    3. Let the scientists go their way  _(OrderStationingExpeditionToPerformTask)_
  - **Site_013_Observatory_Ruins** — dilemme `[Frostland] 013 Observatory Ruins`
    1. Escort the survivors to the city  _(OrderStationingExpeditionToPerformTask)_
    2. Send the survivors to the city  _(OrderStationingExpeditionToPerformTask)_
    3. Leave the survivors to their fate  _(OrderStationingExpeditionToPerformTask)_
  - **Site_021_Rebels_Colony** — dilemme `[Frostland] 021 Rebels Colony`
    1. Escort the exiles to the city  _(OrderStationingExpeditionToPerformTask)_
    2. Leave the exiles to their fate  _(OrderStationingExpeditionToPerformTask)_
    3. Rob the exiles of their supplies  _(OrderStationingExpeditionToPerformTask)_
  - **Site_016_Tesla_City** — dilemme `[Frostland] 016 Tesla City`
    1. Risk entering the city  _(DestroyStationingExpedition)_
    2. Stay away from the city  _(OrderStationingExpeditionToPerformTask)_
  - **Site_022_Burned_Bridge** — dilemme `[Frostland] 022 Steel Bridge`
    1. Send it to the city  _(OrderStationingExpeditionToPerformTask)_
    2. Dismantle it  _(OrderStationingExpeditionToPerformTask)_
  - **Site_014_Fugitives_Camp** — dilemme `[Frostland] 014 Workers Convoy`
    1. Escort the survivors to the city  _(OrderStationingExpeditionToPerformTask)_
    2. Send the survivors to the city  _(OrderStationingExpeditionToPerformTask)_
    3. Leave the survivors to their fate  _(OrderStationingExpeditionToPerformTask)_
  - **Site_049_A0** — dilemme `[Frostland] A0. Ice Crevasse`
    1. Bring them to our city  _(OrderStationingExpeditionToPerformTask)_
    2. Leave them to their fate  _(OrderStationingExpeditionToPerformTask)_
  - **Site_057_C0** — dilemme `[Frostland] C0. Deep Hollow`
    1. Bring the wounded to our city  _(OrderStationingExpeditionToPerformTask)_
    2. Leave them to die  _(OrderStationingExpeditionToPerformTask)_
  - **Site_017_Inventors_End** — dilemme `[Frostland] 017 Inventors End`
    1. Talk to them  _(OrderStationingExpeditionToPerformTask)_
    2. Attack them  _(OrderStationingExpeditionToPerformTask)_
  - **Site_050_A1** — dilemme `[Frostland] A1. Camp Vulcan`
    1. Escort the scientists to our city  _(OrderStationingExpeditionToPerformTask)_
    2. Direct the scientists to our city  _(OrderStationingExpeditionToPerformTask)_
    3. Let the scientists go their way  _(OrderStationingExpeditionToPerformTask)_
  - **Site_018_Fishermen_Village** — dilemme `[Frostland] 018 Fishermen Village`
    1. Leave the village intact  _(OrderStationingExpeditionToPerformTask)_
    2. Dismantle the huts for Wood  _(OrderStationingExpeditionToPerformTask)_
  - **Site_010_Unfinished_Shelter** — dilemme `[Frostland] 010 Children Hideout`
    1. Escort the children to the city  _(OrderStationingExpeditionToPerformTask)_
    2. Send the children to the city  _(OrderStationingExpeditionToPerformTask)_
    3. Leave the children to their fate  _(OrderStationingExpeditionToPerformTask)_
  - **Site_053_B0** — dilemme `[Frostland] B0. Snow Burrows`
    1. Bring the sick to our city  _(OrderStationingExpeditionToPerformTask)_
    2. Leave them to die  _(OrderStationingExpeditionToPerformTask)_
  - **Site_015_Coal_Mine** — dilemme `[Frostland] 015 Coal Mine`
    1. Only take the stored coal  _(OrderStationingExpeditionToPerformTask)_
    2. Dismantle the machines for resources  _(OrderStationingExpeditionToPerformTask)_
  - **Site_001_Lost_Expedition** — dilemme `[Frostland] 001 Lost Expedition`
    1. Escort the survivors to the city  _(OrderStationingExpeditionToPerformTask)_
    2. Send the survivors to the city  _(OrderStationingExpeditionToPerformTask)_
  - **Site_012gc_Bear_Cave** — dilemme `[Frostland] 012gc Bear Cave`
    1. Leave the bears alone  _(OrderStationingExpeditionToPerformTask)_
    2. Risk attacking the bears  _(OrderStationingExpeditionToPerformTask, DestroyStationingExpedition)_
  - **Site_007_City_Ruins** — dilemme `[Frostland] 007 Winterhome Ruins`
    1. We're on our own  _(OrderStationingExpeditionToPerformTask, ModifyCamera, Timed, CreateMessage, SetVariable, Select, AgentLabelCheckExpression, AddAgentLabel)_
    2. May they rest in peace  _(OrderStationingExpeditionToPerformTask)_
  - **Site_009_Geothermal_Vent** — dilemme `[Frostland] 009 Freshwater Springs`
    1. Escort the survivors to the city  _(OrderStationingExpeditionToPerformTask)_
    2. Send the survivors to the city  _(OrderStationingExpeditionToPerformTask)_
    3. Leave the survivors to their fate  _(OrderStationingExpeditionToPerformTask)_

### The Arks (31 sites relevés)

  - **[NM]NewManchester** — dilemme `[Frostland] New Manchester`
    1. We will try to help them  _(Timed, StartQuest, AddUnlock)_
    2. We must focus on our own survival  _(Timed, Switch, CreateDilemma, CreateMessage, SetVariable)_
  - **[NM]NewManchester** — dilemme `[NM] NoHelp - 01 begging for help - survivor alive`
    1. We will try to help them  _(Predefined, SetVariable, StartQuest)_
    2. Our mission comes first  _(Predefined, Timed, Switch, CurrentTimeComparisonExpression, Select, CreateWorldMapEntity, ApplyTag, CreateDilemma)_
  - **[NM]NewManchester** — dilemme `[NM] NoHelp - 01 begging for help - survivor dead`
    1. We will try to help them  _(Predefined, SetVariable, StartQuest)_
    2. Our mission comes first  _(Predefined, Timed, Switch, CurrentTimeComparisonExpression, Select, CreateWorldMapEntity, ApplyTag, CreateDilemma)_

### The Refugees (36 sites relevés)

  - **[t-plain][steamcore]Iron_Bank** — dilemme `[Lore] Iron Bank`
    1. Leave the treasure intact
    2. Take whatever we can use  _(OrderStationingExpeditionToPerformTask)_
  - **[ref]Red_Flag_Camp** — dilemme `[Survivors] Red Flag Camp`
    1. Honour their request  _(OrderStationingExpeditionToPerformTask)_
    2. Save them against their will  _(OrderStationingExpeditionToPerformTask)_
  - **Site_015_Coal_Mine** — dilemme `[Frostland] 015 Coal Mine`
    1. Only take the stored Coal  _(OrderStationingExpeditionToPerformTask)_
    2. Dismantle the machines for resources  _(OrderStationingExpeditionToPerformTask)_
  - **[ref]White_Pass** — dilemme `White Pass`
    1. Escort the refugees to the city  _(OrderStationingExpeditionToPerformTask)_
    2. Send the survivors to the city  _(OrderStationingExpeditionToPerformTask)_
    3. Leave the refugees to their fate  _(OrderStationingExpeditionToPerformTask)_
  - **[t-plain]Hangmans_Hill** — dilemme `[Lore] Hangman's Hill`
    1. Bury the poor man with his tools
    2. Bury the poor man, but take his tools  _(OrderStationingExpeditionToPerformTask)_
  - **[ref]Trail_Bend** — dilemme `[Survivors] Trail Bands`
    1. Escort the refugees to the city  _(OrderStationingExpeditionToPerformTask)_
    2. Send the survivors to the city  _(OrderStationingExpeditionToPerformTask)_
    3. Leave the refugees to their fate  _(OrderStationingExpeditionToPerformTask)_
  - **[ref]Black_Spire** — dilemme `[Survivors] Black Spire`
    1. Escort the refugees to the city  _(OrderStationingExpeditionToPerformTask)_
    2. Send the survivors to the city  _(OrderStationingExpeditionToPerformTask)_
    3. Leave the refugees to their fate  _(OrderStationingExpeditionToPerformTask)_
  - **[outpost]Fishermen_Village** — dilemme `[Outpost] Fishermen Village`
    1. Repair the ice drill  _(OrderStationingExpeditionToPerformTask)_
    2. Dismantle the ice drill  _(OrderStationingExpeditionToPerformTask)_

### Fall of Winterhome (26 sites relevés)

  - **[lore03]_Steel_Bridge** — dilemme `[lore03] Steel Bridge`
    1. Send the automaton to the city  _(OrderStationingExpeditionToPerformTask, Select, UnlockCheckExpression, AbandonOutpost, DismantleOutpost, AddUnlock)_
    2. Leave the automaton on duty
  - **[lore04]_Storm_Shelter** — dilemme `[lore04] Storm Shelter`
    1. Send the scientists back to the city  _(OrderStationingExpeditionToPerformTask, AddUnlock)_
    2. Bring the scientists to the city  _(OrderStationingExpeditionToPerformTask, AddUnlock)_
    3. Let them carry on with their exploration
  - **[lore02]_Coal_Mine** — dilemme `[lore02] Coal Mine`
    1. Send the mine crew to the city
    2. Bring the mine crew to the city
    3. Leave the crew in the mine
  - **[lore01]_Weather_Station** — dilemme `[lore01] Weather Station`
    1. Send the engineers to the city  _(OrderStationingExpeditionToPerformTask, AddUnlock, Timed, Select, CreateDilemma)_
    2. Bring the engineers to the city  _(OrderStationingExpeditionToPerformTask, AddUnlock, Timed, Select, CreateDilemma)_
    3. Leave the engineers at their post
  - **[lore12]_Last_Stand** — dilemme `[lore12] Last Stand`
    1. Take only what can be safely reached  _(OrderStationingExpeditionToPerformTask)_
    2. Risk carrying out as much as you can  _(OrderStationingExpeditionToPerformTask, DestroyStationingExpedition)_
    3. Leave the trailer intact
  - **[lore05]_Trail_South** — dilemme `[lore05] Trail South`
    1. Send the scouts south  _(DestroyStationingExpedition)_
    2. Return to the city  _(OrderStationingExpeditionToPerformTask)_

### The Last Autumn (31 sites relevés)

  - **61. Workers Camp** — dilemme `61. Workers Camp`
    1. Leave them  _(Switch, ProgressCustomQuestObjective)_
    2. Lead them to our camp  _(OrderStationingExpeditionToPerformTask, Switch, ProgressCustomQuestObjective)_
    3. Tell them to stay put
  - **66. Polar Bear Cave** — dilemme `66. Polar Bear Cave`
    1. Search the cave  _(DestroyStationingExpedition)_
    2. Leave the place
  - **46. EIC Steam Cores Store** — dilemme `46. EIC Steam Cores Store`
    1. Nod and sign the papers  _(OrderStationingExpeditionToPerformTask, DestroyStationingExpedition)_
    2. Explain the misunderstanding
  - **53. Goods Station** — dilemme `53. Goods Station`
    1. Deceive them and take their food  _(OrderStationingExpeditionToPerformTask, DestroyStationingExpedition)_
    2. Tell them the truth
  - **62. Transport Lost** — dilemme `62. Transport Lost`
    1. Take the crates  _(OrderStationingExpeditionToPerformTask, AddUnlock, FinishQuest)_
    2. They're not ours to take
  - **65. Generator Gamma** — dilemme `65. Generator Gamma`
    1. Take them to our camp  _(OrderStationingExpeditionToPerformTask)_
    2. Take only the resources  _(OrderStationingExpeditionToPerformTask)_
  - **63. Convoy in Distress** — dilemme `63. Convoy in Distress`
    1. Return the crates  _(Select, ChangeExpeditionResources)_
    2. Keep the resources
    3. Tell them where the crates are
  - **63. Convoy in Distress** — dilemme `63. Convoy in Distress (Winter)`
    1. Return the crates  _(Select, ChangeExpeditionResources)_
    2. Keep the resources
    3. Tell them where the crates are

### On the Edge (45 sites relevés)

  - **31. Ghost Ship** — dilemme `31. Ghost Ship`
    1. Take resources  _(OrderStationingExpeditionToPerformTask)_
    2. Leave resources  _(OrderStationingExpeditionToPerformTask)_
  - **12. City of Rats** — dilemme `12. City of Rats`
    1. Search the place for resources  _(AddUnlock, OrderStationingExpeditionToPerformTask, DestroyStationingExpedition)_
    2. Leave the place
  - **22. Fortified Hut** — dilemme `22. Fortified Hut`
    1. Escort the survivors to our outpost  _(OrderStationingExpeditionToPerformTask)_
    2. Send the survivors to our outpost  _(OrderStationingExpeditionToPerformTask)_
    3. Decide their fate later
  - **06. Storm Watch** — dilemme `06. Storm Watch`
    1. Escort the survivors to our outpost  _(OrderStationingExpeditionToPerformTask)_
    2. Send the survivors to our outpost  _(OrderStationingExpeditionToPerformTask)_
    3. Decide their fate later
  - **15. Distant Colony** — dilemme `15. Distant Colony`
    1. Offer them escort to our outpost  _(AddUnlock, OrderStationingExpeditionToPerformTask)_
    2. Leave the place
  - **27. Steel Bridge** — dilemme `27. Steel Bridge`
    1. Leave and let them work
    2. Stop them  _(AddUnlock, DestroyStationingExpedition)_
    3. Chase them away and dismantle the bridge  _(AddUnlock, OrderStationingExpeditionToPerformTask, DestroyStationingExpedition)_


---

## 2026-11-10 — Conséquences décodées de chaque choix

Demande de l'auteur : « les conséquences de chaque choix », puis décider
d'**une règle générale** avant tout code.

**Décodage de la tâche** (`OrderStationingExpeditionToPerformTaskEffect`,
entier `+0x18`) : Ghidra `FUN_14168f630` (application de l'effet) → appel
`FUN_14189c260(systeme, expedition, tache)` ; noms des 15 valeurs dans la
table `PTR_s_Retrieve_base_resources_141ede5f0` (assert `0xe < valeur`) :

| Valeur | Tâche |
|---|---|
| 0 | Retrieve base resources (absent du journal : `ChampsBruts` saute les zéros) |
| 1 | Retrieve bonus resources |
| 2 | Retrieve both resources |
| 3 | Retrieve technologies |
| 4 | Retrieve artifacts |
| 5 | Retrieve survivors |
| 6 | Reveal (effet initial de tous les dilemmes) |
| 7 | Build outpost |
| 8 | Build settlement |
| 9 | Build settlement road |
| 10 | Send survivors (les survivants partent seuls, l'expédition reste libre) |
| 11 | Escort survivors (l'expédition les ramène) |
| 12 | Abandon survivors (survivants perdus) |
| 13 | Dismantle outpost |
| 14 | Hide in shelter |

Script : `outils\dilemmes_consequences.py logs\console.log` (relit les
lignes `[SCONT]` D/C/E). « résultat N » = issues **aléatoires** du choix
(`choix +0x90`) ; probabilités **non relevées**. `ECLAIREURS TUES` est déjà
neutralisé par le mod (`hkDetruireExpedition`) : l'issue ne rapporte alors
rien (sauf Bear Cave, où les survivants partent quand même).

Constats :
- **Aucun choix ne touche l'espoir ni le mécontentement** par ses effets
  directs (pas de `ChangeHope`/`ChangeDiscontent`) ; seules les Arks ont
  des `SocialComments` (dilemmes enchaînés de New Manchester).
- Effets non décodés : quel déblocage pour `AddUnlock` (Winterhome lore01,
  lore03, lore04 ; On the Edge 12, 15, 27 ; Last Autumn 62), texte des
  `CreateMessage`, valeurs des `SetVariable`, objectifs des quêtes.
- Tesla City choix 1 : résultat 1 sans effet relevé (peut-être rien, ou un
  effet hors des tableaux visités).
- Winterhome `[lore02] Coal Mine` : aucun effet relevé sur les 3 choix
  (effets probablement portés ailleurs : à vérifier si l'auteur y tient).
- Ghost Ship (On the Edge) : les 2 choix ont la même tâche (Retrieve base
  resources) ; la différence est ailleurs (non relevée).


### A New Home

- **Site_058_C1** - `[Frostland] C1. Camp Compass` (New priorities)
  1. Escort the scientists to our city : Tache[Escort survivors]
  2. Direct the scientists to our city : Tache[Send survivors]
  3. Let the scientists go their way : Tache[Abandon survivors]
- **Site_013_Observatory_Ruins** - `[Frostland] 013 Observatory Ruins` (The sun is dimming!)
  1. Escort the survivors to the city : Tache[Escort survivors]
  2. Send the survivors to the city : Tache[Send survivors]
  3. Leave the survivors to their fate : Tache[Abandon survivors]
- **Site_021_Rebels_Colony** - `[Frostland] 021 Rebels Colony` (Dying exiles)
  1. Escort the exiles to the city : Tache[Escort survivors]
  2. Leave the exiles to their fate : Tache[Abandon survivors]
  3. Rob the exiles of their supplies : Tache[Retrieve bonus resources], Tache[Abandon survivors]
- **Site_016_Tesla_City** - `[Frostland] 016 Tesla City` (Thunder unleashed)
  1. Risk entering the city : resultat 2 : ECLAIREURS TUES (neutralise)
  2. Stay away from the city : Tache[Dismantle outpost]
- **Site_022_Burned_Bridge** - `[Frostland] 022 Steel Bridge` (A functional automaton!)
  1. Send it to the city : Tache[Send survivors]
  2. Dismantle it : Tache[Retrieve bonus resources], Tache[Abandon survivors]
- **Site_014_Fugitives_Camp** - `[Frostland] 014 Workers Convoy` (They sought Winterhome)
  1. Escort the survivors to the city : Tache[Escort survivors]
  2. Send the survivors to the city : Tache[Send survivors]
  3. Leave the survivors to their fate : Tache[Abandon survivors]
- **Site_049_A0** - `[Frostland] A0. Ice Crevasse` (On the verge of death)
  1. Bring them to our city : Tache[Escort survivors]
  2. Leave them to their fate : Tache[Abandon survivors]
- **Site_057_C0** - `[Frostland] C0. Deep Hollow` (A long way down)
  1. Bring the wounded to our city : Tache[Escort survivors]
  2. Leave them to die : Tache[Abandon survivors]
- **Site_017_Inventors_End** - `[Frostland] 017 Inventors End` (A few tired men)
  1. Talk to them : Tache[Escort survivors]
  2. Attack them : Tache[Abandon survivors]
- **Site_050_A1** - `[Frostland] A1. Camp Vulcan` (Ever the sceptic)
  1. Escort the scientists to our city : Tache[Escort survivors]
  2. Direct the scientists to our city : Tache[Send survivors]
  3. Let the scientists go their way : Tache[Abandon survivors]
- **Site_018_Fishermen_Village** - `[Frostland] 018 Fishermen Village` (No place for the weak)
  1. Leave the village intact : Tache[Retrieve base resources]
  2. Dismantle the huts for Wood : Tache[Retrieve both resources], Tache[Dismantle outpost]
- **Site_010_Unfinished_Shelter** - `[Frostland] 010 Children Hideout` (Little survivors)
  1. Escort the children to the city : Tache[Escort survivors]
  2. Send the children to the city : Tache[Send survivors]
  3. Leave the children to their fate : Tache[Abandon survivors]
- **Site_053_B0** - `[Frostland] B0. Snow Burrows` (The price of survival)
  1. Bring the sick to our city : Tache[Escort survivors]
  2. Leave them to die : Tache[Abandon survivors]
- **Site_015_Coal_Mine** - `[Frostland] 015 Coal Mine` (More coal than we can use)
  1. Only take the stored coal : Tache[Retrieve base resources]
  2. Dismantle the machines for resources : Tache[Retrieve both resources], Tache[Dismantle outpost]
- **Site_001_Lost_Expedition** - `[Frostland] 001 Lost Expedition` (A happy reunion)
  1. Escort the survivors to the city : Tache[Escort survivors]
  2. Send the survivors to the city : Tache[Send survivors]
- **Site_012gc_Bear_Cave** - `[Frostland] 012gc Bear Cave` (Beset by beasts)
  1. Leave the bears alone : Tache[Abandon survivors]
  2. Risk attacking the bears : resultat 1 : Tache[Escort survivors] ; resultat 2 : Tache[Send survivors], ECLAIREURS TUES (neutralise)
- **Site_007_City_Ruins** - `[Frostland] 007 Winterhome Ruins` (The fall of Winterhome)
  1. We're on our own : Tache[Retrieve base resources], ModifyCamera, Timed, CreateMessage, SetVariable, Select, AgentLabelCheck?, AgentLabelCheck?, AddAgentLabel
  2. May they rest in peace : Tache[Retrieve base resources]
- **Site_009_Geothermal_Vent** - `[Frostland] 009 Freshwater Springs` (Degenerated, but alive)
  1. Escort the survivors to the city : Tache[Escort survivors]
  2. Send the survivors to the city : Tache[Send survivors]
  3. Leave the survivors to their fate : Tache[Abandon survivors]

### The Arks

- **[NM]NewManchester** - `[Frostland] New Manchester` (A dying city)
  1. We will try to help them : Timed, StartQuest, AddUnlock, StartQuest
  2. We must focus on our own survival : Timed, Switch, CreateDilemma, CreateDilemma, CreateMessage, SetVariable, Switch, CreateDilemma, CreateDilemma, CreateMessage, SetVariable | - > dilemme [NM] NoHelp - 01 begging for help - survivor alive choix 1 : Predefined, SetVariable, StartQuest, Predefined, SetVariable, StartQuest ; - > dilemme [NM] NoHelp - 01 begging for help - survivor alive choix 2 : Predefined, Timed, Switch, CurrentTimeComparison?, Timed, Select, CreateWorldMapEntity, ApplyTag, CreateDilemma, Predefined, Timed, Switch, CurrentTimeComparison?, Timed, Select, CreateWorldMapEntity, ApplyTag, CreateDilemma ; - > dilemme [NM] NoHelp - 01 begging for help - survivor alive choix 2 > dilemme [NM] NoHelp - 02 new refugees on horizon choix 1 : Timed, Select, Timed, CreateDilemma, SocialComments, Timed, Select, Timed, CreateDilemma, SocialComments ; - > dilemme [NM] NoHelp - 01 begging for help - survivor dead choix 1 : Predefined, SetVariable, StartQuest, Predefined, SetVariable, StartQuest ; - > dilemme [NM] NoHelp - 01 begging for help - survivor dead choix 2 : Predefined, Timed, Switch, CurrentTimeComparison?, Timed, Select, CreateWorldMapEntity, ApplyTag, CreateDilemma, Predefined, Timed, Switch, CurrentTimeComparison?, Timed, Select, CreateWorldMapEntity, ApplyTag, CreateDilemma

### The Refugees

- **[t-plain][steamcore]Iron_Bank** - `[Lore] Iron Bank` (Useless riches)
  1. Leave the treasure intact : *aucun effet*
  2. Take whatever we can use : Tache[Retrieve base resources]
- **[ref]Red_Flag_Camp** - `[Survivors] Red Flag Camp` (Dying wish)
  1. Honour their request : Tache[Abandon survivors]
  2. Save them against their will : Tache[Escort survivors]
- **Site_015_Coal_Mine** - `[Frostland] 015 Coal Mine` (More Coal than we can use)
  1. Only take the stored Coal : Tache[Retrieve base resources]
  2. Dismantle the machines for resources : Tache[Retrieve both resources], Tache[Dismantle outpost]
- **[ref]White_Pass** - `White Pass` (Led by fools)
  1. Escort the refugees to the city : Tache[Escort survivors]
  2. Send the survivors to the city : Tache[Send survivors]
  3. Leave the refugees to their fate : Tache[Abandon survivors]
- **[t-plain]Hangmans_Hill** - `[Lore] Hangman's Hill` (The engineer's curse)
  1. Bury the poor man with his tools : *aucun effet*
  2. Bury the poor man, but take his tools : Tache[Retrieve base resources]
- **[ref]Trail_Bend** - `[Survivors] Trail Bands` (Lost and found)
  1. Escort the refugees to the city : Tache[Escort survivors]
  2. Send the survivors to the city : Tache[Send survivors]
  3. Leave the refugees to their fate : Tache[Abandon survivors]
- **[ref]Black_Spire** - `[Survivors] Black Spire` (Exhausted travellers)
  1. Escort the refugees to the city : Tache[Escort survivors]
  2. Send the survivors to the city : Tache[Send survivors]
  3. Leave the refugees to their fate : Tache[Abandon survivors]
- **[outpost]Fishermen_Village** - `[Outpost] Fishermen Village` (Broken ice drill)
  1. Repair the ice drill : Tache[Retrieve base resources]
  2. Dismantle the ice drill : Tache[Retrieve both resources], Tache[Dismantle outpost]

### Fall of Winterhome

- **[lore03]_Steel_Bridge** - `[lore03] Steel Bridge` (Mechanical workforce)
  1. Send the automaton to the city : Tache[Send survivors], Select, UnlockCheck?, AbandonOutpost, DismantleOutpost, AddUnlock
  2. Leave the automaton on duty : *aucun effet*
- **[lore04]_Storm_Shelter** - `[lore04] Storm Shelter` (Call up the explorers?)
  1. Send the scientists back to the city : Tache[Send survivors], AddUnlock
  2. Bring the scientists to the city : Tache[Escort survivors], AddUnlock
  3. Let them carry on with their exploration : *aucun effet*
- **[lore02]_Coal_Mine** - `[lore02] Coal Mine` (We need Coal)
  1. Send the mine crew to the city : *aucun effet*
  2. Bring the mine crew to the city : *aucun effet*
  3. Leave the crew in the mine : *aucun effet*
- **[lore01]_Weather_Station** - `[lore01] Weather Station` (Call up the weathermen?)
  1. Send the engineers to the city : Tache[Send survivors], AddUnlock, Timed, Select, CreateDilemma
  2. Bring the engineers to the city : Tache[Escort survivors], AddUnlock, Timed, Select, CreateDilemma
  3. Leave the engineers at their post : *aucun effet*
- **[lore12]_Last_Stand** - `[lore12] Last Stand` (Too heavy a burden)
  1. Take only what can be safely reached : Tache[Retrieve base resources]
  2. Risk carrying out as much as you can : resultat 1 : Tache[Retrieve both resources] ; resultat 2 : ECLAIREURS TUES (neutralise)
  3. Leave the trailer intact : *aucun effet*
- **[lore05]_Trail_South** - `[lore05] Trail South` (Smoke to the south)
  1. Send the scouts south : ECLAIREURS TUES (neutralise)
  2. Return to the city : Tache[Retrieve base resources]

### The Last Autumn

- **61. Workers Camp** - `61. Workers Camp` (Runaway workers)
  1. Leave them : Switch, ProgressCustomQuestObjective, ProgressCustomQuestObjective
  2. Lead them to our camp : Tache[Escort survivors], Switch, ProgressCustomQuestObjective, ProgressCustomQuestObjective
  3. Tell them to stay put : *aucun effet*
- **66. Polar Bear Cave** - `66. Polar Bear Cave` (Winter has come)
  1. Search the cave : resultat 2 : ECLAIREURS TUES (neutralise), ECLAIREURS TUES (neutralise)
  2. Leave the place : *aucun effet*
- **46. EIC Steam Cores Store** - `46. EIC Steam Cores Store` (Steam cores, s'il vous plait)
  1. Nod and sign the papers : resultat 1 : Tache[Retrieve base resources], Tache[Retrieve base resources] ; resultat 2 : ECLAIREURS TUES (neutralise), ECLAIREURS TUES (neutralise)
  2. Explain the misunderstanding : *aucun effet*
- **53. Goods Station** - `53. Goods Station` (To the last can)
  1. Deceive them and take their food : resultat 1 : Tache[Retrieve base resources], Tache[Retrieve base resources] ; resultat 2 : ECLAIREURS TUES (neutralise), ECLAIREURS TUES (neutralise)
  2. Tell them the truth : *aucun effet*
- **62. Transport Lost** - `62. Transport Lost` (Finders keepers?)
  1. Take the crates : Tache[Retrieve base resources], AddUnlock, FinishQuest
  2. They're not ours to take : *aucun effet*
- **65. Generator Gamma** - `65. Generator Gamma` (I told them so)
  1. Take them to our camp : Tache[Escort survivors], Tache[Retrieve base resources]
  2. Take only the resources : Tache[Retrieve base resources]
- **63. Convoy in Distress** - `63. Convoy in Distress` (Are these yours?)
  1. Return the crates : Select, ChangeExpeditionResources, ChangeExpeditionResources
  2. Keep the resources : *aucun effet*
  3. Tell them where the crates are : *aucun effet*
- **63. Convoy in Distress** - `63. Convoy in Distress (Winter)` (Are these yours?)
  1. Return the crates : Select, ChangeExpeditionResources, ChangeExpeditionResources
  2. Keep the resources : *aucun effet*
  3. Tell them where the crates are : *aucun effet*

### On the Edge

- **31. Ghost Ship** - `31. Ghost Ship` (A cargo of shame)
  1. Take resources : Tache[Retrieve base resources]
  2. Leave resources : Tache[Retrieve base resources]
- **12. City of Rats** - `12. City of Rats` (City of Rats)
  1. Search the place for resources : AddUnlock | resultat 1 : Tache[Retrieve base resources] ; resultat 2 : ECLAIREURS TUES (neutralise)
  2. Leave the place : *aucun effet*
- **22. Fortified Hut** - `22. Fortified Hut` (Call us hope)
  1. Escort the survivors to our outpost : Tache[Escort survivors]
  2. Send the survivors to our outpost : Tache[Send survivors]
  3. Decide their fate later : *aucun effet*
- **06. Storm Watch** - `06. Storm Watch` (A few lucky men)
  1. Escort the survivors to our outpost : Tache[Escort survivors]
  2. Send the survivors to our outpost : Tache[Send survivors]
  3. Decide their fate later : *aucun effet*
- **15. Distant Colony** - `15. Distant Colony` (A burden too great)
  1. Offer them escort to our outpost : AddUnlock | resultat 1 : Tache[Escort survivors]
  2. Leave the place : *aucun effet*
- **27. Steel Bridge** - `27. Steel Bridge` (How much for the bridge)
  1. Leave and let them work : *aucun effet*
  2. Stop them : AddUnlock | resultat 2 : ECLAIREURS TUES (neutralise)
  3. Chase them away and dismantle the bridge : AddUnlock | resultat 1 : Tache[Retrieve base resources] ; resultat 2 : ECLAIREURS TUES (neutralise)

47 dilemmes

---

## 2026-11-10 — Règle retenue par l'auteur : gain maximal

**Décision de l'auteur** : le choix forcé est celle qui rapporte le plus à
la ville, dans l'ordre Escort survivors > Send survivors > Retrieve both
resources > Retrieve base resources > rien. Les choix à issue aléatoire
sont permis (mort des éclaireurs neutralisée). Les égalités et les
dilemmes de quête se tranchent à la main (**⚠** = proposition à valider).

| Scénario | Site | Dilemme | Choix forcé | Motif |
|---|---|---|---|---|
| A New Home | Site_058_C1 | C1. Camp Compass | 1 | Escort |
| A New Home | Site_013_Observatory_Ruins | 013 Observatory Ruins | 1 | Escort |
| A New Home | Site_021_Rebels_Colony | 021 Rebels Colony | 1 | Escort (> ressources bonus du 3) |
| A New Home | Site_016_Tesla_City | 016 Tesla City | **⚠ 1** | aucun gain relevé ni d'un côté ni de l'autre ; 1 = choix « aventure » (issue 1 sans effet relevé, issue 2 = mort neutralisée), 2 = Dismantle outpost seul |
| A New Home | Site_022_Burned_Bridge | 022 Steel Bridge | 1 | Send (automate) > ressources bonus |
| A New Home | Site_014_Fugitives_Camp | 014 Workers Convoy | 1 | Escort |
| A New Home | Site_049_A0 | A0. Ice Crevasse | 1 | Escort |
| A New Home | Site_057_C0 | C0. Deep Hollow | 1 | Escort |
| A New Home | Site_017_Inventors_End | 017 Inventors End | 1 | Escort |
| A New Home | Site_050_A1 | A1. Camp Vulcan | 1 | Escort |
| A New Home | Site_018_Fishermen_Village | 018 Fishermen Village | 2 | Both resources |
| A New Home | Site_010_Unfinished_Shelter | 010 Children Hideout | 1 | Escort |
| A New Home | Site_053_B0 | B0. Snow Burrows | 1 | Escort |
| A New Home | Site_015_Coal_Mine | 015 Coal Mine | 2 | Both resources |
| A New Home | Site_001_Lost_Expedition | 001 Lost Expedition | 1 | Escort |
| A New Home | Site_012gc_Bear_Cave | 012gc Bear Cave | 2 | Escort ou Send (aléatoire) > Abandon |
| A New Home | Site_007_City_Ruins | 007 Winterhome Ruins | **⚠ 1** | même tâche (base) des deux côtés ; 1 porte la suite scénarisée (message, variable, étiquettes) |
| A New Home | Site_009_Geothermal_Vent | 009 Freshwater Springs | 1 | Escort |
| The Arks | [NM]NewManchester | New Manchester | **⚠ 1** | quête d'aide + déblocage ; avec 1, les deux dilemmes « NoHelp » enchaînés n'apparaissent jamais (1 aussi par sécurité) |
| The Refugees | Iron_Bank | Iron Bank | 2 | Base > rien |
| The Refugees | Red_Flag_Camp | Red Flag Camp | 2 | Escort |
| The Refugees | Site_015_Coal_Mine | 015 Coal Mine | 2 | Both resources |
| The Refugees | White_Pass | White Pass | 1 | Escort |
| The Refugees | Hangmans_Hill | Hangman's Hill | 2 | Base > rien |
| The Refugees | Trail_Bend | Trail Bands | 1 | Escort |
| The Refugees | Black_Spire | Black Spire | 1 | Escort |
| The Refugees | [outpost]Fishermen_Village | Fishermen Village | 2 | Both resources (le 1 « répare » mais ne fait que Retrieve base) |
| Winterhome | [lore03]_Steel_Bridge | Steel Bridge | 1 | Send + déblocage |
| Winterhome | [lore04]_Storm_Shelter | Storm Shelter | 2 | Escort + déblocage |
| Winterhome | [lore02]_Coal_Mine | Coal Mine | **⚠ 2** | aucun effet relevé ; « Bring » = l'équivalent Escort des autres lore |
| Winterhome | [lore01]_Weather_Station | Weather Station | 2 | Escort + déblocage |
| Winterhome | [lore12]_Last_Stand | Last Stand | 2 | Both resources (aléatoire) > base |
| Winterhome | [lore05]_Trail_South | Trail South | 2 | Base > mort neutralisée |
| The Last Autumn | 61. Workers Camp | Workers Camp | 2 | Escort + avancée de quête |
| The Last Autumn | 66. Polar Bear Cave | Polar Bear Cave | **⚠ 1** | aucun gain relevé ni d'un côté ni de l'autre ; 1 = fouille (issue 2 = mort neutralisée) |
| The Last Autumn | 46. EIC Steam Cores Store | EIC Steam Cores Store | 1 | Base (aléatoire) > rien |
| The Last Autumn | 53. Goods Station | Goods Station | 1 | Base (aléatoire) > rien |
| The Last Autumn | 62. Transport Lost | Transport Lost | 1 | Base + déblocage + fin de quête |
| The Last Autumn | 65. Generator Gamma | Generator Gamma | 1 | Escort + base |
| The Last Autumn | 63. Convoy in Distress (×2) | Convoy in Distress | **⚠ 2** | « Return the crates » retire des ressources à l'expédition (ChangeExpeditionResources) ; 2 les garde |
| On the Edge | 31. Ghost Ship | Ghost Ship | **⚠ 1** | même tâche (base) des deux côtés ; 1 = « Take resources » |
| On the Edge | 12. City of Rats | City of Rats | 1 | Base (aléatoire) + déblocage |
| On the Edge | 22. Fortified Hut | Fortified Hut | 1 | Escort |
| On the Edge | 06. Storm Watch | Storm Watch | 1 | Escort |
| On the Edge | 15. Distant Colony | Distant Colony | 1 | Escort + déblocage |
| On the Edge | 27. Steel Bridge | Steel Bridge | 3 | Base (aléatoire) + déblocage |

---

## 2026-11-10 — Code : choix unique « Archipelago Prevail » (non testé)

Accord de l'auteur sur la table ci-dessus (7 propositions ⚠ validées).
Nouveau module `dilemmes_exploration.h/.cpp` (ajouté à
`compilationVersion.cpp.txt`), pompe `DilemmesExplorationPompe` appelée
par le hook du setter de ressources **après** `QuetesPompe` (thread du jeu,
une fois par `PartieGeneration`).

- Modification **sur place** de la définition (`FrostpunkDilemmaEntry`,
  configuration `base + 0x4069880`, répertoires `+0x130`, entrées `+0x18`) :
  le tableau des choix (`+0x78` ptr, `+0x80` compte, `+0x84` capacité) est
  remplacé par un tableau à nous (`MoteurAlloc`) d'**un seul** élément :
  copie (0x150 octets) du choix retenu, seule la clé du texte (`+0xA8`)
  change → `@AP/Prevail` (`CleMoteur`, modèle = clé d'origine du choix).
  Effets, résultats, description, texte d'effet : ceux du jeu (partagés).
- Pas de clone ni de GUID : la bulle créée par le jeu, ou relue d'une
  sauvegarde, lit la définition.
- Table `CHOIX_FORCES` (scénario interne, nom du dilemme, numéro 1..N) ;
  une définition partagée par un autre scénario retrouve son tableau
  d'origine (jamais libéré) ; définition relue par le jeu détectée (ni notre
  tableau ni l'original) → fiche oubliée.
- Texte : `hkTranslate` (hooks_loc.cpp) branche `AP/Prevail` →
  `ApLocPrevail()` (objet et chaîne à nous, en-tête cloné, compteur figé).
- Journal `[DILEXP]` : une ligne par dilemme modifié, puis
  `scenario X : N dilemme(s) a choix unique, M remis d'origine`.
- Aucune nouvelle adresse (config déjà dans `VERSION_STEAM`).
- Compilé, déployé Epic et Steam.

À vérifier en jeu : ligne `[DILEXP]` au chargement (N attendu : 18 A New
Home, 3 Arks, 8 Refugees, 6 Winterhome, 8 Last Autumn, 6 On the Edge) ;
bulle d'un site avec le seul choix « Archipelago Prevail » ; effets du choix
retenu (survivants escortés, ressources) ; choix désactivé par une condition
du jeu (si le choix retenu a des exigences non remplies, seule l'expiration
resterait). Le relevé `[DILTAB]`/`[SCONT]` d'une partie suivante du même
scénario montre déjà le choix unique (définitions modifiées).