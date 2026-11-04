#pragma once
// ---- sites_contenu.h ----
#include <cstdint>
// Contenu reel des sites d'exploration (2026-11-03, pour l'apworld : survivants
// et ressources de chaque site, dilemmes d'exploration compris).
//
// F9 (thread du jeu, appele par QuetesPompe) : pour chaque site de la partie,
// lignes "[SCONT]" (contenu courant du composant, tresor du gabarit, dilemmes
// d'exploration avec leurs choix et effets, effets instantanes), puis une ligne
// "[SRES]" par ressource de l'economie (nom interne, GUID, nom affiche).
// Lecture seule. Format detaille dans sites_contenu.cpp.
void SitesContenuReleve();

// FrostpunkSurvivorsEntry (0x24 : compte +0x00 mis a 0, classe +0x04,
// statistiques +0x14) de la classe sociale 'classe' ("workers", "engineers",
// "children", "automaton"...), copiee d'un site de la carte (composant,
// tresor ou survivants aleatoires du gabarit). Thread du jeu. false si aucun
// site de la partie ne porte cette classe.
bool SitesEntreeSurvivants(const char* classe, uint8_t entree[0x24]);
