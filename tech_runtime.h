// tech_runtime.h
// ---------------------------------------------------------------------------
// La VERITE LOGIQUE du mod, independante de l'etat du jeu.
//   researched : le joueur a termine la recherche localement
//                -> gouverne l'etiquette [Researched] et la recherchabilite
//   received   : le serveur Archipelago a envoye l'item
//                -> gouverne l'ETAT JEU (donc l'effet / le deblocage)
//
// Stockage EN MEMOIRE seulement (non persistant) : au chargement d'une partie,
// ces donnees seront reconstruites depuis le serveur Archipelago.
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
