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

// TEMPORAIRE : simule la reception des items de scenario de depart
// (REFUGEES), en attendant le serveur.
void ScenariosRecevoirItemsDeTest();

void InstallHooksScenarios(uintptr_t modBase);
