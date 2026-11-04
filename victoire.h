// victoire.h
// ---------------------------------------------------------------------------
// Fin de partie : hook de FrostpunkGameStateGameplay::EndGame (FUN_141301490),
// seul point de passage de toute fin (victoire ou defaite). Toute victoire
// dans un scenario Archipelago est un EVENEMENT pour le goal, sans item
// (SendScenarioVictory, archipelago) : la difficulte est imposee a la
// creation de la partie et l'ironman ne change rien (auteur, 2026-11-04).
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

void InstallHooksVictoire(uintptr_t modBase);

// DEBUG (2026-11-01) : demande une fin de partie de test (tout thread) ;
// executee par VictoirePompe sur le thread du jeu (jeu hors pause) : la 1re
// fin de la config du jeu dont Victory vaut 'victoire', passee a EndGame.
// Maj+F9 = victoire, Ctrl+F9 = defaite. LA PARTIE SE TERMINE VRAIMENT.
void VictoireDemanderTest(bool victoire);
void VictoirePompe();
