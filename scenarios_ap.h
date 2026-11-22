// scenarios_ap.h
// ---------------------------------------------------------------------------
// Deblocage des scenarios par Archipelago : au menu principal, un scenario
// que le mod n'a pas debloque est affiche et traite exactement comme un
// scenario verrouille par le jeu (bouton grise, preset LOCKED, non lancable).
// Un scenario n'est debloque QUE par la reception de son item
// (ReceiveScenarioUnlock, archipelago.h -> ScenarioDebloquer).
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

// Source de verite du deblocage, par nom interne du scenario (DEFAULT,
// TechHaven, REFUGEES, WINTERHOME, BUILDERS, Reconquest). Un scenario hors de
// cette liste (Endless, debug) n'est jamais touche par le mod.
bool ScenarioDebloque(const char* nomInterne);

// Item recu : debloque le scenario. 1 = debloque, 0 = deja debloque,
// -1 = nom inconnu. Tout thread.
int ScenarioDebloquer(const char* nomInterne);

// Changement de session Archipelago (ApDefinirSession) : tous les scenarios
// AP redeviennent verrouilles, avant que les items de la session ne soient
// rejoues. Tout thread.
void ScenariosReverrouiller();

// Difficulte imposee (.claude/chantiers/difficulte.md) : niveau general cible
// des parties AP (0 Easy .. 3 Very Hard), -1 si aucun. Reglee par le
// slot_data (goal_difficulty) ; Hard avant (TEMPORAIRE).
int DifficulteCible();
void DifficulteReglerCible(int niveau);

// Filet au lancement d'une partie neuve : appele par le hook d'ApplyConfig
// (scenario.cpp) avant l'original, 'panneau' = panneau des scenarios.
void DifficulteImposerAuLancement(uintptr_t panneau);

void InstallHooksScenarios(uintptr_t modBase);
