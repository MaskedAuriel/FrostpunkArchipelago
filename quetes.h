#pragma once
// ---- quetes.h ----
#include <cstdint>
// Quetes du jeu (decompiles du 2026-11-01).
//   FrostpunkQuestsConfig (instance statique base + 0x407AE80, construite
//   par FUN_14148d280) : +0x58 HierarchicalEntryCollection<FrostpunkQuestEntry,
//   EntryDirectory<...>>, repertoires en +0x98 (pointeurs, compte +0xA0).

// F9 : demande le releve des quetes (repertoires, quetes, objectifs, effets,
// evenements qui les touchent), execute sur le thread du jeu par QuetesPompe.
void QuetesDemanderReleve();

// Thread du jeu (hook du setter de ressources, a cote des autres pompes).
void QuetesPompe();

// Trace en jeu : hook de FUN_1416e8570 (demarrer une quete), journal
// "[QUETE] DEMARRAGE ..." avec l'evenement en cours et la pile d'appels.
void InstallHooksQuetes(uintptr_t modBase);
