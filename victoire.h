// victoire.h
// ---------------------------------------------------------------------------
// Fin de partie : hook de FrostpunkGameStateGameplay::EndGame (FUN_141301490),
// seul point de passage de toute fin (victoire ou defaite). Une victoire dans
// un scenario Archipelago envoie des checks selon le mode de victoire :
//   Complete : un check par scenario, quelle que soit la difficulte ;
//   General  : un check par niveau de difficulte generale (Easy, Medium,
//              Hard, Very Hard), cumulatif ; niveau general d'une difficulte
//              personnalisee = le plus faible des sous-difficultes ;
//   Detail   : un check par sous-difficulte (ligne de curseur) et par niveau,
//              cumulatif (4 checks par ligne).
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

enum class ModeVictoire { Complete = 0, General = 1, Detail = 2 };

// Mode courant (TEMPORAIRE : constante de test, viendra des options du slot).
ModeVictoire VictoireMode();

void InstallHooksVictoire(uintptr_t modBase);

// DEBUG (2026-11-01) : demande une fin de partie de test (tout thread) ;
// executee par VictoirePompe sur le thread du jeu (jeu hors pause) : la 1re
// fin de la config du jeu dont Victory vaut 'victoire', passee a EndGame.
// Maj+F9 = victoire, Ctrl+F9 = defaite. LA PARTIE SE TERMINE VRAIMENT.
void VictoireDemanderTest(bool victoire);
void VictoirePompe();
