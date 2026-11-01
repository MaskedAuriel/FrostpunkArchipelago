// tech_runtime.h
// ---------------------------------------------------------------------------
// La VERITE LOGIQUE du mod, independante de l'etat du jeu.
//   researched : le joueur a termine la recherche localement
//                -> gouverne l'etiquette [Researched] et la recherchabilite
//   received   : le serveur Archipelago a envoye l'item
//                -> gouverne l'ETAT JEU (donc l'effet / le deblocage)
//
// Stockage en memoire, sauvegarde localement par partie (tech_brain.cpp,
// mods\ap_technos_<GameId>.txt, demande de l'auteur du 2026-10-29 : sauvegarde
// locale PRIORITAIRE, le serveur Archipelago viendra la completer).
// ---------------------------------------------------------------------------
#pragma once
#include "mod_common.h"

struct TechRuntime {
    uint64_t hash;
    bool     researched;
    bool     received;
    bool     used;        // emplacement occupe dans le tableau
};

// Recupere l'entree d'une techno, la CREE au premier acces (booleens a false).
// Renvoie nullptr si le tableau est plein.
TechRuntime* RT_Get(uint64_t hash);

// Peut-on (re)lancer la recherche de cette techno ? Regle : tant que
// researched == false. Une techno inconnue est autorisee par defaut.
bool RT_CanResearch(uint64_t hash);

// Nombre d'entrees actuellement enregistrees (diagnostic).
int RT_Count();

// Entree i (0..RT_Count()-1), ou nullptr.
TechRuntime* RT_At(int i);

// Oublie tout (changement de partie, avant de relire la sauvegarde locale).
void RT_Reset();
