#pragma once
// ---- recettes.h ----
#include <cstdint>
// Recettes de production (2026-11-03, cout de fabrication des ressources de
// Last Autumn pour l'apworld).
//
// F9 (thread du jeu, appele par QuetesPompe) : une ligne "[RECETTE]" par
// recette de production (nom, nom affiche, duree, ameliorations), produit et
// ingredient. Lecture seule. Format detaille dans recettes.cpp.
void RecettesReleve();
