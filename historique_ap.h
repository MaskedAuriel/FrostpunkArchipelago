// historique_ap.h
// ---------------------------------------------------------------------------
// Historique de la session Archipelago en jeu (.claude/chantiers/historique.md) :
// un bouton dans le HUD ouvre un panneau dedie (clone d'elements du jeu) qui
// liste les items recus et les checks envoyes de la session (graine + slot).
//
// Etape 1 (2026-11-21) : RELEVE seulement. Le hook de la creation de l'ecran
// principal (FUN_14196b2d0, FrostpunkUIGameplayController, "UI/MainScreen")
// garde la racine FRAME du HUD ; 5 s plus tard, sur le thread du jeu
// (HistoriquePompe), le journal [HIST] decrit l'arbre du HUD et les recettes
// des panneaux a defilement candidats, pour choisir les modeles a cloner.
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

void InstallHistoriqueAP(uintptr_t modBase);
// Thread du jeu (hook du setter de ressources, avec les autres pompes).
void HistoriquePompe();
// Un noeud de l'arbre des technos vient d'etre construit (hook de
// IconSubManager.cpp, thread de l'interface) : anneau de la legende de
// l'arbre, copie d'un vrai noeud (les pompes ne tournent pas en pause).
void HistoriqueNoeudTechnoConstruit();
// Le livre des lois vient d'etre construit (hook de FUN_141bbc320, lois_ap.cpp,
// thread de l'interface ; 'livre' = le panneau) : legende du livre, a gauche
// de CUSTOM_CLOSE_BUTTON, une fois par panneau.
void HistoriqueLivreConstruit(uintptr_t livre);
