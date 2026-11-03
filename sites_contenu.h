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
