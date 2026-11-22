// dilemmes_exploration.h
// ---------------------------------------------------------------------------
// Dilemmes d'exploration a plusieurs choix (bulle d'un site explore par les
// eclaireurs), 2026-11-10, regle de l'auteur : un seul choix, libelle
// "Archipelago Prevail", qui applique EXACTEMENT les effets d'un choix du
// jeu (regle du gain maximal, table dans .claude/chantiers/dilemmes_exploration.md).
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

// Thread du jeu (appelee par le hook du setter de ressources, apres
// QuetesPompe). A chaque partie chargee : les definitions de dilemme du
// scenario courant ne gardent que le choix retenu ; celles des autres
// scenarios retrouvent leurs choix d'origine.
void DilemmesExplorationPompe();
